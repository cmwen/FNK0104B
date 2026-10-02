"""Regression checks for quiet idle operation and asynchronous transport events."""
import json
import pathlib
import queue
import socket
import struct
import sys
import threading
import unittest
from concurrent.futures import ThreadPoolExecutor
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import server
from test_server import FakeAppServer


def status_responses(active=False):
    threads = [{"id": "a", "status": {"type": "active"}}] if active else []
    return [{"data": threads}, {"data": []}, {"rateLimits": {
        "primary": {"usedPercent": 20, "windowDurationMins": 300}}}]


class IdleStatusTests(unittest.TestCase):
    def setUp(self):
        self.now = 0

    def bridge(self, app):
        return server.Bridge(app, idle_refresh=300, active_refresh=15, clock=lambda: self.now)

    def test_idle_board_polls_share_snapshot_until_fallback_deadline(self):
        app = FakeAppServer(status_responses() + status_responses())
        bridge = self.bridge(app)
        first = bridge.status()
        for self.now in range(5, 300, 5):
            status = bridge.status()
            self.assertEqual(status["updated_at"], first["updated_at"])
            self.assertEqual(status["cache_age_seconds"], self.now)
        self.assertEqual(len(app.calls), 3)
        self.now = 300
        self.assertEqual(bridge.status()["cache_age_seconds"], 0)
        self.assertEqual(len(app.calls), 6)

    def test_active_fallback_does_not_requery_quota(self):
        app = FakeAppServer(status_responses(active=True) + status_responses()[:2])
        bridge = self.bridge(app)
        self.assertEqual(bridge.status()["total_agents"], 1)
        self.now = 14
        self.assertEqual(bridge.status()["total_agents"], 1)
        self.now = 15
        self.assertEqual(bridge.status()["total_agents"], 0)
        self.assertEqual([method for method, _ in app.calls].count("account/rateLimits/read"), 1)

    def test_event_invalidates_idle_cache_without_waiting_five_minutes(self):
        app = FakeAppServer(status_responses() + status_responses(active=True)[:2])
        bridge = self.bridge(app)
        bridge.status()
        app._dispatch({"method": "thread/status/changed", "params": {"threadId": "a", "status": {"type": "active"}}})
        self.now = 5
        self.assertEqual(bridge.status()["agents"][0]["status"], "running")
        self.assertEqual(len(app.calls), 5)

    def test_quota_push_updates_cached_response_without_rpc(self):
        app = FakeAppServer(status_responses())
        bridge = self.bridge(app)
        bridge.status()
        with patch.object(server.time, "monotonic", return_value=5):
            app._dispatch({"method": "account/rateLimits/updated", "params": {"rateLimits": {
                "limitId": "codex", "primary": {"usedPercent": 42, "windowDurationMins": 300}}}})
        self.now = 5
        status = bridge.status()
        self.assertEqual(status["codex"]["usage"]["five_hour"]["used_percent"], 42)
        self.assertEqual(len(app.calls), 3)
        app._dispatch({"method": "account/rateLimits/updated", "params": {"rateLimits": {
            "limitId": "other", "primary": {"usedPercent": 99, "windowDurationMins": 300}}}})
        self.assertEqual(bridge.status()["codex"]["usage"]["five_hour"]["used_percent"], 42)

    def test_auth_change_drops_previous_account_quota(self):
        app = FakeAppServer(status_responses() + [{"data": []}, {"data": []}, server.BridgeError("signed out")])
        bridge = self.bridge(app)
        bridge.status()
        app._dispatch({"method": "account/updated", "params": {"authMode": None}})
        status = bridge.status()
        self.assertEqual(status["integration"], "degraded")
        self.assertIsNone(status["codex"]["usage"]["five_hour"]["used_percent"])

    def test_old_quota_notification_cannot_replace_a_newer_fallback_query(self):
        refreshed = status_responses()
        refreshed[2]["rateLimits"]["primary"]["usedPercent"] = 99
        app = FakeAppServer(status_responses() + status_responses()[:2] + refreshed)
        bridge = self.bridge(app)
        bridge.status()
        with patch.object(server.time, "monotonic", return_value=5):
            app._dispatch({"method": "account/rateLimits/updated", "params": {"rateLimits": {
                "limitId": "codex", "primary": {"usedPercent": 42, "windowDurationMins": 300}}}})
        self.now = 5
        self.assertEqual(bridge.status()["codex"]["usage"]["five_hour"]["used_percent"], 42)
        self.now = 300
        bridge.status()
        self.now = 305
        self.assertEqual(bridge.status()["codex"]["usage"]["five_hour"]["used_percent"], 99)
        self.now = 306
        self.assertEqual(bridge.status()["codex"]["usage"]["five_hour"]["used_percent"], 99)
        self.assertEqual(len(app.calls), 8)

    def test_socket_loss_is_not_hidden_by_cached_connected_status(self):
        app = FakeAppServer(status_responses() + [server.BridgeError("socket missing", "app_server_unavailable")])
        bridge = self.bridge(app)
        self.assertEqual(bridge.status()["integration"], "connected")
        app.connection_lost.set()
        with self.assertRaises(server.BridgeError):
            bridge.status()
        with self.assertRaises(server.BridgeError):
            bridge.status()
        self.assertEqual(len(app.calls), 4)

    def test_unavailable_backoff_and_recovery(self):
        app = FakeAppServer([server.BridgeError("offline", "app_server_unavailable")] + status_responses())
        bridge = self.bridge(app)
        for self.now in (0, 5, 29):
            with self.assertRaises(server.BridgeError):
                bridge.status()
        self.assertEqual(len(app.calls), 1)
        self.now = 30
        self.assertEqual(bridge.status()["integration"], "connected")

    def test_concurrent_board_requests_share_one_refresh_and_cannot_mutate_cache(self):
        app = FakeAppServer(status_responses())
        bridge = self.bridge(app)
        with ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(lambda _: bridge.status(), range(16)))
        self.assertEqual(len(app.calls), 3)
        results[0]["codex"]["usage"]["five_hour"]["used_percent"] = 99
        self.assertEqual(bridge.status()["codex"]["usage"]["five_hour"]["used_percent"], 20)

    def test_command_invalidates_snapshot(self):
        app = FakeAppServer(status_responses() + [{"thread": {"id": "a"}}, {"turn": {"id": "t"}}] + status_responses(active=True)[:2])
        bridge = self.bridge(app)
        bridge.status()
        bridge.command("Check status")
        self.assertEqual(bridge.status()["total_agents"], 1)

    def test_touch_wake_forces_fresh_threads_and_quota(self):
        refreshed = status_responses(active=True)
        refreshed[2]["rateLimits"]["primary"]["usedPercent"] = 77
        app = FakeAppServer(status_responses() + refreshed)
        bridge = self.bridge(app)
        self.assertEqual(bridge.status()["total_agents"], 0)
        self.now = 5
        status = bridge.status(force=True)
        self.assertEqual(status["total_agents"], 1)
        self.assertEqual(status["codex"]["usage"]["five_hour"]["used_percent"], 77)
        self.assertEqual(status["cache_age_seconds"], 0)
        self.assertEqual(len(app.calls), 6)

    def test_refresh_query_requires_authentication_and_rejects_unknown_options(self):
        class Bridge:
            def status(self, force=False):
                return {"force": force}
        handler = server.make_handler(Bridge(), "key")
        instance = object.__new__(handler)
        replies = []
        instance._json = lambda code, value: replies.append((code, value))
        instance.path = "/v1/status?refresh=1"
        instance.headers = {}
        instance.do_GET()
        self.assertEqual(replies[-1][0], 401)
        instance.headers = {"X-Monitor-Key": "key"}
        instance.do_GET()
        self.assertEqual(replies[-1], (200, {"force": True}))
        instance.path = "/v1/status?refresh=0"
        instance.do_GET()
        self.assertEqual(replies[-1][0], 400)

    def test_event_during_refresh_is_not_swallowed(self):
        app = FakeAppServer(status_responses() + status_responses(active=True)[:2])
        call = app.call

        def notify_during_query(method, params=None):
            result = call(method, params)
            if method == "account/rateLimits/read":
                app._dispatch({"method": "thread/status/changed", "params": {"threadId": "a"}})
            return result
        app.call = notify_during_query
        bridge = self.bridge(app)
        self.assertEqual(bridge.status()["total_agents"], 0)
        self.assertEqual(bridge.status()["total_agents"], 1)


class EventReaderTests(unittest.TestCase):
    def setUp(self):
        self.app = server.AppServer(transport="unix", timeout=0.1)
        self.client, self.peer = socket.socketpair()
        self.peer.settimeout(2)
        self.app.sock = self.client
        self.app.initialized = True
        self.app.reader_token = self.token = object()
        self.worker = threading.Thread(target=self.app._read_transport,
            args=(self.token, self.app.responses, None, self.client, self.app.ws_buffer), daemon=True)
        self.worker.start()

    def tearDown(self):
        self.app.close()
        self.peer.close()
        self.worker.join(timeout=2)
        self.assertFalse(self.worker.is_alive())

    def send(self, message):
        data = json.dumps(message).encode()
        header = bytes((0x81, len(data))) if len(data) < 126 else bytes((0x81, 126)) + struct.pack("!H", len(data))
        self.peer.sendall(header + data)

    def wait_for_reader(self):
        # A response after an event provides a deterministic receive barrier.
        self.send({"id": 99, "result": {}})
        self.assertEqual(self.app.responses.get(timeout=2)["id"], 99)

    def test_unsolicited_events_arrive_without_any_client_request(self):
        self.send({"method": "item/tool/requestUserInput", "id": 7, "params": {
            "threadId": "a", "questions": [{"id": "q", "question": "Which option?"}]}})
        self.wait_for_reader()
        self.assertIn("a", self.app.state_snapshot()["pending_inputs"])
        self.send({"method": "serverRequest/resolved", "params": {"threadId": "a", "requestId": 7}})
        self.wait_for_reader()
        self.assertNotIn("a", self.app.state_snapshot()["pending_inputs"])

    def test_idle_ping_receives_pong_without_status_query(self):
        self.peer.sendall(b"\x89\x04idle")
        frame = self.peer.recv(1024)
        self.assertEqual(frame[0], 0x8a)
        self.assertEqual(frame[1], 0x84)
        self.assertEqual(bytes(b ^ frame[2 + i % 4] for i, b in enumerate(frame[6:])), b"idle")

    def test_timeout_disconnects_and_old_reader_cannot_poison_next_connection(self):
        old_responses = self.app.responses
        with self.assertRaises(server.BridgeError) as raised:
            self.app.call("thread/list")
        self.assertEqual(raised.exception.code, "app_server_timeout")
        self.worker.join(timeout=2)
        self.assertFalse(self.worker.is_alive())
        self.app.responses = queue.Queue()
        self.app.reader_token = object()
        self.app.connection_lost.clear()
        self.assertTrue(self.app.responses.empty())
        self.assertIsNotNone(old_responses.get(timeout=2))
        self.assertFalse(self.app.connection_lost.is_set())

    def test_peer_disconnect_is_detected_while_idle(self):
        self.peer.close()
        self.assertTrue(self.app.connection_lost.wait(timeout=2))

    def test_stdio_reader_handles_push_without_calls(self):
        class Proc:
            stdout = iter([json.dumps({"method": "thread/status/changed", "params": {"threadId": "a"}})])
        app = server.AppServer(transport="stdio")
        app.reader_token = token = object()
        app._read_transport(token, app.responses, Proc(), None, None)
        self.assertEqual(app.state_snapshot()["revision"], 1)
        self.assertTrue(app.connection_lost.is_set())


if __name__ == "__main__":
    unittest.main()

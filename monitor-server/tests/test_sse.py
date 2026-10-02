"""SSE push, demand-driven refresh and disconnect regression checks."""
import http.client
import json
import pathlib
import queue
import socket
import sys
import threading
import unittest
from http.server import ThreadingHTTPServer

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import server
import mock_server
from test_server import FakeAppServer
from test_idle_status import status_responses


class StreamTests(unittest.TestCase):
    def setUp(self):
        self.now = 0
        self.stopped = threading.Event()
        self.app = FakeAppServer(status_responses())
        self.bridge = server.Bridge(self.app, clock=lambda: self.now)
        self.stream = self.bridge.status_events(self.stopped, coalesce=0)

    def tearDown(self):
        self.stopped.set()
        self.app.signal_activity()
        self.stream.close()

    def status(self, frame):
        self.assertTrue(frame.startswith("event: status\ndata: "))
        return json.loads(frame.split("data: ", 1)[1])

    def test_initial_status_and_heartbeat_do_not_poll_codex(self):
        self.assertEqual(self.status(next(self.stream))["integration"], "connected")
        self.now = 60
        self.assertEqual(next(self.stream), ": heartbeat\n\n")
        self.assertEqual(len(self.app.calls), 3)
        self.stopped.set()
        with self.assertRaises(StopIteration):
            next(self.stream)
        self.now = 1000
        self.assertEqual(len(self.app.calls), 3)

    def test_quota_push_wakes_stream_without_rpc(self):
        next(self.stream)
        result = queue.Queue()
        worker = threading.Thread(target=lambda: result.put(next(self.stream)))
        worker.start()
        self.app._dispatch({"method": "account/rateLimits/updated", "params": {"rateLimits": {
            "primary": {"usedPercent": 42, "windowDurationMins": 300}}}})
        self.assertEqual(self.status(result.get(timeout=2))["codex"]["usage"]["five_hour"]["used_percent"], 42)
        worker.join(2)
        self.assertEqual(len(self.app.calls), 3)

    def test_thread_event_refreshes_and_pushes_changed_agents(self):
        next(self.stream)
        self.app.responses.extend(status_responses(active=True)[:2])
        self.app._dispatch({"method": "thread/status/changed", "params": {"threadId": "a"}})
        self.assertEqual(self.status(next(self.stream))["total_agents"], 1)
        self.assertEqual(len(self.app.calls), 5)

    def test_fallback_refresh_does_not_send_timestamp_only_changes(self):
        next(self.stream)
        self.app.responses.extend(status_responses())
        self.now = 300
        self.assertEqual(next(self.stream), ": heartbeat\n\n")
        self.assertEqual(len(self.app.calls), 6)

    def test_unavailable_is_an_event_and_recovery_uses_backoff(self):
        self.app.responses = [server.BridgeError("offline", "app_server_unavailable")]
        self.assertEqual(self.status(next(self.stream))["integration"], "unavailable")
        self.app.responses.extend(status_responses())
        self.now = 30
        self.assertEqual(self.status(next(self.stream))["integration"], "connected")

    def test_touch_reconnect_forces_fresh_snapshot(self):
        next(self.stream)
        self.stream.close()
        self.app.responses.extend(status_responses(active=True))
        self.stream = self.bridge.status_events(self.stopped, force=True, coalesce=0)
        self.assertEqual(self.status(next(self.stream))["total_agents"], 1)
        self.assertEqual(len(self.app.calls), 6)

    def test_multiple_subscribers_share_cache_and_active_fallback(self):
        self.app.responses = status_responses(active=True) + status_responses()[:2]
        self.assertEqual(self.status(next(self.stream))["total_agents"], 1)
        another = self.bridge.status_events(self.stopped, coalesce=0)
        self.assertEqual(self.status(next(another))["total_agents"], 1)
        self.assertEqual(len(self.app.calls), 3)
        self.now = 15
        self.assertEqual(self.status(next(self.stream))["total_agents"], 0)
        self.assertEqual(self.status(next(another))["total_agents"], 0)
        self.assertEqual(len(self.app.calls), 5)
        another.close()

    def test_mock_push_and_heartbeat(self):
        state = mock_server.MockState()
        stream = state.status_events(self.stopped, heartbeat=0.01)
        self.assertEqual(self.status(next(stream))["total_agents"], 0)
        state.set_scenario("attention")
        self.assertEqual(self.status(next(stream))["agents"][0]["status"], "needs_attention")
        self.assertEqual(next(stream), ": heartbeat\n\n")
        self.assertEqual(state.status_requests, 2)
        stream.close()


class StreamHttpTests(unittest.TestCase):
    def test_auth_headers_and_client_disconnect_stop_refresh(self):
        app = FakeAppServer(status_responses())
        bridge = server.Bridge(app)
        exited = threading.Event()
        original = bridge.status_events

        def tracked_events(stopped, force=False):
            try:
                yield from original(stopped, force, heartbeat=0.1, coalesce=0)
            finally:
                exited.set()
        bridge.status_events = tracked_events
        httpd = ThreadingHTTPServer(("127.0.0.1", 0), server.make_handler(bridge, "key"))
        worker = threading.Thread(target=httpd.serve_forever, daemon=True)
        worker.start()
        address = httpd.server_address
        try:
            conn = http.client.HTTPConnection(*address, timeout=2)
            conn.request("GET", "/v1/events")
            response = conn.getresponse()
            self.assertEqual(response.status, 401)
            response.read(); conn.close()
            self.assertEqual(app.calls, [])
            sock = socket.create_connection(address, timeout=2)
            sock.sendall(b"GET /v1/events HTTP/1.0\r\nX-Monitor-Key: key\r\n\r\n")
            reader = sock.makefile("rb")
            headers = []
            while True:
                line = reader.readline()
                if line == b"\r\n": break
                headers.append(line)
            self.assertIn(b"Content-Type: text/event-stream\r\n", headers)
            self.assertFalse(any(h.startswith(b"Content-Length:") for h in headers))
            self.assertEqual(reader.readline(), b"event: status\n")
            self.assertTrue(reader.readline().startswith(b"data: "))
            self.assertEqual(reader.readline(), b"\n")
            reader.close(); sock.close()
            self.assertTrue(exited.wait(2), "Disconnect must cancel the producer, including fallback refreshes")
            self.assertEqual(len(app.calls), 3)
        finally:
            httpd.shutdown(); httpd.server_close(); worker.join(2)


if __name__ == "__main__":
    unittest.main()

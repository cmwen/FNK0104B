import base64
import hashlib
import json
import pathlib
import socket
import sys
import tempfile
import threading
import struct
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import server


class FakeAppServer:
    def __init__(self, responses):
        self.responses = list(responses)
        self.calls = []
        self.pending_inputs = {}
        self.pending_approvals = {}
        self.pending_other = {}
        self.latest_messages = {}

    def answer_user_input(self, thread_id, text):
        if thread_id in self.pending_inputs:
            self.pending_inputs.pop(thread_id)
            return True
        return False

    def call(self, method, params=None):
        self.calls.append((method, params or {}))
        response = self.responses.pop(0)
        if isinstance(response, Exception):
            raise response
        return response


class BridgeTests(unittest.TestCase):
    def test_status_maps_only_open_agents_and_exact_usage_windows(self):
        app = FakeAppServer([
            {"data": [
                {"id": "a", "name": "Work", "preview": "Implement", "status": {"type": "active"}},
                {"id": "b", "preview": "Approve", "status": {"type": "active", "activeFlags": ["waitingOnApproval"]}},
                {"id": "c", "preview": "done", "status": {"type": "notLoaded"}},
                {"id": "d", "preview": "Failed", "status": {"type": "systemError"}},
            ]},
            {"data": []},
            {"rateLimitsByLimitId": {"codex": {
                "primary": {"usedPercent": 41, "windowDurationMins": 300, "resetsAt": 1234},
                "secondary": {"usedPercent": 73, "windowDurationMins": 10080, "resetsAt": 5678},
            }}},
        ])
        status = server.Bridge(app).status()
        self.assertEqual(status["integration"], "connected")
        self.assertEqual([a["status"] for a in status["agents"]], ["running", "needs_attention", "error"])
        self.assertEqual(status["codex"]["usage"]["five_hour"], {"used_percent": 41, "resets_at": 1234})
        self.assertEqual(status["codex"]["usage"]["weekly"], {"used_percent": 73, "resets_at": 5678})
        self.assertEqual(app.calls[0][1]["sourceKinds"], ["cli", "vscode", "appServer", "subAgent", "subAgentThreadSpawn"])

    def test_usage_unavailable_is_null_and_degraded(self):
        app = FakeAppServer([{"data": []}, {"data": []}, server.BridgeError("usage unavailable")])
        result = server.Bridge(app).status()
        self.assertEqual(result["integration"], "degraded")
        self.assertIsNone(result["codex"]["usage"]["weekly"]["used_percent"])

    def test_status_caps_agent_payload_and_reports_total(self):
        threads = [{"id": str(i), "name": "Agent", "status": {"type": "active"}} for i in range(10)]
        app = FakeAppServer([{"data": threads}, {"data": []}, {"rateLimits": {}}])
        result = server.Bridge(app).status()
        self.assertEqual(len(result["agents"]), 8)
        self.assertEqual(result["total_agents"], 10)

    def test_not_loaded_summaries_degrade_integration_but_keep_quota_data(self):
        app = FakeAppServer([
            {"data": [{"id": "persisted", "status": {"type": "notLoaded"}}]},
            {"data": []},
            {"rateLimits": {"primary": {"usedPercent": 29, "windowDurationMins": 300}}},
        ])
        result = server.Bridge(app).status()
        self.assertEqual(result["integration"], "degraded")
        self.assertEqual(result["codex"]["usage"]["five_hour"]["used_percent"], 29)
        self.assertEqual(result["agents"], [])
        self.assertTrue(any("live runtime agent states are unavailable" in error for error in result["errors"]))

    def test_new_command_starts_thread_and_turn(self):
        app = FakeAppServer([{"thread": {"id": "thr-1"}}, {"turn": {"id": "turn-1"}}])
        result = server.Bridge(app, agent_cwd="/tmp/monitor-project").command("Fix the display")
        self.assertEqual((result["action"], result["agent_id"]), ("created", "thr-1"))
        self.assertEqual([call[0] for call in app.calls], ["thread/start", "turn/start"])
        self.assertEqual(app.calls[0][1], {"cwd": "/tmp/monitor-project"})

    def test_active_agent_uses_turn_steer_with_expected_turn_id(self):
        app = FakeAppServer([
            {"thread": {"status": {"type": "active"}, "turns": [{"id": "t-9", "status": "inProgress"}]}},
            {"turnId": "t-9"},
        ])
        result = server.Bridge(app).command("Check the failing test", "thr-9")
        self.assertEqual(result["action"], "steered")
        self.assertEqual(app.calls[-1], ("turn/steer", {"threadId": "thr-9", "input": [{"type": "text", "text": "Check the failing test"}], "expectedTurnId": "t-9"}))

    def test_voice_reports_unconfigured_without_faking_action(self):
        bridge = server.Bridge(FakeAppServer([]))
        with self.assertRaises(server.BridgeError) as raised:
            bridge.transcribe(b"RIFF....WAVE")
        self.assertEqual(raised.exception.code, "transcription_unconfigured")

    def test_single_pending_question_is_answered_without_steering(self):
        app = FakeAppServer([])
        app.pending_inputs["thr-q"] = {"request_id": 8, "questions": [{"id": "q1", "question": "Which test?"}]}
        bridge = server.Bridge(app)
        result = bridge.command("The display test", "thr-q")
        self.assertEqual(result["action"], "answered_question")

    def test_voice_does_not_auto_approve(self):
        app = FakeAppServer([{"thread": {"status": {"type": "active", "activeFlags": ["waitingOnApproval"]}}}])
        app.pending_approvals["thr-a"] = "item/commandExecution/requestApproval"
        with self.assertRaises(server.BridgeError) as raised:
            server.Bridge(app).command("Continue", "thr-a")
        self.assertEqual(raised.exception.code, "pending_approval")


class HttpHandlerTests(unittest.TestCase):
    def test_remote_key_is_required_and_not_reflected(self):
        handler = server.make_handler(server.Bridge(FakeAppServer([])), "secret-value")
        # Directly exercise authorization predicate without starting the Codex process.
        instance = object.__new__(handler)
        instance.headers = {"X-Monitor-Key": "wrong"}
        self.assertFalse(instance._authorized())
        instance.headers = {"X-Monitor-Key": "secret-value"}
        self.assertTrue(instance._authorized())

    def test_content_length_rejects_missing_and_negative_values(self):
        handler = server.make_handler(server.Bridge(FakeAppServer([])))
        instance = object.__new__(handler)
        results = []
        instance._json = lambda code, value: results.append((code, value))
        instance.headers = {}
        self.assertIsNone(instance._content_length(100))
        self.assertEqual(results[-1][0], 411)
        instance.headers = {"Content-Length": "-1"}
        self.assertIsNone(instance._content_length(100))
        self.assertEqual(results[-1][0], 400)


class WebSocketTests(unittest.TestCase):
    def test_server_frame_is_decoded_and_client_frame_is_masked(self):
        client = server.AppServer(transport="unix")
        payload = b'{"id":3,"result":{"ok":true}}'
        client.ws_buffer.extend(bytes((0x81, len(payload))) + payload)
        self.assertEqual(client._recv_ws_message(), {"id": 3, "result": {"ok": True}})

        class Sink:
            def __init__(self):
                self.data = b""
            def sendall(self, data):
                self.data += data
        client.sock = Sink()
        client._send({"method": "ping"})
        self.assertEqual(client.sock.data[0] & 0x80, 0x80)
        self.assertTrue(client.sock.data[1] & 0x80)

    def test_closed_socket_invalidates_transport_for_next_request(self):
        class ClosedSocket:
            def sendall(self, _):
                raise BrokenPipeError("closed")
            def close(self):
                pass
        app = server.AppServer(transport="unix")
        app.initialized = True
        app.sock = ClosedSocket()
        with self.assertRaises(server.BridgeError):
            app.call("thread/list")
        self.assertFalse(app.initialized)
        self.assertIsNone(app.sock)

    def test_unix_upgrade_initializes_and_uses_json_frames(self):
        with tempfile.TemporaryDirectory() as directory:
            path = str(pathlib.Path(directory) / "control.sock")
            listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            listener.bind(path)
            listener.listen(1)
            received_methods = []
            thread_errors = []

            def read_exact(conn, count):
                data = bytearray()
                while len(data) < count:
                    data.extend(conn.recv(count - len(data)))
                return bytes(data)

            def read_client_message(conn):
                first, second = read_exact(conn, 2)
                length = second & 0x7f
                if length == 126:
                    length = struct.unpack("!H", read_exact(conn, 2))[0]
                mask = read_exact(conn, 4)
                payload = read_exact(conn, length)
                return json.loads(bytes(byte ^ mask[i % 4] for i, byte in enumerate(payload)))

            def send_server_message(conn, message):
                payload = json.dumps(message, separators=(",", ":")).encode()
                conn.sendall(bytes((0x81, len(payload))) + payload)

            def serve():
                try:
                    conn, _ = listener.accept()
                    with conn:
                        handshake = bytearray()
                        while b"\r\n\r\n" not in handshake:
                            handshake.extend(conn.recv(4096))
                        headers = bytes(handshake).decode("latin1")
                        key = next(line.split(":", 1)[1].strip() for line in headers.split("\r\n") if line.lower().startswith("sec-websocket-key:"))
                        accept = base64.b64encode(hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
                        conn.sendall(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n\r\n").encode())
                        initialize = read_client_message(conn)
                        received_methods.append(initialize["method"])
                        send_server_message(conn, {"id": initialize["id"], "result": {}})
                        received_methods.append(read_client_message(conn)["method"])
                        query = read_client_message(conn)
                        received_methods.append(query["method"])
                        send_server_message(conn, {"id": query["id"], "result": {"data": []}})
                except Exception as exc:
                    thread_errors.append(exc)
                finally:
                    listener.close()

            worker = threading.Thread(target=serve, daemon=True)
            worker.start()
            app = server.AppServer(transport="unix", socket_path=path, timeout=3)
            self.assertEqual(app.call("thread/list", {}), {"data": []})
            app.close()
            worker.join(timeout=3)
            self.assertFalse(thread_errors)
            self.assertEqual(received_methods, ["initialize", "initialized", "thread/list"])


if __name__ == "__main__":
    unittest.main()

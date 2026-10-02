import json
import pathlib
import sys
import threading
import unittest
from http.server import ThreadingHTTPServer
from urllib.error import HTTPError
from urllib.request import Request, urlopen

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import mock_server


class MockServerTests(unittest.TestCase):
    def setUp(self):
        self.state = mock_server.MockState()
        self.server = ThreadingHTTPServer(
            ("127.0.0.1", 0), mock_server.make_handler(self.state, "test-key"))
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base = f"http://127.0.0.1:{self.server.server_port}"

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=2)

    def call(self, path, body=None, token="test-key", content_type="application/json"):
        headers = {"X-Monitor-Key": token}
        if body is not None:
            headers["Content-Type"] = content_type
        data = None if body is None else (
            json.dumps(body).encode() if isinstance(body, dict) else body)
        request = Request(self.base + path, data=data, headers=headers,
                          method="GET" if data is None else "POST")
        try:
            with urlopen(request, timeout=2) as response:
                return response.status, json.load(response)
        except HTTPError as exc:
            with exc:
                return exc.code, json.load(exc)

    def test_idle_attention_complete_and_offline_follow_board_contract(self):
        code, idle = self.call("/v1/status")
        self.assertEqual(code, 200)
        self.assertEqual(idle["agents"], [])
        self.assertEqual(idle["codex"]["usage"]["five_hour"]["used_percent"], 29)

        self.call("/_mock/scenario", {"scenario": "attention"})
        code, attention = self.call("/v1/status")
        self.assertEqual(code, 200)
        self.assertEqual(attention["agents"][0]["status"], "needs_attention")
        self.assertEqual(attention["total_agents"], 1)

        self.call("/_mock/scenario", {"scenario": "complete"})
        self.assertEqual(self.call("/v1/status")[1]["agents"], [])

        self.call("/_mock/scenario", {"scenario": "offline"})
        code, offline = self.call("/v1/status")
        self.assertEqual(code, 503)
        self.assertEqual(offline["integration"], "unavailable")

    def test_agent_and_usage_controls_are_bounded(self):
        self.call("/_mock/agent", {"id": "one", "name": "Work",
                                    "status": "running", "detail": "Building"})
        self.call("/_mock/usage", {"five_hour": 0, "weekly": None})
        status = self.call("/v1/status")[1]
        self.assertEqual(status["agents"][0]["id"], "one")
        self.assertEqual(status["codex"]["usage"]["five_hour"]["used_percent"], 0)
        self.assertIsNone(status["codex"]["usage"]["weekly"]["used_percent"])
        self.assertEqual(self.call("/_mock/agent", {
            "id": "one", "status": "complete"})[1]["status"]["agents"], [])
        self.assertEqual(self.call("/_mock/usage", {
            "five_hour": 101, "weekly": 0})[0], 400)

    def test_voice_upload_records_metadata_without_transcribing(self):
        wav = b"RIFF" + b"\x00" * 4 + b"WAVE" + b"\x00" * 32
        code, result = self.call("/v1/voice?agent_id=one", wav,
                                 content_type="audio/wav")
        self.assertEqual(code, 200)
        self.assertEqual(result["action"], "mock_received")
        self.assertEqual(result["transcript"], "")
        last = self.call("/_mock/state")[1]["last_voice"]
        self.assertEqual(last["audio_bytes"], len(wav))
        self.assertEqual(last["agent_id"], "one")
        self.assertEqual(self.call("/v1/voice", b"bad",
                                   content_type="audio/wav")[0], 400)

    def test_all_endpoints_require_lan_key(self):
        self.assertEqual(self.call("/v1/status", token="wrong")[0], 401)
        self.assertEqual(self.call("/_mock/state", token="wrong")[0], 401)
        self.assertEqual(self.call("/_mock/scenario", {
            "scenario": "attention"}, token="wrong")[0], 401)


if __name__ == "__main__":
    unittest.main()

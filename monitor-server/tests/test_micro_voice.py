import http.client
import json
import pathlib
import sys
import threading
import unittest
from http.server import ThreadingHTTPServer

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import server


class Intake:
    def __init__(self):
        self.calls = []

    def submit(self, text, wav, agent_id, request_id):
        self.calls.append((wav, agent_id, request_id))
        return {"ok": True, "action": "queued", "job_id": "intake-1"}


class MicroVoiceTests(unittest.TestCase):
    def setUp(self):
        self.bridge = server.Bridge(server.AppServer())
        self.bridge.transcriber = "configured"
        self.intake = Intake()
        self.bridge.dispatcher = self.intake
        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), server.make_handler(self.bridge, "key"))
        self.worker = threading.Thread(target=self.httpd.serve_forever, daemon=True)
        self.worker.start()

    def tearDown(self):
        self.httpd.shutdown()
        self.httpd.server_close()
        self.worker.join()

    def voice(self, query, token="key"):
        connection = http.client.HTTPConnection(*self.httpd.server_address, timeout=2)
        try:
            connection.request("POST", "/v1/voice" + query, b"RIFF....WAVEdata", {
                "Content-Type": "audio/wav", "X-Monitor-Key": token, "X-Request-ID": "mic-1"})
            response = connection.getresponse()
            return response.status, json.loads(response.read())
        finally:
            connection.close()

    def test_ble_speech_enters_intake_without_an_agent(self):
        code, result = self.voice("?route=orchestrator")
        self.assertEqual(code, 200)
        self.assertEqual(result["action"], "queued")
        self.assertEqual(self.intake.calls, [(b"RIFF....WAVEdata", None, "mic-1")])

    def test_direct_target_cannot_override_intake(self):
        code, result = self.voice("?route=orchestrator&agent_id=desktop-slot")
        self.assertEqual(code, 400)
        self.assertEqual(result["error"]["code"], "invalid_voice_target")
        self.assertEqual(self.intake.calls, [])

    def test_duplicate_parameters_cannot_override_intake(self):
        self.assertEqual(self.voice("?route=orchestrator&route=other")[0], 400)
        self.assertEqual(self.voice("?route=orchestrator&agent_id=&agent_id=slot")[0], 400)
        self.assertEqual(self.intake.calls, [])

    def test_missing_orchestrator_does_not_fall_back_to_a_new_thread(self):
        self.bridge.dispatcher = None
        code, result = self.voice("?route=orchestrator")
        self.assertEqual(code, 503)
        self.assertEqual(result["error"]["code"], "orchestrator_unconfigured")

    def test_unknown_route_and_unauthorized_requests_are_rejected(self):
        self.assertEqual(self.voice("?route=other")[0], 400)
        self.assertEqual(self.voice("?route=orchestrator", token="wrong")[0], 401)
        self.assertEqual(self.intake.calls, [])


if __name__ == "__main__":
    unittest.main()

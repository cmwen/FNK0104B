#!/usr/bin/env python3
"""Contract-compatible local status source for FNK0104B board tests."""
from __future__ import annotations

import hmac
import ipaddress
import json
import os
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

from server import serve_status_stream

MAX_AUDIO_BYTES = 10 * 1024 * 1024
MAX_JSON_BYTES = 8192
SCENARIOS = ("idle", "running", "attention", "error", "complete", "overflow", "degraded", "offline")
AGENT_STATES = ("running", "needs_attention", "error", "complete")


class MockError(ValueError):
    pass


class MockState:
    def __init__(self):
        self.lock = threading.RLock()
        self.activity = threading.Condition(self.lock)
        self.revision = 0
        self.started_at = int(time.time())
        self.agents = {}
        self.five_hour = 29
        self.weekly = 51
        self.integration = "connected"
        self.offline = False
        self.scenario = "idle"
        self.status_requests = 0
        self.last_status_client = None
        self.last_voice = None
        self.last_text = None

    def set_scenario(self, name):
        if name not in SCENARIOS:
            raise MockError("scenario must be one of: " + ", ".join(SCENARIOS))
        with self.lock:
            self.scenario = name
            self.offline = name == "offline"
            self.integration = "degraded" if name == "degraded" else "connected"
            self.agents.clear()
            if name in ("running", "attention", "error"):
                status = {"running": "running", "attention": "needs_attention", "error": "error"}[name]
                detail = {
                    "running": "Checking the firmware build.",
                    "attention": "Which layout should I use?",
                    "error": "The build failed; inspect the Codex app.",
                }[name]
                self.agents["mock-agent-1"] = {
                    "id": "mock-agent-1", "name": "Display agent",
                    "status": status, "detail": detail,
                }
            elif name == "overflow":
                for index in range(1, 7):
                    agent_id = f"mock-agent-{index}"
                    self.agents[agent_id] = {
                        "id": agent_id, "name": f"Agent {index}",
                        "status": "running", "detail": "Working in the mock scenario.",
                    }
            self.signal_activity()
        return self.snapshot()

    def set_agent(self, data):
        if not isinstance(data, dict):
            raise MockError("agent body must be a JSON object")
        agent_id, name, status, detail = (
            data.get("id"), data.get("name", "Agent"),
            data.get("status"), data.get("detail", ""),
        )
        if not isinstance(agent_id, str) or not 1 <= len(agent_id) <= 79:
            raise MockError("id must be a string of 1–79 characters")
        if not isinstance(name, str) or not 1 <= len(name) <= 47:
            raise MockError("name must be a string of 1–47 characters")
        if status not in AGENT_STATES:
            raise MockError("status must be running, needs_attention, error, or complete")
        if not isinstance(detail, str) or len(detail) > 119:
            raise MockError("detail must be a string of at most 119 characters")
        with self.lock:
            self.offline = False
            self.integration = "connected"
            self.scenario = "custom"
            if status == "complete":
                self.agents.pop(agent_id, None)
            else:
                self.agents[agent_id] = {
                    "id": agent_id, "name": name, "status": status, "detail": detail,
                }
            self.signal_activity()
        return self.snapshot()

    def set_usage(self, data):
        if not isinstance(data, dict) or set(data) != {"five_hour", "weekly"}:
            raise MockError("usage body needs five_hour and weekly")
        for value in data.values():
            if value is not None and (type(value) is not int or not 0 <= value <= 100):
                raise MockError("usage values must be 0–100 or null")
        with self.lock:
            self.five_hour = data["five_hour"]
            self.weekly = data["weekly"]
            self.signal_activity()
        return self.snapshot()

    def signal_activity(self):
        with self.activity:
            self.revision += 1
            self.activity.notify_all()

    def status_events(self, stopped, client=None, heartbeat=60):
        revision = -1
        while not stopped.is_set():
            with self.activity:
                if self.revision != revision:
                    revision = self.revision
                    event = "event: status\ndata: " + json.dumps(self.status(client), separators=(",", ":")) + "\n\n"
                else:
                    event = ": heartbeat\n\n"
            yield event
            with self.activity:
                self.activity.wait_for(lambda: self.revision != revision or stopped.is_set(), heartbeat)

    def status(self, client=None):
        with self.lock:
            self.status_requests += 1
            self.last_status_client = client
            return self._status()

    def _status(self):
        now = int(time.time())
        agents = list(self.agents.values())
        return {
            "integration": "unavailable" if self.offline else self.integration,
            "codex": {"usage": {
                "five_hour": {"used_percent": self.five_hour,
                              "resets_at": self.started_at + 2 * 3600 if self.five_hour is not None else None},
                "weekly": {"used_percent": self.weekly,
                           "resets_at": self.started_at + 4 * 86400 if self.weekly is not None else None},
            }},
            "agents": agents[:8],
            "total_agents": len(agents),
            "updated_at": now,
            "errors": ["Mock integration degraded"] if self.integration == "degraded" else [],
        }

    def snapshot(self):
        with self.lock:
            return {
                "scenario": self.scenario,
                "status": self._status(),
                "status_requests": self.status_requests,
                "last_status_client": self.last_status_client,
                "last_voice": self.last_voice,
                "last_text": self.last_text,
            }

    def record_voice(self, audio_bytes, agent_id):
        with self.lock:
            self.last_voice = {
                "audio_bytes": audio_bytes, "agent_id": agent_id,
                "received_at": int(time.time()),
            }

    def record_text(self, text, agent_id):
        with self.lock:
            self.last_text = {
                "characters": len(text), "agent_id": agent_id,
                "received_at": int(time.time()),
            }


def make_handler(state, token=None):
    class Handler(BaseHTTPRequestHandler):
        server_version = "FNKMonitorMock/1"

        def _json(self, code, value):
            payload = json.dumps(value, separators=(",", ":")).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)

        def _authorized(self):
            return token is None or hmac.compare_digest(
                self.headers.get("X-Monitor-Key", ""), token)

        def _body(self, limit):
            raw = self.headers.get("Content-Length")
            try:
                length = int(raw) if raw is not None else -1
            except ValueError:
                length = -1
            if length < 0 or length > limit:
                raise MockError(f"Content-Length must be between 0 and {limit}")
            data = self.rfile.read(length)
            if len(data) != length:
                raise MockError("incomplete request body")
            return data

        def _object(self):
            if self.headers.get_content_type() != "application/json":
                raise MockError("expected application/json")
            try:
                value = json.loads(self._body(MAX_JSON_BYTES))
            except (ValueError, UnicodeDecodeError) as exc:
                raise MockError("invalid JSON request") from exc
            if not isinstance(value, dict):
                raise MockError("JSON body must be an object")
            return value

        def do_GET(self):
            if not self._authorized():
                return self._json(401, {"error": {"code": "unauthorized", "message": "Invalid monitor key"}})
            if urlparse(self.path).path == "/v1/status":
                payload = state.status(self.client_address[0])
                return self._json(503 if state.offline else 200, payload)
            if urlparse(self.path).path == "/v1/events":
                stopped = threading.Event()
                return serve_status_stream(self, state.status_events(stopped, self.client_address[0]),
                                           stopped, state.signal_activity)
            if self.path == "/_mock/state":
                return self._json(200, state.snapshot())
            return self._json(404, {"error": {"code": "not_found", "message": "Unknown endpoint"}})

        def do_POST(self):
            if not self._authorized():
                return self._json(401, {"error": {"code": "unauthorized", "message": "Invalid monitor key"}})
            parsed = urlparse(self.path)
            try:
                if parsed.path == "/_mock/scenario":
                    data = self._object()
                    return self._json(200, state.set_scenario(data.get("scenario")))
                if parsed.path == "/_mock/agent":
                    return self._json(200, state.set_agent(self._object()))
                if parsed.path == "/_mock/usage":
                    return self._json(200, state.set_usage(self._object()))
                if parsed.path == "/v1/commands/text":
                    data = self._object()
                    text = data.get("text")
                    agent_id = data.get("agent_id")
                    if not isinstance(text, str) or not text.strip() or len(text) > 4000:
                        raise MockError("text must contain 1–4000 characters")
                    if agent_id is not None and not isinstance(agent_id, str):
                        raise MockError("agent_id must be a string")
                    state.record_text(text, agent_id)
                    return self._json(200, {
                        "ok": True, "transcript": text, "action": "mock_received",
                        "agent_id": agent_id, "message": "Mock received text; no Codex action occurred.",
                    })
                if parsed.path == "/v1/voice":
                    if self.headers.get_content_type() != "audio/wav":
                        raise MockError("expected audio/wav")
                    wav = self._body(MAX_AUDIO_BYTES)
                    if len(wav) < 44 or wav[:4] != b"RIFF" or wav[8:12] != b"WAVE":
                        raise MockError("body must be a RIFF/WAVE file")
                    agent_id = parse_qs(parsed.query).get("agent_id", [None])[0]
                    state.record_voice(len(wav), agent_id)
                    return self._json(200, {
                        "ok": True, "transcript": "", "action": "mock_received",
                        "agent_id": agent_id,
                        "message": "Mock received WAV; no transcription or Codex action occurred.",
                    })
            except MockError as exc:
                return self._json(400, {"ok": False, "error": {
                    "code": "invalid_request", "message": str(exc)}})
            return self._json(404, {"error": {"code": "not_found", "message": "Unknown endpoint"}})

        def log_message(self, format, *args):
            # Never log the shared key, audio body, or text command.
            pass

    return Handler


def main():
    host = os.environ.get("MONITOR_HOST", "127.0.0.1")
    port = int(os.environ.get("MONITOR_PORT", "8765"))
    try:
        is_loopback = ipaddress.ip_address(host).is_loopback
    except ValueError:
        is_loopback = False
    token_file = os.environ.get("MONITOR_TOKEN_FILE")
    token = Path(token_file).read_text(encoding="utf-8").strip() if token_file else os.environ.get("MONITOR_TOKEN")
    if not is_loopback and not token:
        raise SystemExit("MONITOR_TOKEN is required for a non-loopback bind")
    server = ThreadingHTTPServer((host, port), make_handler(MockState(), None if is_loopback else token))
    print(f"FNK monitor mock listening on http://{host}:{port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()

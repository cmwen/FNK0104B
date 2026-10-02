#!/usr/bin/env python3
"""Local HTTP bridge between the FNK0104B monitor and Codex app-server."""
from __future__ import annotations

import json
import hmac
import ipaddress
import os
import queue
from collections import deque
import base64
import secrets
import socket
import struct
import hashlib
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse
from urllib.request import Request, urlopen


MAX_AUDIO_BYTES = 10 * 1024 * 1024
MAX_TEXT_CHARS = 4000


class BridgeError(Exception):
    def __init__(self, message: str, code: str = "app_server_error"):
        super().__init__(message)
        self.code = code


class AppServer:
    """Synchronous client for the local daemon WebSocket or explicit stdio mode."""

    def __init__(self, command=None, timeout=20, transport=None, socket_path=None):
        self.transport = transport or os.environ.get("CODEX_APP_SERVER_TRANSPORT", "unix")
        self.command = command or ["codex", "app-server", "--listen", "stdio://"]
        self.socket_path = os.path.expanduser(socket_path or os.environ.get(
            "CODEX_APP_SERVER_SOCKET", "~/.codex/app-server-control/app-server-control.sock"))
        if self.transport not in ("unix", "stdio"):
            raise ValueError("CODEX_APP_SERVER_TRANSPORT must be unix or stdio")
        self.timeout = timeout
        self.proc = None
        self.sock = None
        self.ws_buffer = bytearray()
        self.initialized = False
        self.lock = threading.RLock()
        self.next_id = 1
        self.responses = queue.Queue()
        self.notifications = deque(maxlen=256)
        self.pending_inputs = {}
        self.pending_approvals = {}
        self.pending_other = {}
        self.latest_messages = {}

    def _connect(self):
        if self.initialized:
            return
        self._disconnect()
        if self.transport == "stdio":
            try:
                self.proc = subprocess.Popen(self.command, stdin=subprocess.PIPE,
                                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                             text=True, bufsize=1)
            except (OSError, ValueError) as exc:
                raise BridgeError("Could not start codex app-server: " + str(exc), "app_server_unavailable") from exc
            self.responses = queue.Queue()
            threading.Thread(target=self._read_stdout, daemon=True).start()
        else:
            try:
                self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.sock.settimeout(self.timeout)
                self.sock.connect(self.socket_path)
                key = base64.b64encode(secrets.token_bytes(16)).decode("ascii")
                request = ("GET / HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
                           "Connection: Upgrade\r\nSec-WebSocket-Key: " + key +
                           "\r\nSec-WebSocket-Version: 13\r\n\r\n")
                self.sock.sendall(request.encode("ascii"))
                headers = bytearray()
                while b"\r\n\r\n" not in headers:
                    chunk = self.sock.recv(4096)
                    if not chunk:
                        raise OSError("socket closed during WebSocket handshake")
                    headers.extend(chunk)
                    if len(headers) > 16384:
                        raise OSError("oversized WebSocket handshake")
                head, extra = bytes(headers).split(b"\r\n\r\n", 1)
                if not head.startswith(b"HTTP/1.1 101") and not head.startswith(b"HTTP/1.0 101"):
                    raise OSError("app-server socket rejected WebSocket upgrade")
                response_headers = {}
                for line in head.decode("latin1").split("\r\n")[1:]:
                    if ":" in line:
                        name, value = line.split(":", 1)
                        response_headers[name.strip().lower()] = value.strip()
                expected = base64.b64encode(hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode("ascii")).digest()).decode("ascii")
                if response_headers.get("sec-websocket-accept") != expected:
                    raise OSError("invalid WebSocket accept value")
                self.ws_buffer.extend(extra)
            except (OSError, TimeoutError) as exc:
                if self.sock:
                    self.sock.close()
                    self.sock = None
                raise BridgeError("Could not connect to the existing Codex app-server daemon: " + str(exc), "app_server_unavailable") from exc
        try:
            self._roundtrip("initialize", {"clientInfo": {"name": "fnk0104b-monitor-bridge", "title": "FNK0104B Monitor Bridge", "version": "0.1.0"}})
            self._send({"method": "initialized", "params": {}})
            self.initialized = True
        except BridgeError:
            self._disconnect()
            raise

    def _read_stdout(self):
        try:
            for line in self.proc.stdout:
                try:
                    msg = json.loads(line)
                except (ValueError, TypeError):
                    continue
                if "id" in msg and ("result" in msg or "error" in msg):
                    self.responses.put(msg)
                else:
                    self._dispatch(msg)
        finally:
            self.responses.put({"_eof": True})

    def _write(self, obj):
        try:
            if self.transport == "unix":
                self._send(obj)
                return
            if not self.proc or self.proc.poll() is not None:
                raise BridgeError("Codex app-server is not running", "app_server_unavailable")
            self.proc.stdin.write(json.dumps(obj, separators=(",", ":")) + "\n")
            self.proc.stdin.flush()
        except OSError as exc:
            raise BridgeError("Codex app-server transport closed while sending", "app_server_unavailable") from exc

    def _send(self, obj):
        if self.transport == "stdio":
            self.proc.stdin.write(json.dumps(obj, separators=(",", ":")) + "\n")
            self.proc.stdin.flush()
            return
        if not self.sock:
            raise BridgeError("Codex app-server socket is not connected", "app_server_unavailable")
        payload = json.dumps(obj, separators=(",", ":")).encode("utf-8")
        mask = secrets.token_bytes(4)
        size = len(payload)
        if size < 126:
            header = bytes((0x81, 0x80 | size))
        elif size < 65536:
            header = bytes((0x81, 0x80 | 126)) + struct.pack("!H", size)
        else:
            header = bytes((0x81, 0x80 | 127)) + struct.pack("!Q", size)
        masked = bytes(byte ^ mask[i % 4] for i, byte in enumerate(payload))
        self.sock.sendall(header + mask + masked)

    def _read_exact(self, size):
        while len(self.ws_buffer) < size:
            chunk = self.sock.recv(max(4096, size - len(self.ws_buffer)))
            if not chunk:
                raise BridgeError("Codex app-server socket closed", "app_server_unavailable")
            self.ws_buffer.extend(chunk)
        result = bytes(self.ws_buffer[:size])
        del self.ws_buffer[:size]
        return result

    def _recv_ws_message(self):
        fragments = bytearray()
        message_opcode = None
        while True:
            first, second = self._read_exact(2)
            opcode, masked = first & 0x0F, bool(second & 0x80)
            length = second & 0x7F
            if length == 126:
                length = struct.unpack("!H", self._read_exact(2))[0]
            elif length == 127:
                length = struct.unpack("!Q", self._read_exact(8))[0]
            if length > 16 * 1024 * 1024:
                raise BridgeError("Codex app-server WebSocket frame is too large", "app_server_protocol_error")
            mask = self._read_exact(4) if masked else b""
            payload = self._read_exact(length)
            if masked:
                payload = bytes(byte ^ mask[i % 4] for i, byte in enumerate(payload))
            if opcode == 8:
                raise BridgeError("Codex app-server closed the WebSocket", "app_server_unavailable")
            if opcode == 9:
                self._send_ws_control(10, payload)
                continue
            if opcode == 10:
                continue
            if opcode in (1, 2):
                message_opcode = opcode
                fragments.extend(payload)
            elif opcode == 0:
                fragments.extend(payload)
            else:
                continue
            if first & 0x80:
                if message_opcode != 1:
                    raise BridgeError("Unexpected non-text app-server WebSocket message", "app_server_protocol_error")
                try:
                    return json.loads(fragments.decode("utf-8"))
                except (UnicodeDecodeError, json.JSONDecodeError) as exc:
                    raise BridgeError("Invalid JSON from Codex app-server", "app_server_protocol_error") from exc

    def _send_ws_control(self, opcode, payload):
        mask = secrets.token_bytes(4)
        masked = bytes(byte ^ mask[i % 4] for i, byte in enumerate(payload))
        self.sock.sendall(bytes((0x80 | opcode, 0x80 | len(payload))) + mask + masked)

    def _roundtrip(self, method, params):
        req_id = self.next_id
        self.next_id += 1
        self._write({"id": req_id, "method": method, "params": params or {}})
        deadline = time.monotonic() + self.timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise BridgeError("Codex app-server request timed out", "app_server_timeout")
            try:
                if self.transport == "unix":
                    self.sock.settimeout(remaining)
                    response = self._recv_ws_message()
                else:
                    response = self.responses.get(timeout=remaining)
            except queue.Empty as exc:
                raise BridgeError("Codex app-server request timed out", "app_server_timeout") from exc
            except (OSError, TimeoutError) as exc:
                raise BridgeError("Codex app-server socket request failed", "app_server_unavailable") from exc
            if response.get("_eof"):
                raise BridgeError("Codex app-server closed its output", "app_server_unavailable")
            if "method" in response:
                self._dispatch(response)
                continue
            if response.get("id") != req_id:
                self._dispatch(response)
                continue
            if "error" in response:
                err = response["error"]
                raise BridgeError(str(err.get("message", "Codex app-server rejected the request")))
            return response.get("result", {})

    def _dispatch(self, message):
        method = message.get("method")
        params = message.get("params") or {}
        if method == "item/tool/requestUserInput" and "id" in message:
            self.pending_inputs[params.get("threadId")] = {
                "request_id": message["id"], "questions": params.get("questions") or []}
        elif method and "requestApproval" in method:
            self.pending_approvals[params.get("threadId")] = {"method": method, "reason": params.get("reason")}
        elif method and ("permissions/request" in method or "elicitation/request" in method):
            self.pending_other[params.get("threadId")] = method
        elif method == "serverRequest/resolved":
            thread_id, request_id = params.get("threadId"), params.get("requestId")
            pending = self.pending_inputs.get(thread_id)
            if pending and pending.get("request_id") == request_id:
                self.pending_inputs.pop(thread_id, None)
        elif method == "item/agentMessage/delta":
            thread_id = params.get("threadId")
            if thread_id:
                self.latest_messages[thread_id] = (self.latest_messages.get(thread_id, "") + str(params.get("delta", "")))[-120:]
        elif method == "item/completed":
            item = params.get("item") or {}
            if item.get("type") == "agentMessage" and item.get("text"):
                self.latest_messages[params.get("threadId")] = str(item["text"])[-120:]
        if method or "id" not in message:
            self.notifications.append(message)

    def answer_user_input(self, thread_id, text):
        pending = self.pending_inputs.get(thread_id)
        if not pending:
            return False
        questions = pending["questions"]
        if len(questions) != 1:
            raise BridgeError("This thread has a multi-question Codex prompt; answer it in the Codex app.", "unsupported_pending_input")
        question_id = questions[0].get("id")
        if not question_id:
            raise BridgeError("The pending Codex question has no answer identifier.", "unsupported_pending_input")
        with self.lock:
            try:
                self._write({"id": pending["request_id"], "result": {"answers": {question_id: {"answers": [text]}}}})
            except BridgeError:
                self._disconnect()
                raise
            del self.pending_inputs[thread_id]
        return True

    def close(self):
        self._disconnect()

    def _disconnect(self):
        self.initialized = False
        if self.sock:
            self.sock.close()
            self.sock = None
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
        self.proc = None
        self.ws_buffer.clear()
        self.pending_inputs.clear()
        self.pending_approvals.clear()
        self.pending_other.clear()

    def call(self, method, params=None):
        with self.lock:
            self._connect()
            try:
                return self._roundtrip(method, params)
            except BridgeError as exc:
                if exc.code in ("app_server_unavailable", "app_server_timeout", "app_server_protocol_error"):
                    self._disconnect()
                raise

    def notify(self, method, params=None):
        with self.lock:
            self._connect()
            self._write({"method": method, "params": params or {}})


def _quota_entry(bucket, duration):
    for key in ("primary", "secondary"):
        item = bucket.get(key)
        if isinstance(item, dict) and item.get("windowDurationMins") == duration:
            return {"used_percent": item.get("usedPercent"), "resets_at": item.get("resetsAt")}
    return {"used_percent": None, "resets_at": None}


def _thread_display(thread):
    status = thread.get("status") or {}
    kind = status.get("type")
    flags = status.get("activeFlags") or []
    turns = thread.get("turns") or []
    last = turns[-1] if turns else {}
    turn_status = last.get("status")
    if kind == "active":
        state = "needs_attention" if any("approval" in str(x).lower() or "userinput" in str(x).lower() for x in flags) else "running"
    elif kind in ("error", "errored", "systemError") or turn_status in ("failed", "error"):
        state = "error"
    else:
        return None
    preview = thread.get("preview") or thread.get("name") or ""
    return {"id": thread.get("id"), "name": (thread.get("name") or preview or "Codex agent")[:48],
            "status": state, "detail": preview[:160]}


class Bridge:
    def __init__(self, app_server=None, transcriber=None, transcriber_mode="raw", agent_cwd=None):
        self.app = app_server or AppServer()
        self.transcriber = transcriber
        self.transcriber_mode = transcriber_mode
        self.agent_cwd = os.path.abspath(agent_cwd or os.environ.get("MONITOR_AGENT_CWD") or os.getcwd())

    def status(self):
        now = int(time.time())
        data = self.app.call("thread/list", {"limit": 50, "sortKey": "updated_at", "sortDirection": "desc",
                                              "sourceKinds": ["cli", "vscode", "appServer", "subAgent", "subAgentThreadSpawn"]})
        visibility_errors = []
        threads = data.get("data", [])
        try:
            loaded_ids = set(self.app.call("thread/loaded/list").get("data", []))
        except BridgeError:
            loaded_ids = set()
            visibility_errors.append("Could not inspect app-server loaded-thread state.")
        runtime_types = {(item.get("status") or {}).get("type") for item in threads}
        page_ids = {item.get("id") for item in threads}
        if threads and not page_ids.intersection(loaded_ids) and not (runtime_types & {"active", "idle", "systemError"}):
            visibility_errors.append("The daemon returned persisted thread summaries only; live runtime agent states are unavailable to this bridge connection.")
        agents = []
        for item in threads:
            display = _thread_display(item)
            if not display:
                continue
            thread_id = display["id"]
            active_flags = (item.get("status") or {}).get("activeFlags", [])
            if thread_id in self.app.pending_inputs:
                questions = self.app.pending_inputs[thread_id]["questions"]
                display["status"] = "needs_attention"
                display["detail"] = (questions[0].get("question") or "Codex needs an answer")[:120] if questions else "Codex needs an answer"
            elif any("userinput" in str(flag).lower() for flag in active_flags):
                display["status"] = "needs_attention"
                display["detail"] = "Codex needs an answer; refresh to receive the question."
            elif thread_id in self.app.pending_approvals or any("approval" in str(flag).lower() for flag in (item.get("status") or {}).get("activeFlags", [])):
                display["status"] = "needs_attention"
                pending = self.app.pending_approvals.get(thread_id, {})
                reason = pending.get("reason") if isinstance(pending, dict) else None
                display["detail"] = ("Approval: " + str(reason))[:120] if reason else "Approval needed in Codex app"
            else:
                fallback = "Codex encountered an error" if display["status"] == "error" else "Codex is working"
                display["detail"] = self.app.latest_messages.get(thread_id) or fallback
            agents.append(display)
        try:
            rate_result = self.app.call("account/rateLimits/read")
            bucket = (rate_result.get("rateLimitsByLimitId") or {}).get("codex") or rate_result.get("rateLimits") or {}
            usage = {"five_hour": _quota_entry(bucket, 300), "weekly": _quota_entry(bucket, 10080)}
            health, errors = ("degraded" if visibility_errors else "connected"), visibility_errors
        except BridgeError as exc:
            usage = {"five_hour": {"used_percent": None, "resets_at": None}, "weekly": {"used_percent": None, "resets_at": None}}
            health, errors = "degraded", visibility_errors + [str(exc)]
        return {"integration": health, "codex": {"usage": usage}, "agents": agents[:8],
                "total_agents": len(agents),
                "updated_at": now, "errors": errors}

    def command(self, text, agent_id=None):
        if not isinstance(text, str):
            raise BridgeError("text must be a string", "invalid_request")
        if agent_id is not None and not isinstance(agent_id, str):
            raise BridgeError("agent_id must be a string", "invalid_request")
        text = text.strip()
        if not text or len(text) > MAX_TEXT_CHARS:
            raise BridgeError("text must contain 1 to 4000 characters", "invalid_request")
        if agent_id:
            if self.app.answer_user_input(agent_id, text):
                return {"ok": True, "transcript": text, "action": "answered_question", "agent_id": agent_id, "message": "Answer sent to the pending Codex question."}
            if agent_id in self.app.pending_approvals:
                raise BridgeError("Approval requests must be reviewed in the Codex app; the monitor will not approve them.", "pending_approval")
            if agent_id in self.app.pending_other:
                raise BridgeError("This pending Codex request must be handled in the Codex app.", "unsupported_pending_request")
            read = self.app.call("thread/read", {"threadId": agent_id, "includeTurns": True})
            thread = read.get("thread", {})
            status = thread.get("status") or {}
            if status.get("type") == "active":
                flags = status.get("activeFlags", [])
                if any("approval" in str(flag).lower() for flag in flags):
                    raise BridgeError("Approval requests must be reviewed in the Codex app; the monitor will not approve them.", "pending_approval")
                if any("userinput" in str(flag).lower() for flag in flags):
                    raise BridgeError("A Codex question is pending but its answer choices have not reached the bridge yet; refresh status and try again.", "pending_user_input_unavailable")
                turns = thread.get("turns") or []
                active = next((t for t in reversed(turns) if t.get("status") in ("inProgress", "running", "active")), None)
                if active and active.get("id"):
                    self.app.call("turn/steer", {"threadId": agent_id, "input": [{"type": "text", "text": text}], "expectedTurnId": active["id"]})
                    return {"ok": True, "transcript": text, "action": "steered", "agent_id": agent_id, "message": "Command added to the active Codex turn."}
            self.app.call("thread/resume", {"threadId": agent_id})
            self.app.call("turn/start", {"threadId": agent_id, "input": [{"type": "text", "text": text}]})
            return {"ok": True, "transcript": text, "action": "steered", "agent_id": agent_id, "message": "Command started in the selected Codex thread."}
        started = self.app.call("thread/start", {"cwd": self.agent_cwd})
        thread_id = (started.get("thread") or {}).get("id")
        if not thread_id:
            raise BridgeError("Codex app-server did not return a thread id")
        self.app.call("turn/start", {"threadId": thread_id, "input": [{"type": "text", "text": text}]})
        return {"ok": True, "transcript": text, "action": "created", "agent_id": thread_id, "message": "New Codex thread started."}

    def transcribe(self, wav):
        if not self.transcriber:
            raise BridgeError("Voice transcription is not configured; use POST /v1/commands/text or set TRANSCRIBE_URL.", "transcription_unconfigured")
        if self.transcriber_mode == "multipart":
            boundary = "fnkmonitor-" + secrets.token_hex(16)
            body = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"file\"; filename=\"voice.wav\"\r\n"
                    "Content-Type: audio/wav\r\n\r\n").encode("ascii") + wav + f"\r\n--{boundary}--\r\n".encode("ascii")
            headers = {"Content-Type": f"multipart/form-data; boundary={boundary}"}
        elif self.transcriber_mode == "raw":
            body, headers = wav, {"Content-Type": "audio/wav"}
        else:
            raise BridgeError("TRANSCRIBE_MODE must be raw or multipart", "transcription_configuration_error")
        request = Request(self.transcriber, data=body, headers=headers, method="POST")
        try:
            with urlopen(request, timeout=30) as response:
                result = json.loads(response.read().decode("utf-8"))
        except Exception as exc:
            raise BridgeError("Transcription service failed: " + str(exc), "transcription_failed") from exc
        text = result.get("text") or result.get("transcript")
        if not isinstance(text, str) or not text.strip():
            raise BridgeError("Transcription service returned no transcript", "transcription_failed")
        return text


def make_handler(bridge, monitor_token=None):
    class Handler(BaseHTTPRequestHandler):
        server_version = "FNKMonitorBridge/0.1"

        def _content_length(self, maximum):
            raw = self.headers.get("Content-Length")
            if raw is None:
                self._json(411, {"ok": False, "error": {"code": "content_length_required", "message": "Content-Length is required"}})
                return None
            try:
                length = int(raw)
            except ValueError:
                self._json(400, {"ok": False, "error": {"code": "invalid_content_length", "message": "Content-Length must be an integer"}})
                return None
            if length < 0:
                self._json(400, {"ok": False, "error": {"code": "invalid_content_length", "message": "Content-Length cannot be negative"}})
                return None
            if length > maximum:
                self._json(413, {"ok": False, "error": {"code": "request_too_large", "message": "Request body exceeds the endpoint limit"}})
                return None
            return length

        def _json(self, code, value):
            payload = json.dumps(value).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            if os.environ.get("MONITOR_LOG_REQUESTS") == "1":
                # Fixed endpoint labels only: never log bodies, keys or query values.
                route = urlparse(self.path).path
                endpoint = route if route in ("/v1/status", "/v1/voice", "/v1/commands/text") else "unknown"
                print(json.dumps({"event": "monitor_request", "client": self.client_address[0],
                                  "endpoint": endpoint, "http_status": code,
                                  "integration": value.get("integration")}), flush=True)

        def do_OPTIONS(self):
            self.send_response(204)
            self.end_headers()

        def do_GET(self):
            if not self._authorized():
                return self._json(401, {"error": {"code": "unauthorized", "message": "Missing or invalid X-Monitor-Key"}})
            if self.path != "/v1/status":
                return self._json(404, {"error": {"code": "not_found", "message": "Unknown endpoint"}})
            try:
                self._json(200, bridge.status())
            except BridgeError as exc:
                self._json(503, {"integration": "unavailable", "codex": {"usage": {"five_hour": {"used_percent": None, "resets_at": None}, "weekly": {"used_percent": None, "resets_at": None}}}, "agents": [], "total_agents": 0, "updated_at": int(time.time()), "errors": [str(exc)]})

        def do_POST(self):
            if not self._authorized():
                return self._json(401, {"ok": False, "error": {"code": "unauthorized", "message": "Missing or invalid X-Monitor-Key"}})
            parsed = urlparse(self.path)
            try:
                if parsed.path == "/v1/commands/text":
                    if self.headers.get_content_type() != "application/json":
                        return self._json(415, {"ok": False, "error": {"code": "unsupported_media_type", "message": "Expected application/json"}})
                    length = self._content_length(65536)
                    if length is None:
                        return
                    data = json.loads(self.rfile.read(length))
                    if not isinstance(data, dict):
                        return self._json(400, {"ok": False, "error": {"code": "invalid_request", "message": "JSON body must be an object"}})
                    result = bridge.command(data.get("text", ""), data.get("agent_id"))
                elif parsed.path == "/v1/voice":
                    if self.headers.get_content_type() != "audio/wav":
                        return self._json(415, {"ok": False, "error": {"code": "unsupported_media_type", "message": "Expected audio/wav"}})
                    length = self._content_length(MAX_AUDIO_BYTES)
                    if length is None:
                        return
                    if length == 0:
                        return self._json(400, {"ok": False, "error": {"code": "invalid_audio_size", "message": "WAV body must not be empty"}})
                    wav = self.rfile.read(length)
                    if not wav.startswith(b"RIFF") or wav[8:12] != b"WAVE":
                        return self._json(400, {"ok": False, "error": {"code": "invalid_wav", "message": "Body is not a RIFF/WAVE file"}})
                    text = bridge.transcribe(wav)
                    agent_id = parse_qs(parsed.query).get("agent_id", [None])[0]
                    result = bridge.command(text, agent_id)
                else:
                    return self._json(404, {"ok": False, "error": {"code": "not_found", "message": "Unknown endpoint"}})
                self._json(200, result)
            except BridgeError as exc:
                code = 503 if exc.code in ("app_server_unavailable", "app_server_timeout", "transcription_unconfigured", "transcription_failed") else 409 if exc.code.startswith("pending_") or exc.code.startswith("unsupported_pending_") else 400
                self._json(code, {"ok": False, "error": {"code": exc.code, "message": str(exc)}})
            except (ValueError, TypeError, json.JSONDecodeError) as exc:
                self._json(400, {"ok": False, "error": {"code": "invalid_request", "message": "Invalid JSON request"}})

        def log_message(self, fmt, *args):
            pass

        def _authorized(self):
            return monitor_token is None or hmac.compare_digest(self.headers.get("X-Monitor-Key", ""), monitor_token)
    return Handler


def main():
    host = os.environ.get("MONITOR_HOST", "127.0.0.1")
    port = int(os.environ.get("MONITOR_PORT", "8765"))
    try:
        is_loopback = ipaddress.ip_address(host).is_loopback
    except ValueError:
        is_loopback = False
    token = os.environ.get("MONITOR_TOKEN")
    if not is_loopback and not token:
        raise SystemExit("MONITOR_TOKEN is required when MONITOR_HOST is not a loopback address")
    bridge = Bridge(transcriber=os.environ.get("TRANSCRIBE_URL"), transcriber_mode=os.environ.get("TRANSCRIBE_MODE", "raw"))
    server = ThreadingHTTPServer((host, port), make_handler(bridge, None if is_loopback else token))
    print(f"FNK monitor bridge listening on http://{host}:{port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        if bridge.app.proc and bridge.app.proc.poll() is None:
            bridge.app.proc.terminate()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Small command-line controller for the FNK0104B monitor mock."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default=os.environ.get("MONITOR_MOCK_URL", "http://127.0.0.1:8765"))
    parser.add_argument("--token-file", default=os.environ.get("MONITOR_TOKEN_FILE"))
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("state", help="show current scenario and board poll count")
    scenario = commands.add_parser("scenario", help="set a built-in board state")
    scenario.add_argument("name", choices=("idle", "running", "attention", "error", "complete",
                                           "overflow", "degraded", "offline"))
    usage = commands.add_parser("usage", help="set five-hour and weekly used percentages")
    usage.add_argument("five_hour", type=int)
    usage.add_argument("weekly", type=int)
    agent = commands.add_parser("agent", help="add, update, or complete one mock agent")
    agent.add_argument("id")
    agent.add_argument("status", choices=("running", "needs_attention", "error", "complete"))
    agent.add_argument("--name", default="Agent")
    agent.add_argument("--detail", default="")
    args = parser.parse_args()

    path, payload = {
        "state": ("/_mock/state", None),
        "scenario": ("/_mock/scenario", {"scenario": getattr(args, "name", None)}),
        "usage": ("/_mock/usage", {"five_hour": getattr(args, "five_hour", None),
                                   "weekly": getattr(args, "weekly", None)}),
        "agent": ("/_mock/agent", {"id": getattr(args, "id", None),
                                   "status": getattr(args, "status", None),
                                   "name": getattr(args, "name", "Agent"),
                                   "detail": getattr(args, "detail", "")}),
    }[args.command]
    headers = {}
    if args.token_file:
        headers["X-Monitor-Key"] = Path(args.token_file).read_text(encoding="utf-8").strip()
    elif os.environ.get("MONITOR_TOKEN"):
        headers["X-Monitor-Key"] = os.environ["MONITOR_TOKEN"]
    data = None if payload is None else json.dumps(payload).encode("utf-8")
    if data is not None:
        headers["Content-Type"] = "application/json"
    request = Request(args.url.rstrip("/") + path, data=data, headers=headers,
                      method="GET" if data is None else "POST")
    try:
        with urlopen(request, timeout=5) as response:
            print(json.dumps(json.load(response), indent=2))
    except HTTPError as exc:
        print(json.dumps(json.load(exc), indent=2))
        raise SystemExit(f"mock server returned HTTP {exc.code}") from exc
    except URLError as exc:
        raise SystemExit(f"could not reach mock server: {exc.reason}") from exc


if __name__ == "__main__":
    main()

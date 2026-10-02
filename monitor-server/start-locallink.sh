#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."
export MONITOR_HOST="${FNK_MONITOR_BIND_HOST:?FNK_MONITOR_BIND_HOST is required}"
export MONITOR_PORT="${FNK_MONITOR_PORT:?FNK_MONITOR_PORT is required}"
export MONITOR_LOG_REQUESTS="${FNK_MONITOR_LOG_REQUESTS:-1}"
export TRANSCRIBE_URL="${FNK_MONITOR_TRANSCRIBE_URL:-}"
export TRANSCRIBE_MODE="${FNK_MONITOR_TRANSCRIBE_MODE:-multipart}"

# The ignored firmware header supplies the existing shared key.
exec python3 monitor-server/run.py

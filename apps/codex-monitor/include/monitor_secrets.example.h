#pragma once

// Copy to monitor_secrets.h in this directory. That filename is Git-ignored.
// Optional local-build migration defaults: saved board configuration takes precedence.
// Public firmware can be configured through encrypted Device setup instead.
// Use the LAN address of the machine running monitor-server. The bridge binds
// only to loopback by default; see monitor-server/README.md for LAN setup.
#define MONITOR_SERVER_HOST ""
#define MONITOR_SERVER_PORT 8765
#define MONITOR_SERVER_TOKEN ""

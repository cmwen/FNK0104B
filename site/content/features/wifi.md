---
title: "Wi-Fi & the local bridge"
summary: "Connect the display to services running on your own computer."
status: Implemented
---

## What Wi-Fi enables
The board connects to 2.4 GHz Wi-Fi. It can retrieve status, discover a LAN service or upload captured audio. Bluetooth can help set up the network, but the application data travels over Wi-Fi.

## The Codex monitor connection
The board talks to a Python bridge on your computer. The bridge queries the locally signed-in Codex app-server and sends usage and agent activity to the board, including server-sent status events. GitHub Pages hosts documentation, flashing and BLE controls; it does not host the bridge or carry the board's audio.

A private bridge address and shared key are compiled into an ignored local configuration header. The public browser firmware has no private bridge configuration. You need a locally configured build for your own live monitor. BLE currently changes volume and idle timeout only.

## Useful patterns
Show Wi-Fi connection and application-service health independently. Handle unavailable status without inventing quotas. Redraw on changes instead of continuously. Use a LAN address the board can reach; `localhost` on the board refers to the board itself. WSL needs special attention because Windows LAN access and WSL socket access are separate.

## Start here
Try `wifi` or `wifi-ble`, then `connectivity`, then a service-backed app such as `locallink` or `codex-monitor`. Follow the monitor setup walkthrough before building your private monitor.

## Brief your coding agent
> Connect to my trusted LAN service and show connection, stale data and recovery states separately. Reuse saved Wi-Fi. Keep credentials outside source control. Verify the service from the host and then from the board.

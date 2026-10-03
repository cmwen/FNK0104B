# Codex monitor speech integration — 2026-10-03

User authorized pushing the changes and flashing the connected FNK0104B.
PlatformIO Core uploaded the codex-monitor environment through /dev/ttyACM0;
all four image hashes verified. No erase-all or security setting change was used.
The uploaded partition table preserves NVS at 0x9000, size 0x5000, and replaces
OTA/FATFS with a 6 MiB factory app and the speech models at 0x610000.

The initial device check exposed internal-RAM fragmentation: the AFE feed task
could not allocate a contiguous 4 KiB stack (13,071 internal bytes free, largest
block 2,048). The monitor now statically reserves that internal stack and
initializes the microphone DMA buffers before loading the speech models.
Bluetooth prefers PSRAM; malloc's internal threshold is 1 KiB with a 64 KiB
internal reserve. The diagnostic apps retain their existing dynamic feed task.

Final upload and 115200-baud PlatformIO serial evidence:

```text
codex-monitor SUCCESS
firmware=codex-monitor
version=0.5.0
touch_i2c=ready address=0x38
monitor_ble advertising_data status=0
monitor_ble scan_response status=0
monitor_ble advertising_started status=0
monitor_speech state=ready wake=Hi_ESP wakenet=wn10_hiesp vadnet=vadnet1_medium multinet=mn7_en
monitor_wifi connected=true status=3
monitor_stream state=connected
monitor_status integration=connected agents=1 first_state=running five_hour_used=37 weekly_used=22
monitor_speech state=wake afe_frames=474 max_inference_us=6584 frame_us=32000 heap=7959 psram=4463476
monitor_speech state=wake afe_frames=1416 max_inference_us=6318 frame_us=32000 heap=7959 psram=4463476
```

Saved credentials survived subsequent CLI flashes and resets. Serial output also
showed provisioning obtain an IP, stop its BLE service and automatically restart
into the monitor during the earlier check; this is an observed sequence, not an
agent-driven browser security test. Network identity and raw logs are omitted.

Limits: spoken command accuracy, end-to-end VAD recording/submission, speaker
recovery, physical setup/cancel controls, incorrect PoP rejection on hardware,
browser-update credential preservation and long-duration heap stability remain
UNKNOWN. Internal heap is tight (about 8 KiB during the sampled live stream);
short startup/streaming evidence does not verify all simultaneous workloads.

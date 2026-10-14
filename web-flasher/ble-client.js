import { ESPProvisioner, Security1 } from "esp-ble-prov";
import {preparePhoto, crc32, photoRequest} from "./photo-client.js";

// WiFiProv's UUID byte array is least-significant-byte first on the BLE wire.
const SERVICE_UUID = "021a9004-0382-4aea-bff4-6b3f1c5adfb4";
const encoder = new TextEncoder();
const support = document.querySelector("#ble-support");
const popInput = document.querySelector("#ble-pop");
const connectButton = document.querySelector("#ble-connect");
const wifiForm = document.querySelector("#ble-wifi-form");
const ssidInput = document.querySelector("#ble-ssid");
const passwordInput = document.querySelector("#ble-password");
const sendButton = document.querySelector("#ble-send");
const status = document.querySelector("#ble-status");

let provisioner = null;
let busy = false;
let wifiConfirmed = false;
let secureSession = false;

function showStatus(message, error = false) {
  status.hidden = false;
  status.textContent = message;
  status.classList.toggle("error", error);
}

function setConnected(connected) {
  ssidInput.disabled = !connected;
  passwordInput.disabled = !connected;
  sendButton.disabled = !connected;
}

function browserReady() {
  return window.isSecureContext && "bluetooth" in navigator && Boolean(window.crypto?.subtle);
}

if (browserReady()) {
  support.textContent = "Use Chrome or Edge with Bluetooth enabled. Keep the board powered and nearby.";
} else {
  support.textContent = "Web Bluetooth requires HTTPS and a compatible browser such as Chrome or Edge with Bluetooth enabled.";
  support.classList.add("notice-error");
  connectButton.disabled = true;
}

connectButton.addEventListener("click", async () => {
  if (busy || !browserReady()) return;
  const pop = popInput.value.trim().toUpperCase();
  if (!/^[A-F0-9]{12}$/.test(pop)) {
    showStatus("Enter the 12-character code shown on the board.", true);
    popInput.focus();
    return;
  }
  busy = true;
  wifiConfirmed = false;
  secureSession = false;
  passwordInput.value = "";
  connectButton.disabled = true;
  setConnected(false);
  showStatus("Choose FNK0104B-SETUP in the Bluetooth device picker…");
  try {
    provisioner?.device?.gatt?.disconnect();
    provisioner = new ESPProvisioner({
      deviceNamePrefix: "FNK0104B-SETUP",
      serviceUUID: SERVICE_UUID,
      security: new Security1({ pop }),
    });
    await provisioner.connect();
    const connectedProvisioner = provisioner;
    connectedProvisioner.device?.addEventListener("gattserverdisconnected", () => {
      if (provisioner !== connectedProvisioner) return;
      secureSession = false;
      setConnected(false);
      passwordInput.value = "";
      if (!busy && !wifiConfirmed) showStatus("Disconnected. Connect again using the board's current code.", true);
    });
    showStatus("Securing the Bluetooth connection…");
    await provisioner.establishSession();
    secureSession = true;
    setConnected(true);
    showStatus("Connected securely. Enter your 2.4 GHz Wi-Fi details.");
    ssidInput.focus();
  } catch (error) {
    setConnected(false);
    showStatus(`Bluetooth setup failed: ${error instanceof Error ? error.message : String(error)}`, true);
    if (provisioner) await provisioner.disconnect().catch(() => {});
    provisioner = null;
  } finally {
    busy = false;
    connectButton.disabled = false;
  }
});

wifiForm.addEventListener("submit", async event => {
  event.preventDefault();
  if (busy || wifiConfirmed || !secureSession || !provisioner?.isConnected) {
    showStatus("Connect to the board over Bluetooth first.", true);
    return;
  }
  const ssid = encoder.encode(ssidInput.value);
  const passphrase = encoder.encode(passwordInput.value);
  if (!ssid.length || ssid.length > 32 || passphrase.length > 64) {
    showStatus("Wi-Fi name must be 1–32 bytes and password at most 64 bytes.", true);
    return;
  }
  busy = true;
  sendButton.disabled = true;
  showStatus("Sending Wi-Fi details and waiting for the board to connect…");
  try {
    await provisioner.sendCredentials({ ssid, passphrase }, 60000);
    wifiConfirmed = true;
    passwordInput.value = "";
    popInput.value = "";
    setConnected(false);
    showStatus("Wi-Fi connected and saved. The monitor restarts automatically; standalone setup firmware can now be replaced (leave Erase device unchecked).");
  } catch (error) {
    passwordInput.value = "";
    showStatus(`Could not confirm Wi-Fi connection: ${error instanceof Error ? error.message : String(error)} Check the board's screen.`, true);
  } finally {
    busy = false;
    sendButton.disabled = wifiConfirmed || !provisioner?.isConnected;
  }
});

// Monitor settings use a separate service and do not share Wi-Fi provisioning.
const MONITOR_SERVICE_UUID = "4e4b0104-0001-4d20-8f4b-0104b0000001";
const MONITOR_SETTINGS_UUID = "4e4b0104-0002-4d20-8f4b-0104b0000001";
const MONITOR_LED_PATTERN_SETTINGS_UUID = "4e4b0104-000a-4d20-8f4b-0104b0000001";
const MONITOR_LED_SETTINGS_UUID = "4e4b0104-0009-4d20-8f4b-0104b0000001";
const MONITOR_APPEARANCE_SETTINGS_UUID = "4e4b0104-0006-4d20-8f4b-0104b0000001";
const MONITOR_VOICE_SETTINGS_UUID = "4e4b0104-0004-4d20-8f4b-0104b0000001";
const MONITOR_OTA_UUID = "4e4b0104-0005-4d20-8f4b-0104b0000001";
const MONITOR_EXTENDED_SETTINGS_UUID = "4e4b0104-0003-4d20-8f4b-0104b0000001";
const monitorSupport = document.querySelector("#monitor-support");
const monitorConnect = document.querySelector("#monitor-connect");
const monitorForm = document.querySelector("#monitor-settings-form");
const monitorVolume = document.querySelector("#monitor-volume");
const monitorVolumeValue = document.querySelector("#monitor-volume-value");
const monitorDimTimeout = document.querySelector("#monitor-dim-timeout");
const monitorSave = document.querySelector("#monitor-save");
const monitorStatus = document.querySelector("#monitor-status");
const otaCheck = document.querySelector("#monitor-ota-check");
const otaInstall = document.querySelector("#monitor-ota-install");
const otaStatus = document.querySelector("#monitor-ota-status");
let otaCharacteristic = null;
let otaTimer = null;
let otaPolling = false;
let otaWorking = false;
let otaState = "idle";
let monitorDevice = null;
let monitorCharacteristic = null;
let monitorBusy = false;
let monitorSettingsVersion = 1;
const monitorLed = document.querySelector("#monitor-led");
const monitorLedPatterns = ['working', 'complete', 'attention'].map(state => document.querySelector(`#monitor-led-${state}-pattern`));
const monitorLedBrightness = document.querySelector('#monitor-led-brightness');
const monitorLedPeriod = document.querySelector('#monitor-led-period');
const monitorLedPreview = document.querySelector('#monitor-led-preview');
const monitorLedPreviewButton = document.querySelector('#monitor-led-preview-button');
const monitorLedPreviewState = document.querySelector('#monitor-led-preview-state');
let ledPreviewUntil = 0, ledPreviewStarted = 0;
const monitorLedSeconds = document.querySelector("#monitor-led-seconds");
const monitorAppearance = document.querySelector("#monitor-appearance");
const monitorSlots = document.querySelector("#monitor-slots");
const monitorVoice = document.querySelector("#monitor-voice");
const monitorVoiceHelp = document.querySelector("#monitor-voice-help");
const photoMode = document.querySelector("#monitor-idle-mode");
const photoDelay = document.querySelector("#monitor-photo-delay");
const photoInterval = document.querySelector("#monitor-photo-interval");
const photoFile = document.querySelector("#monitor-photo-file");
const photoPreview = document.querySelector("#monitor-photo-preview");
const photoDescription = document.querySelector("#monitor-photo-description");
const photoUpload = document.querySelector("#monitor-photo-upload");
const photoCancel = document.querySelector("#monitor-photo-cancel");
const photoRefresh = document.querySelector("#monitor-photo-refresh");
const photoList = document.querySelector("#monitor-photo-list");
const photoDelete = document.querySelector("#monitor-photo-delete");
const photoStatus = document.querySelector("#monitor-photo-status");
let photoConfigCharacteristic = null, photoTransferCharacteristic = null;
let preparedPhoto = null, photoWorking = false, photoCancelled = false, photoSequence = 0;

function photoControls() {
  const available = Boolean(photoConfigCharacteristic && monitorCharacteristic);
  const busy = photoWorking || monitorBusy || otaWorking;
  photoMode.disabled = photoDelay.disabled = photoInterval.disabled = !available || busy;
  photoFile.disabled = photoRefresh.disabled = !available || busy;
  photoList.disabled = photoDelete.disabled = !available || busy || !photoList.value;
  photoUpload.disabled = !available || busy || !preparedPhoto;
  photoCancel.disabled = !photoWorking;
}
function showPhotoStatus(message) { photoStatus.textContent = message; }
function readPhotoSettings(value) {
  const bytes = new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  const delay = bytes[2] | bytes[3] << 8, interval = bytes[4] | bytes[5] << 8;
  if (bytes.length !== 6 || bytes[0] !== 1 || bytes[1] > 2 || delay < 5 || delay > 7200 || interval < 5 || interval > 3600)
    throw new Error("Invalid idle photo settings from the board.");
  photoMode.value = String(bytes[1]); photoDelay.value = String(delay); photoInterval.value = String(interval);
}
function photoSettingsPacket() {
  const mode = Number(photoMode.value), delay = Number(photoDelay.value), interval = Number(photoInterval.value);
  if (!Number.isInteger(mode) || mode < 0 || mode > 2 || !Number.isInteger(delay) || delay < 5 || delay > 7200 ||
      !Number.isInteger(interval) || interval < 5 || interval > 3600)
    throw new Error("Choose an idle delay of 5–7200 seconds and photo interval of 5–3600 seconds.");
  if (mode && delay >= Number(monitorDimTimeout.value) * 60)
    throw new Error("The idle-view delay must be shorter than the screen-off timeout.");
  return new Uint8Array([1, mode, delay & 255, delay >> 8, interval & 255, interval >> 8]);
}
async function sendPhoto(op, payload = new Uint8Array()) {
  const characteristic = photoTransferCharacteristic;
  if (!characteristic) throw new Error("Photo connection was lost. Reconnect to the board.");
  photoSequence = (photoSequence % 65535) + 1;
  const result = await photoRequest(characteristic, op, photoSequence, payload);
  if (characteristic !== photoTransferCharacteristic) throw new Error("Photo connection was lost.");
  return result;
}
function updatePhotoList(result) {
  const previous = photoList.value;
  photoList.replaceChildren();
  for (const name of result.files || []) {
    const option = document.createElement("option"); option.value = name; option.textContent = name.replace(/\.fpf$/, "");
    photoList.append(option);
  }
  if ((result.files || []).includes(previous)) photoList.value = previous;
  else photoList.value = result.files?.[0] || "";
  photoControls();
}
async function refreshPhotos() {
  const result = await sendPhoto(4); updatePhotoList(result);
  showPhotoStatus(result.mounted ? `${result.files.length} of 8 photo slots used.` : "SD card unavailable. Insert a FAT32 card, then restart the board.");
}

function showMonitorStatus(message, error = false) {
  monitorStatus.hidden = false;
  monitorStatus.textContent = message;
  monitorStatus.classList.toggle("error", error);
}

function setMonitorControls(connected) {
  photoControls();
  otaCheck.disabled = !connected || !otaCharacteristic || otaWorking || monitorBusy || photoWorking;
  otaInstall.disabled = !connected || !otaCharacteristic || otaWorking || monitorBusy || photoWorking || otaState !== "available";
  monitorVolume.disabled = !connected;
  monitorDimTimeout.disabled = !connected;
  monitorSave.disabled = !connected || monitorBusy || otaWorking || photoWorking;
  for (const control of [...monitorLedPatterns, monitorLedBrightness, monitorLedPeriod])
    control.disabled = !connected || monitorSettingsVersion < 6;
  monitorLed.disabled = !connected || monitorSettingsVersion < 5;
  monitorLedSeconds.disabled = !connected || monitorSettingsVersion < 5;
  monitorAppearance.disabled = !connected || monitorSettingsVersion < 4;
  monitorSlots.disabled = !connected || monitorSettingsVersion < 2;
  monitorVoice.disabled = !connected || monitorSettingsVersion < 3;
  updateVoiceHelp();
}

function updateMonitorVolumeLabel() {
  monitorVolumeValue.value = `${monitorVolume.value}%`;
  monitorVolumeValue.textContent = `${monitorVolume.value}%`;
}

function updateVoiceHelp() {
  monitorVoiceHelp.textContent = !monitorCharacteristic
    ? "Connect to monitor settings to choose microphone controls. Use firmware 0.6.3 or newer; Desktop key mappings are configured separately."
    : monitorSettingsVersion < 3
      ? "Connected, but the board exposed older Bluetooth settings. This selector cannot change microphone behavior through that connection. If firmware 0.6.3 or newer is already installed, disconnect, restart the board and refresh this page. If it persists, remove the board from your computer's Bluetooth devices to clear cached services, then reconnect here. Otherwise update the monitor over USB."
      : monitorVoice.value === "1"
      ? "Hold to talk stays available. Voice toggles the board microphone on/off. Map the two separate microphone keys in Desktop before using Voice. Saving closes the microphone."
      : "Default: board USB audio is off until you hold Hold to talk, and off again on release. Desktop's first Mic key must use Push to talk. Saving closes the microphone.";
}
monitorVoice.addEventListener("change", updateVoiceHelp);

let preparationRevision = 0;
photoFile.addEventListener("change", async () => {
  const revision = ++preparationRevision;
  preparedPhoto = null; photoPreview.hidden = true; photoControls();
  const file = photoFile.files?.[0];
  if (!file) return;
  try {
    const result = await preparePhoto(file, photoPreview);
    if (revision !== preparationRevision) return;
    preparedPhoto = result; photoPreview.hidden = false;
    photoDescription.textContent = `${file.name} → ${result.name.replace(/\.fpf$/, "")} · 320 × 196 · ${Math.ceil(result.bytes.length / 1024)} KB. Uploading the same name replaces its previous photo.`;
    showPhotoStatus("Preview ready. The usage bar remains above this image.");
  } catch (error) { showPhotoStatus(`Could not prepare photo: ${error.message}`); }
  photoControls();
});

photoUpload.addEventListener("click", async () => {
  if (!preparedPhoto || !photoTransferCharacteristic || photoWorking || monitorBusy || otaWorking) return;
  photoWorking = true; photoCancelled = false; setMonitorControls(true);
  const prepared = preparedPhoto;
  let started = false;
  try {
    const name = new TextEncoder().encode(prepared.name);
    const metadata = new Uint8Array(8 + name.length);
    const view = new DataView(metadata.buffer);
    view.setUint32(0, prepared.bytes.length, true); view.setUint32(4, crc32(prepared.bytes), true); metadata.set(name, 8);
    showPhotoStatus("Starting upload…");
    const begin = await sendPhoto(1, metadata); started = true;
    if (begin.offset !== 0) throw new Error("Board did not start a fresh upload.");
    for (let offset = 0; offset < prepared.bytes.length;) {
      if (photoCancelled) throw new Error("Upload cancelled.");
      const chunk = prepared.bytes.subarray(offset, offset + 180);
      const payload = new Uint8Array(4 + chunk.length);
      new DataView(payload.buffer).setUint32(0, offset, true); payload.set(chunk, 4);
      const result = await sendPhoto(2, payload);
      if (result.offset !== offset + chunk.length) throw new Error("Board reported an unexpected upload offset.");
      offset += chunk.length;
      showPhotoStatus(`Uploading ${Math.floor(offset * 100 / prepared.bytes.length)}%…`);
    }
    if (photoCancelled) throw new Error("Upload cancelled.");
    const result = await sendPhoto(3);
    if (result.state !== "complete") throw new Error("Board did not finish the photo.");
    updatePhotoList(result);
    showPhotoStatus("Photo saved and verified on the SD card.");
  } catch (error) {
    if (started && photoTransferCharacteristic) { try { await sendPhoto(6); } catch {} }
    showPhotoStatus(`Photo was not saved: ${error.message}`);
  } finally { photoWorking = false; setMonitorControls(Boolean(monitorCharacteristic)); }
});
photoCancel.addEventListener("click", () => { photoCancelled = true; showPhotoStatus("Cancelling after the current chunk…"); });
photoRefresh.addEventListener("click", async () => {
  if (!photoTransferCharacteristic || photoWorking || monitorBusy || otaWorking) return;
  photoWorking = true; setMonitorControls(true);
  try { await refreshPhotos(); } catch (error) { showPhotoStatus(`Could not list photos: ${error.message}`); }
  finally { photoWorking = false; setMonitorControls(Boolean(monitorCharacteristic)); }
});
photoDelete.addEventListener("click", async () => {
  if (!photoTransferCharacteristic || !photoList.value || photoWorking || monitorBusy || otaWorking) return;
  photoWorking = true; setMonitorControls(true);
  try {
    const result = await sendPhoto(5, new TextEncoder().encode(photoList.value));
    updatePhotoList(result); showPhotoStatus("Photo deleted from the SD card.");
  } catch (error) { showPhotoStatus(`Could not delete photo: ${error.message}`); }
  finally { photoWorking = false; setMonitorControls(Boolean(monitorCharacteristic)); }
});

function decodeMonitorSettings(value) {
  const bytes = new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  if (bytes.length < 4 || bytes[0] < 1 || bytes[0] > 6 || bytes.length !== (bytes[0] === 6 ? 16 : bytes[0] >= 5 ? 10 : bytes[0] + 3))
    throw new Error("The board returned an invalid settings packet.");
  if (bytes[0] >= 2 && bytes[4] !== 3 && bytes[4] !== 6)
    throw new Error("The board returned an invalid Micro layout.");
  if (bytes[0] >= 3 && bytes[5] > 1)
    throw new Error("The board returned an invalid voice control mode.");
  if (bytes[0] >= 4 && bytes[6] > 1)
    throw new Error("The board returned an invalid slot appearance.");
  monitorAppearance.value = String(bytes[0] >= 4 ? bytes[6] : 0);
  const dimTimeout = bytes[2] | (bytes[3] << 8);
  if (bytes[1] > 100 || dimTimeout < 1 || dimTimeout > 120)
    throw new Error("The board returned settings outside the supported range.");
  const ledSeconds = bytes[0] >= 5 ? bytes[8] | bytes[9] << 8 : 60;
  if (bytes[0] >= 5 && (bytes[7] > 1 || ledSeconds < 5 || ledSeconds > 600))
    throw new Error("The board returned invalid status LED settings.");
  monitorLed.value = String(bytes[0] >= 5 ? bytes[7] : 1);
  monitorLedSeconds.value = String(ledSeconds);
  const brightness = bytes[0] === 6 ? bytes[13] : 6;
  const period = bytes[0] === 6 ? bytes[14] | bytes[15] << 8 : 2000;
  const patterns = bytes[0] === 6 ? [bytes[10], bytes[11], bytes[12]] : [0, 0, 0];
  if (patterns.some(pattern => pattern > 3) || brightness < 1 || brightness > 100 || period < 500 || period > 10000)
    throw new Error('The board returned invalid LED pattern settings.');
  monitorLedPatterns.forEach((control, i) => { control.value = String(patterns[i]); });
  monitorLedBrightness.value = String(brightness); monitorLedPeriod.value = String(period);
  monitorSettingsVersion = bytes[0];
  monitorSlots.value = String(bytes[0] >= 2 ? bytes[4] : 6);
  monitorVoice.value = String(bytes[0] >= 3 ? bytes[5] : 0);
  monitorVolume.value = String(bytes[1]);
  monitorDimTimeout.value = String(dimTimeout);
  updateMonitorVolumeLabel();
  updateVoiceHelp();
}

function monitorDisconnected() {
  photoConfigCharacteristic = photoTransferCharacteristic = null;
  photoCancelled = true;
  showPhotoStatus("Photo connection disconnected. Reconnect before uploading.");
  otaCharacteristic = null;
  if (otaTimer !== null) { clearTimeout(otaTimer); otaTimer = null; }
  otaStatus.textContent = otaState === "restarting" || otaState === "installing"
    ? "Board disconnected during the update. Wait for it to restart, reconnect, then check the installed version."
    : "Connect to read update support. Monitor 0.7.0 or newer needs a one-time USB install; updates download over Wi-Fi.";
  otaWorking = false;
  monitorCharacteristic = null;
  monitorDevice = null;
  setMonitorControls(false);
  if (!monitorBusy) showMonitorStatus("Disconnected from the monitor. Connect again to read or save settings.", true);
}

if (window.isSecureContext && "bluetooth" in navigator) {
  monitorSupport.textContent = "Use HTTPS or localhost in Chrome or Edge with Bluetooth enabled. The monitor must be powered and nearby.";
} else {
  monitorSupport.textContent = "Web Bluetooth requires HTTPS or localhost and a compatible browser such as Chrome or Edge.";
  monitorSupport.classList.add("notice-error");
  monitorConnect.disabled = true;
}

monitorVolume.addEventListener("input", updateMonitorVolumeLabel);

monitorConnect.addEventListener("click", async () => {
  if (monitorBusy || !window.isSecureContext || !("bluetooth" in navigator)) return;
  monitorBusy = true;
  monitorConnect.disabled = true;
  setMonitorControls(false);
  try {
    monitorDevice?.gatt?.disconnect();
    showMonitorStatus("Choose Codex Micro (or FNK0104B-MONITOR) in the Bluetooth device picker…");
    monitorDevice = await navigator.bluetooth.requestDevice({
      filters: [{ name: "Codex Micro" }, { name: "FNK0104B-MONITOR" }],
      optionalServices: [MONITOR_SERVICE_UUID],
    });
    monitorDevice.addEventListener("gattserverdisconnected", monitorDisconnected);
    const server = await monitorDevice.gatt.connect();
    const service = await server.getPrimaryService(MONITOR_SERVICE_UUID);
    for (const uuid of [MONITOR_LED_PATTERN_SETTINGS_UUID, MONITOR_LED_SETTINGS_UUID, MONITOR_APPEARANCE_SETTINGS_UUID, MONITOR_VOICE_SETTINGS_UUID, MONITOR_EXTENDED_SETTINGS_UUID, MONITOR_SETTINGS_UUID]) {
      try { monitorCharacteristic = await service.getCharacteristic(uuid); break; }
      catch (error) { if (error.name !== "NotFoundError") throw error; }
    }
    if (!monitorCharacteristic) throw new Error("Monitor settings are unavailable.");
    if (!monitorCharacteristic.properties.read || !monitorCharacteristic.properties.write) {
      throw new Error("The monitor settings characteristic does not support read and write.");
    }
    try { otaCharacteristic = await service.getCharacteristic(MONITOR_OTA_UUID); }
    catch (error) { if (error.name !== "NotFoundError") throw error; otaCharacteristic = null; }
    if (otaCharacteristic) {
      await readOtaStatus(true);
    } else {
      otaStatus.textContent = "Wireless updates are unavailable through this connection. Install monitor 0.7.0 or newer over USB once. If already installed, restart the board and browser; clear cached Bluetooth services if needed.";
    }
    try {
      photoConfigCharacteristic = await service.getCharacteristic("4e4b0104-0007-4d20-8f4b-0104b0000001");
      photoTransferCharacteristic = await service.getCharacteristic("4e4b0104-0008-4d20-8f4b-0104b0000001");
      readPhotoSettings(await photoConfigCharacteristic.readValue());
      photoSequence = Math.floor(Math.random() * 60000) + 1;
      await refreshPhotos();
    } catch (error) {
      photoConfigCharacteristic = photoTransferCharacteristic = null;
      showPhotoStatus(error.name === "NotFoundError" ? "Photo support requires monitor 0.8.0 or newer." : `Photos unavailable: ${error.message}`);
    }
    showMonitorStatus("Connected. Reading settings…");
    decodeMonitorSettings(await monitorCharacteristic.readValue());
    setMonitorControls(true);
    showMonitorStatus(monitorSettingsVersion < 3
      ? "Settings loaded. Volume and supported layout controls are available; microphone controls need the newer Bluetooth settings. See the explanation below Microphone controls."
      : "Settings loaded. Adjust them and save to update the board.");
  } catch (error) {
    monitorDevice?.gatt?.disconnect();
    monitorDevice = null;
    monitorCharacteristic = null;
    otaCharacteristic = null;
    setMonitorControls(false);
    showMonitorStatus(`Could not connect to or read monitor settings: ${error instanceof Error ? error.message : String(error)}`, true);
  } finally {
    monitorBusy = false;
    monitorConnect.disabled = false;
    setMonitorControls(Boolean(monitorCharacteristic));
  }
});

monitorForm.addEventListener("submit", async event => {
  event.preventDefault();
  if (monitorBusy || otaWorking || !monitorCharacteristic) {
    showMonitorStatus("Connect to the monitor before saving settings.", true);
    return;
  }
  const volume = Number(monitorVolume.value);
  const dimTimeout = Number(monitorDimTimeout.value);
  if (!Number.isInteger(volume) || volume < 0 || volume > 100 || !Number.isInteger(dimTimeout) || dimTimeout < 1 || dimTimeout > 120) {
    showMonitorStatus("Volume must be 0–100% and idle screen timeout must be 1–120 minutes.", true);
    return;
  }
  monitorBusy = true;
  monitorSave.disabled = true;
  showMonitorStatus("Saving settings to the board…");
  try {
    const slots = Number(monitorSlots.value);
    if (monitorSettingsVersion >= 2 && slots !== 3 && slots !== 6)
      throw new Error("Choose three or six Micro agent slots.");
    const voice = Number(monitorVoice.value);
    if (monitorSettingsVersion >= 3 && voice !== 0 && voice !== 1)
      throw new Error("Choose Hold to talk or Hold to talk + Voice.");
    const packet = [monitorSettingsVersion, volume, dimTimeout & 0xff, dimTimeout >> 8];
    if (monitorSettingsVersion >= 2) packet.push(slots);
    if (monitorSettingsVersion >= 3) packet.push(voice);
    if (monitorSettingsVersion >= 4) {
      const appearance = Number(monitorAppearance.value);
      if (appearance !== 0 && appearance !== 1) throw new Error("Choose agent numbers or robot avatars.");
      packet.push(appearance);
    }
    if (monitorSettingsVersion >= 5) {
      const enabled = Number(monitorLed.value), seconds = Number(monitorLedSeconds.value);
      if ((enabled !== 0 && enabled !== 1) || !Number.isInteger(seconds) || seconds < 5 || seconds > 600)
        throw new Error("Choose a status LED timeout between 5 and 600 seconds.");
      packet.push(enabled, seconds & 0xff, seconds >> 8);
    }
    if (monitorSettingsVersion >= 6) {
      const patterns = monitorLedPatterns.map(control => Number(control.value));
      const brightness = Number(monitorLedBrightness.value), period = Number(monitorLedPeriod.value);
      if (patterns.some(pattern => !Number.isInteger(pattern) || pattern < 0 || pattern > 3) ||
          !Number.isInteger(brightness) || brightness < 1 || brightness > 100 ||
          !Number.isInteger(period) || period < 500 || period > 10000)
        throw new Error('Choose valid patterns, brightness 1–100%, and a cycle from 500 to 10000 ms.');
      packet.push(...patterns, brightness, period & 0xff, period >> 8);
    }
    const bytes = new Uint8Array(packet);
    const photoBytes = photoConfigCharacteristic ? photoSettingsPacket() : null;
    await monitorCharacteristic.writeValue(bytes);
    if (photoBytes) {
      const characteristic = photoConfigCharacteristic;
      await characteristic.writeValue(photoBytes);
      let matched = false;
      for (let attempt = 0; attempt < 100; attempt++) {
        const value = await characteristic.readValue();
        const actual = new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
        if (actual.length === photoBytes.length && actual.every((byte, i) => byte === photoBytes[i])) {
          readPhotoSettings(value); matched = true; break;
        }
        await new Promise(resolve => setTimeout(resolve, 100));
      }
      if (!matched) throw new Error("Idle photo settings were not accepted. Finish voice or firmware-update activity and retry.");
    }
    decodeMonitorSettings(await monitorCharacteristic.readValue());
    showMonitorStatus(monitorSettingsVersion >= 3
      ? "Board settings saved and read back. Microphone is off. Check the Desktop key mappings before using voice controls."
      : "Settings saved and read back from the monitor.");
  } catch (error) {
    showMonitorStatus(`Could not save monitor settings: ${error instanceof Error ? error.message : String(error)}`, true);
  } finally {
    monitorBusy = false;
    monitorSave.disabled = !monitorCharacteristic;
    photoControls();
  }
});


async function readOtaStatus(initial = false) {
  if (!otaCharacteristic || otaPolling) return;
  if (monitorBusy && !initial) {
    otaTimer = setTimeout(() => { otaTimer = null; readOtaStatus(); }, 1500); return;
  }
  otaPolling = true;
  const characteristic = otaCharacteristic;
  try {
    const value = await characteristic.readValue();
    if (characteristic !== otaCharacteristic) return;
    const status = JSON.parse(new TextDecoder().decode(value));
    if (!status || typeof status.state !== "string" || typeof status.current !== "string") throw new Error("Invalid update status");
    otaState = status.state;
    otaWorking = ["checking", "installing", "restarting"].includes(otaState);
    otaStatus.textContent = `Installed: ${status.current}${status.latest ? ` · Published: ${status.latest}` : ""}. ${status.message}${otaState === "installing" ? ` (${status.progress}%)` : ""}`;
    setMonitorControls(Boolean(monitorCharacteristic));
    if (otaWorking && otaState !== "restarting") otaTimer = setTimeout(() => { otaTimer = null; readOtaStatus(); }, 1500);
  } catch (error) {
    otaStatus.textContent = `Could not read update status: ${error.message}. Reconnect to check the board.`;
    otaWorking = false;
    setMonitorControls(Boolean(monitorCharacteristic));
  } finally { otaPolling = false; }
}
async function commandOta(command) {
  if (!otaCharacteristic || monitorBusy || otaWorking || otaPolling) return;
  if (command === "install" && otaState !== "available") return;
  otaWorking = true;
  setMonitorControls(Boolean(monitorCharacteristic));
  otaStatus.textContent = command === "install" ? "Install requested. Keep the board powered; microphone will stay off." : "Checking update support and Wi-Fi…";
  try {
    await otaCharacteristic.writeValue(new TextEncoder().encode(command));
    // The board starts the worker on its next UI loop, after closing audio.
    otaTimer = setTimeout(() => { otaTimer = null; readOtaStatus(); }, 1500);
  } catch (error) {
    otaWorking = false;
    otaStatus.textContent = `Could not request update: ${error.message}`;
    setMonitorControls(Boolean(monitorCharacteristic));
  }
}
otaCheck.addEventListener("click", () => commandOta("check"));
otaInstall.addEventListener("click", () => commandOta("install"));


// A browser-only preview never writes the board or extends its notification.
if (monitorLedPreviewButton && typeof requestAnimationFrame === 'function') {
  monitorLedPreviewButton.addEventListener('click', () => {
    ledPreviewStarted = performance.now(); ledPreviewUntil = ledPreviewStarted + 10000;
    monitorLedPreviewButton.textContent = 'Restart 10-second preview';
  });
  const animate = now => {
    const state = Number(monitorLedPreviewState.value || 0);
    const pattern = Number(monitorLedPatterns[state].value || 0);
    const period = Number(monitorLedPeriod.value) || 2000;
    const phase = ((now - ledPreviewStarted) % period) / period;
    let opacity = Number(monitorLedBrightness.value || 6) / 100;
    if (now >= ledPreviewUntil) opacity = 0;
    else if (pattern === 1 && phase >= .5) opacity = 0;
    else if (pattern === 2) { const t = 1 - Math.abs(phase * 2 - 1); opacity *= t*t*(3-2*t); }
    monitorLedPreview.style.backgroundColor = pattern === 3 ? `hsl(${phase * 360} 100% 50%)` : ['#00ff00','#ffff00','#ff0000'][state];
    monitorLedPreview.style.opacity = String(opacity);
    requestAnimationFrame(animate);
  };
  requestAnimationFrame(animate);
}

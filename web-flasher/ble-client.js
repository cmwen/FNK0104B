import { ESPProvisioner, Security1 } from "esp-ble-prov";

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
const monitorAppearance = document.querySelector("#monitor-appearance");
const monitorSlots = document.querySelector("#monitor-slots");
const monitorVoice = document.querySelector("#monitor-voice");
const monitorVoiceHelp = document.querySelector("#monitor-voice-help");

function showMonitorStatus(message, error = false) {
  monitorStatus.hidden = false;
  monitorStatus.textContent = message;
  monitorStatus.classList.toggle("error", error);
}

function setMonitorControls(connected) {
  otaCheck.disabled = !connected || !otaCharacteristic || otaWorking || monitorBusy;
  otaInstall.disabled = !connected || !otaCharacteristic || otaWorking || monitorBusy || otaState !== "available";
  monitorVolume.disabled = !connected;
  monitorDimTimeout.disabled = !connected;
  monitorSave.disabled = !connected || monitorBusy || otaWorking;
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

function decodeMonitorSettings(value) {
  const bytes = new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  if (bytes.length < 4 || bytes[0] < 1 || bytes[0] > 4 || bytes.length !== bytes[0] + 3)
    throw new Error("The board returned an invalid settings packet.");
  if (bytes[0] >= 2 && bytes[4] !== 3 && bytes[4] !== 6)
    throw new Error("The board returned an invalid Micro layout.");
  if (bytes[0] >= 3 && bytes[5] > 1)
    throw new Error("The board returned an invalid voice control mode.");
  if (bytes[0] === 4 && bytes[6] > 1)
    throw new Error("The board returned an invalid slot appearance.");
  monitorAppearance.value = String(bytes[0] === 4 ? bytes[6] : 0);
  const dimTimeout = bytes[2] | (bytes[3] << 8);
  if (bytes[1] > 100 || dimTimeout < 1 || dimTimeout > 120)
    throw new Error("The board returned settings outside the supported range.");
  monitorSettingsVersion = bytes[0];
  monitorSlots.value = String(bytes[0] >= 2 ? bytes[4] : 6);
  monitorVoice.value = String(bytes[0] >= 3 ? bytes[5] : 0);
  monitorVolume.value = String(bytes[1]);
  monitorDimTimeout.value = String(dimTimeout);
  updateMonitorVolumeLabel();
  updateVoiceHelp();
}

function monitorDisconnected() {
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
    for (const uuid of [MONITOR_APPEARANCE_SETTINGS_UUID, MONITOR_VOICE_SETTINGS_UUID, MONITOR_EXTENDED_SETTINGS_UUID, MONITOR_SETTINGS_UUID]) {
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
    const bytes = new Uint8Array(packet);
    await monitorCharacteristic.writeValue(bytes);
    decodeMonitorSettings(await monitorCharacteristic.readValue());
    showMonitorStatus(monitorSettingsVersion >= 3
      ? "Board settings saved and read back. Microphone is off. Check the Desktop key mappings before using voice controls."
      : "Settings saved and read back from the monitor.");
  } catch (error) {
    showMonitorStatus(`Could not save monitor settings: ${error instanceof Error ? error.message : String(error)}`, true);
  } finally {
    monitorBusy = false;
    monitorSave.disabled = !monitorCharacteristic;
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

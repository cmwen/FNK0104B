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
const monitorSupport = document.querySelector("#monitor-support");
const monitorConnect = document.querySelector("#monitor-connect");
const monitorForm = document.querySelector("#monitor-settings-form");
const monitorVolume = document.querySelector("#monitor-volume");
const monitorVolumeValue = document.querySelector("#monitor-volume-value");
const monitorDimTimeout = document.querySelector("#monitor-dim-timeout");
const monitorSave = document.querySelector("#monitor-save");
const monitorStatus = document.querySelector("#monitor-status");
let monitorDevice = null;
let monitorCharacteristic = null;
let monitorBusy = false;

function showMonitorStatus(message, error = false) {
  monitorStatus.hidden = false;
  monitorStatus.textContent = message;
  monitorStatus.classList.toggle("error", error);
}

function setMonitorControls(connected) {
  monitorVolume.disabled = !connected;
  monitorDimTimeout.disabled = !connected;
  monitorSave.disabled = !connected || monitorBusy;
}

function updateMonitorVolumeLabel() {
  monitorVolumeValue.value = `${monitorVolume.value}%`;
  monitorVolumeValue.textContent = `${monitorVolume.value}%`;
}

function decodeMonitorSettings(value) {
  if (value.byteLength !== 4) throw new Error("The board returned an invalid settings packet.");
  const bytes = new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  if (bytes[0] !== 1) throw new Error(`Unsupported settings version ${bytes[0]}.`);
  const dimTimeout = bytes[2] | (bytes[3] << 8);
  if (bytes[1] > 100 || dimTimeout < 1 || dimTimeout > 120) {
    throw new Error("The board returned settings outside the supported range.");
  }
  monitorVolume.value = String(bytes[1]);
  monitorDimTimeout.value = String(dimTimeout);
  updateMonitorVolumeLabel();
}

function monitorDisconnected() {
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
    monitorCharacteristic = await service.getCharacteristic(MONITOR_SETTINGS_UUID);
    if (!monitorCharacteristic.properties.read || !monitorCharacteristic.properties.write) {
      throw new Error("The monitor settings characteristic does not support read and write.");
    }
    showMonitorStatus("Connected. Reading settings…");
    decodeMonitorSettings(await monitorCharacteristic.readValue());
    setMonitorControls(true);
    showMonitorStatus("Settings loaded. Adjust them and save to update the board.");
  } catch (error) {
    monitorDevice?.gatt?.disconnect();
    monitorDevice = null;
    monitorCharacteristic = null;
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
  if (monitorBusy || !monitorCharacteristic) {
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
    const bytes = new Uint8Array([1, volume, dimTimeout & 0xff, dimTimeout >> 8]);
    await monitorCharacteristic.writeValue(bytes);
    showMonitorStatus("Settings saved to the monitor.");
  } catch (error) {
    showMonitorStatus(`Could not save monitor settings: ${error instanceof Error ? error.message : String(error)}`, true);
  } finally {
    monitorBusy = false;
    monitorSave.disabled = !monitorCharacteristic;
  }
});

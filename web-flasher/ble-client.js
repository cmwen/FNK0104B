import { ESPProvisioner, Security1 } from "esp-ble-prov";

const SERVICE_UUID = "b4df5a1c-3f6b-f4bf-ea4a-820304901a02";
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
    showStatus("Securing the Bluetooth connection…");
    await provisioner.establishSession();
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
  if (busy || !provisioner?.isConnected) {
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
    passwordInput.value = "";
    setConnected(false);
    showStatus("Wi-Fi connected and saved on the board. Install LocalLink speech firmware next.");
  } catch (error) {
    passwordInput.value = "";
    showStatus(`Could not confirm Wi-Fi connection: ${error instanceof Error ? error.message : String(error)} Check the board's screen.`, true);
  } finally {
    busy = false;
    sendButton.disabled = !provisioner?.isConnected;
  }
});

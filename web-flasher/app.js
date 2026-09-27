const catalogUrl = new URL("firmware/catalog.json", document.baseURI);
const installButton = document.querySelector("#install");
const supportMessage = document.querySelector("#support-message");
const manifestError = document.querySelector("#manifest-error");
const firmwareChoice = document.querySelector("#firmware-choice");
const firmwareDescription = document.querySelector("#firmware-description");
const hint = document.querySelector("#install-hint");
const flashStatus = document.querySelector("#flash-status");
let firmwareBuilds = [];

function setNotice(message, error = false) {
  supportMessage.textContent = message;
  supportMessage.classList.toggle("notice-error", error);
}

function describeState(event) {
  const state = event.detail;
  if (!state) return;
  const labels = {
    initializing: "Connecting to the board…",
    manifest: "Preparing firmware…",
    preparing: "Preparing the ESP32-S3…",
    erasing: "Preparing flash memory…",
    writing: state.details?.percentage == null ? "Writing firmware…" : `Writing firmware… ${Math.round(state.details.percentage)}%`,
    finished: "Firmware installed. You can disconnect the board.",
    error: state.details?.details?.message || state.message || "The install did not finish. Check the USB connection and retry."
  };
  const message = labels[state.state] || state.message;
  if (!message) return;
  flashStatus.hidden = false;
  flashStatus.textContent = message;
  flashStatus.classList.toggle("error", state.state === "error");
}

installButton.addEventListener("state-changed", describeState);

async function selectFirmware(id) {
  const firmware = firmwareBuilds.find(build => build.id === id);
  if (!firmware) return;
  installButton.hidden = true;
  hint.hidden = false;
  hint.textContent = "Checking firmware release…";
  manifestError.hidden = true;
  firmwareDescription.textContent = firmware.description || "Firmware built for this FNK0104B board.";
  try {
    const manifestUrl = new URL(firmware.manifest, document.baseURI);
    const response = await fetch(manifestUrl, { cache: "no-store" });
    if (!response.ok) throw new Error(`Firmware manifest returned HTTP ${response.status}.`);
    const manifest = await response.json();
    if (!Array.isArray(manifest.builds) || !manifest.builds.some(build => build.chipFamily === "ESP32-S3" && Array.isArray(build.parts) && build.parts.length > 0)) {
      throw new Error("This release does not contain an ESP32-S3 build.");
    }
    installButton.setAttribute("manifest", manifestUrl.href);
    installButton.hidden = false;
    hint.hidden = true;
  } catch (error) {
    hint.hidden = true;
    manifestError.hidden = false;
    manifestError.textContent = `Could not load ${firmware.name} firmware. ${error instanceof Error ? error.message : String(error)} Try again after the firmware build has been published.`;
  }
}

async function prepareInstaller() {
  if (!window.isSecureContext) {
    setNotice("USB access is available on HTTPS or localhost. Open the published GitHub Pages address to install firmware.", true);
  } else if (!("serial" in navigator)) {
    setNotice("Web Serial is unavailable in this browser. Use desktop Chrome or Microsoft Edge to flash the board.", true);
  } else {
    setNotice("Ready when you are. The browser will ask you to select the board’s USB serial port.");
  }

  try {
    const response = await fetch(catalogUrl, { cache: "no-store" });
    if (!response.ok) throw new Error(`Firmware manifest returned HTTP ${response.status}.`);
    const catalog = await response.json();
    if (!Array.isArray(catalog.builds) || catalog.builds.length === 0) throw new Error("No firmware environments are listed.");
    firmwareBuilds = catalog.builds.filter(build => typeof build.id === "string" && typeof build.name === "string" && typeof build.manifest === "string");
    if (!firmwareBuilds.length) throw new Error("The firmware catalog has no valid builds.");
    firmwareChoice.replaceChildren(...firmwareBuilds.map(build => {
      const option = document.createElement("option");
      option.value = build.id;
      option.textContent = build.name;
      return option;
    }));
    firmwareChoice.disabled = false;
    firmwareChoice.addEventListener("change", () => selectFirmware(firmwareChoice.value));
    await selectFirmware(firmwareChoice.value);
  } catch (error) {
    firmwareChoice.replaceChildren(new Option("Firmware unavailable", ""));
    hint.hidden = true;
    manifestError.hidden = false;
    manifestError.textContent = `Could not load this repository’s firmware catalog. ${error instanceof Error ? error.message : String(error)} Try again after the firmware build has been published.`;
  }
}

prepareInstaller();

if ("serviceWorker" in navigator && window.isSecureContext) {
  window.addEventListener("load", () => navigator.serviceWorker.register(new URL("service-worker.js", document.baseURI)).catch(() => {}));
}

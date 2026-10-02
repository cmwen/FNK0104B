const taskLinks = [...document.querySelectorAll(".task-nav a")];
const flashPanel = document.querySelector("#flash-panel");
const setupPanel = document.querySelector("#setup-panel");

function showTask() {
  // Preserve existing links to the monitor and Wi-Fi settings sections.
  const anchor = window.location.hash.slice(1);
  const setup = anchor === "setup" || setupPanel.querySelector(`[id="${CSS.escape(anchor)}"]`) !== null;
  flashPanel.hidden = setup;
  setupPanel.hidden = !setup;
  for (const link of taskLinks) {
    if (link.hash === (setup ? "#setup" : "#flash")) link.setAttribute("aria-current", "page");
    else link.removeAttribute("aria-current");
  }
  // Deep-link targets may have been hidden when the browser first navigated.
  const target = anchor === "setup" || anchor === "flash"
    ? document.querySelector(".task-nav") : document.getElementById(anchor);
  if (target && anchor) target.scrollIntoView({ block: "start" });
}

window.addEventListener("hashchange", showTask);
showTask();

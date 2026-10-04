const CACHE_NAME = "fnk0104b-guide-shell-v9";
const APP_SHELL = ["./", "./index.html", "./setup.html", "./styles.css", "./navigation.js", "./app.js", "./ble-client.bundle.js", "./manifest.webmanifest", "./icon.svg", "./icon-192.png", "./icon-512.png", "./docs/images/recorder-ui.png", "./docs/images/codex-monitor-screen.png"];

self.addEventListener("install", event => {
  event.waitUntil(caches.open(CACHE_NAME).then(cache => cache.addAll(APP_SHELL)));
  self.skipWaiting();
});

self.addEventListener("activate", event => {
  event.waitUntil(caches.keys().then(keys => Promise.all(keys.filter(key => key !== CACHE_NAME).map(key => caches.delete(key)))));
  self.clients.claim();
});

self.addEventListener("fetch", event => {
  const request = event.request;
  const url = new URL(request.url);
  // Firmware must stay network-fetched so a cached page can never flash a stale release.
  if (request.method !== "GET" || url.origin !== self.location.origin || url.pathname.includes("/firmware/")) return;
  event.respondWith(fetch(request).then(response => {
    if (response.ok) caches.open(CACHE_NAME).then(cache => cache.put(request, response.clone()));
    return response;
  }).catch(() => caches.match(request)));
});

import { cp, mkdir, readdir, rm } from 'node:fs/promises';
const root = new URL('../../', import.meta.url);
const target = new URL('../public/', import.meta.url);
await rm(target, { recursive: true, force: true });
await mkdir(target, { recursive: true });
const assets = ['index.html', 'styles.css', 'navigation.js', 'app.js', 'ble-client.bundle.js', 'service-worker.js', 'manifest.webmanifest', 'icon.svg', 'icon-192.png', 'icon-512.png', 'THIRD_PARTY_LICENSES.txt'];
for (const asset of assets) {
  await cp(new URL(`web-flasher/${asset}`, root), new URL(asset === 'index.html' ? 'setup.html' : asset, target));
}
await cp(new URL('docs/images/', root), new URL('docs/images/', target), { recursive: true });

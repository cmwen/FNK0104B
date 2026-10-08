import { chromium } from '@playwright/test';
import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { createServer } from 'node:http';
const server=createServer(async(req,res)=>{
 try {
  const url=new URL(req.url,'http://localhost');
  if(!url.pathname.startsWith('/FNK0104B/')) {res.writeHead(404);res.end();return;}
  let file=url.pathname.slice('/FNK0104B/'.length);
  if(file.includes('..')) {res.writeHead(400);res.end();return;}
  if(file.endsWith('/')||file==='')file+='index.html';
  const types={html:'text/html',js:'application/javascript',css:'text/css',png:'image/png',svg:'image/svg+xml',json:'application/json',webmanifest:'application/manifest+json'};
  const bytes=await readFile(new URL(`../dist/${file}`,import.meta.url));
  res.setHeader('Content-Type',types[file.split('.').at(-1)]||'application/octet-stream');res.end(bytes);
 } catch {res.writeHead(404);res.end();}
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const browser = await chromium.launch({headless:true, args:['--no-sandbox']});
const origin=process.env.GUIDE_PREVIEW_URL||`http://127.0.0.1:${server.address().port}/FNK0104B/`;
const captures=process.env.GUIDE_CAPTURE_DIR
 ? new URL(`file://${process.env.GUIDE_CAPTURE_DIR.replace(/\/$/, '')}/`)
 : new URL('../../docs/site-previews/',import.meta.url);
await mkdir(captures,{recursive:true});
try {
 const page=await browser.newPage({viewport:{width:1440,height:1000},serviceWorkers:'block'});
 const errors=[];
 page.on('pageerror',e=>errors.push(e.message));
 await page.route('https://unpkg.com/**', route=>route.fulfill({contentType:'application/javascript',body:''}));
 // Exercise the catalog and manifest selection without installing or accessing USB.
 await page.route('**/firmware/catalog.json',route=>route.fulfill({json:{builds:[{id:'hello',name:'Hello',description:'Serial diagnostic',manifest:'firmware/hello/manifest.json'},{id:'codex-monitor',name:'Codex monitor',description:'Monitor',manifest:'firmware/codex-monitor/manifest.json'}]}}));
 await page.route('**/firmware/*/manifest.json',route=>route.fulfill({json:{name:'Test',builds:[{chipFamily:'ESP32-S3',parts:[{path:'firmware.bin',offset:65536}]}]}}));
 await page.goto(origin);
 await page.screenshot({path:new URL('desktop.png',captures).pathname,fullPage:true});
 await page.getByRole('link',{name:'Explore the capabilities →'}).click();
 await page.locator('#search').fill('USB');
 assert.equal(await page.locator('#capabilities .card:visible').count(),1);
 await page.locator('#capabilities .card:visible a').click();
 await page.getByRole('heading',{name:'USB emoji and number-pad keyboard'}).waitFor();
 await page.goto(`${origin}firmware/`);
 await page.getByRole('heading',{name:'Find your next firmware.'}).waitFor();
 await page.goto(`${origin}monitor/`);
 await page.getByRole('heading',{name:'Map buttons to custom actions in Codex Desktop'}).waitFor();
 await page.getByRole('link',{name:'Device setup → Monitor settings'}).click();
 await page.locator('#monitor-slots option[value="3"]').waitFor({state:'attached'});
 assert.equal(await page.locator('#monitor-slots option[value="6"]').textContent(),'6 agent slots');
 await page.goto(`${origin}monitor/`);
 await page.screenshot({path:new URL('monitor.png',captures).pathname,fullPage:true});
 await page.goto(`${origin}firmware/codex-monitor/`);
 await page.getByRole('link',{name:'Install Codex monitor over USB ↗'}).click();
 await page.waitForFunction(()=>document.querySelector('#install').getAttribute('manifest')?.endsWith('firmware/codex-monitor/manifest.json'));
 assert.match(await page.locator('#install').getAttribute('manifest'),/firmware\/codex-monitor\/manifest.json$/);
 await page.goto(`${origin}index.html#setup`);
 await page.waitForURL('**/setup.html#setup');
 assert.equal(await page.locator('#setup-panel').isVisible(),true);
 assert.equal(await page.locator('#flash-panel').isVisible(),false);
 await page.goto(`${origin}#flash`);
 await page.waitForURL('**/setup.html#flash');
 assert.equal(await page.locator('#flash-panel').isVisible(),true);
 await page.setViewportSize({width:390,height:844});
 for(const path of ['', 'features/','features/usb-hid/','firmware/','monitor/','journey/']) {
  await page.goto(origin+path);
  assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),`Mobile overflow: ${path}`);
 }
 await page.goto(origin);
 await page.screenshot({path:new URL('mobile.png',captures).pathname,fullPage:true});
 assert.deepEqual(errors,[]);
 console.log('Browser checks passed: navigation, search, firmware selection, legacy setup/flash links, mobile widths and page errors.');
} finally {await browser.close();server.close();}

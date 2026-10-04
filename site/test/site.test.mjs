import test from 'node:test';
import assert from 'node:assert/strict';
import { readdir, readFile, stat } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const root = fileURLToPath(new URL('../dist/', import.meta.url));
async function walk(dir) {
 const files=[];
 for(const entry of await readdir(dir,{withFileTypes:true})) {
  const name=path.join(dir,entry.name);
  if(entry.isDirectory())files.push(...await walk(name));else files.push(name);
 }
 return files;
}
test('generated guide has no broken local page, asset or fragment links',async()=>{
 const files=(await walk(root)).filter(f=>f.endsWith('.html'));
 assert.ok(files.length>=40,`Only ${files.length} pages were built`);
 for(const file of files){
  const html=await readFile(file,'utf8');
  for(const match of html.matchAll(/(?:href|src)="([^"]+)"/g)){
   const href=match[1].replaceAll('&amp;','&');
   if(/^(https?:|data:|mailto:)/.test(href))continue;
   const rel=path.relative(root,file).split(path.sep).join('/');
   const url=new URL(href,`https://cmwen.github.io/FNK0104B/${rel}`);
   assert.ok(url.pathname.startsWith('/FNK0104B/'),`${rel}: escapes Pages base: ${href}`);
   let target=path.join(root,decodeURIComponent(url.pathname.slice('/FNK0104B/'.length)));
   if(url.pathname.endsWith('/'))target=path.join(target,'index.html');
   assert.ok((await stat(target).catch(()=>null))?.isFile(),`${rel}: missing ${href}`);
   if(url.hash && target.endsWith('.html') && !href.endsWith('#flash') && !href.endsWith('#setup')) {
    const content=await readFile(target,'utf8');
    const id=decodeURIComponent(url.hash.slice(1));
    assert.ok(content.includes(`id="${id}"`),`${rel}: missing fragment ${href}`);
   }
  }
 }
});
test('installer retains security and compatibility controls',async()=>{
 const setup=await readFile(path.join(root,'setup.html'),'utf8');
 assert.ok(setup.includes('id="ble-pop"'));
 assert.ok(setup.includes('Erase device'));
 assert.ok(setup.includes('esp-web-tools@10.4.0'));
 assert.ok(setup.includes('id="monitor-settings-form"'));
 const worker=await readFile(path.join(root,'service-worker.js'),'utf8');
 assert.ok(worker.includes('/firmware/'));
 assert.ok(worker.includes('./setup.html'));
 const app=await readFile(path.join(root,'app.js'),'utf8');
 assert.ok(app.includes('URLSearchParams(location.search).get("firmware")'));
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const milestones=JSON.parse(await readFile(new URL('../src/data/journey.json',import.meta.url),'utf8'));

test('journal milestones stay newest first with Sydney dates and evidence links',async()=>{
 let previous=Infinity;
 const date=new Intl.DateTimeFormat('en-CA',{timeZone:'Australia/Sydney',year:'numeric',month:'2-digit',day:'2-digit'});
 for(const entry of milestones){
  const time=Date.parse(entry.timestamp);
  assert.ok(Number.isFinite(time),entry.title);
  assert.ok(time<=previous,`${entry.title} is out of order`);previous=time;
  assert.equal(date.format(new Date(time)),entry.timestamp.slice(0,10));
  assert.match(entry.sha,/^[0-9a-f]{7}$/);
  assert.ok(entry.paragraphs.length>0);
  for(const {path} of entry.links)await readFile(new URL(`../../${path}`,import.meta.url));
 }
 const source=await readFile(new URL('../src/pages/journey.astro',import.meta.url),'utf8');
 assert.ok(source.includes('milestones.map('));
 assert.ok(source.includes('<time datetime={timestamp}>'));
});

test('photo integration follows the October 10 avatar option and October 11 status change',()=>{
 const photos=milestones.findIndex(e=>e.sha==='ea00021');
 const status=milestones.findIndex(e=>e.sha==='f0c7883');
 const avatars=milestones.findIndex(e=>e.sha==='c1accac');
 assert.ok(photos>=0 && photos<status && status<avatars);
 assert.equal(milestones[photos].timestamp,'2026-10-11T23:00:14+11:00');
 assert.equal(milestones[status].timestamp.slice(0,10),'2026-10-11');
 assert.equal(milestones[avatars].timestamp.slice(0,10),'2026-10-10');
});

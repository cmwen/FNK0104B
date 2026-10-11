const {test} = require('node:test');
const assert = require('node:assert/strict');

test('browser prepares a bounded RGB565 photo with matching checksums', async () => {
  const {encodeFrame, crc32, photoFilename} = await import('../photo-client.js');
  assert.equal(crc32(new TextEncoder().encode('123456789')), 0xcbf43926);
  const rgba = new Uint8Array(320 * 196 * 4);
  rgba.set([255,0,0,255, 0,255,0,255, 0,0,255,255, 255,0,0,128]);
  const frame = encodeFrame(rgba), view = new DataView(frame.buffer);
  assert.equal(frame.length, 125456);
  assert.equal(new TextDecoder().decode(frame.subarray(0,4)), 'FPF1');
  assert.equal(view.getUint16(4,true),320); assert.equal(view.getUint16(6,true),196);
  assert.equal(view.getUint16(16,true),0xf800);
  assert.equal(view.getUint16(18,true),0x07e0);
  assert.equal(view.getUint16(20,true),0x001f);
  assert.equal(view.getUint16(22,true),0x8000);
  assert.equal(view.getUint32(12,true),crc32(frame.subarray(16)));
  assert.match(photoFilename('../../long dangerous name.jpeg'), /^[a-zA-Z0-9_-]{1,20}\.fpf$/);
  assert.throws(() => encodeFrame(new Uint8Array(10)), /dimensions/);
});

test('BLE requests wait for matching acknowledgement and reject board failures', async () => {
  const {photoRequest} = await import('../photo-client.js');
  let reads = 0, writes = 0;
  const c = {
    async writeValueWithResponse(packet) { writes++; assert.ok(packet[0] === 2 || packet[0] === 3); },
    async readValue() {
      const bytes = new TextEncoder().encode(JSON.stringify({seq: ++reads === 1 ? 3 : 7, state:'receiving',offset:180}));
      return new DataView(bytes.buffer);
    },
  };
  const result = await photoRequest(c,2,7,new Uint8Array(4),async () => {});
  assert.equal(result.offset,180); assert.equal(reads,2); assert.equal(writes,1);
  c.readValue = async () => new DataView(new TextEncoder().encode(JSON.stringify({seq:7,state:'checksum_or_size_failed'})).buffer);
  await assert.rejects(() => photoRequest(c,3,7,new Uint8Array(),async () => {}), /checksum or size failed/);
});

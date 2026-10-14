const { test } = require('node:test');
const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { runInNewContext } = require('node:vm');
const source = readFileSync(`${__dirname}/../photo-client.js`, 'utf8').replace(/^export /gm, '') + '\n' +
  readFileSync(`${__dirname}/../ble-client.js`, 'utf8').replace(/^import .*;\n/gm, '');

function setup({ rejectHandshake = false, settingsPacket = [2, 50, 30, 0, 3], ota = false, photos = false, failPhotoChunk = false } = {}) {
  let packet = Uint8Array.from(settingsPacket);
  const timers = [];
  const otaWrites = [];
  let update = {state: 'idle', current: '0.7.0', message: 'Check for update'};
  const otaCharacteristic = {
    async readValue() { const bytes=new TextEncoder().encode(JSON.stringify(update)); return new DataView(bytes.buffer); },
    async writeValue(bytes) { otaWrites.push(new TextDecoder().decode(bytes)); update={...update,state:'checking'}; },
  };
  const writes = [];
  let photoSettings = Uint8Array.from([1, 0, 60, 0, 10, 0]);
  let photoReply = {seq: 0, state: 'ready', offset: 0, mounted: true, files: []};
  const photoWrites = [];
  const photoConfig = {
    async readValue() { return new DataView(photoSettings.buffer); },
    async writeValue(value) { photoSettings = Uint8Array.from(value); },
  };
  const photoTransfer = {
    async readValue() { const value = new TextEncoder().encode(JSON.stringify(photoReply)); return new DataView(value.buffer); },
    async writeValue(value) {
      const bytes = Uint8Array.from(value); photoWrites.push(bytes);
      const sequence = bytes[1] | bytes[2] << 8;
      let state = 'ready', offset = photoReply.offset, files = photoReply.files;
      if (bytes[0] === 1) { state = 'receiving'; offset = 0; }
      if (bytes[0] === 2) { state = failPhotoChunk ? 'write_failed' : 'receiving'; if (!failPhotoChunk) offset += bytes.length - 7; }
      if (bytes[0] === 3) { state = 'complete'; files = ['test.fpf']; }
      if (bytes[0] === 5) { state = 'deleted'; files = []; }
      if (bytes[0] === 6) { state = 'cancelled'; offset = 0; }
      photoReply = {seq: sequence, state, offset, mounted: true, files};
    },
  };
  const characteristic = {
    properties: { read: true, write: true },
    async readValue() { return new DataView(packet.buffer); },
    async writeValue(bytes) { writes.push(Array.from(bytes)); packet = Uint8Array.from(bytes); },
  };
  const elements = new Map();
  function element(id) {
    if (!elements.has(id)) elements.set(id, {
      value: '', disabled: false, hidden: true, textContent: '',
      listeners: {}, classList: { add() {}, toggle() {} }, focus() {},
      replaceChildren() {}, append() {},
      getContext() { return { fillRect() {}, drawImage() {}, getImageData() {
        const data = new Uint8ClampedArray(320 * 196 * 4);
        for (let i = 0; i < data.length; i += 4) { data[i] = 255; data[i + 3] = 255; }
        return {data};
      } }; },
      addEventListener(name, fn) { this.listeners[name] = fn; },
    });
    return elements.get(id);
  }
  let client;
  class ESPProvisioner {
    constructor() {
      client = this;
      this.sent = 0;
      this.isConnected = false;
      this.device = { addEventListener: (_, fn) => { this.onDisconnect = fn; },
        gatt: { disconnect: () => this.disconnect() } };
    }
    async connect() { this.isConnected = true; }
    async establishSession() {
      assert.equal(element('#ble-password').disabled, true);
      if (rejectHandshake) throw Error('Incorrect proof of possession');
    }
    async disconnect() { this.isConnected = false; this.onDisconnect?.(); }
    async sendCredentials() { this.sent++; }
  }
  runInNewContext(source, {
    ESPProvisioner, Security1: class {}, TextEncoder, TextDecoder, Error,
    createImageBitmap: async () => ({width: 800, height: 600, close() {}}),
    setTimeout: fn => { timers.push(fn); return timers.length; }, clearTimeout() {},
    document: { querySelector: element, createElement: () => ({}) },
    window: { isSecureContext: true, crypto: { subtle: {} } },
    navigator: { bluetooth: { async requestDevice() { return {
      addEventListener() {},
      gatt: { disconnect() {}, async connect() { return {
        async getPrimaryService() { return { async getCharacteristic(uuid) {
          if (uuid.includes('0007') || uuid.includes('0008')) {
            if (photos) return uuid.includes('0007') ? photoConfig : photoTransfer;
            const error = new Error('Characteristic not found'); error.name = 'NotFoundError'; throw error;
          }
          if (uuid.includes('0005')) {
            if(ota) return otaCharacteristic;
            const error=new Error('Characteristic not found'); error.name='NotFoundError'; throw error;
          }
          if ((settingsPacket[0] < 6 && uuid.includes('000a')) ||
              (settingsPacket[0] < 5 && uuid.includes('0009')) ||
              (settingsPacket[0] < 4 && uuid.includes('0006')) ||
              (settingsPacket[0] < 3 && uuid.includes('0004')) ||
              (settingsPacket[0] === 1 && uuid.includes('0003'))) {
            const error = new Error('Characteristic not found'); error.name = 'NotFoundError'; throw error;
          }
          return characteristic;
        } }; },
      }; } },
    }; } } },
  });
  element('#ble-pop').value = '012345ABCDEF';
  return { element, writes, otaWrites, photoWrites,
    setUpdate: value => { update={...update,...value}; },
    poll: async () => { const fn=timers.shift(); if(fn) { fn(); await new Promise(resolve => setImmediate(resolve)); } },
    checkOta: () => element('#monitor-ota-check').listeners.click(),
    installOta: () => element('#monitor-ota-install').listeners.click(),
    connectMonitor: () => element("#monitor-connect").listeners.click(),
    saveMonitor: () => element("#monitor-settings-form").listeners.submit({ preventDefault() {} }),
    client: () => client,
    connect: () => element('#ble-connect').listeners.click(),
    submit: () => element('#ble-wifi-form').listeners.submit({ preventDefault() {} }) };
}

test('wrong proof never enables credential submission', async () => {
  const app = setup({ rejectHandshake: true });
  await app.connect();
  assert.equal(app.element('#ble-password').disabled, true);
  await app.submit();
  assert.equal(app.client().sent, 0);
});

test('idle-photo preferences round trip and require a delay before screen-off', async () => {
  const app = setup({photos:true, settingsPacket:[4,50,30,0,3,0,0]});
  await app.connectMonitor();
  assert.equal(app.element('#monitor-idle-mode').disabled, false);
  app.element('#monitor-idle-mode').value = '2';
  app.element('#monitor-photo-delay').value = '20';
  app.element('#monitor-photo-interval').value = '15';
  await app.saveMonitor();
  assert.equal(app.element('#monitor-idle-mode').value, '2');
  app.element('#monitor-photo-delay').value = '1800';
  await app.saveMonitor();
  assert.match(app.element('#monitor-status').textContent, /shorter than the screen-off/);
});

test('photo uploads prepare pixels, verify every offset, then commit', async () => {
  const app = setup({photos:true, settingsPacket:[4,50,30,0,3,0,0]});
  await app.connectMonitor();
  app.element('#monitor-photo-file').files = [{name:'test.jpg',size:100}];
  await app.element('#monitor-photo-file').listeners.change();
  await app.element('#monitor-photo-upload').listeners.click();
  assert.match(app.element('#monitor-photo-status').textContent, /saved and verified/);
  assert.equal(app.photoWrites.at(-1)[0],3);
  const chunks = app.photoWrites.filter(packet => packet[0] === 2);
  assert.equal(chunks.reduce((total,packet) => total + packet.length - 7,0),125456);
  assert.ok(chunks.every(packet => packet.length <= 187));
});

test('failed photo chunk cancels without committing a partial image', async () => {
  const app = setup({photos:true,failPhotoChunk:true,settingsPacket:[4,50,30,0,3,0,0]});
  await app.connectMonitor();
  app.element('#monitor-photo-file').files = [{name:'test.png',size:100}];
  await app.element('#monitor-photo-file').listeners.change();
  await app.element('#monitor-photo-upload').listeners.click();
  assert.match(app.element('#monitor-photo-status').textContent, /not saved/);
  assert.ok(app.photoWrites.some(packet => packet[0] === 6));
  assert.ok(!app.photoWrites.some(packet => packet[0] === 3));
});

test('successful setup clears password and cannot submit again', async () => {
  const app = setup();
  await app.connect();
  assert.equal(app.element('#ble-password').disabled, false);
  app.element('#ble-ssid').value = 'test-network';
  app.element('#ble-password').value = 'test-password';
  await app.submit();
  assert.equal(app.client().sent, 1);
  assert.equal(app.element('#ble-password').value, '');
  assert.equal(app.element('#ble-pop').value, '');
  assert.equal(app.element('#ble-send').disabled, true);
  await app.submit();
  assert.equal(app.client().sent, 1);
});

test('disconnect clears password and closes credential controls', async () => {
  const app = setup();
  await app.connect();
  app.element('#ble-password').value = 'test-password';
  await app.client().disconnect();
  assert.equal(app.element('#ble-password').value, '');
  assert.equal(app.element('#ble-password').disabled, true);
  await app.submit();
  assert.equal(app.client().sent, 0);
});


test('new firmware reads compact layout and saves six slots with readback', async () => {
  const app = setup();
  await app.connectMonitor();
  assert.equal(app.element('#monitor-slots').value, '3');
  assert.equal(app.element('#monitor-slots').disabled, false);
  app.element('#monitor-slots').value = '6';
  await app.saveMonitor();
  assert.deepEqual(app.writes, [[2, 50, 30, 0, 6]]);
  assert.equal(app.element('#monitor-slots').value, '6');
  assert.match(app.element('#monitor-status').textContent, /read back/);
});

test('old firmware keeps four-byte settings and disables layout selection', async () => {
  const app = setup({ settingsPacket: [1, 25, 5, 0] });
  await app.connectMonitor();
  assert.equal(app.element('#monitor-slots').disabled, true);
  assert.equal(app.element('#monitor-voice').disabled, true);
  await app.saveMonitor();
  assert.deepEqual(app.writes, [[1, 25, 5, 0]]);
});

test('older settings explain why microphone selection is disabled', async () => {
  const app = setup({ settingsPacket: [2, 50, 30, 0, 3] });
  await app.connectMonitor();
  assert.equal(app.element('#monitor-volume').disabled, false);
  assert.equal(app.element('#monitor-slots').disabled, false);
  assert.equal(app.element('#monitor-voice').disabled, true);
  assert.match(app.element('#monitor-voice-help').textContent, /older Bluetooth settings/);
  assert.match(app.element('#monitor-voice-help').textContent, /cached services/);
  assert.match(app.element('#monitor-status').textContent, /microphone controls need/);
});

test('v3 defaults to hold-to-talk and saves optional independent Voice control', async () => {
  const app = setup({ settingsPacket: [3, 50, 30, 0, 3, 0] });
  await app.connectMonitor();
  assert.equal(app.element('#monitor-voice').value, '0');
  assert.equal(app.element('#monitor-voice').disabled, false);
  app.element('#monitor-voice').value = '1';
  await app.saveMonitor();
  assert.deepEqual(app.writes, [[3, 50, 30, 0, 3, 1]]);
  assert.equal(app.element('#monitor-voice').value, '1');
  assert.match(app.element('#monitor-status').textContent, /Desktop key mappings/);
});

test('invalid voice packets and unsupported saved modes cannot change settings', async () => {
  const invalid = setup({ settingsPacket: [3, 50, 30, 0, 3, 2] });
  await invalid.connectMonitor();
  assert.equal(invalid.element('#monitor-save').disabled, true);
  assert.match(invalid.element('#monitor-status').textContent, /invalid voice/);
  const app = setup({ settingsPacket: [3, 50, 30, 0, 3, 0] });
  await app.connectMonitor();
  app.element('#monitor-voice').value = '9';
  await app.saveMonitor();
  assert.deepEqual(app.writes, []);
});

test('unsupported layout prevents settings controls from opening', async () => {
  const app = setup({ settingsPacket: [2, 50, 30, 0, 4] });
  await app.connectMonitor();
  assert.equal(app.element('#monitor-save').disabled, true);
  assert.match(app.element('#monitor-status').textContent, /invalid Micro layout/);
});

test('OTA needs newer firmware and an explicit compatible update before installation', async () => {
  const old = setup(); await old.connectMonitor();
  assert.equal(old.element('#monitor-ota-check').disabled, true);
  assert.match(old.element('#monitor-ota-status').textContent, /0.7.0.*USB/);
  const app = setup({ota:true,settingsPacket:[3,50,30,0,3,0]}); await app.connectMonitor();
  assert.equal(app.element('#monitor-ota-check').disabled, false);
  assert.equal(app.element('#monitor-ota-install').disabled, true);
  await app.installOta(); assert.deepEqual(app.otaWrites, []);
  await app.checkOta(); assert.deepEqual(app.otaWrites, ['check']);
  assert.equal(app.element('#monitor-save').disabled, true);
  app.setUpdate({state:'available',latest:'0.7.1',message:'Choose Install'}); await app.poll();
  assert.equal(app.element('#monitor-ota-install').disabled, false);
  await app.installOta(); assert.deepEqual(app.otaWrites,['check','install']);
  app.setUpdate({state:'error',message:'Current firmware kept'}); await app.poll();
  assert.equal(app.element('#monitor-ota-install').disabled,true);
  assert.equal(app.element('#monitor-save').disabled,false);
  assert.match(app.element('#monitor-ota-status').textContent,/Current firmware kept/);
});
test('incompatible speech models never enable the OTA install button', async () => {
  const app=setup({ota:true}); await app.connectMonitor(); await app.checkOta();
  app.setUpdate({state:'usb_required',message:'Speech models changed; USB required'}); await app.poll();
  assert.equal(app.element('#monitor-ota-install').disabled,true);
  await app.installOta(); assert.deepEqual(app.otaWrites,['check']);
});

 test('avatar settings round trip without changing voice or layout', async () => {
  const app = setup({settingsPacket: [4, 50, 30, 0, 3, 1, 0]});
  await app.connectMonitor();
  assert.equal(app.element('#monitor-appearance').disabled, false);
  app.element('#monitor-appearance').value = '1';
  await app.saveMonitor();
  assert.deepEqual(app.writes[0], [4, 50, 30, 0, 3, 1, 1]);
  assert.equal(app.element('#monitor-appearance').value, '1');
});
 test('older firmware keeps appearance unavailable', async () => {
  const app = setup({settingsPacket: [3, 50, 30, 0, 3, 0]});
  await app.connectMonitor();
  assert.equal(app.element('#monitor-appearance').disabled, true);
});

test('six-slot layout and avatar appearance can change independently', async () => {
  const app = setup({settingsPacket: [4, 50, 30, 0, 3, 1, 1]});
  await app.connectMonitor();
  app.element('#monitor-slots').value = '6';
  await app.saveMonitor();
  assert.deepEqual(app.writes[0], [4, 50, 30, 0, 6, 1, 1]);
  assert.equal(app.element('#monitor-appearance').value, '1');
  app.element('#monitor-appearance').value = '0';
  await app.saveMonitor();
  assert.deepEqual(app.writes[1], [4, 50, 30, 0, 6, 1, 0]);
  assert.equal(app.element('#monitor-slots').value, '6');
  app.element('#monitor-appearance').value = '1';
  await app.saveMonitor();
  assert.deepEqual(app.writes[2], [4, 50, 30, 0, 6, 1, 1]);
});


test('status LED preferences load, persist, and reject invalid timeouts', async () => {
  const app = setup({settingsPacket: [5, 50, 30, 0, 3, 0, 1, 1, 60, 0]});
  await app.connectMonitor();
  assert.equal(app.element('#monitor-led').disabled, false);
  assert.equal(app.element('#monitor-led').value, '1');
  assert.equal(app.element('#monitor-led-seconds').value, '60');
  app.element('#monitor-led').value = '0';
  app.element('#monitor-led-seconds').value = '120';
  await app.saveMonitor();
  assert.deepEqual(app.writes.at(-1), [5, 50, 30, 0, 3, 0, 1, 0, 120, 0]);
  app.element('#monitor-led-seconds').value = '0';
  await app.saveMonitor();
  assert.equal(app.writes.length, 1);
});


test('per-status LED patterns persist with brightness and speed', async () => {
  const app = setup({settingsPacket: [6,50,30,0,3,0,1,1,60,0,2,0,1,6,208,7]});
  await app.connectMonitor();
  assert.equal(app.element('#monitor-led-working-pattern').disabled, false);
  assert.equal(app.element('#monitor-led-working-pattern').value, '2');
  app.element('#monitor-led-complete-pattern').value = '3';
  app.element('#monitor-led-brightness').value = '20';
  app.element('#monitor-led-period').value = '4000';
  await app.saveMonitor();
  assert.deepEqual(app.writes.at(-1), [6,50,30,0,3,0,1,1,60,0,2,3,1,20,160,15]);
  app.element('#monitor-led-attention-pattern').value = '4';
  await app.saveMonitor();
  assert.equal(app.writes.length, 1);
});

test('old LED clients keep animation controls unavailable', async () => {
  const app = setup({settingsPacket: [5,50,30,0,3,0,0,1,60,0]});
  await app.connectMonitor();
  assert.equal(app.element('#monitor-led-working-pattern').disabled, true);
  assert.equal(app.element('#monitor-led-brightness').disabled, true);
  await app.saveMonitor();
  assert.equal(app.writes[0].length, 10);
});

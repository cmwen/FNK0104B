const { test } = require('node:test');
const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { runInNewContext } = require('node:vm');
const source = readFileSync(`${__dirname}/../ble-client.js`, 'utf8').replace(/^import .*;\n/, '');

function setup({ rejectHandshake = false, settingsPacket = [2, 50, 30, 0, 3], ota = false } = {}) {
  let packet = Uint8Array.from(settingsPacket);
  const timers = [];
  const otaWrites = [];
  let update = {state: 'idle', current: '0.7.0', message: 'Check for update'};
  const otaCharacteristic = {
    async readValue() { const bytes=new TextEncoder().encode(JSON.stringify(update)); return new DataView(bytes.buffer); },
    async writeValue(bytes) { otaWrites.push(new TextDecoder().decode(bytes)); update={...update,state:'checking'}; },
  };
  const writes = [];
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
    setTimeout: fn => { timers.push(fn); return timers.length; }, clearTimeout() {},
    document: { querySelector: element },
    window: { isSecureContext: true, crypto: { subtle: {} } },
    navigator: { bluetooth: { async requestDevice() { return {
      addEventListener() {},
      gatt: { disconnect() {}, async connect() { return {
        async getPrimaryService() { return { async getCharacteristic(uuid) {
          if (uuid.includes('0005')) {
            if(ota) return otaCharacteristic;
            const error=new Error('Characteristic not found'); error.name='NotFoundError'; throw error;
          }
          if ((settingsPacket[0] < 3 && uuid.includes('0004')) ||
              (settingsPacket[0] === 1 && uuid.includes('0003'))) {
            const error = new Error('Characteristic not found'); error.name = 'NotFoundError'; throw error;
          }
          return characteristic;
        } }; },
      }; } },
    }; } } },
  });
  element('#ble-pop').value = '012345ABCDEF';
  return { element, writes, otaWrites,
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

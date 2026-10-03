const { test } = require('node:test');
const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { runInNewContext } = require('node:vm');
const source = readFileSync(`${__dirname}/../ble-client.js`, 'utf8').replace(/^import .*;\n/, '');

function setup({ rejectHandshake = false } = {}) {
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
    ESPProvisioner, Security1: class {}, TextEncoder, Error,
    document: { querySelector: element },
    window: { isSecureContext: true, crypto: { subtle: {} } },
    navigator: { bluetooth: {} },
  });
  element('#ble-pop').value = '012345ABCDEF';
  return { element, client: () => client,
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

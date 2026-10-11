export const PHOTO_WIDTH = 320, PHOTO_HEIGHT = 196;

export function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
  }
  return (crc ^ 0xffffffff) >>> 0;
}

export function encodeFrame(rgba) {
  if (rgba.length !== PHOTO_WIDTH * PHOTO_HEIGHT * 4) throw new Error("Invalid photo dimensions.");
  const output = new Uint8Array(16 + PHOTO_WIDTH * PHOTO_HEIGHT * 2);
  output.set([70, 80, 70, 49]);
  const view = new DataView(output.buffer);
  view.setUint16(4, PHOTO_WIDTH, true); view.setUint16(6, PHOTO_HEIGHT, true);
  view.setUint32(8, output.length - 16, true);
  for (let i = 0, p = 16; i < rgba.length; i += 4, p += 2) {
    const a = rgba[i + 3] / 255;
    const r = Math.round(rgba[i] * a), g = Math.round(rgba[i + 1] * a), b = Math.round(rgba[i + 2] * a);
    view.setUint16(p, ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3), true);
  }
  view.setUint32(12, crc32(output.subarray(16)), true);
  return output;
}

export function photoFilename(name) {
  const stem = name.replace(/\.[^.]+$/, "").replace(/[^a-zA-Z0-9_-]/g, "-").slice(0, 20) || "photo";
  return `${stem}.fpf`;
}

export async function preparePhoto(file, canvas) {
  if (file.size > 20 * 1024 * 1024) throw new Error("Choose an image smaller than 20 MB.");
  const bitmap = await createImageBitmap(file, {imageOrientation: "from-image"});
  try {
    canvas.width = PHOTO_WIDTH; canvas.height = PHOTO_HEIGHT;
    const ctx = canvas.getContext("2d", {willReadFrequently: true});
    if (!ctx) throw new Error("Photo preview is unavailable.");
    const scale = Math.max(PHOTO_WIDTH / bitmap.width, PHOTO_HEIGHT / bitmap.height);
    const width = bitmap.width * scale, height = bitmap.height * scale;
    ctx.fillStyle = "black"; ctx.fillRect(0, 0, PHOTO_WIDTH, PHOTO_HEIGHT);
    ctx.drawImage(bitmap, (PHOTO_WIDTH - width) / 2, (PHOTO_HEIGHT - height) / 2, width, height);
    return {name: photoFilename(file.name), bytes: encodeFrame(ctx.getImageData(0, 0, PHOTO_WIDTH, PHOTO_HEIGHT).data)};
  } finally { bitmap.close(); }
}

export function photoPacket(op, sequence, payload = new Uint8Array()) {
  const packet = new Uint8Array(3 + payload.length);
  packet[0] = op; new DataView(packet.buffer).setUint16(1, sequence, true); packet.set(payload, 3);
  return packet;
}

export async function photoRequest(characteristic, op, sequence, payload, sleep = ms => new Promise(resolve => setTimeout(resolve, ms))) {
  const packet = photoPacket(op, sequence, payload);
  // Firmware requests MTU 247; cap packets below the receiver's fixed mailbox.
  if (packet.length > 200) throw new Error("Photo packet exceeds the supported BLE limit.");
  if (characteristic.writeValueWithResponse) await characteristic.writeValueWithResponse(packet);
  else await characteristic.writeValue(packet);
  for (let tries = 0; tries < 300; tries++) {
    const value = await characteristic.readValue();
    const result = JSON.parse(new TextDecoder().decode(value));
    if (result.seq === sequence) {
      if (!["ready", "receiving", "complete", "deleted", "cancelled"].includes(result.state))
        throw new Error(`Board reported ${result.state.replaceAll("_", " ")}.`);
      return result;
    }
    await sleep(100);
  }
  throw new Error("Board did not finish the request. Finish voice or update activity, then reconnect.");
}

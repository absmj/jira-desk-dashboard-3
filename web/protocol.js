// BLE file-transfer framing (pure functions; see docs/ble-protocol.md).
(function (root) {
  'use strict';

  const UUID = {
    service: '4a5b0001-7d8e-4f10-9a2b-3c4d5e6f7081',
    rx:      '4a5b0002-7d8e-4f10-9a2b-3c4d5e6f7081',   // write: web -> device
    status:  '4a5b0003-7d8e-4f10-9a2b-3c4d5e6f7081',   // notify: device -> web
  };
  const FILE = { sprint: 1, alerts: 2 };
  const T = { BEGIN: 1, DATA: 2, END: 3, ABORT: 4 };
  const CHUNK = 18;          // 20-byte ATT payload (default MTU 23) - 2 header bytes
  const STATUS = { 0: 'OK', 1: 'uzunluq səhvdir', 2: 'CRC səhvdir', 3: 'JSON rədd edildi',
                   4: 'yaddaş səhvi', 5: 'ardıcıllıq pozulub', 6: 'fayl çox böyükdür' };

  let table = null;
  function crc32(bytes) {                      // IEEE 802.3, same as zlib.crc32
    if (!table) {
      table = new Uint32Array(256);
      for (let n = 0; n < 256; n++) {
        let c = n;
        for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
        table[n] = c >>> 0;
      }
    }
    let c = 0xffffffff;
    for (let i = 0; i < bytes.length; i++) c = table[(c ^ bytes[i]) & 0xff] ^ (c >>> 8);
    return (c ^ 0xffffffff) >>> 0;
  }

  // text -> list of Uint8Array frames: BEGIN, DATA..., END
  function buildFrames(fileId, text) {
    const bytes = new TextEncoder().encode(text);
    if (bytes.length > 0xffff) throw new Error('fayl çox böyükdür');
    const frames = [];
    let seq = 0;
    const begin = new Uint8Array(9);
    const dv = new DataView(begin.buffer);
    begin[0] = T.BEGIN; begin[1] = seq++; begin[2] = fileId;
    dv.setUint16(3, bytes.length, true);
    dv.setUint32(5, crc32(bytes), true);
    frames.push(begin);
    for (let i = 0; i < bytes.length; i += CHUNK) {
      const part = bytes.subarray(i, i + CHUNK);
      const f = new Uint8Array(2 + part.length);
      f[0] = T.DATA; f[1] = seq++ & 0xff; f.set(part, 2);
      frames.push(f);
    }
    frames.push(Uint8Array.of(T.END, seq++ & 0xff));
    return frames;
  }

  const api = { UUID, FILE, T, CHUNK, STATUS, crc32, buildFrames };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.DeskProtocol = api;
})(typeof self !== 'undefined' ? self : this);

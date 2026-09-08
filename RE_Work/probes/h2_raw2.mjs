// Raw: send preface, dump every server frame; then send one hand-built HEADERS request.
import tls from 'node:tls';

const host = 'updates.ghub.logitechg.com';
const s = tls.connect({ host, port: 443, servername: host, ALPNProtocols: ['h2'] }, () => {
  const magic = Buffer.from('PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n');
  const settings = Buffer.from([0, 0, 0, 4, 0, 0, 0, 0, 0]);
  s.write(Buffer.concat([magic, settings]));
  console.log('preface sent');
});

let buf = Buffer.alloc(0);
function frameName(t) {
  return { 0: 'DATA', 1: 'HEADERS', 2: 'PRIORITY', 3: 'RST_STREAM', 4: 'SETTINGS', 5: 'PUSH_PROMISE', 6: 'PING', 7: 'GOAWAY', 8: 'WINDOW_UPDATE', 9: 'CONTINUATION' }[t] ?? '???(' + t + ')';
}
function parse() {
  while (buf.length >= 9) {
    const len = (buf[0] << 16) | (buf[1] << 8) | buf[2];
    const type = buf[3];
    const flags = buf[4];
    const stream = buf.readUInt32BE(5) & 0x7fffffff;
    if (buf.length < 9 + len) break;
    console.log(`FRAME ${frameName(type)} stream=${stream} len=${len} flags=${flags}`);
    if (type === 4) {
      const p = buf.slice(9, 9 + len);
      for (let i = 0; i + 6 <= p.length; i += 6) {
        const id = p.readUInt16BE(i), val = p.readUInt32BE(i + 2);
        console.log(`   SETTINGS id=${id} val=${val}`);
      }
    }
    if (type === 1 && len > 0) console.log('   HEADERS payload hex:', buf.slice(9, 9 + Math.min(len, 48)).toString('hex'));
    buf = buf.slice(9 + len);
  }
}
s.on('data', (d) => { buf = Buffer.concat([buf, d]); parse(); });
s.on('error', (e) => console.log('err', e.message));

// After 2s, send a hand-built HEADERS request (no HPACK compression, literal headers).
setTimeout(() => {
  // build HEADERS frame: 5 pseudo + 2 regular headers, literal without indexing (0x00 prefix + name len + name + value len + value)
  const enc = (n, v) => {
    const nb = Buffer.from(n, 'latin1'), vb = Buffer.from(v, 'latin1');
    const b = Buffer.alloc(1 + 1 + nb.length + 1 + vb.length);
    b[0] = 0x00; // literal, no indexing, name follows
    b.writeUInt8(nb.length, 1); nb.copy(b, 2);
    b.writeUInt8(vb.length, 2 + nb.length); vb.copy(b, 3 + nb.length);
    return b;
  };
  const body = Buffer.concat([
    enc(':method', 'GET'), enc(':scheme', 'https'), enc(':authority', host),
    enc(':path', '/pipeline/v2/update/ghub10/win/public/update.json'),
    enc('user-agent', 'probe'), enc('accept', '*/*'),
  ]);
  // HEADERS frame: len=body.length, type=1, flags=0x05 (END_STREAM|END_HEADERS)
  const hdr = Buffer.alloc(9);
  hdr.writeUIntBE(body.length, 0, 3); hdr[3] = 1; hdr[4] = 0x05; hdr.writeUInt32BE(1, 5);
  s.write(Buffer.concat([hdr, body]));
  console.log('HEADERS sent, stream 1, bytes=', body.length);
}, 2000);
setTimeout(() => process.exit(0), 8000);

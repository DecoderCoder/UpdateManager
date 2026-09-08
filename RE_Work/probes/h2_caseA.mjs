// Case A: preface + HEADERS in ONE write (burst), no delay.
import tls from 'node:tls';

const host = 'updates.ghub.logitechg.com';
const s = tls.connect({ host, port: 443, servername: host, ALPNProtocols: ['h2'] }, () => {
  const magic = Buffer.from('PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n');
  const settings = Buffer.from([0, 0, 0, 4, 0, 0, 0, 0, 0]);
  const enc = (n, v) => {
    const nb = Buffer.from(n, 'latin1'), vb = Buffer.from(v, 'latin1');
    const b = Buffer.alloc(1 + 1 + nb.length + 1 + vb.length);
    b[0] = 0x00; b.writeUInt8(nb.length, 1); nb.copy(b, 2);
    b.writeUInt8(vb.length, 2 + nb.length); vb.copy(b, 3 + nb.length);
    return b;
  };
  const body = Buffer.concat([
    enc(':method', 'GET'), enc(':scheme', 'https'), enc(':authority', host),
    enc(':path', '/pipeline/v2/update/ghub10/win/public/update.json'),
    enc('user-agent', 'probe'), enc('accept', '*/*'),
  ]);
  const hdr = Buffer.alloc(9);
  hdr.writeUIntBE(body.length, 0, 3); hdr[3] = 1; hdr[4] = 0x05; hdr.writeUInt32BE(1, 5);
  s.write(Buffer.concat([magic, settings, hdr, body]));
  console.log('burst sent (preface + HEADERS in one write)');
});
let frames = 0;
s.on('data', (d) => { frames++; if (frames < 8) console.log('got', d.length, 'bytes'); });
s.on('error', (e) => console.log('err', e.message));
setTimeout(() => { console.log('total data events:', frames); process.exit(0); }, 6000);

// Raw h2 preface test: TLS (ALPN h2) + send client preface, expect server SETTINGS.
import tls from 'node:tls';

const host = 'updates.ghub.logitechg.com';
const s = tls.connect(
  { host, port: 443, servername: host, ALPNProtocols: ['h2', 'http/1.1'] },
  () => {
    const alpn = s.getPeerCertificate && 'n/a';
    console.log('handshake done');
    // client preface: magic + empty SETTINGS frame
    const magic = Buffer.from('PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n');
    const settings = Buffer.from([0, 0, 0, 0, 4, 0, 0, 0, 0]); // len=0 type=0x4(SETTINGS)
    s.write(Buffer.concat([magic, settings]), (e) => {
      if (e) console.log('write err', e.message);
      console.log('preface sent, waiting for server SETTINGS...');
    });
  },
);
let got = 0;
s.on('data', (d) => {
  got += d.length;
  console.log('server sent', d.length, 'bytes:', d.slice(0, 16).toString('hex'));
  if (d[4] === 0x4 && got < 64) console.log('-> that is an h2 SETTINGS frame: server is real h2');
  if (got > 64) process.exit(0);
});
s.on('error', (e) => console.log('err', e.message));
setTimeout(() => { console.log('no server data after preface -> h2 is NOT really served'); process.exit(3); }, 10000);

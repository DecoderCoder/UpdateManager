// Minimal h2 connect+request debug. Usage: node h2_min.mjs <authority> <path>
import http2 from 'node:http2';

const authority = process.argv[2] || 'updates.ghub.logitechg.com';
const path = process.argv[3] || '/pipeline/v2/update/ghub10/win/public/update.json';

console.log('connecting', authority);
const conn = http2.connect('https://' + authority);
conn.on('open', () => {
  console.log('OPEN');
  const rs = conn.state.remoteSettings || {};
  console.log('remoteSettings:', JSON.stringify(rs));
  const req = conn.request({ ':path': path, ':method': 'GET', 'user-agent': 'probe' });
  req.on('response', (h) => console.log('status', h[':status'], JSON.stringify(h)));
  let b = 0;
  req.on('data', (d) => { b += d.length; });
  req.on('end', () => { console.log('END bytes=', b); conn.close(); process.exit(0); });
  req.on('error', (e) => console.log('req error', e.message));
});
conn.on('error', (e) => console.log('conn error', e.message, e.code));
conn.on('close', () => console.log('close'));
setTimeout(() => { console.log('TIMEOUT - still waiting'); process.exit(2); }, 20000);

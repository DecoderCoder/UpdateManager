// Single-connection HTTP/2 concurrency probe for the G HUB update endpoint.
// Usage: node h2_probe.mjs [inflight] [total]
import http2 from 'node:http2';

const N = +(process.argv[2] || 1024); // desired in-flight streams
const TOTAL = +(process.argv[3] || 20000);

const conn = http2.connect('https://updates.ghub.logitechg.com');
let inflight = 0, issued = 0, finished = 0, errors = 0, ok200 = 0, other = 0;
let maxConcurrent = 0;
const t0 = Date.now();

function report() {
  const s = (Date.now() - t0) / 1000;
  console.log(`server_max_concurrent_streams=${maxConcurrent} finished=${finished} rate=${(finished / s).toFixed(0)}/s ok200=${ok200} other_status=${other} errors=${errors}`);
}

function next() {
  while (inflight < N && issued < TOTAL) {
    const i = issued++;
    inflight++;
    const req = conn.request({
      ':path': `/pipeline/v2/update/ghub10/win/zz${i}/update.json`,
      ':method': 'GET',
      'user-agent': 'LGHUB/2026.6.957899 (Windows NT 10.0; x64)',
    });
    let status = 0;
    req.on('response', (h) => { status = h[':status']; });
    req.on('end', () => {
      inflight--; finished++;
      if (status === 200) ok200++; else other++;
      next();
    });
    req.on('error', () => { inflight--; finished++; errors++; next(); });
  }
}

conn.on('open', () => {
  const rs = conn.state.remoteSettings || {};
  maxConcurrent = rs.MAX_CONCURRENT_STREAMS ?? rs.maxConcurrentStreams ?? -1;
  console.log(`open; server MAX_CONCURRENT_STREAMS=${maxConcurrent}`);
  next();
});
conn.on('error', (e) => { console.log('conn error:', e.message); report(); process.exit(1); });
conn.on('close', () => { report(); });
setTimeout(() => { report(); process.exit(0); }, 15000);

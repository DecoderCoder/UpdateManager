// diag_fetch.mjs - minimal connectivity diagnostic with detailed cause.
const urls = [
  'https://updates.ghub.logitechg.com/pipeline/v2/update/ghub10/win/public/update.json',
  'https://updates.ghub.logitechg.com/pipeline/v2/access/323e77f5-68c2-43d8-8457-818bbd663938/content.json',
];
for (const url of urls) {
  const t0 = Date.now();
  try {
    const res = await fetch(url, { headers: { 'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)' } });
    const buf = Buffer.from(await res.arrayBuffer());
    console.log(JSON.stringify({ url, status: res.status, bytes: buf.length, ms: Date.now() - t0, head: buf.slice(0, 80).toString('utf8') }));
  } catch (e) {
    console.log(JSON.stringify({ url, err: String(e), cause: e.cause ? (e.cause.code || String(e.cause)) : null, ms: Date.now() - t0 }));
  }
  await new Promise(r => setTimeout(r, 800));
}

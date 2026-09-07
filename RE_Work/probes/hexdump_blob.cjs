// hexdump_blob.cjs — hexdump the 396-B SecureStorage container blob
const fs = require('fs');
const path = require('path');
const here = __dirname;
const hex = fs.readFileSync(path.join(here, 'h6_blob_full.txt'), 'utf8').replace(/\s/g, '');
const b = Buffer.from(hex, 'hex');
console.log('total bytes:', b.length);
for (let i = 0; i < b.length; i += 16) {
  const c = b.subarray(i, i + 16);
  let hx = '';
  for (let j = 0; j < c.length; j++) hx += c[j].toString(16).padStart(2, '0') + ' ';
  const asc = Array.from(c, (ch) => (ch >= 32 && ch < 127 ? String.fromCharCode(ch) : '.')).join('');
  console.log(i.toString(16).toUpperCase().padStart(3, '0') + ': ' + hx.padEnd(48) + ' ' + asc);
}

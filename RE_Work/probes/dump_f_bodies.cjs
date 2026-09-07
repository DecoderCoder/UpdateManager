const fs = require('fs');
const dir = 'C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes/api_matrix_run3_2026-09-07/';
for (const id of ['F1_appversion_cur', 'F2_appversion_fut', 'F3_both_headers']) {
  const b = fs.readFileSync(dir + id + '.body', 'utf8');
  const ch = b.match(/"channel": "([^"]+)"/);
  const lm = b.match(/"lastModified": "([^"]+)"/);
  console.log(id, '->', ch && ch[1], lm && lm[1], b.length + 'B');
}

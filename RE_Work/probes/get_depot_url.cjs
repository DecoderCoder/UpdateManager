const fs = require('fs');
const t = fs.readFileSync('C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes/reprobe_channel_names_2026-09-07/public_details.body', 'utf8');
const i = t.indexOf('g560_dfu');
const j = t.indexOf('"url"', i);
console.log(JSON.stringify(t.substring(j, j + 130)));

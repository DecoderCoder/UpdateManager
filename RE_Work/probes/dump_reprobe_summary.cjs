const fs = require('fs');
const dir = 'C:/Users/Decode/source/repos/UpdateManager/RE_Work/probes/reprobe_2026_09_07/';
for (const f of ['resp_pipeline_v1_update_ghub12_win_public_update.json.json', 'resp_pipeline_v2_update_ghub12_win_public_update.json.json']) {
  const t = fs.readFileSync(dir + f, 'utf8');
  console.log('=== ' + f + ' (' + t.length + ' chars) ===');
  console.log(t.slice(0, 400));
  console.log('...');
}

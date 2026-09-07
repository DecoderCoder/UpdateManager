const fs = require('fs');
const lines = fs.readFileSync('C:/Users/Decode/source/repos/UpdateManager/RE/LOGITECH_API_REFERENCE.md', 'utf8').split('\n');
for (let i = 76; i < 82; i++) console.log(i + 1, JSON.stringify(lines[i]));

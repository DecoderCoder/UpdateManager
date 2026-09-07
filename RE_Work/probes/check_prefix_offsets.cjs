const { readFileSync } = require("node:fs");
const pub = readFileSync("RE_Work/probes/reprobe_channel_names_2026-09-07/public_details.body").toString("utf8");
for (const f of ["appId", "platform", "channel", "buildId", "version", "branch", "lastModified", "uuid", "depots"]) {
  const i = pub.indexOf('"' + f + '"');
  console.log(f.padEnd(13), "key at", i);
}
console.log("---");
for (const off of [170, 171, 172, 173, 174, 175, 186, 187, 188, 189, 190, 191, 192, 193, 194, 203, 204, 205, 206, 207, 210, 215]) {
  console.log(off, JSON.stringify(pub.substring(off - 12, off + 24)));
}

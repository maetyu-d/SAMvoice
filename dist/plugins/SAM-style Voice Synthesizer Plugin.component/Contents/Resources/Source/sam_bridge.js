const path = require('path');
const fs = require('fs');

const outPath = process.argv[2];
const speed = Math.max(1, Math.min(255, Number(process.argv[3] || 72) | 0));
const pitch = Math.max(0, Math.min(255, Number(process.argv[4] || 64) | 0));
const mouth = Math.max(0, Math.min(255, Number(process.argv[5] || 128) | 0));
const throat = Math.max(0, Math.min(255, Number(process.argv[6] || 128) | 0));
const singmode = (process.argv[7] || "0") === "1";
const phonetic = (process.argv[8] || "0") === "1";
const backend = (process.argv[9] || "classic").trim().toLowerCase();
const text = process.argv.slice(10).join(' ').trim();

if (!outPath || !text) {
  process.exit(0);
}

const oldLog = console.log;
const oldWarn = console.warn;
const oldError = console.error;
console.log = () => {};
console.warn = () => {};
console.error = () => {};

try {
  const lib = backend === "better" ? "better-samjs.common.js" : "samjs.common.js";
  const samPath = path.resolve(__dirname, '..', 'third_party', lib);
  const Sam = require(samPath);
  const sam = new Sam({ speed, pitch, mouth, throat, singmode, phonetic: false });
  let out = null;
  if (phonetic) {
    try {
      out = sam.buf8(text, true);
    } catch (_) {
      out = null;
    }
  }
  if (!out || !out.length) {
    out = sam.buf8(text, false);
  }
  if (!out || !out.length) {
    throw new Error("SAM produced no audio");
  }
  fs.writeFileSync(outPath, Buffer.from(out));
} catch (err) {
  try {
    fs.writeFileSync(outPath + ".err.txt", String(err && err.stack ? err.stack : err));
  } catch (_) {}
  process.exitCode = 1;
} finally {
  console.log = oldLog;
  console.warn = oldWarn;
  console.error = oldError;
}

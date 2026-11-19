// A static host like GitHub Pages: no COOP/COEP headers (pass --coop to send them, for comparison). Serves dist/,
// then public/ (models, images, coi-serviceworker.js), and counts the bytes sent per file: GET /__bytes answers and
// resets the count.   node serve.mjs <port> [--coop]
import { createReadStream, existsSync, statSync } from "node:fs";
import { createServer } from "node:http";
import { extname, join } from "node:path";

const [port = 8740, flag] = process.argv.slice(2);
const roots = ["dist", "public"].map((d) => join(import.meta.dirname, d));
const types = { ".html": "text/html", ".js": "text/javascript", ".mjs": "text/javascript", ".wasm": "application/wasm",
  ".json": "application/json", ".png": "image/png", ".jpg": "image/jpeg", ".pdf": "application/pdf", ".css": "text/css" };
let bytes = {};

createServer((req, res) => {
  const path = decodeURIComponent(new URL(req.url, "http://x").pathname);
  if (path === "/__bytes") {
    res.writeHead(200, { "content-type": "application/json" }).end(JSON.stringify(bytes));
    bytes = {};
    return;
  }
  const file = roots.map((r) => join(r, path.endsWith("/") ? `${path}index.html` : path)).find((f) => existsSync(f) && statSync(f).isFile());
  if (!file) return res.writeHead(404).end();
  const size = statSync(file).size;
  const headers = { "content-type": types[extname(file)] ?? "application/octet-stream", "content-length": size, "cache-control": "max-age=600" };
  if (flag === "--coop") Object.assign(headers, { "cross-origin-opener-policy": "same-origin", "cross-origin-embedder-policy": "require-corp" });
  res.writeHead(200, headers);
  bytes[path] = (bytes[path] ?? 0) + size;
  createReadStream(file).pipe(res);
}).listen(port, () => console.log(`http://localhost:${port}/ ${flag ?? "(no COOP/COEP, like GitHub Pages)"}`));

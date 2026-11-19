// Drive the page in a fresh Chrome (own profile, so the model cache starts empty) and record timings, downloaded
// bytes and the peak memory of the whole Chrome process tree.
//   node run.mjs <reader> <webgpu|wasm> <none|coi|headers> <port> img1 [img2 ...]
// The first image is read twice: cold (download + session start) and warm.
import { execSync } from "node:child_process";
import { mkdirSync, mkdtempSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import puppeteer from "puppeteer-core";

const [reader, prefer, hosting, port, ...images] = process.argv.slice(2);
const base = `http://localhost:${port}/`;
const url = base + (hosting === "coi" ? "?coi=1" : "");
const firefox = process.env.BROWSER === "firefox";  // BROWSER=firefox: the same run in Firefox (WebDriver BiDi)
const browser = await puppeteer.launch({
  ...(firefox ? { browser: "firefox", executablePath: "/Applications/Firefox.app/Contents/MacOS/firefox" } : {}),
  executablePath: firefox ? "/Applications/Firefox.app/Contents/MacOS/firefox" : "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
  headless: true,
  protocolTimeout: 3_600_000, // a WASM run of all pages takes minutes
  userDataDir: mkdtempSync(join(import.meta.dirname, "..", "tmp", "omr", "profile-")),
  args: firefox ? [] : ["--enable-unsafe-webgpu", "--enable-features=Vulkan", "--use-angle=metal", "--no-first-run"],
});
const root = browser.process().pid;
const treeRss = () => {  // MB of RSS over Chrome and all its descendants
  const rows = execSync("ps -A -o pid=,ppid=,rss=").toString().trim().split("\n").map((l) => l.trim().split(/\s+/).map(Number));
  const kids = new Set([root]);
  for (let grew = true; grew;) { grew = false; for (const [p, pp] of rows) if (kids.has(pp) && !kids.has(p)) { kids.add(p); grew = true; } }
  return rows.filter(([p]) => kids.has(p)).reduce((s, [, , r]) => s + r, 0) / 1024;
};

const page = await browser.newPage();
page.on("console", (m) => m.type() === "error" && console.error("page:", m.text().slice(0, 300)));
await page.goto(url, { waitUntil: "load" });
if (hosting === "coi") await page.waitForFunction(() => self.crossOriginIsolated, { timeout: 20000 }).catch(() => {});
await page.waitForFunction(() => window.omrRun, { timeout: 20000 });
await fetch(`${base}__bytes`);  // reset the server's count
const baseline = treeRss();
let peak = baseline;
const timer = setInterval(() => { peak = Math.max(peak, treeRss()); }, 200);
const res = await page.evaluate((o) => window.omrRun(o), { reader, prefer, images: [images[0], ...images] });
clearInterval(timer);
res.bytes = await (await fetch(`${base}__bytes`)).json();
res.downloadMB = +(Object.entries(res.bytes).filter(([p]) => p.startsWith("/models/")).reduce((s, [, b]) => s + b, 0) / 1e6).toFixed(1);
res.memory = { baselineMB: Math.round(baseline), peakMB: Math.round(peak), deltaMB: Math.round(peak - baseline) };
res.hosting = hosting;
res.browser = firefox ? "firefox" : "chrome";
await browser.close();

const dir = join(import.meta.dirname, "results", `${reader}-${prefer}-${hosting}${firefox ? "-firefox" : ""}`);
mkdirSync(dir, { recursive: true });
res.pages.forEach((p, i) => p.musicXml && writeFileSync(join(dir, `${i}-${p.image.split("/").pop()}.musicxml`), p.musicXml));
writeFileSync(join(dir, "run.json"), JSON.stringify({ ...res, pages: res.pages.map(({ musicXml, ...p }) => p) }, null, 1));
console.log(JSON.stringify({ reader, prefer, hosting, backend: res.backend, coi: res.crossOriginIsolated, initMs: res.initMs,
  pages: res.pages.map((p) => `${p.image.split("/").pop()} ${p.ms}ms${p.error ? ` ERR ${p.error.slice(0, 120)}` : ""}`), downloadMB: res.downloadMB, memory: res.memory }));

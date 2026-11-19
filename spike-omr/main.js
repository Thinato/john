// Throwaway harness for issue #74. window.omrRun({ reader, prefer, images }) reads each image (or PDF page 1) with
// one reader and answers timings and MusicXML; run.mjs drives it from puppeteer, the file picker drives it by hand.
import { createRecognizer } from "homr-web";
import * as pdfjs from "pdfjs-dist";
import pdfWorker from "pdfjs-dist/build/pdf.worker.min.mjs?url";

pdfjs.GlobalWorkerOptions.workerSrc = pdfWorker;
const log = (s) => { document.getElementById("log").textContent += `${s}\n`; console.log(s); };

/** RGBA pixels of an image URL or Blob; a PDF's first page is rendered at 3x (about 216 dpi), as #75 would. */
async function pixels(src) {
  const blob = src instanceof Blob ? src : await (await fetch(src)).blob();
  let canvas;
  if (blob.type === "application/pdf" || String(src).endsWith(".pdf")) {
    const page = await (await pdfjs.getDocument({ data: await blob.arrayBuffer() }).promise).getPage(1);
    const vp = page.getViewport({ scale: 3 });
    canvas = new OffscreenCanvas(vp.width, vp.height);
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#fff"; ctx.fillRect(0, 0, vp.width, vp.height);
    await page.render({ canvasContext: ctx, viewport: vp }).promise;
  } else {
    const bmp = await createImageBitmap(blob);
    canvas = new OffscreenCanvas(bmp.width, bmp.height);
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#fff"; ctx.fillRect(0, 0, bmp.width, bmp.height);
    ctx.drawImage(bmp, 0, 0);
  }
  return canvas.getContext("2d").getImageData(0, 0, canvas.width, canvas.height);
}

async function homrReader(prefer) {
  const r = await createRecognizer({ baseUrl: new URL("models/homr/", location.href), prefer });
  return {
    backend: `${r.backend} (${r.backendReason})`,
    async read(img) {
      const res = await r.recognizePage(img, { ocr: false }); // chord/title OCR off: John needs the notes only
      if (!res.ok) throw new Error(`${res.error}: ${(res.log || []).slice(-3).join(" | ")}`);
      return res.musicXml;
    },
  };
}

async function copistaReader(size, prefer) {
  const w = prefer === "wasm"
    ? new Worker(new URL("./copista-worker.js", import.meta.url), { type: "module", name: "wasm" })
    : new Worker(new URL("./copista-worker.js", import.meta.url), { type: "module" });
  await new Promise((ok) => w.addEventListener("message", (e) => e.data?.type === "ready" && ok(), { once: false }));
  let id = 0;
  return {
    backend: `copista-${size}: trunk ${prefer === "wasm" || !navigator.gpu ? "wasm" : "webgpu (falls back to wasm)"}, rest wasm; threads ${self.crossOriginIsolated ? "yes" : "no"}`,
    read(img) {
      const my = ++id;
      return new Promise((ok, fail) => {
        const on = (e) => {
          if (e.data.id !== my || e.data.type === "progress") return;
          w.removeEventListener("message", on);
          e.data.type === "result" ? ok(e.data.result.musicXml) : fail(new Error(e.data.message));
        };
        w.addEventListener("message", on);
        w.postMessage({ id: my, task: "recognize", size, image: { data: img.data, width: img.width, height: img.height }, page: 0, count: 1 });
      });
    },
  };
}

window.omrRun = async ({ reader, prefer = "webgpu", images }) => {
  const out = { reader, prefer, crossOriginIsolated: self.crossOriginIsolated, webgpu: !!navigator.gpu, pages: [] };
  let t = performance.now();
  const r = reader === "homr" ? await homrReader(prefer) : await copistaReader(reader.replace("copista-", ""), prefer);
  out.initMs = Math.round(performance.now() - t);
  out.backend = r.backend;
  for (const src of images) {
    const img = await pixels(src);
    t = performance.now();
    try {
      const xml = await r.read(img);
      out.pages.push({ image: String(src), w: img.width, h: img.height, ms: Math.round(performance.now() - t), musicXml: xml });
    } catch (e) {
      out.pages.push({ image: String(src), w: img.width, h: img.height, ms: Math.round(performance.now() - t), error: String(e.message || e) });
    }
    log(`${reader} ${src}: ${out.pages.at(-1).ms} ms ${out.pages.at(-1).error ?? "ok"}`);
  }
  return out;
};

document.getElementById("file").onchange = async (e) => {
  const reader = document.getElementById("reader").value, prefer = document.getElementById("prefer").value;
  const res = await window.omrRun({ reader, prefer, images: [e.target.files[0]] });
  log(res.backend);
  log(res.pages[0].musicXml ?? res.pages[0].error);
};
log(`ready; crossOriginIsolated=${self.crossOriginIsolated}, WebGPU=${!!navigator.gpu}`);

// John's sheet-music reader (#75): a module Web Worker started by omr.js. It reads pages with copisteria's JavaScript
// port (copisteria/, vendored from brunombo/OpticalMusicRecognition, see NOTICE.txt) and the copista-2m networks on
// ONNX Runtime Web: the detector trunk on WebGPU where it works, everything else on WASM. GitHub Pages sends no
// COOP/COEP headers, so the page isn't cross-origin isolated and ONNX Runtime runs WASM on one thread (it checks this
// itself; coi-serviceworker was tried and dropped, see kb/notes/web-coi-serviceworker-dropped.md). Messages, one at a time:
//   in:  { type: "page", image: { data, width, height }, title }  (RGBA; several, in page order; the title on the first)
//                                                                ->  out: { type: "page", ms, upscaled, ... }
//        { type: "finish" }                                                                ->  out: { type: "result", musicXml }
//   out meanwhile: { type: "progress", stage: "download", loaded, total } (bytes still to fetch), { stage: "start" },
//        { stage: "read", ratio } (of the current page); { type: "error", code, message } ends the run.
// The models and the WASM runtime are listed with their sizes in manifest.json (written by CMake) and kept in the
// Cache API under "john-omr-<version>", so only the first import downloads them.
import * as ort from "./ort/ort.min.mjs";
import { rgbaToGray } from "./copisteria/image.js";
import { recognizePage } from "./copisteria/index.js";
import { createModels } from "./copisteria/runtime.js";
import { findStaves } from "./copisteria/staves.js";
import { writeTree, xmlDocument } from "./copisteria/write.js";
import { joinTrees } from "./join.js";

// #74: copista-2m read an 865 px wide tune-book scan (staff space about 6.5 px) at 75 % and the same page upscaled 2x
// at 97 %, so pages whose staff lines are closer than this are upscaled 2x first: with the canvas's smoothing (the port's
// resizeGrayLanczos gave 88 % on that scan), and only while the page stays within the 4096 px copisteria reads at most
// (findStaves misjudged an already enlarged copy of the scan at half its spacing; 4x read worse).
const MIN_STAFF_SPACE = 10;
const MAX_SIDE = 4096;
const at = (file) => new URL(file, import.meta.url).href;
const post = (message) => self.postMessage(message);

/** fetch(), failing with code "download" (offline, HTTP error) so John shows the download message. */
async function download(file, init) {
	const response = await fetch(at(file), init).catch((e) => {
		throw Object.assign(new Error(`${file}: ${e.message}`), { code: "download" });
	});
	if (!response.ok) throw Object.assign(new Error(`${file}: HTTP ${response.status}`), { code: "download" });
	return response;
}

/** Fetch a file the reader needs, from the cache when it is there, reporting bytes as they arrive. */
async function fetchFile(cache, file, size, onBytes) {
	const cached = await cache?.match(at(file));
	const response = cached ?? (await download(file));
	const bytes = new Uint8Array(size);
	let n = 0;
	for (const reader = response.body.getReader(); ; ) {
		const { done, value } = await reader.read();
		if (done) break;
		if (n + value.length > size) throw Object.assign(new Error(`${file}: larger than expected`), { code: "download" });
		bytes.set(value, n);
		n += value.length;
		if (!cached) onBytes(value.length);
	}
	if (n !== size) throw Object.assign(new Error(`${file}: ${n} of ${size} bytes`), { code: "download" });
	if (!cached) await cache?.put(at(file), new Response(bytes)).catch(() => {}); // a full quota only costs a new download
	return bytes;
}

const withTimeout = (promise, ms) =>
	Promise.race([promise, new Promise((_, fail) => setTimeout(() => fail(new Error(`timed out after ${ms} ms`)), ms))]);

let backend = "wasm";
// errors only: otherwise every WebGPU session logs a console.error warning about shape ops kept on the CPU
const QUIET = { logSeverityLevel: 3 };

/** The trunk's session: WebGPU when it starts (a stalled driver times out), moving to WASM if a run fails there. */
async function trunkSession(bytes) {
	let session = null;
	if (navigator.gpu) {
		try {
			session = await withTimeout(ort.InferenceSession.create(bytes, { ...QUIET, executionProviders: ["webgpu", "wasm"] }), 20000);
			backend = "webgpu";
		} catch (e) {
			console.warn("John reader: WebGPU unusable, using WASM:", e);
		}
	}
	session ??= await ort.InferenceSession.create(bytes, { ...QUIET, executionProviders: ["wasm"] });
	return {
		async run(feeds) {
			try {
				return await session.run(feeds);
			} catch (e) {
				if (backend !== "webgpu") throw e;
				console.warn("John reader: WebGPU run failed, using WASM:", e); // e.g. a page over the GPU's buffer limits
				backend = "wasm";
				await session.release().catch(() => {});
				session = await ort.InferenceSession.create(bytes, { ...QUIET, executionProviders: ["wasm"] });
				return session.run(feeds);
			}
		},
	};
}

let reader = null; // { models, meta }, once loaded

async function load() {
	const manifest = await (await download("manifest.json", { cache: "no-cache" })).json();
	const name = `john-omr-${manifest.version}`;
	let cache = null;
	try {
		cache = await caches.open(name);
		for (const old of await caches.keys()) if (old.startsWith("john-omr-") && old !== name) await caches.delete(old);
	} catch {
		cache = null; // no Cache API (some private windows): download every time
	}
	const files = Object.entries(manifest.files);
	const missing = [];
	for (const [file, size] of files) if (!(await cache?.match(at(file)))) missing.push(size);
	const total = missing.reduce((a, b) => a + b, 0);
	let loaded = 0;
	if (total) post({ type: "progress", stage: "download", loaded, total });
	const bytes = {};
	for (const [file, size] of files)
		bytes[file] = await fetchFile(cache, file, size, (n) => {
			loaded += n;
			post({ type: "progress", stage: "download", loaded, total });
		});
	post({ type: "progress", stage: "start" });
	ort.env.logLevel = "error";
	ort.env.wasm.wasmBinary = bytes["ort/ort-wasm-simd-threaded.jsep.wasm"];
	ort.env.wasm.wasmPaths = { mjs: at("ort/ort-wasm-simd-threaded.jsep.mjs") };
	const meta = JSON.parse(new TextDecoder().decode(bytes["models/copista-meta.json"]));
	const wasm = (file) => ort.InferenceSession.create(bytes[file], { ...QUIET, executionProviders: ["wasm"] });
	const sessions = {
		trunk: await trunkSession(bytes["models/copista-2m-trunk.onnx"]),
		verifier: await wasm("models/copista-2m-verifier.onnx"),
		evidence: await wasm("models/copista-evidence.onnx"),
	};
	return { models: createModels(ort, sessions, meta), meta: { ...meta, detector: meta.detectors["2m"], size: "2m" } };
}

const trees = []; // each page's MusicXML tree, joined by "finish"

async function readPage({ data, width, height }, title) {
	reader ??= await load();
	let gray = rgbaToGray(data, width, height);
	const space = findStaves(gray, width, height).spacing;
	const upscaled = space > 0 && space < MIN_STAFF_SPACE && 2 * Math.max(width, height) <= MAX_SIDE;
	if (upscaled) {
		const ctx = new OffscreenCanvas(width * 2, height * 2).getContext("2d");
		ctx.imageSmoothingQuality = "high";
		ctx.drawImage(await createImageBitmap(new ImageData(data, width, height)), 0, 0, width * 2, height * 2);
		width *= 2;
		height *= 2;
		gray = rgbaToGray(ctx.getImageData(0, 0, width, height).data, width, height);
	}
	const start = performance.now();
	const { reading } = await recognizePage({ data: gray, width, height }, reader.models, reader.meta, {
		onProgress: (p) => post({ type: "progress", stage: "read", ratio: p.ratio }),
	});
	trees.push(writeTree(reading, { title }));
	return { type: "page", ms: Math.round(performance.now() - start), upscaled, space, backend, threads: self.crossOriginIsolated };
}

let queue = Promise.resolve();
self.onmessage = (event) => {
	const message = event.data;
	queue = queue.then(async () => {
		try {
			if (message.type === "page") post(await readPage(message.image, message.title));
			else if (message.type === "finish") post({ type: "result", musicXml: xmlDocument(joinTrees(trees)) });
		} catch (e) {
			console.error("John reader:", e);
			post({ type: "error", code: e?.code ?? "read", message: String(e?.message ?? e) });
		}
	});
};

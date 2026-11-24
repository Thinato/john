// Image import, main-thread side (#75). pickFile() in gui_import.cpp imports this module when images or PDFs are picked
// and calls read(). It turns them into pages (pdf.js renders PDF pages), sends them one at a time to worker.js and
// keeps `state` up to date; the Import song window reads it every frame through omrState(). When done it writes
//   /omr/result.musicxml  the score (all pages, in order)
//   /omr/original         the first page for comparing: width, height (uint32, little endian), then RGBA pixels
// to Emscripten's in-memory FS, where abcWindow() imports it. Nothing is uploaded: the image never leaves the device.

const state = { stage: "idle" }; // idle | open | download | start | read | done | error
let run = null; // the read in progress: { worker, cancelled }

const MAX_SIDE = 4096; // copisteria reads pages at up to 4096 px; bigger photos only cost memory
const ORIGINAL_SIDE = 2048; // the copy shown next to the sheet
const PDF_SCALE = 3; // about 216 dpi, as #74 measured

const fail = (code, message) => Object.assign(new Error(message), { code });
const isPdf = (f) => f.type === "application/pdf" || /\.pdf$/i.test(f.name);

/** RGBA pixels of a canvas-drawable source, scaled to fit `side`, on white (transparent PNGs). */
function pixels(source, width, height, side) {
	const s = Math.min(1, side / Math.max(width, height));
	const w = Math.max(1, Math.round(width * s));
	const h = Math.max(1, Math.round(height * s));
	const ctx = new OffscreenCanvas(w, h).getContext("2d");
	ctx.fillStyle = "#fff";
	ctx.fillRect(0, 0, w, h);
	ctx.drawImage(source, 0, 0, w, h);
	return ctx.getImageData(0, 0, w, h);
}

async function decodeImage(file) {
	try {
		const bitmap = await createImageBitmap(file); // EXIF orientation applied
		const image = pixels(bitmap, bitmap.width, bitmap.height, MAX_SIDE);
		bitmap.close();
		return image;
	} catch {
		throw fail(/\.hei[cf]$/i.test(file.name) || /hei[cf]/.test(file.type) ? "heic" : "decode", file.name);
	}
}

/** One function per page, in order: each renders its page when called. `docs` collects the pdf.js loading tasks, which
 * the caller must destroy (each runs its own pdf.js worker holding the file). */
async function pagesOf(files, docs) {
	const pages = [];
	for (const file of files) {
		if (!isPdf(file)) {
			pages.push(() => decodeImage(file));
			continue;
		}
		const pdfjs = await import("./pdfjs/pdf.min.mjs");
		pdfjs.GlobalWorkerOptions.workerSrc = new URL("./pdfjs/pdf.worker.min.mjs", import.meta.url).href;
		let pdf;
		try {
			const data = new Uint8Array(await file.arrayBuffer());
			const task = pdfjs.getDocument({ data, wasmUrl: new URL("./pdfjs/wasm/", import.meta.url).href, isEvalSupported: false });
			docs.push(task);
			pdf = await task.promise;
		} catch (e) {
			throw fail("pdf", `${file.name}: ${e?.message ?? e}`);
		}
		for (let n = 1; n <= pdf.numPages; n++)
			pages.push(async () => {
				const page = await pdf.getPage(n);
				const size = page.getViewport({ scale: 1 });
				const viewport = page.getViewport({ scale: Math.min(PDF_SCALE, MAX_SIDE / Math.max(size.width, size.height)) });
				const canvas = new OffscreenCanvas(Math.ceil(viewport.width), Math.ceil(viewport.height));
				const ctx = canvas.getContext("2d");
				ctx.fillStyle = "#fff";
				ctx.fillRect(0, 0, canvas.width, canvas.height);
				await page.render({ canvas, canvasContext: ctx, viewport }).promise;
				return ctx.getImageData(0, 0, canvas.width, canvas.height);
			});
	}
	return pages;
}

function writeOriginal(FS, image) {
	const page = new OffscreenCanvas(image.width, image.height);
	page.getContext("2d").putImageData(image, 0, 0);
	const small = pixels(page, image.width, image.height, ORIGINAL_SIDE);
	const out = new Uint8Array(8 + small.data.length);
	new DataView(out.buffer).setUint32(0, small.width, true);
	new DataView(out.buffer).setUint32(4, small.height, true);
	out.set(small.data, 8);
	FS.writeFile("/omr/original", out);
}

/** Read `files` (images or PDFs) into one score; a new call cancels the read in progress. */
export async function read(files, FS) {
	cancel();
	// pending: the reply the worker owes, { ok, fail }
	const me = { worker: new Worker(new URL("./worker.js", import.meta.url), { type: "module" }), cancelled: false, pending: null };
	run = me;
	const set = (fields) => me === run && Object.assign(state, fields);
	me.worker.onmessage = ({ data: m }) => {
		if (m.type === "progress") set(m.stage === "read" ? { stage: "read", ratio: m.ratio } : { stage: m.stage, loaded: m.loaded, total: m.total });
		else if (m.type === "error") me.pending?.fail(fail(m.code, m.message));
		else me.pending?.ok(m);
	};
	// a worker that fails to load fires "error" once, maybe before anything is asked: remember it
	let broken = null;
	me.worker.onerror = (e) => {
		broken ??= fail("read", e.message || "the reader could not start");
		me.pending?.fail(broken);
	};
	const ask = (message, transfer) =>
		new Promise((ok, failed) => {
			if (broken) return failed(broken);
			me.pending = { ok, fail: failed };
			me.worker.postMessage(message, transfer);
		});
	const docs = [];
	files = [...files].sort((a, b) => a.name.localeCompare(b.name, undefined, { numeric: true }));
	Object.keys(state).forEach((k) => delete state[k]);
	set({ stage: "open", name: files[0].name, page: 0, pages: 0, ms: 0, upscaled: 0 });
	try {
		try {
			FS.unlink("/omr/result.musicxml"); // a finished read John hasn't imported yet would be taken for this one
		} catch {}
		const pages = await pagesOf(files, docs);
		set({ pages: pages.length });
		try {
			FS.mkdir("/omr");
		} catch {}
		for (const [i, render] of pages.entries()) {
			if (me.cancelled) return;
			set({ stage: i ? "read" : state.stage, page: i + 1, ratio: 0 });
			const image = await render();
			if (me.cancelled) return; // before writing: a newer read may own /omr/original by now
			if (i === 0) writeOriginal(FS, image);
			const title = i ? null : files[0].name.replace(/\.[^.]*$/, ""); // the song is named after the file
			const done = await ask({ type: "page", title, image: { data: image.data, width: image.width, height: image.height } }, [image.data.buffer]);
			set({ ms: state.ms + done.ms, upscaled: state.upscaled + done.upscaled, backend: done.backend, threads: done.threads });
			console.log(`John reader: page ${i + 1}/${pages.length} in ${done.ms} ms (${done.backend}, threads ${done.threads}, staff space ${done.space.toFixed(1)} px${done.upscaled ? ", upscaled 2x" : ""})`);
		}
		const { musicXml } = await ask({ type: "finish" });
		if (me.cancelled) return;
		FS.writeFile("/omr/result.musicxml", musicXml);
		set({ stage: "done" });
	} catch (e) {
		if (!me.cancelled) {
			console.error("John reader:", e);
			set({ stage: "error", error: e?.code ?? "read", detail: String(e?.message ?? e) });
		}
	} finally {
		me.worker.terminate(); // frees the reader's memory; the next import loads it again from the Cache API
		for (const task of docs) task.destroy();
	}
}

/** Stop the read in progress, if any, and go back to "idle" (also after "done" or "error", once John has seen it). */
export function cancel() {
	if (run) {
		run.cancelled = true;
		run.worker.terminate();
		run.pending?.fail(fail("cancelled", "cancelled"));
		run = null;
	}
	Object.keys(state).forEach((k) => delete state[k]);
	state.stage = "idle";
}

globalThis.johnOmr = { state, cancel };

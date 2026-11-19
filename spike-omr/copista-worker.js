// The copisteria JS port's own recognition worker (vendor/, brunombo/OpticalMusicRecognition), loaded after an
// optional "no WebGPU" switch: a worker named "wasm" hides navigator.gpu so the detector trunk runs on WASM too.
if (self.name === "wasm") Object.defineProperty(self.navigator, "gpu", { value: undefined });
await import("./vendor/OpticalMusicRecognition/src/pipeline/worker.js");
self.postMessage({ type: "ready" });

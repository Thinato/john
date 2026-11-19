import { defineConfig } from "vite";

// homr-web's README: exclude it and onnxruntime-web from pre-bundling, include its OpenCV build.
export default defineConfig({
  base: "./",
  optimizeDeps: { exclude: ["homr-web", "onnxruntime-web"], include: ["homr-web > @techstark/opencv-js"] },
  worker: { format: "es" },
  build: { target: "es2022", copyPublicDir: false }, // models and images are served from public/ by serve.mjs
});

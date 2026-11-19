// Engrave ABC tunes onto one A4 page with Verovio and write it as SVG wrapped in HTML, ready for a headless
// Chrome screenshot. Several ABC files = several tunes on one page.
//   node render.mjs out.html a.abc [b.abc ...]
import { readFileSync, writeFileSync } from "node:fs";
import createVerovioModule from "verovio/wasm";
import { VerovioToolkit } from "verovio/esm";

const [out, ...abcs] = process.argv.slice(2);
const vrv = new VerovioToolkit(await createVerovioModule());
const svgs = abcs.map((f) => {
  vrv.setOptions({
    inputFrom: "abc", pageWidth: 2100, pageHeight: 2970, pageMarginLeft: 150, pageMarginRight: 150,
    pageMarginTop: 100, scale: 45, adjustPageHeight: true, footer: "none", breaks: "auto",
  });
  vrv.loadData(readFileSync(f, "utf8"));
  return vrv.renderToSVG(1);
});
writeFileSync(out, `<!doctype html><html><body style="margin:0;background:#fff;width:2100px">
${svgs.map((s) => `<div style="width:2100px">${s}</div>`).join("\n")}</body></html>`);

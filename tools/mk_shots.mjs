// SPDX-License-Identifier: GPL-3.0-only
// Headless screenshots of MAKU's screen in every preview look (firmware/src/ui_maku_style.c), then one comparison sheet.
//   node tools/mk_shots.mjs [OUT.png [STYLES [SEEDS]]]   (default docs/shots/mk_styles.png, styles 0,1,2,3,4 (0: the shipped look), seeds 7,99;
//                                           one style only: a strip of its worlds;
//                                           needs build/emu: web/emu/build.sh;
//                                           Playwright with a Chromium, e.g. PLAYWRIGHT_BROWSERS_PATH)
// Rows: two worlds (#seed=), a verb held (SINK: key "1"), SELECT turned (the arrow keys); columns: the styles given (#style=N).
import { createRequire } from "module";
import http from "http"; import fs from "fs"; import path from "path"; import { execFileSync } from "child_process";
const require = createRequire(process.env.PW_MODULES || "/opt/node22/lib/node_modules/");
const { chromium } = require("playwright");
const out = process.argv[2] || "docs/shots/mk_styles.png", tiles = "build/shots/tiles";
const styles = (process.argv[3] || "0,1,2,3,4");
fs.mkdirSync(tiles, { recursive: true });
const root = "build/emu", mime = { ".html": "text/html", ".js": "text/javascript", ".wasm": "application/wasm", ".json": "application/json", ".woff": "font/woff", ".png": "image/png", ".jpg": "image/jpeg" };
const srv = http.createServer((q, r) => { const p = path.join(root, q.url.split(/[?#]/)[0].replace(/\/$/, "/index.html")); fs.readFile(p, (e, d) => { if (e) { r.writeHead(404); r.end(); } else { r.writeHead(200, { "content-type": mime[path.extname(p)] || "application/octet-stream" }); r.end(d); } }); }).listen(0);
const port = srv.address().port;
const b = await chromium.launch({ executablePath: process.env.CHROMIUM || "/opt/pw-browsers/chromium", args: ["--autoplay-policy=no-user-gesture-required", "--no-sandbox"] });
const STYLES = (process.argv[3] || "0,1,2,3,4").split(",").map(Number), SEEDS = (process.argv[4] || "7,99").split(",").map(Number);
for (const style of STYLES) for (const seed of SEEDS) {
  const pg = await b.newPage({ viewport: { width: 1280, height: 900 }, deviceScaleFactor: 3 });
  pg.on("pageerror", (e) => console.log("err:", e.message));
  await pg.goto(`http://localhost:${port}/#seed=${seed}&style=${style}`);
  await pg.click("#go");
  await pg.waitForTimeout(3500);
  await pg.locator("#screen").screenshot({ path: `${tiles}/s${style}_w${seed}.png` });
  if (seed === SEEDS[0]) {
    await pg.keyboard.down("1"); await pg.waitForTimeout(700);
    await pg.locator("#screen").screenshot({ path: `${tiles}/s${style}_sink.png` });
    await pg.keyboard.up("1");
    await pg.waitForTimeout(500);
    for (let i = 0; i < 6; i++) { await pg.keyboard.press("ArrowUp"); await pg.waitForTimeout(40); }   // SELECT turned
    await pg.waitForTimeout(350);
    await pg.locator("#screen").screenshot({ path: `${tiles}/s${style}_turn.png` });
  }
  await pg.close();
}
await b.close(); srv.close();
execFileSync("python3", ["-I", path.join(path.dirname(new URL(import.meta.url).pathname), "mk_shots_sheet.py"), tiles, out, styles, process.argv[4] || "7,99"], { stdio: "inherit" });

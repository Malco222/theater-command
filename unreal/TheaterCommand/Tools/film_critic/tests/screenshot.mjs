// Screenshots of the report page at 1440 and 390 wide (deviceScaleFactor 1), light and dark, and a check that the
// page does not scroll sideways. Usage: NODE_PATH=/opt/node-tools/node_modules node screenshot.mjs PAGE.html OUT_DIR
import { createRequire } from "module";
import path from "path";
const require = createRequire(import.meta.url);
const { chromium } = require("playwright");

const [page_, out] = process.argv.slice(2);
const url = "file://" + path.resolve(page_);
const browser = await chromium.launch();
let bad = 0;
for (const [w, h] of [[1440, 900], [390, 844]]) {
  for (const scheme of ["light", "dark"]) {
    const ctx = await browser.newContext({ viewport: { width: w, height: h }, deviceScaleFactor: 1, colorScheme: scheme });
    const p = await ctx.newPage();
    await p.goto(url);
    await p.waitForLoadState("networkidle");
    const sideways = await p.evaluate(() => document.documentElement.scrollWidth - document.documentElement.clientWidth);
    const broken = await p.evaluate(() => [...document.images].filter((i) => !i.complete || i.naturalWidth === 0).map((i) => i.src));
    const file = path.join(out, `shot-${w}-${scheme}.png`);
    await p.screenshot({ path: file, fullPage: true });
    console.log(`${file}: sideways scroll ${sideways}px, broken images ${broken.length}`);
    if (sideways > 0 || broken.length) bad++;
    await ctx.close();
  }
}
await browser.close();
process.exit(bad ? 1 : 0);

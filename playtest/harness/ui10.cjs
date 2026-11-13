// Small-item checks: is the Begin button in view at three window sizes, and how an estimate of a hidden army reads.
const { chromium } = require('playwright');
const out = process.argv[2] ?? 'shots';
(async () => {
  const browser = await chromium.launch();
  for (const [w, h] of [[1440, 900], [1280, 720], [1024, 640]]) {
    const page = await browser.newPage({ viewport: { width: w, height: h }, deviceScaleFactor: 1 });
    await page.goto(process.env.TC_PAGE || 'file:///home/user/theater-command/dist/play.html');
    await page.waitForTimeout(1000);
    const box = await page.locator('[data-act="start"]').boundingBox();
    console.log(`${w}x${h}: Begin button ${box ? `top ${Math.round(box.y)}, bottom ${Math.round(box.y + box.height)}: ${box.y + box.height <= h ? 'in view' : 'below the fold'}` : 'not found'}`);
    if (w === 1280) await page.screenshot({ path: `${out}/64-start-1280-v55.png` });
    if (w === 1280) {
      await page.addInitScript(() => { try { localStorage.setItem('tc-watch', '0'); } catch {} });
      await page.evaluate(() => window.theater.newGame('GER', 7, 1, 'grand', 'colonel'));
      await page.waitForTimeout(1500);
      const est = await page.evaluate(() => [...document.querySelectorAll('#map text, svg text')].map((t) => t.textContent).filter((t) => /\?/.test(t || '')).slice(0, 12));
      console.log('estimates on the map:', JSON.stringify(est));
      await page.screenshot({ path: `${out}/65-map-estimates-v55.png` });
    }
    await page.close();
  }
  await browser.close();
})();

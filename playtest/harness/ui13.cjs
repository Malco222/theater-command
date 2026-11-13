// How long is the province panel, and what is in it, in two builds.
const { chromium } = require('playwright');
(async () => {
  const browser = await chromium.launch();
  for (const url of process.argv.slice(2)) {
    const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 });
    await page.addInitScript(() => { try { localStorage.setItem('tc-watch', '0'); } catch {} });
    await page.goto(url);
    await page.waitForTimeout(800);
    await page.locator('[data-act="start"]').click();
    await page.waitForTimeout(2500);
    for (const name of ['Hamburg', 'Ruhr', 'Berlin', 'Bavaria']) {
      const id = await page.evaluate((n) => window.theater.game().regions.find((r) => r.name === n)?.id ?? -1, name);
      if (id < 0) continue;
      await page.evaluate((i) => window.theater.onRegion(i), id);
      await page.waitForTimeout(400);
      const info = await page.evaluate(() => { const p = document.querySelector('.pane'); if (!p) return null; const kids = [...p.children].map((c) => ({ cls: (c.className || c.tagName).toString().slice(0, 30), h: Math.round(c.getBoundingClientRect().height), head: (c.querySelector('b, h3, h4, .head, small')?.textContent || '').trim().slice(0, 40) })).filter((k) => k.h > 0); return { scrollH: p.scrollHeight, clientH: p.clientHeight, kids }; });
      console.log(`${url.includes('tc56') ? 'V56' : 'V55'} ${name}: panel ${info.scrollH}px tall in a ${info.clientH}px window (${(info.scrollH / info.clientH).toFixed(1)} screens)`);
      if (name === 'Hamburg') console.log('   blocks: ' + info.kids.map((k) => `${k.head || k.cls} ${k.h}`).join(' | ').slice(0, 1500));
    }
    await page.close();
  }
  await browser.close();
})();

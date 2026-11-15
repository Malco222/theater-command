// Where the player's army stands in a save, read through the page: node peek74.cjs SAVE.json...
const { chromium } = require('playwright');
const fs = require('fs');
(async () => {
  const browser = await chromium.launch();
  for (const save of process.argv.slice(2)) {
    const ctx = await browser.newContext({ viewport: { width: 1440, height: 900 } });
    const page = await ctx.newPage();
    await page.addInitScript((s) => { try { if (!sessionStorage.getItem('tc-once')) { sessionStorage.setItem('tc-once', '1'); localStorage.clear(); localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } } catch {} }, fs.readFileSync(save, 'utf8'));
    await page.goto(process.env.TC_PAGE || 'file:///home/user/tc74/dist/play.html'); await page.waitForTimeout(800);
    const c = page.locator('[data-act="continue"]').first(); if (await c.count()) await c.click(); else await page.evaluate(() => window.theater.continueGame());
    await page.waitForTimeout(1200);
    const out = await page.evaluate(() => {
      const g = window.theater.game(); const me = g.human; const n = g.nations[me];
      const by = {}; let home = 0, all = 0, divs = 0;
      for (const c of g.corps) { if (c.nat !== me) continue; const r = g.regions[c.reg]; const k = `${r.name} (${r.owner === r.ctrl ? r.owner : `${r.owner} held by ${r.ctrl}`})`; by[k] = (by[k] || 0) + 1; all++; if (r.owner === me) home++; divs += c.divs ?? (c.div ? c.div.length : 0); }
      const fleets = g.fleets.filter((f) => f.nat === me).map((f) => `${f.name}: ${Object.values(f.ships).reduce((a, b) => a + b, 0)} ships, ${g.seas[f.zone]?.name ?? 'harbour'} (${f.stance ?? f.duty ?? ''})`);
      return `turn ${g.turn}; at war with ${n.atWar.join(',') || 'nobody'}; treasury ${Math.round(n.money ?? n.treasury ?? 0)}; sea lift ${n.sealift}; corps ${all} (${home} at home): ${Object.entries(by).sort((a, b) => b[1] - a[1]).slice(0, 16).map(([k, v]) => `${k} ${v}`).join('; ')}\n   fleets: ${fleets.join('; ').slice(0, 500)}`;
    });
    console.log(`${save.split('/').pop()}: ${out}`);
    await ctx.close();
  }
  await browser.close();
})();

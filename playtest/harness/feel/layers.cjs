// layers: which layer of the browser map makes it slow to move. Plays NAT for MONTHS months (ending each month, with
// one attack a month if asked), then drags the map at a place with each top layer hidden in turn.
// usage: node layers.cjs NAT MONTHS PLACE [W]
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc842/dist/play.html';
const [nat = 'Germany', monthsS = '4', place = 'Warsaw', wS = '240'] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 } })).newPage();
  await page.goto(PAGE); await page.evaluate((nw) => { localStorage.clear(); if (!nw) localStorage.setItem('tc-watch', '0'); localStorage.setItem('tc-ask-end', '0'); }, process.env.TC_NEW === '1'); await page.reload(); await page.waitForSelector('#card');
  const press = (words, root) => page.evaluate(({ words, root }) => { const r = document.querySelector(root); const b = r && [...r.querySelectorAll('button')].find((x) => (x.innerText || '').replace(/\s+/g, ' ').trim().toLowerCase().startsWith(words.toLowerCase())); if (b) b.click(); return !!b; }, { words, root });
  const saved = process.env.TC_SAVE ? require('fs').readFileSync(process.env.TC_SAVE, 'utf8') : '';
  if (saved) { await page.evaluate((t) => localStorage.setItem('theater-command-save-v1', t), saved); await page.reload(); await page.waitForSelector('#card'); await page.evaluate(() => { try { window.theater.continueGame(); } catch (e) {} }); await page.waitForTimeout(2500); await page.evaluate(() => window.theater.openModal(null)); }
  else { await press(nat, '#card'); await page.waitForTimeout(300); await press('BEGIN THE CAMPAIGN', '#card'); await page.waitForTimeout(2500); }
  const drag = async () => {
    const box = await page.evaluate(() => { const r = document.querySelector('#map').getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; });
    await page.evaluate(() => { const f = (window.__frames = { on: true, t: [], last: performance.now() }); const tick = (now) => { if (!f.on) return; f.t.push(now - f.last); f.last = now; requestAnimationFrame(tick); }; requestAnimationFrame(tick); });
    await page.mouse.move(box.x, box.y); await page.mouse.down();
    for (let i = 0; i < 24; i++) { const a = (i / 24) * Math.PI * 2; await page.mouse.move(box.x + Math.cos(a) * 160 - 160, box.y + Math.sin(a) * 110); await page.waitForTimeout(16); }
    await page.mouse.up();
    return page.evaluate(() => { const f = window.__frames; f.on = false; const t = f.t.slice(2).sort((a, b) => a - b); const q = (p) => t[Math.min(t.length - 1, Math.floor(t.length * p))]; return `${t.length} pictures, middle ${q(0.5).toFixed(0)}ms, slow ${q(0.9).toFixed(0)}ms, worst ${t[t.length - 1].toFixed(0)}ms`; });
  };
  const centre = () => page.evaluate(({ place, w }) => { const t = window.theater, g = t.game(), el = document.querySelector('#map'); const r = g.regions.find((x) => x.name === place); const tall = el.clientHeight / el.clientWidth; t.ui.view.w = w; t.ui.view.x = r.x - w / 2; t.ui.view.y = r.y - w * tall / 2; t.applyView(); }, { place, w: Number(wS) });
  await centre(); await page.waitForTimeout(400); console.log(`month 1, all layers: ${await drag()}`);
  for (let m = 0; m < Number(monthsS); m++) {
    await page.evaluate(() => { const b = document.querySelector('#barbtns [data-act="end"]'); if (b) b.click(); }); const t0 = Date.now(); const turn = await page.evaluate(() => window.theater.game().turn);
    while (Date.now() - t0 < 60000) { await page.waitForTimeout(400); if (await page.evaluate(() => window.theater.game().turn) > turn) break; }
    for (let k = 0; k < 6; k++) { await page.waitForTimeout(700); const open = await page.evaluate(() => document.querySelector('#veil')?.classList.contains('open')); if (!open) break; (await press('GIVE ORDERS', '#card')) || (await press('CLOSE', '#card')) || (await page.evaluate(() => window.theater.openModal(null))); }
  }
  await centre(); await page.waitForTimeout(500);
  console.log(`after ${monthsS} months, all layers: ${await drag()}`);
  const layers = await page.evaluate(() => [...document.querySelector('#map').children].map((c, i) => ({ i, id: c.id || c.tagName, kids: c.querySelectorAll('*').length })));
  console.log(layers.map((l) => `${l.id}:${l.kids}`).join(' '));
  for (const l of layers.filter((x) => x.kids > 20 && process.env.TC_NOLAYERS !== '1')) {
    await page.evaluate((i) => { const c = document.querySelector('#map').children[i]; c.dataset.was = c.style.display; c.style.display = 'none'; }, l.i);
    await page.waitForTimeout(200); const r = await drag();
    await page.evaluate((i) => { const c = document.querySelector('#map').children[i]; c.style.display = c.dataset.was || ''; }, l.i);
    console.log(`   without ${l.id} (${l.kids} nodes): ${r}`);
  }
  await browser.close();
})();

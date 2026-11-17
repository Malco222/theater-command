// Times one month's end on a saved game: how long until the month card, how much of it is the page working, and what shows meanwhile.
const { chromium } = require('playwright'); const fs = require('fs');
const PAGE = process.env.TC_PAGE; const [save, watch = ''] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const page = await (await browser.newContext({ viewport: { width: 1600, height: 1000 } })).newPage();
  await page.addInitScript(() => { window.__lt = []; try { new PerformanceObserver((l) => { for (const e of l.getEntries()) window.__lt.push([Math.round(e.startTime), Math.round(e.duration)]); }).observe({ entryTypes: ['longtask'] }); } catch (e) {} });
  await page.goto(PAGE); await page.evaluate(([t, w]) => { localStorage.clear(); localStorage.setItem('theater-command-save-v1', t); if (w) localStorage.setItem('tc-watch', w); }, [fs.readFileSync(save, 'utf8'), watch]); await page.reload(); await page.waitForSelector('#card');
  await page.evaluate(() => { try { window.theater.continueGame(); } catch (e) {} }); await page.waitForTimeout(2500); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(500);
  for (let m = 0; m < 2; m++) {
    const t0 = await page.evaluate(() => { window.__lt.length = 0; return performance.now(); });
    await page.keyboard.press('Enter'); const seen = []; let card = '', at = 0;
    for (let i = 0; i < 150; i++) { await page.waitForTimeout(100); const s = await page.evaluate(() => { const v = document.querySelector('#veil'), c = document.querySelector('#card'); const open = v && v.classList.contains('open'); const say = (q) => { const e = document.querySelector(q); return e && e.offsetParent !== null ? e.innerText.replace(/\s+/g, ' ').slice(0, 70) : ''; }; return { now: performance.now(), card: open ? c.innerText.replace(/\s+/g, ' ').slice(0, 50) : '', hint: say('#hint'), turn: window.theater.game().turn, cls: document.body.className + ' | ' + document.querySelector('#map').getAttribute('class') }; });
      const k = `${s.hint} [${s.cls}]`; if (!seen.includes(k)) seen.push(k);
      if (s.card && /BEFORE THE MONTH ENDS/.test(s.card)) { await page.keyboard.press('Enter'); continue; }
      if (s.card) { card = s.card; at = s.now - t0; break; } }
    const lt = await page.evaluate((t0) => window.__lt.filter((x) => x[0] >= t0), t0);
    console.log(`month ${m + 1}: card after ${(at / 1000).toFixed(1)}s "${card.slice(0, 40)}"; page working ${(lt.reduce((a, x) => a + x[1], 0) / 1000).toFixed(1)}s in ${lt.length} stretches, longest ${Math.max(0, ...lt.map((x) => x[1]))}ms`);
    console.log('  shown meanwhile: ' + seen.slice(0, 5).join(' >> ').slice(0, 420));
    await page.keyboard.press('Enter'); await page.waitForTimeout(1200); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300);
  }
  await browser.close();
})();

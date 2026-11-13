const { chromium } = require('playwright');
const out = process.argv[2];
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push(`pageerror: ${e.message}`));
  await page.addInitScript(() => { try { localStorage.setItem('tc-watch', '0'); } catch {} });
  await page.goto('file:///home/user/theater-command/dist/play.html');
  await page.waitForTimeout(800);
  await page.evaluate(() => window.theater.newGame('ITA', 7, 1, 'grand', 'colonel'));
  const state = () => page.evaluate(() => { const g = window.theater.game(); const n = g.nations.ITA; return { turn: g.turn, human: g.human, alive: n.alive, prov: g.regions.filter((r) => r.ctrl === 'ITA').length, corps: g.corps.filter((c) => c.nat === 'ITA').length, wars: n.atWar.join(','), hunger: n.res && n.res.hunger, modal: (document.querySelector('#card')?.textContent || '').replace(/\s+/g, ' ').slice(0, 260), modalOpen: !!document.querySelector('#modal.show,#veil.show,.veil.show') || getComputedStyle(document.querySelector('#modal') || document.body).display }; });
  const closeAll = async () => { for (let k = 0; k < 8; k++) { const open = await page.evaluate(() => /open/.test(document.querySelector('#veil').className)); if (!open) return k; await page.keyboard.press('Escape'); await page.waitForTimeout(40); } return 9; };
  const end = async () => { await closeAll(); await page.evaluate(() => document.querySelector('[data-act="end"]')?.click()); await page.waitForTimeout(120); };
  let s = await state();
  while (s.turn < 10) { await end(); s = await state(); }
  console.log('at', JSON.stringify(s));
  console.log('modal controls:', await page.evaluate(() => [...document.querySelectorAll('#veil [data-act], #veil button')].map((e) => `${e.tagName}.${e.className}|${e.getAttribute('data-act')}|${(e.textContent || '').trim().slice(0, 20)}`).slice(0, 12).join(' ; ')));
  await page.screenshot({ path: out + '/09-ita-dispatches.png' });
  console.log('modals closed with', await closeAll(), 'presses of Escape');
  await page.locator('[data-act="declare"]').first().click();
  await page.waitForTimeout(200);
  await page.screenshot({ path: out + '/10-ita-declare.png' });
  console.log((await page.evaluate(() => [...document.querySelectorAll('#card [data-act]')].map((e) => `${e.getAttribute('data-act')}|${e.getAttribute('data-id') ?? ''}|${e.getAttribute('data-nat') ?? ''}|${(e.textContent || '').trim().replace(/\s+/g, ' ').slice(0, 50)}`))).join('\n'));
  // Pick the United Kingdom and confirm.
  await page.evaluate(() => { const b = document.querySelector('#card [data-act="askdeclare"][data-nat="UK"]'); b && b.click(); });
  await page.waitForTimeout(200);
  await page.screenshot({ path: out + '/11-ita-declare-uk.png' });
  console.log((await page.evaluate(() => [...document.querySelectorAll('#card [data-act]')].map((e) => `${e.getAttribute('data-act')}|${e.getAttribute('data-id') ?? ''}|${(e.textContent || '').trim().replace(/\s+/g, ' ').slice(0, 60)}`))).join('\n'));
  await page.evaluate(() => { const b = [...document.querySelectorAll('#card [data-act]')].find((e) => /^Declare war/.test((e.textContent || '').trim())); b && b.click(); });
  await page.waitForTimeout(200);
  s = await state(); console.log('after declaring', JSON.stringify(s).slice(0, 200));
  // End months until Italy is gone.
  let t0 = Date.now(), last = s;
  for (let i = 0; i < 80; i++) {
    await end(); await page.waitForTimeout(30);
    s = await state();
    if (!s.alive || s.prov === 0 || s.human !== 'ITA') {
      console.log(`month ${s.turn}: alive ${s.alive} human ${s.human} prov ${s.prov} corps ${s.corps} | modal open: ${await page.evaluate(() => document.querySelector('#veil').className)} | modal: ${s.modal.slice(0, 200)}`);
      await page.screenshot({ path: out + `/12-ita-fallen-${s.turn}.png` });
      const top = await page.evaluate(() => (document.querySelector('#bar, header, .bar')?.textContent || '').replace(/\s+/g, ' ').slice(0, 300));
      console.log('   top bar:', top);
      if (s.human !== 'ITA' || i > 75) break;
      // press end again and see
      console.log('   Escapes needed:', await closeAll());
      await page.screenshot({ path: out + `/12b-ita-fallen-map.png` });
      console.log('   controls:', await page.evaluate(() => [...document.querySelectorAll('#barbtns [data-act]')].map((e) => e.textContent.trim()).join(' | ')), '| alerts:', await page.evaluate(() => [...document.querySelectorAll('[data-act="alert"]')].map((e) => e.textContent.trim()).join(' | ')));
      await page.evaluate(() => document.querySelector('[data-act="end"]')?.click());
      await page.waitForTimeout(200);
      s = await state();
      console.log(`  after pressing End month again: turn ${s.turn} alive ${s.alive} human ${s.human} | modal: ${s.modal.slice(0, 300)}`);
      await page.screenshot({ path: out + `/13-ita-after-end.png` });
      break;
    }
    last = s;
  }
  console.log('months took', Date.now() - t0, 'ms; errors', errors.join('; '));
  await browser.close();
})();

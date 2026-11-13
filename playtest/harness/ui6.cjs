// A first German month by hand: pick an army, open the attack card, fight, read the ticket, look at Build, Research, Powers and the ledger.
const { chromium } = require('playwright');
const dir = __dirname;
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
  const errs = [];
  page.on('pageerror', (e) => errs.push(e.message));
  await page.addInitScript(() => { try { localStorage.setItem('tc-watch', '0'); } catch {} });
  await page.goto('file:///home/user/theater-command/dist/play.html');
  await page.waitForTimeout(800);
  await page.evaluate(() => window.theater.newGame('GER', 7, 1, 'grand', 'colonel'));
  await page.waitForTimeout(800);
  const closeAll = async () => { for (let i = 0; i < 12; i++) { const open = await page.evaluate(() => document.querySelector('#veil')?.classList.contains('open')); if (!open) break; await page.keyboard.press('Escape'); await page.waitForTimeout(120); } };
  const card = () => page.evaluate(() => document.querySelector('#veil')?.classList.contains('open') ? document.querySelector('#card')?.innerText?.replace(/\s+/g, ' ').slice(0, 1800) : '(no card)');
  const side = () => page.evaluate(() => document.querySelector('#side, aside, .side')?.innerText?.replace(/\s+/g, ' ').slice(0, 1500));
  const acts = () => page.evaluate(() => [...new Set([...document.querySelectorAll('[data-act]')].filter((e) => e.offsetParent !== null).map((e) => `${e.getAttribute('data-act')}|${(e.textContent || '').trim().replace(/\s+/g, ' ').slice(0, 40)}`))].join('  ;  '));
  console.log('FIRST CARD:', await card());
  await closeAll();
  // A German province with corps beside a Polish one.
  const pick = await page.evaluate(() => { const g = window.theater.game(); for (const r of g.regions) { if (r.ctrl !== 'GER' || !r.units.some((u) => u.nat === 'GER' && !u.dead)) continue; const t = r.nb.find((j) => g.regions[j].ctrl === 'POL' && g.regions[j].units.some((u) => u.nat === 'POL')); if (t !== undefined) return { from: r.id, fromName: r.name, to: t, toName: g.regions[t].name }; } return null; });
  console.log('pick', JSON.stringify(pick));
  await page.evaluate((id) => window.theater.onRegion(id), pick.from);
  await page.waitForTimeout(300);
  await page.screenshot({ path: dir + '/shots/30-ger-picked.png' });
  console.log('SIDE after pick:', await side());
  console.log('ACTS:', await acts());
  await page.evaluate((id) => window.theater.onRegion(id), pick.to);
  await page.waitForTimeout(400);
  await page.screenshot({ path: dir + '/shots/31-ger-attack-card.png' });
  console.log('ATTACK CARD:', await card());
  console.log('ACTS:', await acts());
  // Confirm the attack if a button offers it.
  const go = await page.evaluate(() => { const b = [...document.querySelectorAll('#card [data-act]')].find((e) => /attack|assault|go/i.test(e.getAttribute('data-act')) && !/close/.test(e.getAttribute('data-act'))); if (b) { const a = b.getAttribute('data-act'); b.dispatchEvent(new MouseEvent('click', { bubbles: true })); return a; } return null; });
  await page.waitForTimeout(600);
  console.log('pressed', go);
  await page.screenshot({ path: dir + '/shots/32-ger-battle-ticket.png' });
  console.log('AFTER ATTACK:', await card());
  await closeAll();
  for (const tab of ['build', 'research', 'powers', 'navy', 'air', 'spies', 'generals']) {
    const ok = await page.evaluate((t) => { const b = [...document.querySelectorAll('[data-act="tab"]')].find((e) => (e.getAttribute('data-id') || e.getAttribute('data-tab') || e.textContent || '').toLowerCase().includes(t)); if (b) { b.dispatchEvent(new MouseEvent('click', { bubbles: true })); return true; } return false; }, tab);
    await page.waitForTimeout(300);
    await page.screenshot({ path: dir + `/shots/33-tab-${tab}.png` });
    console.log(`TAB ${tab} (${ok}):`, (await side() || '').slice(0, 700));
  }
  // The ledger from the treasury pill.
  const led = await page.evaluate(() => { const b = document.querySelector('[data-act="ledger"], [data-act="treasury"], [data-act="money"]'); if (b) { b.dispatchEvent(new MouseEvent('click', { bubbles: true })); return b.getAttribute('data-act'); } return null; });
  await page.waitForTimeout(300);
  await page.screenshot({ path: dir + '/shots/34-ledger.png' });
  console.log('LEDGER', led, await card());
  console.log('errors', errs);
  await browser.close();
})();

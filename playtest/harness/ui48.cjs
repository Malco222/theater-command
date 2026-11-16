// Version 78 on the page: what each city has on order. A player (Germany by default) places the first ORDERS offers of
// the Build tab, then: the Build tab grouped by city, the badges on the map (how many, their words when pointed at),
// the same one month on, and the page errors.
// usage: [TC_PAGE=file:///home/user/tc78/dist/play.html] node ui48.cjs NAT ORDERS PREFIX
const { chromium } = require('playwright');
const [nat = 'GER', ordersS = '4', prefix = '99-v78-orders'] = process.argv.slice(2);
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc78/dist/play.html';
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 200)));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(`console: ${m.text().slice(0, 200)}`); });
  const say = (k, v) => console.log(`${k}: ${typeof v === 'string' ? v : JSON.stringify(v)}`);
  const shot = (name) => page.screenshot({ path: `shots/${prefix}-${name}.png` });
  const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  const shut = async () => { for (let i = 0; i < 12 && await veil(); i++) { await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(200); } };
  const pane = (tab) => page.evaluate((tab) => { const t = window.theater; t.ui.tab = tab; t.refresh(); return (document.querySelector('#pane')?.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 1800); }, tab);
  const badges = () => page.evaluate(() => [...document.querySelectorAll('.obadge')].map((b) => ({ id: b.dataset.id, cls: b.getAttribute('class'), text: (b.textContent || '').replace(/\s+/g, ' ').trim().slice(0, 40) })));
  await page.goto(PAGE); await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); }); await page.reload(); await page.waitForSelector('#card');
  await page.click(`.gcnat[data-nat="${nat}"]`); await page.waitForTimeout(250); await page.click('.gcgo');
  await page.waitForFunction((nat) => { const g = window.theater.game(); return g && g.human === nat && !document.querySelector('#veil.open .busy'); }, nat, { timeout: 300000 });
  await page.waitForTimeout(1500); await shut();
  say('badges as the campaign opens', await badges());
  await pane('build');
  const placed = [];
  for (let i = 0; i < Number(ordersS); i++) {
    const got = await page.evaluate((i) => { const b = [...document.querySelectorAll('#pane button.buy.offer:not([disabled])')][i]; if (!b) return null; const t = b.innerText.replace(/\s+/g, ' ').trim().slice(0, 90); b.click(); return t; }, i);
    if (!got) break; placed.push(got); await page.waitForTimeout(350);
    if (await veil()) { say('a card after the order', await page.evaluate(() => (document.querySelector('#card')?.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 300))); await shut(); }
    await pane('build');
  }
  say('orders placed', placed);
  say('build tab', await pane('build')); await shot('1-build-by-city');
  const b = await badges(); say('badges on the map', b);
  if (b.length) {
    const box = await page.evaluate(() => { const r = document.querySelector('.obadge').getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2, w: r.width, h: r.height }; });
    say('first badge on the screen', box);
    await page.mouse.move(box.x, box.y); await page.waitForTimeout(500);
    say('pointing at it', await page.evaluate(() => { const t = document.querySelector('#tip, .tip'); return t ? t.innerText.replace(/\s+/g, ' ').trim().slice(0, 400) : 'no tip element'; }));
    await shot('2-map-badge-pointed');
  }
  await shot('3-map');
  await page.evaluate(() => { const g = window.theater.game(); return g.turn; });
  const end = await page.$('[data-act="end"], #end, .endturn');
  if (end) { await end.click(); await page.waitForFunction(() => !document.querySelector('#veil.open .busy'), null, { timeout: 300000 }).catch(() => {}); await page.waitForTimeout(2500); await shut();
    say('one month on, badges', await badges()); say('build tab one month on', (await pane('build')).slice(0, 700)); await shot('4-one-month-on'); }
  else say('end turn button', 'not found');
  say('page errors', errors.length ? errors.slice(0, 8) : 'none');
  await browser.close();
})();

// What a seat holds at the start, for writing a click-through step list. usage: node ui66.cjs NAT [SEED]
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc84/dist/play.html';
const [nat = 'USA', seedS = '25'] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch();
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 } })).newPage();
  await page.goto(PAGE); await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); }); await page.reload(); await page.waitForSelector('#card');
  await page.evaluate(({ nat, seed }) => window.theater.newGame(nat, seed, 1, 'grand', 'colonel', true, false, 'theatre', 3), { nat, seed: Number(seedS) }); await page.waitForTimeout(2500);
  for (let i = 0; i < 5; i++) await page.evaluate(() => { const v = document.querySelector('#veil'); if (v && v.classList.contains('open')) window.theater.openModal(null); });
  const f = await page.evaluate(() => {
    const g = window.theater.game(), me = g.human, R = g.regions;
    const armies = R.filter((r) => r.units.some((c) => c.nat === me && !c.dead)).map((r) => `${r.name}: ${Object.entries(r.units.filter((c) => c.nat === me).reduce((m, c) => (m[c.kind] = (m[c.kind] || 0) + 1, m), {})).map(([k, n]) => n + ' ' + k).join(', ')}`);
    const seaName = (z) => { const s = (g.seas || g.sea || [])[z]; return s && s.name ? s.name : 'sea ' + z; };
    const fleets = g.fleets.filter((x) => x.nat === me && !x.dead).map((x) => `${x.name} at ${x.port != null && x.port >= 0 ? R[x.port]?.name : seaName(x.zone ?? x.sea)} (${Object.entries(x.ships || {}).filter(([, n]) => n).map(([k, n]) => n + ' ' + k).join(', ')})`);
    const gates = g.gates.map((x) => `${x.name}: ${seaName(x.a)} / ${seaName(x.b)}`);
    const tabs = [...document.querySelectorAll('[data-tab]')].map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim() || e.dataset.tab);
    const top = [...document.querySelectorAll('header button, #top button, .topbar button')].map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim()).filter(Boolean);
    return { armies, fleets, gates, tabs, top, wars: g.nations[me].atWar, money: g.nations[me].money ?? g.nations[me].treasury, keysG: Object.keys(g).filter((k) => /sea/i.test(k)) };
  });
  console.log('ARMIES ' + f.armies.join(' | '));
  console.log('FLEETS ' + f.fleets.join(' | '));
  console.log('GATES ' + f.gates.join(' | '));
  console.log('TABS ' + f.tabs.join(' | '));
  console.log('TOP ' + f.top.join(' | '));
  console.log('OTHER ' + JSON.stringify({ wars: f.wars, money: f.money, keysG: f.keysG }));
  await browser.close();
})();

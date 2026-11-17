// names3: the pieces a seat starts with, entered through the menu at a chosen depth. usage: node names3.cjs "United States" "Supreme command"
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc842/dist/play.html';
const [nat = 'United States', depth = ''] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 } })).newPage();
  await page.goto(PAGE); await page.evaluate(() => localStorage.clear()); await page.reload(); await page.waitForSelector('#card');
  const press = (words, root) => page.evaluate(({ words, root }) => { const r = document.querySelector(root); const b = [...r.querySelectorAll('button')].find((x) => (x.innerText || '').replace(/\s+/g, ' ').trim().toLowerCase().startsWith(words.toLowerCase())); if (b) b.click(); return !!b; }, { words, root });
  await press(nat, '#card'); await page.waitForTimeout(300); if (depth) { await press(depth, '#card'); await page.waitForTimeout(300); } await press('BEGIN THE CAMPAIGN', '#card'); await page.waitForTimeout(2500);
  const out = await page.evaluate(() => { const g = window.theater.game(), me = g.human, R = g.regions; const by = {}; for (const c of g.corps) if (c.nat === me && !c.dead) { const k = R[c.at ?? c.region ?? c.loc]?.name || '?'; (by[k] = by[k] || []).push(c.kind); }
    const keys = Object.keys(g.corps[0] || {}).join(',');
    const fleets = g.fleets.filter((f) => f.nat === me && !f.dead).map((f) => `${f.name} @ ${f.port != null && f.port >= 0 ? R[f.port].name : g.seas[f.zone]?.name}`);
    const air = (g.airFleets || []).filter((a) => a.nat === me && !a.dead).map((a) => `${a.name} @ ${R[a.base ?? a.at ?? a.field]?.name || (a.carrier != null ? 'carrier' : '?')} f${a.ftr || 0} b${a.bmr || 0}`);
    const akeys = Object.keys((g.airFleets || [])[0] || {}).join(',');
    const tabs = [...document.querySelectorAll('#tabs .tab')].map((b) => b.innerText.replace(/\s+/g, ' ').trim()).join(' | ');
    const alerts = [...document.querySelectorAll('#alerts > *')].map((b) => b.innerText.replace(/\s+/g, ' ').trim()).join(' || ');
    return { me, keys, by, fleets, air, akeys, tabs, alerts, hint: document.querySelector('#hint')?.innerText, cap: R.find((r) => r.id === g.nations[me].capital)?.name };
  });
  console.log(JSON.stringify(out, null, 0).replace(/],/g, '],\n').slice(0, 4000));
  await browser.close();
})();

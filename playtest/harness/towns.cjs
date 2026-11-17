// The towns of a country as the page words them: kind, guard, corps there. usage: node towns.cjs ME THEM [SEED]
const { chromium } = require('playwright');
const [me = 'USA', them = 'MEX', seedS = '25'] = process.argv.slice(2);
(async () => { const b = await chromium.launch(); const p = await (await b.newContext({ viewport: { width: 1440, height: 900 } })).newPage(); await p.goto(process.env.TC_PAGE || 'file:///home/user/tc84/dist/play.html'); await p.waitForSelector('#card');
  await p.evaluate(({ me, seed }) => window.theater.newGame(me, seed, 1, 'grand', 'colonel'), { me, seed: Number(seedS) }); await p.waitForTimeout(2000);
  for (let i = 0; i < 5; i++) await p.evaluate(() => { const v = document.querySelector('#veil'); if (v && v.classList.contains('open')) window.theater.openModal(null); });
  const ids = await p.evaluate((them) => window.theater.game().regions.filter((r) => r.owner === them).map((r) => r.id), them);
  const out = [];
  for (const id of ids) { await p.evaluate((id) => window.theater.onRegion(id), id); await p.waitForTimeout(120); out.push(await p.evaluate((id) => { const g = window.theater.game(), r = g.regions[id]; const t = (document.querySelector('#pane').innerText || '').replace(/\s+/g, ' '); const kind = (t.match(new RegExp(r.name + ' · (Village|Town|City|Capital)')) || [])[1] || '?'; const guard = (t.match(/TOWN GUARD ([^ ]+( div)?)/) || [])[1] || '?'; return `${r.name}: ${kind}, guard ${guard}, corps ${r.units.filter((c) => !c.dead).length}; next to ${r.nb.map((j) => g.regions[j].name).join('/')}`; }, id)); }
  console.log(out.join('\n')); await b.close(); })();

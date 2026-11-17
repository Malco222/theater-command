// Brian's five problems of 2026-11-17 (found as America in the 3D game), read on the browser page: a new campaign as
// the United States from September 1939. usage: [TC_PAGE=...] node ui64.cjs PREFIX [NAT=USA] [SEED=25]
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc84/dist/play.html';
const [prefix, nat = 'USA', seedS = '25'] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 300)));
  await page.goto(PAGE); await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); localStorage.setItem('tc-ask-end', '0'); }); await page.reload(); await page.waitForSelector('#card');
  await page.evaluate(({ nat, seed }) => window.theater.newGame(nat, seed, 1, 'grand', 'colonel', true, false, 'theatre', 3), { nat, seed: Number(seedS) }); await page.waitForTimeout(2500);
  const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  const closeAll = async () => { for (let i = 0; i < 5 && await veil(); i++) { await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300); } };
  await closeAll();
  const facts = await page.evaluate(() => {
    const g = window.theater.game(), me = g.human; const R = g.regions;
    const air = (g.airFleets || []).filter((a) => a.nat === me && !a.dead).map((a) => `${a.name} (${a.kind || a.type || '?'}, ${a.planes ?? a.str ?? '?'}) at ${a.carrier != null || a.fleet != null ? 'a carrier' : (R[a.reg ?? a.base]?.name ?? '?')}`);
    const fields = R.filter((r) => r.ctrl === me && (r.airfield || r.air || (r.works && r.works.airfield))).map((r) => `${r.name} (level ${r.airfield ?? r.air ?? r.works.airfield})`);
    const pan = R.filter((r) => /Panama|Canal/i.test(r.name)).map((r) => ({ id: r.id, name: r.name, owner: r.owner, ctrl: r.ctrl, units: r.units.filter((c) => !c.dead).map((c) => `${c.nat} ${c.name} [${c.kind}] ap ${c.ap}/${c.apMax}${c.hold ? ' hold' : ''}${c.garrison ? ' garrison' : ''}${c.fixed ? ' fixed' : ''}${c.static ? ' static' : ''}`), nb: r.nb.map((j) => `${R[j].name} (${R[j].ctrl})`) }));
    return { me, turn: g.turn, air, fields: fields.slice(0, 12), nFields: fields.length, pan, gates: (g.gates || []).map((x) => JSON.stringify(x)).slice(0, 6), keys: Object.keys(R.find((r) => r.ctrl === me)).join(',') };
  });
  console.log(`seat ${facts.me} turn ${facts.turn}`);
  console.log(`1) my air groups: ${facts.air.length ? facts.air.join('; ') : 'NONE'}`);
  console.log(`   my provinces with an airfield: ${facts.nFields}: ${facts.fields.join(', ')}`);
  console.log(`   region keys: ${facts.keys}`);
  console.log(`4) Panama: ${JSON.stringify(facts.pan).slice(0, 1200)}`);
  console.log(`5) gates: ${facts.gates.join(' ').slice(0, 600)}`);
  console.log(`page errors: ${errors.length}${errors.length ? ' ' + JSON.stringify(errors.slice(0, 3)) : ''}`);
  await browser.close();
})();

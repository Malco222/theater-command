// Writes a seat's click-through step list from what that seat holds as the campaign opens: every army, fleet and air
// group once, every page and every button on it, the province cards, three months ended.
// usage: [TC_PAGE=...] node mksteps.cjs NAT [YYYY-MM=1939-09] [SEED=25] > NAT-run-1.steps.txt
// The format: /mnt/project-files/playtest/click-through/README.md. Played by click.cjs on the browser page.
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc84/dist/play.html';
const [nat = 'USA', when = '1939-09', seedS = '25'] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch();
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 } })).newPage();
  await page.goto(PAGE); await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); }); await page.reload(); await page.waitForSelector('#card');
  const [y, m] = when.split('-').map(Number); const from = (y - 1939) * 12 + (m - 9) + 1;
  await page.evaluate(({ nat, seed, from }) => window.theater.newGame(nat, seed, from, 'grand', 'colonel'), { nat, seed: Number(seedS), from }); await page.waitForTimeout(2500);
  const f = await page.evaluate(() => {
    const g = window.theater.game(), me = g.human, R = g.regions, n = g.nations[me];
    const war = (x) => n.atWar.includes(x); const friend = (x) => x === me;
    const armies = R.filter((r) => r.units.some((c) => c.nat === me && !c.dead)).map((r) => { const mine = r.units.filter((c) => c.nat === me && !c.dead); const nb = r.nb.map((j) => R[j]); const foeHeld = nb.filter((t) => war(t.ctrl)); return { name: r.name, kinds: [...new Set(mine.map((c) => c.kind))], fort: mine.every((c) => c.kind === 'fort'), own: (nb.find((t) => friend(t.ctrl) && !t.units.some((c) => !c.dead)) || nb.find((t) => friend(t.ctrl)) || {}).name || '', neutral: (nb.find((t) => !friend(t.ctrl) && !war(t.ctrl)) || {}).name || '', foe: (foeHeld.find((t) => t.units.some((c) => !c.dead && war(c.nat))) || {}).name || '', empty: (foeHeld.find((t) => !t.units.some((c) => !c.dead)) || {}).name || '', n: mine.length }; });
    const fleets = g.fleets.filter((x) => x.nat === me && !x.dead).map((x) => ({ name: x.name, sea: g.seas[x.zone] ? g.seas[x.zone].name : '', next: g.seas[x.zone] ? (g.seas[x.zone].adj || []).map((z) => g.seas[z] && g.seas[z].name).filter(Boolean).slice(0, 2) : [] }));
    const groups = (g.airFleets || []).filter((x) => x.nat === me && !x.dead && ((x.ftr || 0) + (x.bmr || 0)) > 0).map((x) => ({ name: x.name, wing: x.ship !== undefined && x.ship !== null, base: x.base != null && R[x.base] ? R[x.base].name : '' }));
    const cap = R[n.capital] ? R[n.capital].name : ''; const port = (R.find((r) => r.ctrl === me && r.port && r.id !== n.capital) || {}).name || '';
    const gates = g.gates.map((x) => ({ name: x.name.replace(/^the /, ''), mine: x.region >= 0 && R[x.region].ctrl === me }));
    const tabs = [...document.querySelectorAll('#tabs [data-tab]')].map((e) => e.dataset.tab);
    return { me, name: n.name, armies, fleets, groups, cap, port, gates, tabs, wars: n.atWar };
  });
  const out = []; const p = (...a) => out.push(a.join('\t'));
  out.push(`# ${f.name}, run 1: ${when}, ${f.wars.length ? 'at war with ' + f.wars.join(', ') : 'at peace'}. Step file version 1 (see README.md). Written by mksteps.cjs from what the seat holds.`);
  p('start', `${nat} ${when} seed ${seedS} colonel`, 'does'); p('snapshot', 'start');
  out.push('# --- A. Every army once: a march on its own ground, a neutral frontier, enemy ground with and without an army in it.');
  const moving = f.armies.filter((a) => !a.fort); const forts = f.armies.filter((a) => a.fort);
  const pickSome = (list, n) => { const seen = new Set(); const o = []; for (const a of list) { const k = a.kinds.join('+'); if (seen.has(k) && o.length >= 3) continue; seen.add(k); o.push(a); if (o.length >= n) break; } return o; };
  for (const a of pickSome(moving, 6)) { p('select-province', a.name, 'does'); if (a.own) p('order-province', a.own, 'moves'); p('press-each', 'tray'); p('restore', 'start'); }
  const front = moving.find((a) => a.foe); if (front) { p('select-province', front.name, 'does'); p('order-province', front.foe, 'card'); p('press-each', 'card'); p('close'); p('restore', 'start'); }
  const walk = moving.find((a) => a.empty); if (walk) { p('select-province', walk.name, 'does'); p('order-province', walk.empty, 'does'); p('check-province', walk.empty); p('restore', 'start'); }
  const edge = moving.find((a) => a.neutral); if (edge) { p('select-province', edge.name, 'does'); p('order-province', edge.neutral, 'card'); p('press-each', 'card'); p('close'); p('restore', 'start'); }
  for (const a of forts.slice(0, 2)) { out.push('# a fortress garrison: it moves, or the game says it never marches'); p('select-province', a.name, 'does'); if (a.own || a.neutral) p('order-province', a.own || a.neutral, 'says-why'); }
  out.push('# --- B. Every fleet once: a right click on a sea, then a click on a sea next door.');
  for (const fl of f.fleets.slice(0, 4)) { p('select-unit', fl.name, 'does'); if (fl.sea) p('order-sea', fl.sea, 'says-why'); fl.next.forEach((z, i) => p('select-sea', z, i === 0 ? 'moves' : '')); p('press-each', 'tray'); p('restore', 'start'); }
  out.push('# --- C. Every air group once.');
  if (f.groups.length) p('open-page', 'Air', 'does');
  for (const a of f.groups.slice(0, 4)) { p('select-unit', a.name, 'does'); if (a.wing && f.port) p('order-province', f.port, 'does'); p('press-each', 'page'); p('restore', 'start'); p('open-page', 'Air'); }
  for (const gt of f.gates.filter((x) => x.mine)) { out.push('# a gate he holds: it can be closed and opened again'); p('point-at', `gate ${gt.name}`, 'says-why'); p('press', `Close ${gt.name.replace(/^the /i, 'the ')}`, 'does'); }
  out.push('# --- D. Every page, and every button on it.');
  const label = { build: 'Build', research: 'Research', generals: 'Generals', honours: 'Honours', navy: 'Navy', air: 'Air', spies: 'Spies', reel: 'News', battles: 'Battles', powers: 'Powers', staff: 'Staff' };
  for (const t of f.tabs) { if (!label[t]) continue; p('open-page', label[t], t === f.tabs[0] ? '' : 'does'); p('press-each', 'page'); p('restore', 'start'); }
  for (const b of ['CABINET', 'ABILITIES', 'DECLARE WAR', 'Treasury']) { p('press', b, 'card'); p('press-each', 'card'); p('close'); p('restore', 'start'); }
  out.push('# --- E. Province cards: the capital, a port, a neighbour.');
  for (const name of [f.cap, f.port, (moving.find((a) => a.neutral) || {}).neutral].filter(Boolean)) { p('select-province', name, 'does'); p('press-each', 'page'); p('restore', 'start'); }
  out.push('# --- F. Three months ended by the game\'s own button, the month card read each time.');
  p('end-month', '', 'card'); p('press-each', 'card'); p('close'); p('end-month', '', 'card'); p('close'); p('end-month', '', 'card');
  console.log(out.join('\n'));
  await browser.close();
})();

// A saved game from a headless campaign (probe88, TC_SAVE) opened on the page, to look at a moment as the player sees
// it. usage: [TC_PAGE=...] node ui57.cjs SAVE.json PREFIX ACTION...   actions: raid:Lille (the raid card for a bombing
// raid on that target), army:Riviera (take the army there in hand: the tray, the rail line), lamps (cut lines on the
// map), build:Ruhr (the BUILD tab's works for that province), card (the month card), tabs, quays (where a landing can
// start), land:QUAY>BEACH[>N] (the landing card), guard:REGION (the railway guard signs with an army in hand), zero
const fs = require('fs');
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc823/dist/play.html';
const [save, prefix, ...acts] = process.argv.slice(2);
const clean = (s) => (s || '').replace(/\s+/g, ' ').trim();
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 300)));
  await page.goto(PAGE);
  await page.evaluate((text) => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', text); }, fs.readFileSync(save, 'utf8'));
  await page.reload(); await page.waitForSelector('#card');
  const cont = await page.evaluate(() => { try { window.theater.continueGame(); return 'continued'; } catch (e) { return 'threw ' + e.message; } });
  await page.waitForTimeout(2500);
  const st = await page.evaluate(() => { const g = window.theater.game(); return g ? `${g.human} turn ${g.turn}` : 'no game'; });
  console.log(`the save: ${cont}; ${st}`);
  const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  const cardText = () => page.evaluate(() => (document.querySelector('#card').innerText || '').replace(/\s+/g, ' ').trim());
  if (await veil()) { console.log('the card on opening: ' + (await cardText()).slice(0, 700)); await page.screenshot({ path: `shots/${prefix}-0-open.png` }); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(400); }
  const idOf = (name) => page.evaluate((name) => { const r = window.theater.game().regions.find((x) => x.name === name); return r ? r.id : -1; }, name);
  let n = 0;
  for (const a of acts) {
    n++; const [what, arg] = a.split(':');
    if (what === 'tabs') console.log('tabs: ' + JSON.stringify(await page.evaluate(() => [...document.querySelectorAll('[data-act="tab"]')].map((b) => b.dataset.tab))));
    if (what === 'raid') {
      await page.evaluate(() => document.querySelector('[data-act="tab"][data-tab="air"]')?.click()); await page.waitForTimeout(400);
      const menu = await page.evaluate(() => { const b = document.querySelector('[data-act="raidmenu"]'); if (!b) return null; b.click(); return (b.innerText || '').trim(); }); await page.waitForTimeout(400);
      if (!menu) { console.log(`raid ${arg}: no Plan a raid button; the pane: ` + clean(await page.evaluate(() => document.querySelector('#pane')?.innerText || '')).slice(0, 300)); continue; }
      await page.evaluate(() => document.querySelector('[data-act="raidkind"][data-kind="bomb"]')?.click()); await page.waitForTimeout(400);
      const hit = await page.evaluate((arg) => { const bs = [...document.querySelectorAll('[data-act="raidtarget"]')]; const b = bs.find((x) => (x.innerText || '').trim().startsWith(arg)); const all = bs.slice(0, 14).map((x) => (x.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 90)); if (b) b.click(); return { found: !!b, all }; }, arg); await page.waitForTimeout(600);
      console.log(`raid ${arg}: targets on the list: ${JSON.stringify(hit.all)}`);
      if (!hit.found) continue;
      console.log(`raid ${arg}: in common: ` + JSON.stringify(await page.evaluate(() => [...document.querySelectorAll('.waycommon')].map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim()))));
      console.log(`raid ${arg}: the two columns: ` + JSON.stringify(await page.evaluate(() => [...document.querySelectorAll('.daynight .way')].map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim()))));
      console.log(`raid ${arg}: the whole card: ` + (await cardText()).slice(0, 1500));
      await page.screenshot({ path: `shots/${prefix}-${n}-raid-${arg.replace(/\W+/g, '')}.png` });
      await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300);
    }
    if (what === 'army') {
      const id = await idOf(arg); await page.evaluate(() => document.querySelector('[data-act="tab"][data-tab="region"]')?.click()); await page.evaluate((id) => window.theater.onRegion(id), id); await page.waitForTimeout(700);
      console.log(`army ${arg}: the rail line: ` + JSON.stringify(await page.evaluate(() => [...document.querySelectorAll('.railsay')].map((e) => `${e.className}: ${(e.innerText || '').replace(/\s+/g, ' ').trim()}`))));
      console.log(`army ${arg}: the tray: ` + clean(await page.evaluate(() => document.querySelector('#tray')?.innerText || '')).slice(0, 500));
      console.log(`army ${arg}: marks: ` + JSON.stringify(await page.evaluate(() => { const by = {}; for (const e of document.querySelectorAll('#map .mk')) { const k = e.getAttribute('class'); by[k] = (by[k] || 0) + 1; } return by; })));
      console.log(`army ${arg}: the panel's railway rows: ` + JSON.stringify(await page.evaluate(() => [...document.querySelectorAll('#pane .rrow, #pane p')].map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim()).filter((t) => /railway|train/i.test(t) && t.length < 300).slice(0, 4))));
      await page.screenshot({ path: `shots/${prefix}-${n}-army-${arg.replace(/\W+/g, '')}.png` });
    }
    if (what === 'lamps') {
      console.log('lamps: ' + JSON.stringify(await page.evaluate(() => ({ lamps: document.querySelectorAll('#map .raillamp').length, hollow: document.querySelectorAll('#map .raillamp.old').length, bars: document.querySelectorAll('#map .railstop').length, legend: (document.querySelector('#legrailcut')?.innerText || '').trim() }))));
      for (let i = 0; i < 2; i++) { await page.evaluate(() => document.querySelector('[data-act="zout"]')?.click()); await page.waitForTimeout(300); }
      await page.screenshot({ path: `shots/${prefix}-${n}-lamps.png` });
    }
    if (what === 'build') {
      const id = await idOf(arg); await page.evaluate((id) => window.theater.onRegion(id), id); await page.waitForTimeout(400);
      await page.evaluate(() => document.querySelector('[data-act="tab"][data-tab="build"]')?.click()); await page.waitForTimeout(500);
      console.log(`build ${arg}: ` + JSON.stringify(await page.evaluate(() => [...document.querySelectorAll('#pane *')].filter((e) => e.children.length < 8 && /anti-aircraft|artillery corps/i.test(e.innerText || '') && (e.innerText || '').length < 420).map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim()).filter((t, i, a) => a.indexOf(t) === i).slice(0, 4))));
      await page.screenshot({ path: `shots/${prefix}-${n}-build-${arg.replace(/\W+/g, '')}.png` });
    }
    if (what === 'card') {
      await page.keyboard.press('r'); await page.waitForTimeout(700);
      if (await veil()) { console.log('the month card: ' + (await cardText()).slice(0, 1800)); await page.screenshot({ path: `shots/${prefix}-${n}-card.png` }); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300); } else console.log('the month card: R opened nothing');
    }
    if (what === 'quays') {
      // Where the player's corps stand in a port, and the enemy's coastal home provinces: what a landing can start from and aim at.
      console.log('quays: ' + JSON.stringify(await page.evaluate(() => { const g = window.theater.game(); const me = g.human; const by = {}; for (const c of g.corps) if (c.nat === me && !c.dead && c.kind !== 'fort') { const r = g.regions[c.reg]; if (r.port) by[r.name] = (by[r.name] || 0) + 1; } const foes = g.nations[me].atWar.filter((e) => g.nations[e].alive); const beaches = g.regions.filter((r) => foes.includes(r.ctrl) && r.coastal !== false && (r.port || r.coast || r.sea !== undefined)).map((r) => `${r.name} (${r.ctrl})`).slice(0, 60); return { me, turn: g.turn, lift: g.nations[me].sealift, quays: by, foes: foes.join(','), beaches }; })));
    }
    if (what === 'lands') {
      // lands:QUAY[>N]: every beach the corps on that quay can be aimed at, with the landing card's head for each.
      const [qn, nn] = arg.split('>'); const q = await idOf(qn);
      const pickThem = async () => { await page.evaluate((q) => { window.theater.ui.picked.clear(); window.theater.onRegion(q); }, q); await page.waitForTimeout(300); await page.evaluate(([q, n]) => { const g = window.theater.game(); const ids = g.corps.filter((c) => c.nat === g.human && !c.dead && c.reg === q && c.kind !== 'fort' && c.kind !== 'eng').slice(0, n || 99).map((c) => c.id); window.theater.ui.picked.clear(); for (const id of ids) window.theater.ui.picked.add(id); window.theater.refresh(); }, [q, Number(nn || 0)]); await page.waitForTimeout(300); };
      await page.evaluate(() => document.querySelector('[data-act="tab"][data-tab="region"]')?.click()); await pickThem();
      if (!(await page.locator('[data-act="land"]').count())) { console.log(`lands ${arg}: no Invade by sea button with ${await page.evaluate(() => [...window.theater.ui.picked].length)} corps picked`); continue; }
      const beaches = await page.evaluate(() => { const g = window.theater.game(); const foes = g.nations[g.human].atWar.filter((e) => g.nations[e].alive); return g.regions.filter((r) => foes.includes(r.ctrl) && r.seas && r.seas.length).map((r) => r.id); });
      const seen = [];
      for (const b of beaches) {
        await pickThem(); const inv = page.locator('[data-act="land"]').first(); if (!(await inv.count())) break; await inv.click(); await page.waitForTimeout(200);
        await page.evaluate((b) => window.theater.onRegion(b), b); await page.waitForTimeout(450);
        if (await veil()) { seen.push(await page.evaluate((b) => { const c = document.querySelector('#card'); const txt = (e) => (e ? e.textContent.replace(/\s+/g, ' ').trim() : ''); return `${window.theater.game().regions[b].name}: ${txt(c.querySelector('.verdict'))} | ${[...c.querySelectorAll('.note.ashore')].map(txt).join(' ').slice(0, 230)}`; }, b)); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(150); }
        await page.keyboard.press('Escape'); await page.waitForTimeout(120);
      }
      console.log(`lands ${arg}: ${seen.length} beaches of ${beaches.length} open a card:\n   ` + seen.join('\n   '));
    }
    if (what === 'land') {
      // land:QUAY>BEACH[>N]: the landing card for the corps on that quay (all of them, or the first N) aimed at that beach, as a player orders it.
      const [qn, bn, nn] = arg.split('>'); const q = await idOf(qn), b = await idOf(bn);
      await page.evaluate(() => document.querySelector('[data-act="tab"][data-tab="region"]')?.click()); await page.evaluate((q) => window.theater.onRegion(q), q); await page.waitForTimeout(500);
      if (nn) await page.evaluate(([q, n]) => { const g = window.theater.game(); const ids = g.corps.filter((c) => c.nat === g.human && !c.dead && c.reg === q && c.kind !== 'fort' && c.kind !== 'eng').slice(0, n).map((c) => c.id); window.theater.ui.picked.clear(); for (const id of ids) window.theater.ui.picked.add(id); window.theater.refresh(); }, [q, Number(nn)]);
      else { const all = page.locator('[data-act="pickall"]').first(); if (await all.count()) await all.click(); }
      await page.waitForTimeout(400);
      console.log(`land ${arg}: corps picked ${await page.evaluate(() => [...window.theater.ui.picked].length)}; orders offered: ` + JSON.stringify(await page.evaluate(() => [...document.querySelectorAll('.acts button')].filter((x) => x.offsetParent !== null).map((x) => x.textContent.trim()))));
      const inv = page.locator('[data-act="land"]').first();
      if (!(await inv.count())) { console.log(`land ${arg}: no Invade by sea button; the page says: ` + (await page.evaluate(() => window.theater.ui.msg))); continue; }
      await inv.click(); await page.waitForTimeout(400); await page.evaluate((b) => window.theater.onRegion(b), b); await page.waitForTimeout(800);
      if (!(await veil())) { console.log(`land ${arg}: no landing card; the page says: ` + (await page.evaluate(() => window.theater.ui.msg))); await page.screenshot({ path: `shots/${prefix}-${n}-land-refused.png` }); continue; }
      console.log(`land ${arg}: the head: ` + JSON.stringify(await page.evaluate(() => { const c = document.querySelector('#card'); const txt = (e) => (e ? e.textContent.replace(/\s+/g, ' ').trim() : ''); const v = c.querySelector('.verdict'); const col = (e) => (e ? getComputedStyle(e).color : ''); return { title: txt(c.querySelector('h2')), verdict: txt(v), verdictClass: v ? v.className : '', verdictColour: col(v), under: [...c.querySelectorAll('.holdwarn')].map(txt), fight: txt(c.querySelector('h3.fighthead')), fightline: txt(c.querySelector('.fightline')), ashore: [...c.querySelectorAll('.note.ashore')].map((e) => `${e.className}: ${txt(e)}`), tickets: [...c.querySelectorAll('h4.crh')].map((h) => `${txt(h)}${h.classList.contains('bad') ? ' [bad]' : ''}`) }; })));
      console.log(`land ${arg}: the whole card: ` + (await cardText()).slice(0, 2200));
      await page.screenshot({ path: `shots/${prefix}-${n}-land-${bn.replace(/\W+/g, '')}.png` });
      await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300); await page.keyboard.press('Escape'); await page.waitForTimeout(300);
    }
    if (what === 'guard') {
      // guard:REGION: the signs of the guns that guard a railway (Version 83.1): none at rest, shown with an army in hand, gone when it is put down.
      const tabs = () => page.evaluate(() => { const g = window.theater.game(); const all = [...document.querySelectorAll('#map .gtab')]; return { signs: all.length, gaps: all.filter((e) => e.classList.contains('gap')).length, where: all.slice(0, 14).map((e) => `${g.regions[Number(e.dataset.region)]?.name} ${e.dataset.level}`) }; });
      await page.evaluate(() => { window.theater.ui.picked.clear(); window.theater.refresh(); }); await page.waitForTimeout(300);
      console.log(`guard ${arg}: at rest: ` + JSON.stringify(await tabs()));
      let id = await idOf(arg);
      if (arg === 'auto') {
        // The first of the player's armies that the page shows guard signs for when it is taken in hand.
        const mine = await page.evaluate(() => { const g = window.theater.game(); const by = {}; for (const c of g.corps) if (c.nat === g.human && !c.dead && c.kind !== 'fort') by[c.reg] = (by[c.reg] || 0) + 1; return Object.keys(by).map(Number); });
        let tried = 0;
        for (const r of mine.slice(0, 40)) { tried++; await page.evaluate((r) => { window.theater.ui.picked.clear(); window.theater.onRegion(r); }, r); await page.waitForTimeout(250); if ((await tabs()).signs) { id = r; break; } await page.keyboard.press('Escape'); await page.waitForTimeout(150); }
        console.log(`guard auto: provinces with my corps ${mine.length}, tried ${tried}, signs first at ${id >= 0 ? await page.evaluate((id) => window.theater.game().regions[id].name, id) : 'none'}`);
        await page.keyboard.press('Escape'); await page.waitForTimeout(300);
        if (id < 0) continue;
      }
      await page.evaluate(() => document.querySelector('[data-act="tab"][data-tab="region"]')?.click()); await page.evaluate((id) => window.theater.onRegion(id), id); await page.waitForTimeout(500);
      let picked = await page.evaluate(() => [...window.theater.ui.picked].length);
      if (!picked) { const all = page.locator('[data-act="pickall"]').first(); if (await all.count()) await all.click(); await page.waitForTimeout(400); picked = await page.evaluate(() => [...window.theater.ui.picked].length); }
      console.log(`guard ${arg}: with ${picked} corps in hand: ` + JSON.stringify(await tabs()));
      console.log(`guard ${arg}: the rail line: ` + JSON.stringify(await page.evaluate(() => [...document.querySelectorAll('.railsay')].map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim()))));
      await page.screenshot({ path: `shots/${prefix}-${n}-guard-${arg.replace(/\W+/g, '')}.png` });
      await page.keyboard.press('Escape'); await page.waitForTimeout(500);
      console.log(`guard ${arg}: put down (Escape), corps in hand ${await page.evaluate(() => [...window.theater.ui.picked].length)}: ` + JSON.stringify(await tabs()));
    }
    if (what === 'zero') console.log('any "0 km" on the page: ' + (await page.evaluate(() => /(^|[^\d.,])0 km/.test(document.body.innerText || ''))));
    if (what === 'news') {
      const g = await page.evaluate((arg) => { const g = window.theater.game(); const re = new RegExp(arg || 'bomb|raid|railway|guns', 'i'); return g.headlines.filter((h) => h.turn >= g.turn - 1 && re.test(h.text)).map((h) => `t${h.turn} [${h.kind}] ${h.text.slice(0, 220)}`).slice(0, 14); }, arg);
      console.log('news: ' + JSON.stringify(g, null, 1));
    }
  }
  console.log('page errors: ' + JSON.stringify(errors));
  await browser.close();
})();

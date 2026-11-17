// Version 84: the attack card's WHAT YOU FACE block and the result's who-paid line, by hand on the page.
// usage: [TC_PAGE=...] node ui61.cjs SAVE.json PREFIX [N=4] [tanks]
// Opens the save, and N times: takes an army in hand beside an enemy province, gives the order with a right-click on
// the map as a player does, reads the attack card (What you face, the ticket's rows, the odds), presses Auto-resolve
// and reads the result card (the losses' sentence and the line of who paid). With "tanks", armies with tank corps
// in them go first. Prints the weapon signs on the corps cards in the tray too. Pictures in shots/PREFIX-*.png.
const fs = require('fs');
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc84/dist/play.html';
const [save, prefix, nS, pref] = process.argv.slice(2);
const N = Number(nS || 4);
const clean = (s) => (s || '').replace(/\s+/g, ' ').trim();
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 300)));
  await page.goto(PAGE);
  await page.evaluate((text) => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', text); }, fs.readFileSync(save, 'utf8'));
  await page.reload(); await page.waitForSelector('#card');
  await page.evaluate(() => { try { window.theater.continueGame(); } catch (e) {} });
  await page.waitForTimeout(2500);
  console.log('the save: ' + await page.evaluate(() => { const g = window.theater.game(); return g ? `${g.human} turn ${g.turn}` : 'no game'; }));
  const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  const closeAll = async () => { for (let i = 0; i < 4 && await veil(); i++) { await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300); } };
  await closeAll();
  const done = new Set();
  for (let k = 0; k < N; k++) {
    // an army of mine beside an enemy province, as the game's own state has it
    const pick = await page.evaluate(({ done, tanksFirst }) => {
      const g = window.theater.game(), me = g.human; const out = [];
      const war = (a, b) => { const w = g.wars || []; return w.some((x) => (x[0] === a && x[1] === b) || (x[0] === b && x[1] === a)) || (g.nations[a] && g.nations[a].war && g.nations[a].war.includes && g.nations[a].war.includes(b)); };
      for (const r of g.regions) {
        const mine = r.units.filter((c) => c.nat === me && !c.dead && c.kind !== 'fort' && (c.ap === undefined || c.ap > 0));
        if (!mine.length) continue;
        for (const j of r.nb) { const t = g.regions[j]; if (done.includes(`${r.id}>${j}`)) continue; const foes = t.units.filter((c) => !c.dead && c.nat !== me && t.ctrl !== me && t.ctrl === c.nat); if (!foes.length) continue; out.push({ from: r.id, to: j, fromName: r.name, toName: t.name, tanks: mine.filter((c) => c.kind === 'arm').length, n: mine.length, foes: foes.length, foe: t.ctrl }); }
      }
      out.sort((a, b) => (tanksFirst ? b.tanks - a.tanks : 0) || b.n - a.n);
      return out.slice(0, 12);
    }, { done: [...done], tanksFirst: pref === 'tanks' });
    if (!pick.length) { console.log('no army beside an enemy province is left'); break; }
    let opened = false;
    for (const p of pick) {
      done.add(`${p.from}>${p.to}`);
      await page.evaluate((id) => window.theater.onRegion(id), p.from); await page.waitForTimeout(400);
      // bring the target to the middle of the map, as a player drags it there
      await page.evaluate((id) => { const t = window.theater, r = t.game().regions[id], el = document.querySelector('#map'); const w = Math.min(t.ui.view.w, 620), tall = el.clientHeight / Math.max(1, el.clientWidth); t.ui.view.x = r.x - w / 2; t.ui.view.y = r.y - w * tall * 0.45; t.ui.view.w = w; t.applyView(); }, p.to); await page.waitForTimeout(500);
      // the target province on screen: try points inside its outline until the map itself answers with that province
      const pt = await page.evaluate((id) => { const el = document.querySelector(`#regs .reg[data-id="${id}"]`); if (!el) return null; const b = el.getBoundingClientRect(); for (const [fx, fy] of [[0.5, 0.5], [0.35, 0.35], [0.65, 0.65], [0.35, 0.65], [0.65, 0.35], [0.5, 0.25], [0.5, 0.75], [0.25, 0.5], [0.75, 0.5]]) { const x = b.x + b.width * fx, y = b.y + b.height * fy; if (x < 10 || y < 130 || x > 1430 || y > 890) continue; const hit = document.elementFromPoint(x, y); const reg = hit && hit.closest('.reg'); if (reg && Number(reg.dataset.id) === id) return { x, y }; } return null; }, p.to);
      if (!pt) continue;
      if (k === 0 && !opened) { const signs = await page.evaluate(() => [...document.querySelectorAll('#tray .wsign, #pane .wsign')].length); const titles = await page.evaluate(() => [...new Set([...document.querySelectorAll('#tray [title], #pane [title]')].map((e) => e.getAttribute('title')).filter((t) => /anti-tank|machine gun|mortar|sniper|guns?\b/i.test(t || '')).map((t) => t.slice(0, 110)))].slice(0, 8)); console.log(`corps cards in hand at ${p.fromName}: ${signs} weapon signs; their hover words: ${JSON.stringify(titles)}`); await page.screenshot({ path: `shots/${prefix}-0-army.png` }); }
      await page.mouse.click(pt.x, pt.y, { button: 'right' }); await page.waitForTimeout(700);
      if (!(await veil())) continue;
      const card = await page.evaluate(() => { const c = document.querySelector('#card'); const t = (e) => (e ? e.innerText.replace(/\s+/g, ' ').trim() : ''); return { head: t(c.querySelector('h2, header')), face: [...c.querySelectorAll('.facepart .tc-signline')].map((e) => `${e.classList.contains('is-unknown') ? '(not scouted) ' : ''}${t(e)}`), hasBlock: !!c.querySelector('.facepart'), rows: [...c.querySelectorAll('.tline, .ticket tr, .trow')].map(t).filter((s) => /Tanks alone|Who pays|Machine guns|Minefields|Dug in|Anti-tank/i.test(s)).slice(0, 8), odds: t(c.querySelector('.odds, .oddsline, .outlook')), go: !!c.querySelector('[data-act="attack"]:not([disabled])'), all: t(c).slice(0, 900) }; });
      if (!card.hasBlock && !/Auto-resolve/.test(card.all)) { await closeAll(); continue; }
      opened = true;
      console.log(`\nATTACK ${k + 1}: ${p.fromName} (${p.n} corps, ${p.tanks} of tanks) on ${p.toName} (${p.foe})`);
      console.log(`  WHAT YOU FACE: ${card.face.length ? card.face.join(' | ') : (card.hasBlock ? 'block with no line' : 'NO BLOCK')}`);
      if (card.rows.length) console.log(`  ticket rows: ${card.rows.join(' | ')}`);
      if (!card.face.length) console.log(`  the card: ${card.all}`);
      await page.screenshot({ path: `shots/${prefix}-${k + 1}a-card.png` });
      // where the block stands on a 1440 by 900 screen: in view when the card opens, or below the fold
      const where = await page.evaluate(() => { const f = document.querySelector('#card .facepart'); const go = document.querySelector('#card [data-act="attack"]'); if (!f) return null; const b = f.getBoundingClientRect(), gb = go ? go.getBoundingClientRect() : null; const sc = [...document.querySelectorAll('#card, #card *')].find((e) => e.scrollHeight > e.clientHeight + 20 && /auto|scroll/.test(getComputedStyle(e).overflowY)); return { top: Math.round(b.top), bottom: Math.round(b.bottom), vh: window.innerHeight, goTop: gb ? Math.round(gb.top) : null, scrollH: sc ? sc.scrollHeight : 0, clientH: sc ? sc.clientHeight : 0 }; });
      if (where) console.log(`  the block on a ${where.vh} high screen: from ${where.top} to ${where.bottom} (${where.top < where.vh - 40 ? 'in view' : 'BELOW THE FOLD'}); the Auto-resolve button at ${where.goTop}; the card is ${where.scrollH} high in a window of ${where.clientH}`);
      await page.evaluate(() => { const f = document.querySelector('#card .facepart'); if (f) f.scrollIntoView({ block: 'center' }); }); await page.waitForTimeout(300);
      await page.screenshot({ path: `shots/${prefix}-${k + 1}a2-face.png` });
      // "alone": hold every corps but the tanks back with the card's own corps buttons, and read the card again
      if (process.argv.slice(5).includes('alone') || pref === 'alone') {
        let held = 0;
        for (let i = 0; i < 14; i++) {
          const did = await page.evaluate(() => { const g = window.theater.game(); const b = [...document.querySelectorAll('#card .ucard.pick[data-act="atkpick"]')].find((x) => { const c = g.corps.find((u) => String(u.id) === x.dataset.id); return c && c.kind !== 'arm'; }); if (!b) return false; b.click(); return true; });
          if (!did) break; held++; await page.waitForTimeout(250);
        }
        const lone = await page.evaluate(() => { const c = document.querySelector('#card'); const t = (e) => (e ? e.innerText.replace(/\s+/g, ' ').trim() : ''); const all = t(c); const m = all.match(/TANKS ALONE[^A-Z]{0,4}.{0,200}/i); return { inNow: c.querySelectorAll('.ucard.pick[data-act="atkpick"]').length, face: [...c.querySelectorAll('.facepart .tc-signline')].map((e) => `${e.classList.contains('is-unknown') ? '(not scouted) ' : ''}${t(e)}`), row: m ? m[0].slice(0, 230) : 'no "Tanks alone" row', who: (all.match(/WHO PAYS.{0,120}/i) || [''])[0], odds: (all.match(/(ODDS|Odds)[^.]{0,80}/) || [''])[0], go: !!c.querySelector('[data-act="attack"]:not([disabled])') }; });
        console.log(`  TANKS ALONE (${held} corps held back, ${lone.inNow} in): WHAT YOU FACE: ${lone.face.join(' | ') || 'no block'}`);
        console.log(`    ticket: ${lone.row} | ${lone.who} | ${lone.odds}`);
        await page.evaluate(() => { const f = document.querySelector('#card .facepart'); if (f) f.scrollIntoView({ block: 'center' }); }); await page.waitForTimeout(300);
        await page.screenshot({ path: `shots/${prefix}-${k + 1}a4-alone.png` });
        card.go = lone.go;
      }
      // "scout" or "probe": press the card's own button for what is not known, and read the block again
      for (const way of ['scoutnow', 'probe']) {
        if (!(process.argv.slice(5).includes(way === 'scoutnow' ? 'scout' : 'probe') || pref === (way === 'scoutnow' ? 'scout' : 'probe'))) continue;
        const b = await page.evaluate((way) => { const x = document.querySelector(`#card [data-act="${way}"]:not([disabled])`); if (!x) return null; const t = (x.innerText || '').replace(/\s+/g, ' ').trim(); x.click(); return t; }, way);
        if (!b) { console.log(`  no "${way}" button on this card`); continue; }
        await page.waitForTimeout(1500);
        const after = await page.evaluate(() => { const c = document.querySelector('#card'); const t = (e) => (e ? e.innerText.replace(/\s+/g, ' ').trim() : ''); return { open: document.querySelector('#veil').classList.contains('open'), face: [...c.querySelectorAll('.facepart .tc-signline')].map((e) => `${e.classList.contains('is-unknown') ? '(not scouted) ' : ''}${t(e)}`), head: t(c).slice(0, 260) }; });
        if (!after.face.length) { console.log(`  after "${b}": the card now reads: ${after.head}`); await closeAll(); await page.evaluate((id) => window.theater.onRegion(id), p.from); await page.waitForTimeout(300); await page.mouse.click(pt.x, pt.y, { button: 'right' }); await page.waitForTimeout(700); }
        const again = await page.evaluate(() => { const c = document.querySelector('#card'); const t = (e) => (e ? e.innerText.replace(/\s+/g, ' ').trim() : ''); return { face: [...c.querySelectorAll('.facepart .tc-signline')].map((e) => `${e.classList.contains('is-unknown') ? '(not scouted) ' : ''}${t(e)}`), go: !!c.querySelector('[data-act="attack"]:not([disabled])') }; });
        console.log(`  AFTER "${b}": WHAT YOU FACE: ${again.face.join(' | ') || 'no block'}`);
        await page.evaluate(() => { const f = document.querySelector('#card .facepart'); if (f) f.scrollIntoView({ block: 'center' }); }); await page.waitForTimeout(300);
        await page.screenshot({ path: `shots/${prefix}-${k + 1}a3-${way}.png` });
        card.go = again.go;
      }
      if (!card.go) { console.log('  the attack cannot be given'); await closeAll(); break; }
      await page.evaluate(() => document.querySelector('#card [data-act="attack"]').click()); await page.waitForTimeout(1200);
      const res = await page.evaluate(() => { const c = document.querySelector('#card'); const t = (e) => (e ? e.innerText.replace(/\s+/g, ' ').trim() : ''); return { open: document.querySelector('#veil').classList.contains('open'), paid: [...c.querySelectorAll('.tc-signline')].map(t), all: t(c).slice(0, 700) }; });
      console.log(`  RESULT: ${res.paid.length ? res.paid.join(' | ') : 'no who-paid line'}`);
      console.log(`  the result card: ${res.all.slice(0, 420)}`);
      await page.screenshot({ path: `shots/${prefix}-${k + 1}b-result.png` });
      await closeAll();
      break;
    }
    if (!opened) { console.log('no attack card could be opened from the map for the armies tried'); break; }
  }
  console.log(`\npage errors: ${errors.length}${errors.length ? ' ' + JSON.stringify(errors.slice(0, 3)) : ''}`);
  await browser.close();
})();

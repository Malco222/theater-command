// Brian's five problems of 2026-11-17, clicked on the browser page as a player would.
// usage: [TC_PAGE=...] node ui65.cjs PREFIX usa            a new campaign as the United States: Panama's troops, the canal's mark, the carrier's planes
//        [TC_PAGE=...] node ui65.cjs PREFIX empty SAVE.json  a saved war: march an army into enemy ground that no corps defends
const fs = require('fs');
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc84/dist/play.html';
const [prefix, mode, save] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 300)));
  await page.goto(PAGE);
  await page.evaluate((text) => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); localStorage.setItem('tc-ask-end', '0'); if (text) localStorage.setItem('theater-command-save-v1', text); }, save ? fs.readFileSync(save, 'utf8') : '');
  await page.reload(); await page.waitForSelector('#card');
  if (save) await page.evaluate(() => { try { window.theater.continueGame(); } catch (e) {} }); else await page.evaluate(() => window.theater.newGame('USA', 25, 1, 'grand', 'colonel', true, false, 'theatre', 3));
  await page.waitForTimeout(2500);
  const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  const closeAll = async () => { for (let i = 0; i < 5 && await veil(); i++) { await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300); } };
  const centre = (id) => page.evaluate((id) => { const t = window.theater, r = t.game().regions[id], el = document.querySelector('#map'); const w = Math.min(t.ui.view.w, 520), tall = el.clientHeight / Math.max(1, el.clientWidth); t.ui.view.x = r.x - w / 2; t.ui.view.y = r.y - w * tall * 0.45; t.ui.view.w = w; t.applyView(); }, id);
  const pointIn = (id) => page.evaluate((id) => { const el = document.querySelector(`#regs .reg[data-id="${id}"]`); if (!el) return null; const b = el.getBoundingClientRect(); for (const [fx, fy] of [[0.5, 0.5], [0.35, 0.35], [0.65, 0.65], [0.35, 0.65], [0.65, 0.35], [0.5, 0.25], [0.5, 0.75], [0.25, 0.5], [0.75, 0.5]]) { const x = b.x + b.width * fx, y = b.y + b.height * fy; if (x < 10 || y < 130 || x > 1430 || y > 890) continue; const hit = document.elementFromPoint(x, y); const reg = hit && hit.closest('.reg'); if (reg && Number(reg.dataset.id) === id) return { x, y }; } return null; }, id);
  const words = (sel, n = 500) => page.evaluate(({ sel, n }) => [...document.querySelectorAll(sel)].map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim()).join(' || ').slice(0, n), { sel, n });
  const msg = () => page.evaluate(() => (window.theater.ui.msg || '') + ' | ' + ((document.querySelector('#msg, .msg, #toast, .toast')?.innerText || '').replace(/\s+/g, ' ').trim()));
  await closeAll();
  if (mode === 'usa') {
    // 4) the troops in Panama
    const cz = await page.evaluate(() => { const g = window.theater.game(); const r = g.regions.find((x) => x.name === 'Canal Zone'); return { id: r.id, nb: r.nb, units: r.units.map((c) => `${c.name} [${c.kind}]`) }; });
    await centre(cz.id); await page.waitForTimeout(400);
    await page.evaluate((id) => window.theater.onRegion(id), cz.id); await page.waitForTimeout(600);
    console.log(`4) Canal Zone holds ${cz.units.join(', ')}`);
    console.log(`   in hand, the page says: ${await words('#tray', 620)}`);
    const hov = await page.evaluate(() => [...new Set([...document.querySelectorAll('#tray [title]')].map((e) => e.getAttribute('title')).filter((t) => /fort|garrison|move|march|stay|cannot|never/i.test(t || '')).map((t) => t.slice(0, 200)))].slice(0, 6));
    console.log(`   hover words: ${JSON.stringify(hov)}`);
    await page.screenshot({ path: `shots/${prefix}-4-panama.png` });
    for (const j of cz.nb) { const pt = await pointIn(j); if (!pt) { console.log(`   neighbour ${j}: not on screen`); continue; } const before = await page.evaluate((id) => window.theater.game().regions[id].units.map((c) => c.name).join(','), j); await page.mouse.click(pt.x, pt.y, { button: 'right' }); await page.waitForTimeout(700); const open = await veil(); console.log(`   right-click on ${await page.evaluate((id) => window.theater.game().regions[id].name, j)}: ${open ? 'a card opens: ' + (await words('#card', 320)) : 'no card'}; the line under the map: ${await msg()}`); await closeAll(); await page.evaluate((id) => window.theater.onRegion(id), cz.id); await page.waitForTimeout(300); }
    // 5) the canal's mark
    const gate = await page.evaluate(() => { const m = [...document.querySelectorAll('#gates .gatemark')]; const g = window.theater.game(); const i = g.gates.findIndex((x) => /Panama/.test(x.name)); const el = m.find((e) => Number(e.dataset.id) === i); if (!el) return { n: m.length }; const b = el.getBoundingClientRect(); return { n: m.length, x: b.x + b.width / 2, y: b.y + b.height / 2, shut: el.classList.contains('shut') }; });
    if (gate.x) {
      await page.mouse.move(gate.x, gate.y); await page.waitForTimeout(500);
      const tipWords = await words('#tip, .tip', 420);
      console.log("5) the canal's mark (" + (gate.shut ? 'shut' : 'open') + '), pointed at: ' + tipWords);
      await page.screenshot({ path: 'shots/' + prefix + '-5-canal.png' });
      await page.mouse.click(gate.x, gate.y); await page.waitForTimeout(600);
      const opened = await veil(); const cw = opened ? await words('#card', 400) : '';
      const btns = await page.evaluate(() => [...document.querySelectorAll('button')].filter((b) => new RegExp('canal|lock|unlock|close the|open the', 'i').test(b.innerText || '')).map((b) => (b.innerText || '').trim()).join(' / '));
      console.log('   clicked: ' + (opened ? 'a card opens: ' + cw : 'no card') + '; buttons that name the canal or a lock anywhere on the page: ' + (btns || 'none'));
      await closeAll();
    }
    else console.log(`5) no mark for the canal on screen (${gate.n} gate marks)`);
    // 2) the carrier's planes
    await page.evaluate(() => { const b = [...document.querySelectorAll('[data-tab], .tab, nav button')].find((x) => /^air$/i.test((x.innerText || '').trim())); if (b) b.click(); else { window.theater.ui.tab = 'air'; window.theater.refresh(); } }); await page.waitForTimeout(700);
    const airText = await words('#side, #pane, #panel', 1500);
    const at = airText.indexOf('Your carriers');
    console.log(`2) the air page: ${airText.slice(Math.max(0, at), Math.max(0, at) + 700)}`);
    const wing = await page.evaluate(() => { const b = [...document.querySelectorAll('button, [data-act]')].find((x) => /carrier fighters|strike wing/i.test(x.innerText || '')); if (!b) return null; const t = (b.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 80); b.click(); return t; });
    await page.waitForTimeout(700);
    console.log(`   picked "${wing}": its orders: ${await page.evaluate(() => [...document.querySelectorAll('#side button, #pane button, #panel button, #tray button')].map((b) => (b.innerText || '').replace(/\s+/g, ' ').trim()).filter((t) => t && t.length < 60).slice(0, 30).join(' / '))}`);
    console.log(`   words about moving ashore: ${(await words('#side, #pane, #panel, #tray', 4000)).match(/[^.]*\b(ashore|airfield|land base|go where|goes where|its fleet)[^.]*\./gi)?.slice(0, 4).join(' ') || 'none'}`);
    await page.screenshot({ path: `shots/${prefix}-2-carrier.png` });
  } else {
    // 3) enemy ground that no corps defends
    const picks = await page.evaluate(() => { const g = window.theater.game(), me = g.human, out = []; const war = (n) => (g.nations[me].atWar || []).includes(n); for (const r of g.regions) { const mine = r.units.filter((c) => c.nat === me && !c.dead && c.kind !== 'fort' && c.ap > 0); if (!mine.length) continue; for (const j of r.nb) { const t = g.regions[j]; if (t.ctrl === me || !war(t.ctrl)) continue; if (t.units.some((c) => !c.dead)) continue; out.push({ from: r.id, to: j, fromName: r.name, toName: t.name, foe: t.ctrl, n: mine.length }); } } return { me, turn: g.turn, out: out.slice(0, 6), wars: g.nations[me].atWar }; });
    console.log(`3) ${picks.me} turn ${picks.turn}, at war with ${picks.wars}: enemy provinces with no corps beside an army of mine: ${picks.out.map((p) => `${p.toName} (${p.foe}) from ${p.fromName}`).join('; ') || 'none'}`);
    let k = 0;
    for (const p of picks.out) {
      if (k >= 3) break;
      await centre(p.to); await page.waitForTimeout(400); await page.evaluate((id) => window.theater.onRegion(id), p.from); await page.waitForTimeout(400);
      const pt = await pointIn(p.to); if (!pt) continue; k++;
      const town = await page.evaluate((id) => { const g = window.theater.game(); const t = (g.towns || []).find((x) => x.region === id) || null; return t ? `${t.name || 'a town'} (${t.kind || t.size || ''}, guard ${t.guard ?? t.militia ?? '?'}, garrison ${(t.garrison || []).length})` : 'no town found in the list'; }, p.to);
      await page.mouse.click(pt.x, pt.y, { button: 'right' }); await page.waitForTimeout(900);
      const open = await veil(); const cardWords = open ? await words('#card', 420) : '';
      const after = await page.evaluate(({ to, me }) => { const g = window.theater.game(); const t = g.regions[to]; return `held by ${t.ctrl}; my corps there now ${t.units.filter((c) => c.nat === me && !c.dead).length}`; }, { to: p.to, me: picks.me });
      console.log(`   march ${p.fromName} > ${p.toName} [its town: ${town}]: ${open ? 'a card opens: ' + cardWords : 'no card'}; after the order: ${after}; the line: ${await msg()}`);
      await page.screenshot({ path: `shots/${prefix}-3-empty-${k}.png` });
      if (k === 1) {
        await closeAll(); await page.evaluate((id) => window.theater.onRegion(id), p.to); await page.waitForTimeout(600);
        const pane = await words('#pane, #side', 2400);
        const i = Math.max(0, pane.search(/THE KEY TO|besieg|siege|town guard/i));
        console.log('      the province card after the march: ' + pane.slice(i, i + 700));
        console.log('      buttons there: ' + await page.evaluate(() => [...document.querySelectorAll('#pane button, #side button, #tray button')].map((b) => (b.innerText || '').replace(/\s+/g, ' ').trim()).filter((t) => t && t.length < 70).slice(0, 16).join(' / ')));
        await page.screenshot({ path: 'shots/' + prefix + '-3-empty-after.png' });
        const storm = await page.evaluate(() => { const b = [...document.querySelectorAll('#pane button, #side button, #tray button')].find((x) => /storm|assault|take the town/i.test(x.innerText || '')); if (!b) return null; const t = (b.innerText || '').trim(); b.click(); return t; });
        await page.waitForTimeout(800);
        if (storm) { console.log('      pressed "' + storm + '": ' + ((await veil()) ? 'a card opens: ' + (await words('#card', 500)) : 'no card') + '; ' + await page.evaluate((to) => 'held by ' + window.theater.game().regions[to].ctrl, p.to)); await page.screenshot({ path: 'shots/' + prefix + '-3-empty-storm.png' }); }
        break;
      }
      await closeAll();
    }
  }
  console.log(`page errors: ${errors.length}${errors.length ? ' ' + JSON.stringify(errors.slice(0, 3)) : ''}`);
  await browser.close();
})();

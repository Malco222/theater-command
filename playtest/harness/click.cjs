// The click-through (Brian, 2026-11-17: "i want the playtester to really click everything"): plays a step file on the
// browser page with a real pointer, and holds every step to one rule: a click does something, or the game says why not.
// usage: [TC_PAGE=file:///.../play.html] [TC_PICS=all|bad] node click.cjs STEPS.txt PREFIX
// The step file's format and the log's columns: /mnt/project-files/playtest/click-through/README.md
// Writes out/click/PREFIX.log.tsv (one line a step), out/click/PREFIX.summary.txt and pictures in shots/click/.
// Verdicts: DID (the game changed, or a card, a page or what is in hand changed), SAID WHY (nothing changed and new
// words were shown), DEAD (nothing changed and nothing was said), MUTE (a button that cannot be pressed and does not
// say why), MISSING (no such button, page, unit or place on the screen), ERROR (the page threw an error).
const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc84/dist/play.html';
const PICS = process.env.TC_PICS || 'bad';
const [stepFile, prefix] = process.argv.slice(2);
const HERE = __dirname;
fs.mkdirSync(path.join(HERE, 'out/click'), { recursive: true }); fs.mkdirSync(path.join(HERE, 'shots/click'), { recursive: true });
const steps = fs.readFileSync(stepFile, 'utf8').split('\n').map((l, i) => ({ line: i + 1, raw: l })).filter((s) => s.raw.trim() && !s.raw.startsWith('#')).map((s) => { const [verb, target = '', expect = ''] = s.raw.split('\t').map((x) => x.trim()); return { ...s, verb, target, expect }; });
const clean = (s) => (s || '').replace(/\s+/g, ' ').trim();
const log = []; const json = []; const snaps = {}; let errors = []; let trail = [];

(async () => {
  const browser = await chromium.launch();
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 240)));
  await page.goto(PAGE);
  await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); localStorage.setItem('tc-ask-end', '0'); });
  await page.reload(); await page.waitForSelector('#card');

  const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  const closeAll = async () => { for (let i = 0; i < 5 && await veil(); i++) { await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(200); } };
  // What the player can see and what the game holds, in one small record.
  const look = (target) => page.evaluate((target) => {
    const t = (e, n = 400) => (e ? (e.innerText || '').replace(/\s+/g, ' ').trim().slice(0, n) : '');
    const hash = (s) => { let h = 5381; for (let i = 0; i < s.length; i++) h = ((h << 5) + h + s.charCodeAt(i)) | 0; return h; };
    const th = window.theater, g = th.game(), ui = th.ui;
    if (!g || !ui.live) return { live: false, dig: null, game: 0, card: '', cardWords: t(document.querySelector('#card'), 200), tray: '', pane: '', hint: '', tip: '', tab: '', digest: 'no game' };
    const me = g.human, n = me ? g.nations[me] : null; const R = g.regions;
    const open = document.querySelector('#veil')?.classList.contains('open');
    const card = document.querySelector('#card');
    const tipEl = document.querySelector('#tip'); const tipOn = tipEl && getComputedStyle(tipEl).display !== 'none' && getComputedStyle(tipEl).visibility !== 'hidden' && Number(getComputedStyle(tipEl).opacity) > 0.1;
    const tr = target ? R.find((r) => r.name === target) : null;
    const digest = [`t${g.turn}`, `pp ${n ? Math.round(n.pp * 100) / 100 : '-'}`, `wars ${n ? n.atWar.join('+') || 'none' : '-'}`, `corps ${g.corps.filter((c) => c.nat === me && !c.dead).length}`, `fleets ${g.fleets.filter((f) => f.nat === me && !f.dead).length}`, `air ${(g.airFleets || []).filter((a) => a.nat === me && !a.dead).length}`, `on order ${n && n.queue ? n.queue.length : 0}`, `lab ${n && n.lab ? (n.lab.now ?? n.lab.tech ?? n.lab.id ?? '-') : '-'}`, tr ? `${tr.name}: ${tr.ctrl}, mine there ${tr.units.filter((c) => c.nat === me && !c.dead).length}` : ''].filter(Boolean).join('; ');
    const MON = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
    const k = g.turn - 1 + 8; const mine = g.corps.filter((c) => c.nat === me && !c.dead); const air = (g.airFleets || []).filter((x) => x.nat === me && !x.dead && ((x.ftr || 0) + (x.bmr || 0)) > 0);
    const inHand = ui.tab === 'navy' && ui.fleet >= 0 ? g.fleets.filter((f) => f.id === ui.fleet).map((f) => [f.id, f.port != null && f.port >= 0 && R[f.port] ? R[f.port].name : (g.seas[f.zone] ? g.seas[f.zone].name : '')]) : [...(ui.picked || [])].map((id) => g.corps.find((c) => c.id === id)).filter(Boolean).map((c) => [c.id, R[c.reg].name, c.ap]);
    const dig = { month: `${MON[k % 12]} ${1939 + Math.floor(k / 12)}`, turn: g.turn, holder: tr ? tr.ctrl : '', treasury: n ? Math.round(n.pp * 100) / 100 : 0, wars: n ? [...n.atWar].sort() : [], corps: mine.filter((c) => c.kind !== 'fort').length, forts: mine.filter((c) => c.kind === 'fort').length, fleets: g.fleets.filter((f) => f.nat === me && !f.dead && Object.values(f.ships || {}).some((v) => v > 0)).length, groups: air.length, wings: air.filter((x) => x.ship !== undefined && x.ship !== null).length, onOrder: n && n.queue ? n.queue.length : 0, hand: inHand, stamp: hash(th.save()) };
    return { live: true, dig, hand: ui.sel >= 0 && ui.picked && ui.picked.size > 0 && R[ui.sel] ? R[ui.sel].name : '', game: hash(th.save()), turn: g.turn, tab: ui.tab, card: open ? (ui.modal ? ui.modal.kind : 'card') : '', cardWords: open ? t(card, 260) : '', cardHash: open ? hash(t(card, 6000)) : 0, tray: t(document.querySelector('#tray'), 300), trayHash: hash(t(document.querySelector('#tray'), 6000)), pane: hash(t(document.querySelector('#pane'), 20000)), paneWords: t(document.querySelector('#pane'), 4000), hint: t(document.querySelector('#hint'), 260), msg: ui.msg || '', tip: tipOn ? t(tipEl, 260) : '', alerts: hash(t(document.querySelector('#alerts'), 3000)), digest };
  }, target || '');
  const judge = (a, b, errs, pointing = false) => {
    if (errs.length) return 'ERROR';
    const game = a.game !== b.game || a.live !== b.live;
    const view = a.card !== b.card || a.cardHash !== b.cardHash || a.tab !== b.tab || a.trayHash !== b.trayHash || a.pane !== b.pane || a.alerts !== b.alerts;
    // The box that follows the pointer names what is under it: it is an answer only to pointing.
    const said = (b.msg && b.msg !== a.msg) || (pointing && b.tip && b.tip !== a.tip) || (b.hint && b.hint !== a.hint);
    if (game || view) return 'DID';
    if (said) return 'SAID WHY';
    return 'DEAD';
  };
  const what = (a, b, pointing = false) => [a.game !== b.game ? 'the game changed' : 'the game is as it was', b.card && (a.card !== b.card || a.cardHash !== b.cardHash) ? `card "${b.card}": ${b.cardWords.slice(0, 150)}` : (a.card && !b.card ? 'the card closed' : ''), a.tab !== b.tab ? `page ${b.tab}` : '', a.trayHash !== b.trayHash ? `in hand: ${b.tray.slice(0, 110) || 'nothing'}` : (a.pane !== b.pane ? 'the side page changed' : ''), b.msg && b.msg !== a.msg ? `said: ${b.msg.slice(0, 200)}` : (b.hint && b.hint !== a.hint ? `hint: ${b.hint.slice(0, 200)}` : ''), pointing && b.tip && b.tip !== a.tip ? `pointing says: ${b.tip.slice(0, 200)}` : ''].filter(Boolean).join(' | ');

  // The map: a province or a sea brought to the middle, and a point on it that the map itself answers for.
  const centreOn = (x, y) => page.evaluate(({ x, y }) => { const t = window.theater, el = document.querySelector('#map'); const w = Math.min(t.ui.view.w, 520), tall = el.clientHeight / Math.max(1, el.clientWidth); t.ui.view.x = x - w / 2; t.ui.view.y = y - w * tall * 0.45; t.ui.view.w = w; t.applyView(); }, { x, y });
  const regionOf = (name) => page.evaluate((name) => { const g = window.theater.game(); const r = g.regions.find((x) => x.name === name) || g.regions.find((x) => x.name.toLowerCase() === name.toLowerCase()); return r ? { id: r.id, x: r.x, y: r.y } : null; }, name);
  const seaOf = (name) => page.evaluate((name) => { const g = window.theater.game(); const s = g.seas.find((x) => x.name === name) || g.seas.find((x) => x.name.toLowerCase() === name.toLowerCase()); return s ? { id: s.id, x: s.x, y: s.y } : null; }, name);
  const pointInRegion = (id) => page.evaluate((id) => { const el = document.querySelector(`#regs .reg[data-id="${id}"]`); if (!el) return null; const b = el.getBoundingClientRect(); for (const [fx, fy] of [[0.5, 0.5], [0.35, 0.35], [0.65, 0.65], [0.35, 0.65], [0.65, 0.35], [0.5, 0.25], [0.5, 0.75], [0.25, 0.5], [0.75, 0.5], [0.42, 0.58], [0.58, 0.42]]) { const x = b.x + b.width * fx, y = b.y + b.height * fy; const hit = document.elementFromPoint(x, y); const reg = hit && hit.closest && hit.closest('.reg'); if (reg && Number(reg.dataset.id) === id && hit.closest('#map')) return { x, y }; } return null; }, id);
  const pointInSea = (s) => page.evaluate((s) => { const svg = document.querySelector('#map'); const m = svg.getScreenCTM(); if (!m) return null; for (const [dx, dy] of [[0, 0], [6, 6], [-6, 6], [6, -6], [-6, -6], [12, 0], [0, 12], [-12, 0], [0, -12]]) { const p = svg.createSVGPoint(); p.x = s.x + dx; p.y = s.y + dy; const q = p.matrixTransform(m); const hit = document.elementFromPoint(q.x, q.y); if (hit && hit.closest('#map') && !hit.closest('.reg') && !hit.closest('[data-fleets]') && !hit.closest('.gatemark') && !hit.closest('.afm')) return { x: q.x, y: q.y }; } return null; }, s);
  // A button by its words: on the open card first, then the tray, the side pane, the tabs and the top bar.
  const findButton = (label, scope) => page.evaluate(({ label, scope }) => {
    const open = document.querySelector('#veil')?.classList.contains('open');
    const roots = scope ? [scope] : open ? ['#card'] : ['#tray', '#pane', '#tabs', '#barbtns', '#alerts', '#app'];
    const want = label.toLowerCase(); const seen = [];
    for (const sel of roots) { const root = document.querySelector(sel); if (!root) continue; for (const b of root.querySelectorAll('button, [data-act], a[href], [role="button"]')) { const r = b.getBoundingClientRect(); if (r.width < 2 || r.height < 2) continue; const txt = (b.innerText || b.getAttribute('aria-label') || b.getAttribute('title') || '').replace(/\s+/g, ' ').trim(); if (!txt) continue; const low = txt.toLowerCase(); const score = low === want ? 3 : low.startsWith(want) ? 2 : low.includes(want) ? 1 : 0; if (score) seen.push({ score, len: txt.length, txt, sel }); if (score) b.setAttribute('data-click-pick', String(seen.length)); } if (seen.length) break; }
    if (!seen.length) return null; seen.sort((a, b) => b.score - a.score || a.len - b.len); const best = seen[0];
    const el = [...document.querySelectorAll('[data-click-pick]')].find((e) => (e.innerText || e.getAttribute('aria-label') || e.getAttribute('title') || '').replace(/\s+/g, ' ').trim() === best.txt);
    for (const e of document.querySelectorAll('[data-click-pick]')) e.removeAttribute('data-click-pick');
    if (!el) return null; el.scrollIntoView({ block: 'center', inline: 'nearest' }); const r = el.getBoundingClientRect();
    return { txt: best.txt.slice(0, 90), x: r.x + r.width / 2, y: r.y + r.height / 2, off: !!el.disabled || el.getAttribute('aria-disabled') === 'true', why: (el.getAttribute('title') || '').slice(0, 200), n: seen.length };
  }, { label, scope: scope || '' });
  const listButtons = (sel) => page.evaluate((sel) => { const root = document.querySelector(sel); if (!root) return []; const out = []; const keys = new Set(); for (const b of root.querySelectorAll('button, [data-act]')) { const r = b.getBoundingClientRect(); if (r.width < 2 || r.height < 2) continue; const txt = (b.innerText || b.getAttribute('aria-label') || b.getAttribute('title') || '').replace(/\s+/g, ' ').trim(); const act = b.dataset.act || ''; if (!txt && !act) continue; if (b.classList.contains('on') || b.getAttribute('aria-pressed') === 'true') continue; const kind = act || 'words'; const key = `${kind}:${txt.slice(0, 40)}`; if (keys.has(key)) continue; const many = [...keys].filter((k) => k.startsWith(kind + ':')).length; if (many >= 6) continue; keys.add(key); out.push({ key, act, txt: txt.slice(0, 80), off: !!b.disabled, why: (b.getAttribute('title') || '').slice(0, 200), nth: [...root.querySelectorAll(act ? `[data-act="${act}"]` : 'button')].indexOf(b) }); } return out; }, sel);
  const pressNth = (sel, btn) => page.evaluate(({ sel, btn }) => { const root = document.querySelector(sel); if (!root) return null; const all = [...root.querySelectorAll(btn.act ? `[data-act="${btn.act}"]` : 'button')].filter((b) => { const r = b.getBoundingClientRect(); return r.width >= 2 && r.height >= 2; }); const words = (b) => (b.innerText || b.getAttribute('aria-label') || b.getAttribute('title') || '').replace(/\s+/g, ' ').trim().slice(0, 80); const el = all.find((b) => words(b) === btn.txt); if (!el) return null; el.scrollIntoView({ block: 'center', inline: 'nearest' }); const r = el.getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2, off: !!el.disabled }; }, { sel, btn });
  const endMonth = async () => { if (await veil()) { const go = await findButton('GIVE ORDERS', '#card'); if (go) { await page.mouse.click(go.x, go.y); await page.waitForTimeout(400); } await closeAll(); } const before = await page.evaluate(() => window.theater.game().turn); const b = await findButton('END MONTH', '#barbtns'); if (!b) return 'no END MONTH button'; await page.mouse.click(b.x, b.y); await page.waitForTimeout(600); await page.evaluate(() => { const q = [...document.querySelectorAll('#card button')].find((x) => /end the month/i.test(x.innerText || '')); if (q) q.click(); }); const t0 = Date.now(); while (Date.now() - t0 < 300000) { await page.waitForTimeout(600); if (await page.evaluate(() => window.theater.game().turn) !== before) { await page.waitForTimeout(1500); return ''; } } return 'the month did not end in 300 seconds'; };

  // One step, played. Returns a note (why it could not be played) or ''.
  const play = async (s) => {
    const [a, b = '', c = '', d = '', e = '', f = ''] = s.target.split(/\s+/);
    switch (s.verb) {
      case 'start': { const [y, m] = b.split('-').map(Number); const from = (y - 1939) * 12 + (m - 9) + 1; const seed = c === 'seed' ? Number(d) : 25; const level = c === 'seed' ? e : c; await page.evaluate(({ nat, seed, from, level }) => window.theater.newGame(nat, seed, from, 'grand', level || 'colonel'), { nat: a, seed, from, level }); await page.waitForTimeout(2500); await closeAll(); return ''; }
      case 'load': { const text = fs.readFileSync(path.isAbsolute(s.target) ? s.target : path.join(HERE, s.target), 'utf8'); await page.evaluate((text) => localStorage.setItem('theater-command-save-v1', text), text); await page.reload(); await page.waitForSelector('#card'); await page.evaluate(() => { try { window.theater.continueGame(); } catch (err) {} }); await page.waitForTimeout(2500); await closeAll(); return ''; }
      case 'snapshot': snaps[s.target] = await page.evaluate(() => window.theater.save()); return '';
      case 'restore': { if (!snaps[s.target]) return `no snapshot named ${s.target}`; await page.evaluate((text) => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); localStorage.setItem('tc-ask-end', '0'); localStorage.setItem('theater-command-save-v1', text); }, snaps[s.target]); await page.reload(); await page.waitForSelector('#card'); await page.evaluate(() => { try { window.theater.continueGame(); } catch (err) {} }); await page.waitForTimeout(2200); await closeAll(); return ''; }
      case 'select-province': case 'order-province': { const r = await regionOf(s.target); if (!r) return `no province named ${s.target}`; await centreOn(r.x, r.y); await page.waitForTimeout(350); const pt = await pointInRegion(r.id); if (!pt) return `${s.target} cannot be clicked on the map`; await page.mouse.click(pt.x, pt.y, { button: s.verb === 'order-province' ? 'right' : 'left' }); await page.waitForTimeout(800); return ''; }
      case 'select-sea': case 'order-sea': { const z = await seaOf(s.target); if (!z) return `no sea named ${s.target}`; await centreOn(z.x, z.y); await page.waitForTimeout(350); const pt = await pointInSea(z); if (!pt) return `${s.target} cannot be clicked on the map`; await page.mouse.click(pt.x, pt.y, { button: s.verb === 'order-sea' ? 'right' : 'left' }); await page.waitForTimeout(800); return ''; }
      case 'select-unit': {
        // a fleet's piece on the map first; then anything on the screen that carries the name
        const fl = await page.evaluate((name) => { const g = window.theater.game(); const f = g.fleets.find((x) => x.name === name && x.nat === g.human && !x.dead); if (!f) return null; const z = g.seas[f.zone] || null; const p = f.port != null && f.port >= 0 ? g.regions[f.port] : null; return { id: f.id, x: p ? p.x : z ? z.x : 0, y: p ? p.y : z ? z.y : 0 }; }, s.target);
        if (fl) { await centreOn(fl.x, fl.y); await page.waitForTimeout(350); const pt = await page.evaluate((id) => { const el = [...document.querySelectorAll('[data-fleets]')].find((x) => (x.dataset.fleets || '').split(',').map(Number).includes(id)); if (!el) return null; const r = el.getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; }, fl.id); if (pt) { await page.mouse.click(pt.x, pt.y); await page.waitForTimeout(800); return ''; } }
        const btn = await findButton(s.target); if (!btn) return `nothing on the screen is named ${s.target}${fl ? ' (and the fleet has no piece on the map)' : ''}`; await page.mouse.click(btn.x, btn.y); await page.waitForTimeout(800); return '';
      }
      case 'point-at': case 'click-mark': {
        const name = s.target.replace(/^(airfield|gate)\s+/i, ''); const kind = /^gate/i.test(s.target) ? 'gate' : 'airfield';
        const at = await page.evaluate(({ kind, name }) => { const g = window.theater.game(); if (kind === 'gate') { const i = g.gates.findIndex((x) => x.name.toLowerCase().includes(name.toLowerCase())); const el = document.querySelector(`#gates .gatemark[data-id="${i}"]`); return el ? { x: Number(el.dataset.x), y: Number(el.dataset.y), sel: `#gates .gatemark[data-id="${i}"]` } : null; } const r = g.regions.find((x) => x.name === name); if (!r) return null; return { x: r.x, y: r.y, sel: `.afm[data-id="${r.id}"]` }; }, { kind, name });
        if (!at) return `no ${kind} named ${name}`; await centreOn(at.x, at.y); await page.waitForTimeout(350);
        const pt = await page.evaluate((sel) => { const el = document.querySelector(sel); if (!el) return null; const r = el.getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; }, at.sel);
        if (!pt) return `the ${kind} ${name} has no mark on the map`; await page.mouse.move(pt.x - 3, pt.y - 3); await page.mouse.move(pt.x, pt.y); await page.waitForTimeout(700); if (s.verb === 'click-mark') { await page.mouse.click(pt.x, pt.y); await page.waitForTimeout(800); } return '';
      }
      case 'open-page': { if (await veil()) { await page.keyboard.press('Escape'); await page.waitForTimeout(300); await closeAll(); } const names = { build: 'build', research: 'research', air: 'air', navy: 'navy', powers: 'powers', spies: 'spies', staff: 'staff', news: 'reel', battles: 'battles', generals: 'generals', honours: 'honours', region: 'region' }; const key = names[s.target.toLowerCase()]; if (key) { const pt = await page.evaluate((key) => { const el = document.querySelector(`#tabs [data-tab="${key}"]`); if (!el) return null; const r = el.getBoundingClientRect(); return r.width > 1 ? { x: r.x + r.width / 2, y: r.y + r.height / 2 } : null; }, key); if (!pt) return `no ${s.target} tab on the screen`; await page.mouse.click(pt.x, pt.y); await page.waitForTimeout(700); return ''; } const btn = await findButton(s.target, '#barbtns') || await findButton(s.target, '#app'); if (!btn) return `no door to a page named ${s.target}`; await page.mouse.click(btn.x, btn.y); await page.waitForTimeout(800); return ''; }
      case 'press': { const btn = await findButton(s.target); if (!btn) return `no button reads "${s.target}"`; if (btn.off) return `MUTE?${btn.why}`; await page.mouse.click(btn.x, btn.y); await page.waitForTimeout(900); return ''; }
      case 'check-province': { const r = await regionOf(s.target); return r ? '' : `no province named ${s.target}`; }
      case 'key': { await page.keyboard.press(s.target); await page.waitForTimeout(700); return ''; }
      case 'close': { await page.keyboard.press('Escape'); await page.waitForTimeout(400); if (await veil()) await closeAll(); return ''; }
      case 'end-month': return endMonth();
      default: return `unknown verb ${s.verb}`;
    }
  };
  const passes = (s, a, b, verdict) => {
    if (!s.expect) return '';
    const x = s.expect.toLowerCase();
    if (x === 'does') return verdict === 'DID' ? 'PASS' : 'FAIL';
    if (x.startsWith('says ')) return [b.msg, b.hint, b.tip, b.cardWords, b.tray, b.paneWords].join(' ').toLowerCase().includes(s.expect.slice(5).toLowerCase()) ? 'PASS' : 'FAIL';
    if (x === 'says-why') return verdict === 'SAID WHY' || (verdict === 'DID' && a.game === b.game && (b.card || b.msg)) ? 'PASS' : 'FAIL';
    if (x.startsWith('card')) return b.card && (a.card !== b.card || a.cardHash !== b.cardHash) ? 'PASS' : 'FAIL';
    if (x === 'moves') return a.game !== b.game ? 'PASS' : 'FAIL';
    if (x.startsWith('holder')) return new RegExp(`: ${s.expect.split(/\s+/)[1]},`).test(b.digest) ? 'PASS' : 'FAIL';
    return '';
  };
  const record = async (n, s, a, b, verdict, note, extra = '') => {
    const pass = passes(s, a, b, verdict);
    const bad = verdict === 'DEAD' || verdict === 'ERROR' || verdict === 'MISSING' || verdict === 'MUTE' || pass === 'FAIL';
    let pic = '';
    if (PICS === 'all' || bad) { pic = `shots/click/${prefix}-${String(n).padStart(3, '0')}.png`; try { await page.screenshot({ path: path.join(HERE, pic) }); } catch (err) { pic = ''; } }
    json.push(JSON.stringify({ n, line: s.line, verb: s.verb, target: s.target + extra, expect: s.expect, hand: b.dig ? b.dig.hand : [], open: b.card ? `card ${b.card}: ${b.cardWords.slice(0, 70)}` : `page ${b.tab}`, words: [b.msg, b.msg ? '' : b.hint, s.verb === 'point-at' ? b.tip : ''].filter(Boolean).join(' | ').slice(0, 300), before: a.dig, after: b.dig, verdict, pass, note: clean(note).slice(0, 200), picture: pic }));
    log.push([n, s.line, s.verb, s.target + extra, s.expect, verdict, pass, clean(note || what(a, b, s.verb === 'point-at')), a.digest, b.digest, pic].join('\t'));
  };

  let n = 0;
  for (const s of steps) {
    if (s.verb === 'press-each') {
      const sel = (await veil()) || s.target === 'card' ? '#card' : s.target === 'tray' ? '#tray' : '#pane';
      const base = await page.evaluate(() => window.theater.save());
      const ui = trail.slice();
      const list = await listButtons(sel);
      const soft = ui.some((u) => u.verb === 'key'); const again = async (hard) => { if (hard && !soft) { await page.evaluate((text) => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); localStorage.setItem('tc-ask-end', '0'); localStorage.setItem('theater-command-save-v1', text); }, base); await page.reload(); await page.waitForSelector('#card'); await page.evaluate(() => { try { window.theater.continueGame(); } catch (err) {} }); await page.waitForTimeout(2000); } await closeAll(); for (const u of ui) await play(u); if (soft && !(await veil())) { await page.keyboard.press('Escape'); await page.waitForTimeout(2500); for (const u of ui) await play(u); } };
      for (const btn of list) {
        n++; errors = [];
        await page.evaluate(() => { try { const th = window.theater; if (th && th.ui && th.ui.msg) { th.ui.msg = ''; if (th.refresh) th.refresh(); } } catch (e) {} }); const a = await look('');
        const at = await pressNth(sel, btn);
        const sub = { ...s, verb: 'press', target: btn.txt || btn.act, expect: '' };
        if (!at) { await record(n, sub, a, a, 'MISSING', 'the button was there and is gone', ` [${btn.act || 'no act'} in ${s.target}]`); await again(true); continue; }
        if (at.off) { await record(n, sub, a, a, btn.why ? 'SAID WHY' : 'MUTE', btn.why ? `it cannot be pressed, and says: ${btn.why}` : 'it cannot be pressed, and does not say why', ` [${btn.act || 'no act'} in ${s.target}]`); continue; }
        await page.mouse.click(at.x, at.y); await page.waitForTimeout(700);
        const b = await look('');
        const verdict = judge(a, b, errors);
        await record(n, sub, a, b, verdict, errors.length ? errors.join(' // ') : '', ` [${btn.act || 'no act'} in ${s.target}]`);
        if (verdict !== 'DEAD' && verdict !== 'SAID WHY') await again(true);
      }
      continue;
    }
    n++; errors = [];
    // the game's last words are wiped before each step, so that the same reason given twice is heard the second time
    await page.evaluate(() => { try { const th = window.theater; if (th && th.ui && th.ui.msg) { th.ui.msg = ''; if (th.refresh) th.refresh(); } } catch (e) {} }); const a = await look(/province|sea/.test(s.verb) ? s.target : '');
    let note = '';
    try { note = await play(s); } catch (err) { note = `the runner failed: ${String(err.message || err).slice(0, 160)}`; }
    const b = await look(/province|sea/.test(s.verb) ? s.target : '');
    let verdict = errors.length ? 'ERROR' : note.startsWith('MUTE?') ? (note.length > 5 ? 'SAID WHY' : 'MUTE') : note ? 'MISSING' : ['start', 'load', 'snapshot', 'restore', 'close'].includes(s.verb) ? 'DID' : s.verb === 'check-province' ? 'READ' : judge(a, b, errors, s.verb === 'point-at');
    if (note.startsWith('MUTE?')) note = note.length > 5 ? `it cannot be pressed, and says: ${note.slice(5)}` : 'it cannot be pressed, and does not say why';
    await record(n, s, a, b, verdict, errors.length ? errors.join(' // ') : note);
    // The trail: the clicks that only changed what is on the screen since the game last moved. A sweep plays it again
    // after each restore, to stand where it stood.
    const opened = !!b.card && (a.card !== b.card || a.cardHash !== b.cardHash);
    if (['start', 'load', 'restore'].includes(s.verb)) trail = [];
    else if (s.verb === 'close') { const i = trail.findIndex((u) => u.card); if (i >= 0) trail = trail.slice(0, i); }
    else if (s.verb === 'end-month') trail = opened ? [{ verb: 'key', target: 'r', expect: '', card: true }] : [];
    else if (a.game !== b.game) { const unit = [...trail].reverse().find((u) => u.verb === 'select-unit' && !u.card); trail = trail.filter((u) => u.verb === 'open-page' && !u.card).slice(-1); if (b.hand) trail.push({ verb: 'select-province', target: b.hand, expect: '' }); else if (unit) trail.push({ verb: 'select-unit', target: unit.target, expect: '' }); }
    else if (['select-province', 'select-sea', 'select-unit', 'open-page', 'press', 'click-mark', 'order-province', 'order-sea', 'key'].includes(s.verb) && verdict !== 'MISSING' && verdict !== 'DEAD') {
      if (s.verb === 'open-page' && !opened) trail = trail.filter((u) => u.verb !== 'open-page' || u.card);
      trail.push({ ...s, card: opened });
    }
  }
  const head = ['step', 'line', 'verb', 'target', 'expect', 'verdict', 'check', 'what happened', 'before', 'after', 'picture'].join('\t');
  fs.writeFileSync(path.join(HERE, `out/click/${prefix}.log.tsv`), head + '\n' + log.join('\n') + '\n');
  fs.writeFileSync(path.join(HERE, `out/click/${prefix}.steps.jsonl`), json.join('\n') + '\n');
  const rows = log.map((l) => l.split('\t')); const count = (v) => rows.filter((r) => r[5] === v).length;
  const bad = rows.filter((r) => ['DEAD', 'ERROR', 'MISSING', 'MUTE'].includes(r[5]) || r[6] === 'FAIL');
  const sum = [`${path.basename(stepFile)} on ${PAGE}`, `steps ${rows.length}: DID ${count('DID')}, SAID WHY ${count('SAID WHY')}, DEAD ${count('DEAD')}, MUTE ${count('MUTE')}, MISSING ${count('MISSING')}, ERROR ${count('ERROR')}; checks passed ${rows.filter((r) => r[6] === 'PASS').length}, failed ${rows.filter((r) => r[6] === 'FAIL').length}`, '', ...bad.map((r) => `${r[5]}${r[6] === 'FAIL' ? ' (FAIL, wanted ' + r[4] + ')' : ''}  step ${r[0]} (line ${r[1]}): ${r[2]} ${r[3]}  --  ${r[7].slice(0, 230)}`)].join('\n');
  fs.writeFileSync(path.join(HERE, `out/click/${prefix}.summary.txt`), sum + '\n');
  console.log(sum.split('\n').slice(0, 2).join('\n'));
  await browser.close();
})();

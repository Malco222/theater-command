// Version 74 (ui39 with the headline): the landing card's headline as ruled (red "Ashore, then cut off", amber "A costly
// crossing", the fight on its own line), read and pictured before the card is scrolled to its foot.
// Version 73 (ui38 with the new parts): the Staff tab's pointer to the crossings, the list standing first in the Navy
// pane, the red line and the ticket "Once ashore: holding the sea behind the beach", and the landing card's warning.
// Version 72: the sea crossing as the player meets it on the page. Loads a save in which a crossing is named as the plan
// (probe55 with "cross": true), and reads: which tabs there are, where the navy is at this depth, the list "Crossing the sea"
// with each row's loss, bar and reckoning, the sea lift, and the landing card for the force on the quay with its reckoning
// before the order. usage: [TC_PAGE=...] node ui40.cjs SAVE.json PREFIX [W H]
const { chromium } = require('playwright');
const fs = require('fs');
const [save, prefix, W = '1440', H = '900'] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch();
  const phone = Math.min(+W, +H) < 500;
  const ctx = await browser.newContext({ viewport: { width: +W, height: +H }, deviceScaleFactor: 2, isMobile: phone, hasTouch: phone });
  const page = await ctx.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.addInitScript((s) => { try { if (!sessionStorage.getItem('tc-once')) { sessionStorage.setItem('tc-once', '1'); localStorage.clear(); localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } } catch {} }, fs.readFileSync(save, 'utf8'));
  await page.goto(process.env.TC_PAGE || 'file:///home/user/tc74/dist/play.html'); await page.waitForTimeout(1000);
  const c = page.locator('[data-act="continue"]').first(); if (await c.count()) await c.click(); else await page.evaluate(() => window.theater.continueGame());
  await page.waitForTimeout(1500);
  const open = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  for (let k = 0; k < 12 && await open(); k++) { await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(200); }
  const say = (k, v) => console.log(`${k}: ${typeof v === 'string' ? v : JSON.stringify(v)}`);
  const wide = () => page.evaluate(() => document.documentElement.scrollWidth - document.documentElement.clientWidth);
  const st = await page.evaluate(() => { const g = window.theater.game(); const me = g.human; const aim = (g.flags[`st:aim:${me}`] ?? 0) - 1, quay = (g.flags[`st:quay:${me}`] ?? 0) - 1; return { me, turn: g.turn, aim, aimName: g.regions[aim]?.name, quay, quayName: g.regions[quay]?.name, lift: g.nations[me].sealift, onQuay: quay >= 0 ? g.regions[quay].units.filter((u) => u.nat === me && !u.dead && u.kind !== 'fort').map((u) => ({ id: u.id, kind: u.kind, ap: u.ap, apMax: u.apMax })) : [] }; });
  say('state', st);
  say('tabs on the bar', await page.evaluate(() => [...document.querySelectorAll('#tabs [data-act="tab"]')].filter((b) => b.offsetParent !== null).map((b) => b.textContent.replace(/\s+/g, ' ').trim())));
  // The navy: on the bar, or the staff's at this depth (then the player looks in from the Staff tab).
  let navyOnBar = await page.evaluate(() => { const b = document.querySelector('#tabs [data-act="tab"][data-tab="navy"]'); return !!b && b.offsetParent !== null; });
  say('navy tab on the bar', navyOnBar);
  let clicks = 0;
  if (navyOnBar) { await page.click('#tabs [data-act="tab"][data-tab="navy"]'); clicks = 1; }
  else {
    await page.evaluate(() => document.querySelector('#tabs [data-act="tab"][data-tab="staff"]')?.click()); await page.waitForTimeout(500); clicks = 1;
    await page.screenshot({ path: `shots/${prefix}-staff.png` });
    say('the staff tab, the navy desk', await page.evaluate(() => { const b = document.querySelector('.pane [data-act="tab"][data-tab="navy"], #pane [data-act="tab"][data-tab="navy"]'); const d = b?.closest('.desk') ?? b?.parentElement; return d ? d.textContent.replace(/\s+/g, ' ').trim().slice(0, 500) : null; }));
    say('the staff tab, the pointer to the crossings', await page.evaluate(() => { const e = document.querySelector('#pane .crosslead'); if (!e) return null; const r = e.getBoundingClientRect(), p = document.querySelector('#pane').getBoundingClientRect(); return { text: e.textContent.replace(/\s+/g, ' ').trim().slice(0, 600), buttons: [...e.querySelectorAll('button, a')].map((b) => `${b.dataset.act ?? ''}:${b.dataset.tab ?? ''} "${b.textContent.trim()}"`), topInPane: Math.round(r.top - p.top), seenWithoutScrolling: r.bottom <= p.bottom && r.top >= p.top }; }));
    say('does the staff tab itself mention a crossing', await page.evaluate(() => /cross/i.test(document.querySelector('#pane')?.innerText ?? '')));
    const viaLead = await page.evaluate(() => { const b = document.querySelector('#pane .crosslead button, #pane .crosslead a'); if (b) { b.click(); return true; } return false; });
    say('went by the pointer on the staff tab', viaLead);
    if (viaLead) clicks = 2; else {
      await page.evaluate(() => (document.querySelector('#pane [data-act="tab"][data-tab="navy"]') ?? document.querySelector('[data-act="tab"][data-tab="navy"]'))?.click()); clicks = 2;
    }
  }
  await page.waitForTimeout(700);
  say('clicks from the map to the crossing list', clicks);
  say('tab now', await page.evaluate(() => window.theater.ui.tab));
  const list = await page.evaluate(() => {
    const txt = (e) => (e ? e.textContent.replace(/\s+/g, ' ').trim() : '');
    const pane = document.querySelector('#pane');
    const h = [...pane.querySelectorAll('h3')].map(txt);
    const rows = [...pane.querySelectorAll('.cross')].map((x) => ({ plan: x.classList.contains('plan'), head: txt(x.querySelector('.crhead')), sub: txt(x.querySelector('.sub')).slice(0, 400), bar: txt(x.querySelector('.note')).slice(0, 300), ticket: [...x.querySelectorAll('table tr')].map(txt).slice(0, 14), prep: [...x.querySelectorAll('p.sub')].map(txt).join(' ').slice(0, 500), buttons: [...x.querySelectorAll('button')].map(txt) }));
    const lift = [...pane.querySelectorAll('p, .fact, .note, .sub')].map(txt).filter((t) => /sea lift|troopship/i.test(t)).slice(0, 4).map((t) => t.slice(0, 300));
    const first = pane.querySelector('.cross'); const top = first ? Math.round(first.getBoundingClientRect().top) : -1;
    const pr = pane.getBoundingClientRect();
    const hold = [...pane.querySelectorAll('.cross')].map((x) => ({ head: txt(x.querySelector('.crhead')), warn: txt(x.querySelector('.holdwarn')), warnColour: x.querySelector('.holdwarn') ? getComputedStyle(x.querySelector('.holdwarn')).color : '', ticketHead: [...x.querySelectorAll('h4.crh')].map((h) => `${txt(h)}${h.classList.contains('bad') ? ' [bad]' : ''}`), onceAshore: (() => { const h = [...x.querySelectorAll('h4.crh')].find((e) => /Once ashore/.test(e.textContent)); const t = h?.nextElementSibling; return t ? [...t.querySelectorAll('tr')].map(txt) : []; })() }));
    const firstH = pane.querySelector('h3'); const crossTopInPane = first ? Math.round(first.getBoundingClientRect().top - pr.top + pane.scrollTop) : -1;
    window.__hold = { hold, firstHeading: txt(firstH), crossTopInPane };
    return { headings: h, rows, lift, firstRowTop: top, paneScroll: pane.scrollHeight, paneHeight: pane.clientHeight };
  });
  say('crossing list', list);
  say('Version 73: where the list stands and what it says of holding the beach', await page.evaluate(() => window.__hold));
  say('page wider than the screen by', await wide());
  await page.screenshot({ path: `shots/${prefix}-navy.png` });
  const plan = page.locator('#pane .cross.plan').first();
  if (await plan.count()) { await plan.scrollIntoViewIfNeeded(); await page.waitForTimeout(300); await page.screenshot({ path: `shots/${prefix}-plan.png` }); }
  // The landing: pick the force on the quay as a player does (click the province, pick the corps, "Invade by sea", click the beach).
  if (st.quay >= 0 && st.onQuay.length) {
    await page.evaluate(() => { const b = document.querySelector('#tabs [data-act="tab"][data-tab="map"], #tabs [data-act="tab"]'); b?.click(); });
    await page.waitForTimeout(300);
    await page.evaluate((q) => window.theater.onRegion(q), st.quay); await page.waitForTimeout(500);
    say('after a click on the quay: corps picked', await page.evaluate(() => [...window.theater.ui.picked]));
    const picks = await page.evaluate(() => [...document.querySelectorAll('#pane [data-act="pick"], #tray [data-act="pick"], [data-act="pickall"]')].map((b) => `${b.dataset.act} ${b.textContent.replace(/\s+/g, ' ').trim().slice(0, 40)}`).slice(0, 12));
    say('pick buttons', picks);
    const all = page.locator('[data-act="pickall"]').first();
    if (await all.count()) await all.click();
    else await page.evaluate((ids) => { for (const id of ids) window.theater.ui.picked.add(id); window.theater.refresh(); }, st.onQuay.filter((u) => u.kind !== 'eng').slice(0, Math.max(1, st.lift)).map((u) => u.id));
    await page.waitForTimeout(400);
    say('corps picked', await page.evaluate(() => [...window.theater.ui.picked].length));
    say('order buttons for the picked corps', await page.evaluate(() => [...document.querySelectorAll('.acts button')].filter((b) => b.offsetParent !== null).map((b) => b.textContent.trim())));
    const inv = page.locator('[data-act="land"]').first();
    if (await inv.count()) {
      await inv.click(); await page.waitForTimeout(400);
      await page.screenshot({ path: `shots/${prefix}-invade-mode.png` });
      say('message on the page in invade mode', await page.evaluate(() => (document.querySelector('.msg, #msg, .hintbar')?.textContent ?? '').trim().slice(0, 300)));
      await page.evaluate((a) => window.theater.onRegion(a), st.aim); await page.waitForTimeout(700);
      if (await open()) {
        await page.screenshot({ path: `shots/${prefix}-landing-card.png` });
        say('landing card, the headline', await page.evaluate(() => { const c = document.querySelector('#card'); const txt = (e) => (e ? e.textContent.replace(/\s+/g, ' ').trim() : ''); const v = c.querySelector('.verdict'); const fh = c.querySelector('h3.fighthead'); const fl = c.querySelector('.fightline'); const hw = c.querySelector('.holdwarn'); const sub = v?.nextElementSibling; return { headline: txt(v), headlineClass: v ? v.className : null, headlineColour: v ? getComputedStyle(v).color : null, under: sub && sub !== fh ? txt(sub).slice(0, 300) : null, fightHead: txt(fh), fightLine: txt(fl), fightLineClass: fl ? fl.className : null, fightLineColour: fl ? getComputedStyle(fl).color : null, holdwarn: txt(hw) }; }));
        say('landing card', await page.evaluate(() => document.querySelector('#card').innerText.replace(/\s+\n/g, '\n').slice(0, 3000)));
        say('landing card, once ashore', await page.evaluate(() => { const c = document.querySelector('#card'); const txt = (e) => (e ? e.textContent.replace(/\s+/g, ' ').trim() : ''); const h = [...c.querySelectorAll('h4.crh')].find((e) => /Once ashore/.test(e.textContent)); const n = [...c.querySelectorAll('.note')].find((e) => /Once ashore|Once down/.test(e.textContent)); return { ticketHead: h ? `${txt(h)}${h.classList.contains('bad') ? ' [bad]' : ''}` : null, ticket: h?.nextElementSibling ? [...h.nextElementSibling.querySelectorAll('tr')].map(txt) : [], note: txt(n), noteBad: !!n && n.classList.contains('holdbad'), noteColour: n ? getComputedStyle(n).color : '', cardScroll: c.scrollHeight, cardHeight: c.clientHeight, button: txt(c.querySelector('[data-act="doland"]')) }; }));
        const body = page.locator('#card .body').first(); if (await body.count()) { say('what scrolls on the card', await page.evaluate(() => { const out = []; for (const e of [document.querySelector('#card'), ...document.querySelectorAll('#card *')]) { if (e.scrollHeight > e.clientHeight + 4 && /auto|scroll/.test(getComputedStyle(e).overflowY)) { out.push(`${e.id || e.className || e.tagName} ${e.clientHeight} of ${e.scrollHeight}`); e.scrollTop = e.scrollHeight; } } const f = document.querySelector('#card footer'); const r = f?.getBoundingClientRect(); return { out, footerTop: r ? Math.round(r.top) : null, footerSeen: r ? r.top < innerHeight : null }; })); await page.waitForTimeout(200); await page.screenshot({ path: `shots/${prefix}-landing-card-foot.png` }); await page.evaluate(() => { const b = document.querySelector('#card .body'); if (b) b.scrollTop = 0; }); }
        await page.screenshot({ path: `shots/${prefix}-landing-card-foot.png` });
      } else { say('no landing card; the page says', await page.evaluate(() => window.theater.ui.msg)); await page.screenshot({ path: `shots/${prefix}-refused.png` }); say('where the refusal shows', await page.evaluate(() => { const m = window.theater.ui.msg; const e = [...document.querySelectorAll('body *')].filter((x) => x.children.length === 0 && x.textContent.includes(m.slice(0, 30)) && x.offsetParent !== null)[0]; if (!e) return 'nowhere on the page'; const r = e.getBoundingClientRect(); return `${e.className || e.tagName} at ${Math.round(r.left)},${Math.round(r.top)} size ${Math.round(r.width)}x${Math.round(r.height)}`; })); }
    } else say('no Invade by sea button', true);
  }
  say('page errors', errors.length ? errors.slice(0, 4) : 'none');
  await browser.close();
})();

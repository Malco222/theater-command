// Version 70 on the page: the history dial as a player meets it.
//   card W H PREFIX            the new-campaign card at a screen size: the row of five stops, what each says, whether it fits
//   claims SAVE PREFIX [W H]   an Italian campaign with claims on it: the Powers tab, then End month three times with the
//                              news of claims, the card that opens, and how a claim on the player's ground is marked
//   book PREFIX [MONTHS]       Germany by the book that gives no orders: when the book is closed and how the player is told
//   settings SAVE PREFIX       the dial turned in the campaign settings and kept through a reload
// usage: [TC_PAGE=file:///home/user/tc70/dist/play.html] node ui33.cjs MODE ...   (shots go to shots/PREFIX-*.png)
const { chromium } = require('playwright');
const fs = require('fs');
const [mode, ...rest] = process.argv.slice(2);
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc70/dist/play.html';
const flat = (s, n = 600) => String(s ?? '').replace(/\s+/g, ' ').trim().slice(0, n);
(async () => {
  const browser = await chromium.launch();
  const errors = [];
  const open = async (w, h, save) => {
    const phone = Math.min(w, h) < 500;
    const ctx = await browser.newContext({ viewport: { width: w, height: h }, deviceScaleFactor: 2, isMobile: phone, hasTouch: phone });
    const page = await ctx.newPage();
    page.on('pageerror', (e) => errors.push(`${e.message} @ ${(e.stack || '').split('\n').slice(1, 3).join(' | ').slice(0, 200)}`));
    page.on('console', (m) => { if (m.type() === 'error') errors.push(`console: ${m.text().slice(0, 300)}`); });
    await page.addInitScript((s) => { try { if (!sessionStorage.getItem('tc-once')) { sessionStorage.setItem('tc-once', '1'); localStorage.clear(); localStorage.setItem('tc-watch', '0'); if (s) localStorage.setItem('theater-command-save-v1', s); } } catch {} }, save ? fs.readFileSync(save, 'utf8') : '');
    await page.goto(PAGE); await page.waitForTimeout(1000);
    return page;
  };
  const closeAll = async (page) => { for (let k = 0; k < 12; k++) { const was = await page.evaluate(() => { const v = document.querySelector('#veil'); if (!v || !v.classList.contains('open')) return false; window.theater.openModal(null); return true; }); if (!was) break; await page.waitForTimeout(200); } };
  const flags = (page) => page.evaluate(() => { const g = window.theater.game(); return { turn: g.turn, human: g.human, hist: g.flags.hist ?? null, histMax: g.flags.histMax ?? null }; });
  const cardText = (page, n = 500) => page.evaluate((n) => { const v = document.querySelector('#veil'); return v && v.classList.contains('open') ? (document.querySelector('#card')?.innerText || '').replace(/\s+/g, ' ').slice(0, n) : null; }, n);
  const month = async (page) => {
    const t = (await flags(page)).turn;
    await page.evaluate(() => window.theater.nextMonth());
    try { await page.waitForFunction((t0) => window.theater.game().turn > t0, t, { timeout: 240000 }); } catch { console.log('  the date did not move'); }
    await page.waitForTimeout(1500);
    const cards = [];
    for (let k = 0; k < 12; k++) { const c = await cardText(page, 260); if (!c) break; cards.push(c); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(250); }
    return cards;
  };
  const dialNews = (page, from) => page.evaluate((from) => window.theater.game().headlines.filter((h) => h.turn >= from && /presses its claim|marches for|stands by|sign a treaty|history books|lets its claim|History's book|declares war on/i.test(h.text)).map((h) => `${h.turn}: ${h.text.slice(0, 230)}`), from);
  const cont = async (page) => { const c = page.locator('[data-act="continue"]').first(); if (await c.count()) await c.click(); else await page.evaluate(() => window.theater.continueGame()); await page.waitForTimeout(1500); await closeAll(page); };
  const powers = async (page) => { await page.evaluate(() => document.querySelector('[data-act="tab"][data-tab="powers"]')?.click()); await page.waitForTimeout(500); return page.evaluate(() => ({ head: !!document.querySelector('.claimhead'), sub: (document.querySelector('.claimhead + .sub')?.textContent || '').replace(/\s+/g, ' ').trim(), rows: [...document.querySelectorAll('.nat.claim')].map((x) => `${x.classList.contains('onyou') ? '[ON YOU] ' : ''}${x.textContent.replace(/\s+/g, ' ').trim().slice(0, 220)}`) })); };

  if (mode === 'card') {
    const [W, H, prefix] = rest; const w = +W, h = +H;
    const page = await open(w, h);
    await page.waitForSelector('#card');
    await page.click('.gcnat[data-nat="ITA"]'); await page.waitForTimeout(200);
    const fit = () => page.evaluate(() => {
      const card = document.querySelector('#card'), row = document.querySelector('#card .gclevels.dial');
      if (!row) return { row: false };
      const cr = card.getBoundingClientRect(), rr = row.getBoundingClientRect();
      const stops = [...row.querySelectorAll('.gclevel')].map((b) => { const r = b.getBoundingClientRect(); const cs = getComputedStyle(b.querySelector('b')); return { stop: b.dataset.stop, on: b.classList.contains('on'), name: b.querySelector('b').textContent, under: b.querySelector('span:not(.dialmark)')?.textContent ?? '', w: Math.round(r.width), h: Math.round(r.height), font: cs.fontSize, cut: b.scrollWidth > b.clientWidth + 1 || r.right > cr.right + 1 || r.left < cr.left - 1 }; });
      const note = document.querySelector('#card .gclevels.dial ~ .gclevelnote');
      return { row: true, card: [Math.round(cr.width), Math.round(cr.height)], rowBox: [Math.round(rr.left), Math.round(rr.right), Math.round(rr.width)], rowScrolls: row.scrollWidth > row.clientWidth + 1, pageScrollsSideways: document.documentElement.scrollWidth > window.innerWidth + 1, cardScrollsSideways: card.scrollWidth > card.clientWidth + 1, stops, noteFont: note ? getComputedStyle(note).fontSize : null, note: (note?.textContent || '').replace(/\s+/g, ' ').trim().slice(0, 700), ends: (document.querySelector('#card .dialends')?.textContent || '').replace(/\s+/g, ' ').trim() };
    });
    console.log(`card at ${w}x${h}:`, JSON.stringify(await fit()));
    await page.evaluate(() => document.querySelector('#card #gchist')?.scrollIntoView({ block: 'start' })); await page.waitForTimeout(300);
    await page.screenshot({ path: `shots/${prefix}-1-card-middle-stop.png` });
    for (const s of [1, 2, 4, 5]) {
      await page.evaluate((s) => document.querySelector(`#card .gclevel[data-act="gchist"][data-stop="${s}"]`)?.click(), s); await page.waitForTimeout(200);
      const f = await fit();
      console.log(`  stop ${s}: ${f.note}`);
      if (s === 1 || s === 5) { await page.evaluate(() => document.querySelector('#card #gchist')?.scrollIntoView({ block: 'start' })); await page.waitForTimeout(300); await page.screenshot({ path: `shots/${prefix}-${s === 1 ? 2 : 3}-card-stop-${s}.png` }); }
    }
    console.log('  foot:', flat(await page.evaluate(() => document.querySelector('#card .gcfoot')?.textContent), 300));
    const footSeen = await page.evaluate(() => { const f = document.querySelector('#card .gcfoot'); if (!f) return null; const r = f.getBoundingClientRect(); return { top: Math.round(r.top), bottom: Math.round(r.bottom), screen: window.innerHeight }; });
    console.log('  the Begin button row on screen:', JSON.stringify(footSeen));
    // Begin at Anything goes and look at the first month: the badge, the first card, the Powers tab.
    await page.click('.gcgo');
    await page.waitForFunction(() => { const g = window.theater.game(); return g && g.human === 'ITA'; }, null, { timeout: 120000 });
    await page.waitForTimeout(1500);
    console.log('  first card after Begin:', JSON.stringify(await cardText(page, 500)));
    await closeAll(page);
    console.log('  begun:', JSON.stringify(await flags(page)));
    console.log('  the badge:', JSON.stringify(await page.evaluate(() => { const r = document.querySelector('.rank'); return r ? { text: r.textContent.replace(/\s+/g, ' ').trim(), dial: !!r.querySelector('.rdial'), title: r.querySelector('.rdial')?.title ?? null, shown: r.offsetParent !== null } : null; })));
    await page.screenshot({ path: `shots/${prefix}-4-first-month.png` });
    const p = await powers(page);
    console.log('  Powers tab claims:', JSON.stringify(p));
    await page.evaluate(() => document.querySelector('.claimhead')?.scrollIntoView({ block: 'start' })); await page.waitForTimeout(300);
    await page.screenshot({ path: `shots/${prefix}-5-powers-claims.png` });
  } else if (mode === 'claims') {
    const [save, prefix, W = '1440', H = '900'] = rest;
    const page = await open(+W, +H, save);
    await cont(page);
    const f0 = await flags(page);
    console.log('loaded:', JSON.stringify(f0));
    console.log('Powers tab before:', JSON.stringify(await powers(page)));
    await page.evaluate(() => document.querySelector('.claimhead')?.scrollIntoView({ block: 'start' })); await page.waitForTimeout(300);
    await page.screenshot({ path: `shots/${prefix}-1-claims-before.png` });
    // A claim clicked shows its ground on the map.
    const sel0 = await page.evaluate(() => window.theater.ui?.sel ?? null);
    await page.evaluate(() => document.querySelector('.nat.claim')?.click()); await page.waitForTimeout(500);
    console.log('a claim clicked: selection', sel0, 'to', await page.evaluate(() => window.theater.ui?.sel ?? null), '; tab now', await page.evaluate(() => window.theater.ui?.tab ?? null));
    for (let i = 1; i <= 3; i++) {
      const before = (await flags(page)).turn;
      const cards = await month(page);
      const now = await flags(page);
      console.log(`month ${i}: turn ${before} to ${now.turn}; cards opened ${cards.length}: ${JSON.stringify(cards.map((c) => c.slice(0, 200)))}`);
      console.log('  news of the dial this month:', JSON.stringify(await dialNews(page, before)));
      const feed = await page.evaluate(() => { const rows = [...document.querySelectorAll('.news li, .newsrow, .headline, .feed li, #news li')].filter((x) => /presses its claim|marches for|stands by|sign a treaty/i.test(x.textContent)); return rows.slice(0, 6).map((x) => ({ cls: x.className, text: x.textContent.replace(/\s+/g, ' ').trim().slice(0, 160), shown: x.offsetParent !== null })); });
      console.log('  on screen in the news:', JSON.stringify(feed));
      await page.screenshot({ path: `shots/${prefix}-${i + 1}-after-month-${i}.png` });
      const p = await powers(page);
      console.log('  Powers tab:', JSON.stringify(p.rows));
      await page.evaluate(() => document.querySelector('.claimhead')?.scrollIntoView({ block: 'start' })); await page.waitForTimeout(300);
      await page.screenshot({ path: `shots/${prefix}-${i + 1}b-claims-after-month-${i}.png` });
      const war = await page.evaluate(() => { const g = window.theater.game(); return g.nations[g.human].atWar.slice(); });
      console.log('  at war with:', war.join(' '));
    }
  } else if (mode === 'book') {
    const [prefix, MONTHS = '11'] = rest;
    const page = await open(1440, 900);
    await page.waitForSelector('#card');
    await page.evaluate(() => { window.theater.newGame('GER', 1, 1, 'grand', 'colonel', true, false, '', 1); window.theater.refresh(); });
    await page.waitForTimeout(1500);
    console.log('first card:', JSON.stringify(await cardText(page, 400)));
    await closeAll(page);
    console.log('begun:', JSON.stringify(await flags(page)));
    for (let i = 0; i < +MONTHS; i++) {
      const before = (await flags(page)).turn;
      const cards = await month(page);
      const news = (await dialNews(page, before)).filter((x) => /History's book/.test(x));
      if (news.length || cards.some((c) => /book|history/i.test(c))) {
        console.log(`turn ${before}: news ${JSON.stringify(news)}; cards that speak of it: ${JSON.stringify(cards.filter((c) => /book|history/i.test(c)))}`);
        await page.screenshot({ path: `shots/${prefix}-book-closed-turn-${before + 1}.png` });
      }
    }
    console.log('at the end:', JSON.stringify(await flags(page)), 'flags:', JSON.stringify(await page.evaluate(() => Object.fromEntries(Object.entries(window.theater.game().flags).filter(([k]) => k.startsWith('hist'))))));
    await page.click('.rank'); await page.waitForSelector('#veil.open');
    await page.evaluate(() => document.querySelector('#card #sethist')?.scrollIntoView({ block: 'start' })); await page.waitForTimeout(300);
    console.log('settings say:', flat(await page.evaluate(() => document.querySelector('#card .dialclosed')?.textContent ?? 'nothing about a closed book'), 400));
    await page.screenshot({ path: `shots/${prefix}-settings-book-closed.png` });
  } else if (mode === 'settings') {
    const [save, prefix] = rest;
    const page = await open(1440, 900, save);
    await cont(page);
    console.log('loaded:', JSON.stringify(await flags(page)));
    await page.click('.rank'); await page.waitForSelector('#veil.open');
    await page.evaluate(() => document.querySelector('#card #sethist')?.scrollIntoView({ block: 'start' })); await page.waitForTimeout(300);
    console.log('settings note:', flat(await page.evaluate(() => document.querySelector('#card .gclevels.dial ~ .gclevelnote')?.textContent), 700));
    await page.screenshot({ path: `shots/${prefix}-1-settings.png` });
    await page.click('#card .gclevel[data-act="sethist"][data-stop="1"]'); await page.waitForTimeout(500);
    console.log('turned to 1:', JSON.stringify(await flags(page)));
    console.log('note after:', flat(await page.evaluate(() => document.querySelector('#card .gclevels.dial ~ .gclevelnote')?.textContent), 900));
    await page.evaluate(() => document.querySelector('#card #sethist')?.scrollIntoView({ block: 'start' })); await page.waitForTimeout(300);
    await page.screenshot({ path: `shots/${prefix}-2-turned-to-1.png` });
    await closeAll(page);
    console.log('Powers tab after turning down:', JSON.stringify(await powers(page)));
    await page.waitForTimeout(1500);
    await page.reload(); await page.waitForTimeout(1000);
    await cont(page);
    console.log('after a reload:', JSON.stringify(await flags(page)));
  }
  console.log('page errors:', errors.length ? JSON.stringify([...new Set(errors)].slice(0, 6)) : 'none');
  await browser.close();
})();

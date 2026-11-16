// Version 82.1 on the page: the raid card's forecast in two columns, BY DAY and BY NIGHT (ruling 107(d)), and the
// siege on the month card (rulings 104 to 106).
// usage: [TC_PAGE=file:///home/user/tc821/dist/play.html] [TC_DEPTH=supreme] [TC_SEED=n] node ui50.cjs NAT MONTHS PREFIX [raid|noraid]
// With "raid" (default) the player opens the air desk, picks a bombing raid and its first target, reads both columns,
// ticks an escort, turns to night (the escort "stays home"), turns back to day, and flies the raid by night. Then it
// ends MONTHS months and reads every row of the month card (the tags HELD, LOST, WON and the siege lines).
// Screenshots go to shots/PREFIX-*.png; page errors are listed.
const { chromium } = require('playwright');
const [nat = 'GER', monthsS = '2', prefix = '100-v821', raidS = 'raid'] = process.argv.slice(2);
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc821/dist/play.html';
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 200)));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(`console: ${m.text().slice(0, 200)}`); });
  const say = (k, v) => console.log(`${k}: ${typeof v === 'string' ? v : JSON.stringify(v)}`);
  const shot = (name) => page.screenshot({ path: `shots/${prefix}-${name}.png` });
  const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  const shut = async () => { for (let i = 0; i < 12 && await veil(); i++) { await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(200); } };
  const txt = (sel, n = 300) => page.evaluate(([sel, n]) => [...document.querySelectorAll(sel)].map((e) => (e.innerText || e.textContent || '').replace(/\s+/g, ' ').trim().slice(0, n)), [sel, n]);
  const click = (sel) => page.evaluate((sel) => { const b = document.querySelector(sel); if (!b || b.disabled) return b ? 'disabled' : null; b.click(); return (b.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 120) || 'clicked'; }, sel);
  await page.goto(PAGE); await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); }); await page.reload(); await page.waitForSelector('#card');
  // The air desk is the player's own only at Supreme command (TC_DEPTH: field, theatre or supreme). TC_SEED fixes the war.
  if (process.env.TC_SEED) await page.evaluate(([nat, seed, depth]) => window.theater.newGame(nat, seed, 1, 'grand', 'colonel', true, false, depth, 3), [nat, Number(process.env.TC_SEED), process.env.TC_DEPTH || 'supreme']);
  else { await page.click(`.gcnat[data-nat="${nat}"]`); await page.waitForTimeout(250); await page.click(`[data-act="gcdepth"][data-depth="${process.env.TC_DEPTH || 'supreme'}"]`); await page.waitForTimeout(250); await page.click('.gcgo'); }
  await page.waitForFunction((nat) => { const g = window.theater.game(); return g && g.human === nat && !document.querySelector('#veil.open .busy'); }, nat, { timeout: 300000 });
  await page.waitForTimeout(1500); await shut();
  if (raidS === 'raid') {
    say('tabs', await page.evaluate(() => [...document.querySelectorAll('[data-act="tab"]')].map((b) => b.dataset.tab).join(' ')));
    say('air tab', await click('[data-act="tab"][data-tab="air"]')); await page.waitForTimeout(400);
    say('plan a raid', await click('[data-act="raidmenu"][data-id="-1"]')); await page.waitForTimeout(400);
    say('kinds of raid', await page.evaluate(() => [...document.querySelectorAll('[data-act="raidkind"]')].map((b) => `${b.dataset.kind}: ${(b.innerText || '').replace(/\s+/g, ' ').trim()}${b.disabled ? ' (off: ' + (b.title || '') + ')' : ''}`)));
    say('bombing', await click('[data-act="raidkind"][data-kind="bomb"]')); await page.waitForTimeout(400);
    say('targets', (await txt('[data-act="raidtarget"]', 110)).slice(0, 6));
    say('first target', await click('[data-act="raidtarget"]')); await page.waitForTimeout(600);
    const card = async (label) => {
      say(`${label}: head`, await txt('#card h3', 80));
      say(`${label}: in common`, await txt('.waycommon', 120));
      say(`${label}: the two columns`, await page.evaluate(() => [...document.querySelectorAll('.daynight .way')].map((b) => `${b.className}${b.disabled ? ' [disabled]' : ''} pressed=${b.getAttribute('aria-pressed')} | ${(b.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 420)}`)));
      say(`${label}: seats`, await page.evaluate(() => [...document.querySelectorAll('#card .pickrow.seat')].map((b) => `${b.className.replace('pickrow seat', '').trim() || 'open'}: ${(b.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 130)}`)));
      say(`${label}: buttons at the foot`, await txt('#card footer button', 60));
    };
    await card('the raid card as it opens'); await shot('1-raid-day');
    // Tick a fighter that could join as escort, if none goes yet.
    const ticked = await page.evaluate(() => { const rows = [...document.querySelectorAll('#card .pickrow.seat:not(.pick):not(.ghost)')].filter((b) => !b.disabled && /fighter|escort|Jagd|Fighter/i.test(b.innerText || '')); if (!rows.length) return null; rows[0].click(); return (rows[0].innerText || '').replace(/\s+/g, ' ').trim().slice(0, 120); });
    say('an escort ticked', ticked); await page.waitForTimeout(500);
    if (ticked) { await card('with the escort'); await shot('2-raid-day-escort'); }
    say('BY NIGHT pressed', await click('.daynight .way.night')); await page.waitForTimeout(600);
    await card('by night'); await shot('3-raid-night');
    say('BY DAY pressed', await click('.daynight .way.day')); await page.waitForTimeout(600);
    await card('back to day'); 
    say('BY NIGHT pressed again', await click('.daynight .way.night')); await page.waitForTimeout(500);
    say('fly it', await click('#card footer [data-act="doraid"]')); await page.waitForTimeout(1500);
    say('after the raid', (await txt('#card', 900))[0] || '(no card)'); await shot('4-raid-flown');
    say('the raid as the game has it', await page.evaluate(() => { const g = window.theater.game(); const h = g.headlines.filter((x) => x.turn === g.turn).slice(-3).map((x) => `[${x.kind}] ${x.text.slice(0, 220)}`); return h; }));
    await shut();
  }
  // A country that is attacked as the war opens places its army first (FINISH DEPLOYMENT): the staff's advice is taken.
  if (await page.evaluate(() => /DEPLOYMENT/i.test(document.querySelector('[data-act="end"]')?.innerText || ''))) {
    say('deployment: the staff places the army', await click('[data-act="eveplan"]')); await page.waitForTimeout(500);
    say('deployment finished', await click('[data-act="end"]')); await page.waitForTimeout(700);
    if (await veil()) { say('a card after deployment', (await txt('#card', 300))[0]); await page.evaluate(() => { const bs = [...document.querySelectorAll('#card button')]; const b = bs.find((x) => /finish|deploy|begin|yes/i.test(x.innerText || '')) || bs[bs.length - 1]; if (b) b.click(); }); }
    await page.waitForFunction(() => !document.querySelector('#veil.open .busy') && !/DEPLOYMENT/i.test(document.querySelector('[data-act="end"]')?.innerText || ''), null, { timeout: 300000 }).catch(() => errors.push('deployment did not finish'));
    await page.waitForTimeout(2500);
    if (await veil()) { say('the card after the opening blow', (await txt('#card', 500))[0]); await shot('5-opening-card'); await shut(); }
    say('the button now', (await txt('[data-act="end"]', 60))[0]);
  }
  for (let m = 1; m <= Number(monthsS); m++) {
    const before = await page.evaluate(() => window.theater.game().turn);
    await page.evaluate(() => { const b = document.querySelector('[data-act="end"]'); if (b) b.click(); }); await page.waitForTimeout(700);
    { const ask = (await txt('#card', 300))[0] || ''; if ((await veil()) && !/Dispatches/.test(ask)) { await page.evaluate(() => { const bs = [...document.querySelectorAll('#card button')]; const b = bs.find((x) => /end the month|END MONTH|End it/i.test(x.innerText || '')) || bs[bs.length - 1]; if (b) b.click(); }); } }
    await page.waitForFunction((t) => { const g = window.theater.game(); return g.turn > t && !document.querySelector('#veil.open .busy'); }, before, { timeout: 300000 }).catch(() => errors.push(`month ${m}: the month did not turn`));
    await page.waitForTimeout(2500);
    say(`month ${m}: where the player stands`, await page.evaluate(() => { const g = window.theater.game(); const me = g.human; if (!me) return 'no player any more (watching)'; const own = g.regions.filter((r) => r.owner === me && !r.overseas); const by = {}; for (const r of own) by[r.ctrl] = (by[r.ctrl] || 0) + 1; return `${me} ${g.nations[me].alive ? 'stands' : 'is out'}, turn ${g.turn}; its ${own.length} home provinces are held by ${Object.entries(by).map(([k, v]) => k + ' ' + v).join(', ')}; lost: ${own.filter((r) => r.ctrl !== me).map((r) => r.name + ' (since turn ' + r.since + ')').join(', ')}`; }));
    if (await veil() && !(await txt('.mc-head', 50)).length) { say(`month ${m}: another card is open`, (await txt('#card', 900))[0]); await shot(`5-month${m}-other-card`); }
    say(`month ${m}: the month card, head`, await txt('.mc-head', 200));
    say(`month ${m}: red bands`, await txt('.mc-banner', 260));
    say(`month ${m}: tiles`, await txt('.mc-tile', 60));
    say(`month ${m}: lead story`, await txt('.mc-leadcol', 520));
    say(`month ${m}: rows`, await txt('.mc-row', 260));
    say(`month ${m}: the three boxes`, await txt('.mc-box', 520));
    say(`month ${m}: every line on the card that reads "No"`, await page.evaluate(() => [...document.querySelectorAll('#veil.open #card *')].filter((e) => !e.children.length && /^No\.?$/.test((e.textContent || '').trim())).map((e) => `${e.className || e.tagName} in ${(e.closest('.mc-box, .mc-row, .mc-leadcol')?.className || '?')}: "${(e.parentElement?.innerText || '').replace(/\s+/g, ' ').slice(0, 80)}"`)));
    say(`month ${m}: sieges as the game has them`, await page.evaluate(() => { const g = window.theater.game(); return g.regions.filter((r) => r.garr && r.garr.length && r.units.some((u) => g.nations[u.nat].atWar.includes(r.ctrl))).map((r) => `${r.name} (${r.ctrl}; inside ${r.garr.length} corps; outside ${r.units.filter((u) => u.kind !== 'fort').map((u) => u.nat).join(' ')})`); }));
    await shot(`5-month${m}-card`);
    await shut(); await page.waitForTimeout(300);
  }
  say('page errors', errors);
  await browser.close();
})();

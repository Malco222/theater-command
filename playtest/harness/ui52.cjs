// Version 82.1 on the page, ruling 103(c): a player's Italy in a history campaign is taken out by Rome's change of
// government and the armistice only in the campaign that opens in July 1943.
// usage: [TC_PAGE=...] node ui52.cjs FROM [UNTIL=51] [PREFIX]   (FROM: the month the Italian seat is taken: 13 the
// desert 1940, 14 the Balkans, 33 the desert 1942, 39 Torch, 47 Italy 1943; "list" prints the seats the menu offers)
// The seat is taken from the menu of history campaigns; the player gives no orders and ends each month. Each month
// one line: Italy standing or out, the two events, which of the six grounds an Ally holds, the card's red bands.
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc821/dist/play.html';
const [fromS = 'list', untilS = '51', prefix = '102-stake'] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 300)));
  await page.goto(PAGE); await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); }); await page.reload(); await page.waitForSelector('#card');
  await page.evaluate(() => document.querySelector('[data-act="menu"][data-menu="scenarios"]')?.click()); await page.waitForTimeout(500);
  const seats = await page.evaluate(() => [...document.querySelectorAll('#card [data-act="start"]')].map((b) => `${b.dataset.nat || '-'} from ${b.dataset.from} view "${b.dataset.view || ''}": ${(b.innerText || '').replace(/\s+/g, ' ').slice(0, 110)}`));
  if (fromS === 'list') { for (const s of seats) console.log(s); await browser.close(); return; }
  const picked = await page.evaluate((from) => { const b = [...document.querySelectorAll('#card [data-act="start"]')].find((b) => b.dataset.nat === 'ITA' && b.dataset.from === from); if (!b) return null; const t = (b.innerText || '').replace(/\s+/g, ' ').slice(0, 300); b.click(); return t; }, fromS);
  console.log(`the seat: ${picked}`);
  if (!picked) { await browser.close(); return; }
  await page.waitForFunction(() => { const g = window.theater.game(); return g && g.human === 'ITA' && !document.querySelector('#veil.open .busy'); }, null, { timeout: 300000 });
  await page.waitForTimeout(2500);
  const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  const cardText = (n = 700) => page.evaluate((n) => (document.querySelector('#card')?.innerText || '').replace(/\s+/g, ' ').slice(0, n), n);
  if (await veil()) { console.log(`the card that opens the campaign: ${await cardText(1100)}`); await page.screenshot({ path: `shots/${prefix}-${fromS}-0-open.png` }); }
  console.log(`seat recorded: ${await page.evaluate(() => { const g = window.theater.game(); return `flags.seat ${g.flags.seat}, turn ${g.turn}, open ${g.flags.open}, goals: ${(document.querySelector('.goals, #goals')?.innerText || '').replace(/\s+/g, ' ').slice(0, 300)}`; })}`);
  const SIX = ['Sicily', 'Sardinia', 'Calabria', 'Apulia', 'Naples', 'Rome'];
  for (let m = 1; m <= 40; m++) {
    for (let i = 0; i < 6 && await veil(); i++) { await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(200); }
    const before = await page.evaluate(() => window.theater.game().turn);
    if (before >= Number(untilS)) break;
    const top = await page.evaluate(() => (document.querySelector('[data-act="end"]')?.innerText || '').replace(/\s+/g, ' '));
    if (/DEPLOYMENT/i.test(top)) { await page.evaluate(() => document.querySelector('[data-act="eveplan"]')?.click()); await page.waitForTimeout(300); }
    else if (!/END MONTH/i.test(top)) { console.log(`the top button reads "${top}": stopping`); break; }
    await page.evaluate(() => document.querySelector('[data-act="end"]')?.click()); await page.waitForTimeout(700);
    await page.evaluate(() => { const v = document.querySelector('#veil'); if (!v || !v.classList.contains('open')) return; if (!/BEFORE THE MONTH ENDS/i.test(document.querySelector('#card').innerText)) return; [...document.querySelectorAll('#card button')].find((x) => /end the month/i.test(x.innerText || ''))?.click(); });
    await page.waitForFunction((t) => { const g = window.theater.game(); return (g.turn > t || !g.human) && !document.querySelector('#veil.open .busy'); }, before, { timeout: 300000 }).catch(() => errors.push(`month ${m}: the month did not turn`));
    await page.waitForTimeout(1800);
    const line = await page.evaluate((SIX) => { const g = window.theater.game(), I = g.nations.ITA; const ally = (id) => ['allies', 'allies_cobelligerent'].includes(g.nations[id]?.align); const bands = [...document.querySelectorAll('#veil.open .mc-banner')].map((e) => (e.innerText || '').replace(/\s+/g, ' ').slice(0, 200)); const D = ['September', 'October', 'November', 'December', 'January', 'February', 'March', 'April', 'May', 'June', 'July', 'August']; const date = `${D[(g.turn - 1) % 12]} ${1939 + Math.floor((g.turn + 7) / 12)}`; return `as ${date} opens (turn ${g.turn}): player ${g.human}; Italy ${I.alive ? 'stands' : 'OUT (' + JSON.stringify(I.fellOn) + ')'}, ${g.corps.filter((c) => c.nat === 'ITA' && !c.dead && c.kind !== 'fort').length} field corps; mussolini-falls ${g.flags['event:mussolini-falls'] ?? 'no'}, armistice ${g.flags['event:italian-armistice'] ?? 'no'}; Allies hold ${SIX.filter((n) => ally(g.byName ? g.byName[n]?.ctrl : g.regions.find((r) => r.name === n)?.ctrl)).join(', ') || 'none of the six'}; red bands ${JSON.stringify(bands)}`; }, SIX);
    console.log(line);
    { const told = await page.evaluate(() => { const g = window.theater.game(); return g.headlines.filter((h) => h.turn >= g.turn - 1 && /Mussolini|Grand Council|the King|armistice|Badoglio/i.test(h.text)).map((h) => `t${h.turn} [${h.kind}${h.warn ? ' warn ' + h.warn : ''}] ${h.text.slice(0, 230)}`); }); if (told.length) console.log(`   told: ${JSON.stringify(told)}`); }
    if (process.env.TC_CARD && await veil()) { console.log(`   the card: ${await cardText(1700)}`); await page.screenshot({ path: `shots/${prefix}-${fromS}-card-t${before + 1}.png` }); }
    if (/OUT \(|player null/.test(line)) { console.log(`the card: ${await cardText(900)}`); await page.screenshot({ path: `shots/${prefix}-${fromS}-out.png` }); break; }
    if (/Rome|Mussolini|armistice/i.test(line.split('red bands')[1] || '')) await page.screenshot({ path: `shots/${prefix}-${fromS}-t${before + 1}.png` });
  }
  console.log('page errors: ' + JSON.stringify(errors));
  await browser.close();
})();

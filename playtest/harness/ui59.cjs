// Version 83.1: the front door by hand. usage: [TC_PAGE=...] node ui59.cjs PREFIX [SAVE.json]
// 1. The start screen as the United States: what stands under the years (a long peace comes first), year by year.
// 2. With a save in the browser: a new campaign asks before it replaces the saved one; "Keep it" keeps it.
// 3. Italy from 1943: the first card ("As July 1943 opens") and its warning band, then the card a month on.
const fs = require('fs');
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc831/dist/play.html';
const [prefix, save] = process.argv.slice(2);
const txt = "(e) => (e ? (e.innerText || e.textContent || '').replace(/\\s+/g, ' ').trim() : '')";
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 300)));
  await page.goto(PAGE); await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); }); await page.reload(); await page.waitForSelector('#card'); await page.waitForTimeout(800);
  const under = () => page.evaluate(() => { const e = document.querySelector('.gcunder'); return e ? e.innerText.replace(/\s+/g, ' ').trim() : null; });
  const years = () => page.evaluate(() => [...document.querySelectorAll('[data-act="gcyear"]')].map((b) => `${b.dataset.year}${b.disabled ? ' (off)' : ''}${b.classList.contains('on') ? ' [on]' : ''}`));
  const pick = async (nat, year) => { await page.evaluate((nat) => document.querySelector(`[data-act="gcnat"][data-nat="${nat}"]`)?.click(), nat); await page.waitForTimeout(300); if (year) { await page.evaluate((year) => document.querySelector(`[data-act="gcyear"][data-year="${year}"]`)?.click(), year); await page.waitForTimeout(300); } };
  // 1. The United States, year by year; Germany for comparison.
  await pick('USA'); console.log('USA picked: years ' + JSON.stringify(await years()));
  for (const y of ['1939', '1940', '1941', '1942', '1943', '1944']) { await pick('USA', y); console.log(`USA ${y}: ` + JSON.stringify(await under())); if (y === '1939') { await page.screenshot({ path: `shots/${prefix}-door-usa-1939.png` }); const box = await page.evaluate(() => { const e = document.querySelector('.gcunder'); if (!e) return null; const r = e.getBoundingClientRect(), s = getComputedStyle(e); return { top: Math.round(r.top), height: Math.round(r.height), size: s.fontSize, colour: s.color, onScreen: r.bottom <= innerHeight }; }); console.log('USA 1939, where the line stands: ' + JSON.stringify(box)); } }
  await pick('GER', '1939'); console.log('GER 1939: ' + JSON.stringify(await under()));
  await pick('UK', '1939'); console.log('UK 1939: ' + JSON.stringify(await under()));
  // What the campaign then is: begin as the United States in 1939 and read the first card.
  await pick('USA', '1939');
  await page.evaluate(() => document.querySelector('.gcfoot [data-act="start"]')?.click()); await page.waitForTimeout(3500);
  console.log('USA 1939 begun: ' + JSON.stringify(await page.evaluate(() => { const g = window.theater.game(); const c = document.querySelector('#card'); return { human: g?.human, turn: g?.turn, atWar: g ? g.nations[g.human].atWar.length : null, card: (c.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 600) }; })));
  await page.screenshot({ path: `shots/${prefix}-door-usa-first-card.png` });
  // 2. A save in the browser, then a new campaign.
  if (save) {
    await page.evaluate((text) => { localStorage.setItem('theater-command-save-v1', text); }, fs.readFileSync(save, 'utf8')); await page.reload(); await page.waitForSelector('#card'); await page.waitForTimeout(800);
    const bytes = () => page.evaluate(() => (localStorage.getItem('theater-command-save-v1') || '').length);
    const b0 = await bytes();
    console.log('the front door with a save: ' + JSON.stringify(await page.evaluate(() => { const b = document.querySelector('[data-act="continue"]'); return b ? b.innerText.replace(/\s+/g, ' ').trim() : null; })));
    await pick('ITA', '1943');
    await page.evaluate(() => document.querySelector('.gcfoot [data-act="start"]')?.click()); await page.waitForTimeout(900);
    const q = () => page.evaluate(() => { const c = document.querySelector('#card'); const t = (e) => (e ? e.innerText.replace(/\s+/g, ' ').trim() : ''); return { kick: t(c.querySelector('.kick')), head: t(c.querySelector('h2')), body: t(c.querySelector('.body')), buttons: [...c.querySelectorAll('footer button')].map((b) => `${t(b)} [${b.className}]`), human: window.theater.game()?.human ?? null }; });
    console.log('the question: ' + JSON.stringify(await q())); await page.screenshot({ path: `shots/${prefix}-door-question.png` });
    await page.evaluate(() => document.querySelector('#card footer [data-act="close"]')?.click()); await page.waitForTimeout(600);
    console.log(`after "Keep it": save bytes ${b0} then ${await bytes()}; game in play: ` + JSON.stringify(await page.evaluate(() => window.theater.game()?.human ?? null)));
    // The same press again and "Start the new one".
    await pick('ITA', '1943'); await page.evaluate(() => document.querySelector('.gcfoot [data-act="start"]')?.click()); await page.waitForTimeout(900);
    await page.evaluate(() => document.querySelector('#card footer [data-act="start"][data-sure="1"]')?.click()); await page.waitForTimeout(3500);
  } else { await page.evaluate(() => { localStorage.removeItem('theater-command-save-v1'); }); await page.reload(); await page.waitForSelector('#card'); await page.waitForTimeout(800); await pick('ITA', '1943'); await page.evaluate(() => document.querySelector('.gcfoot [data-act="start"]')?.click()); await page.waitForTimeout(3500); }
  // 3. Italy's first card and the card a month on.
  const card = () => page.evaluate(() => { const c = document.querySelector('#card'); const t = (e) => (e ? e.innerText.replace(/\s+/g, ' ').trim() : ''); const g = window.theater.game(); return { me: g?.human, turn: g?.turn, open: document.querySelector('#veil')?.classList.contains('open'), title: t(c.querySelector('h2')), bands: [...c.querySelectorAll('.mc-band, .band')].map((b) => `${t(b)} | colour ${getComputedStyle(b).borderLeftColor || ''} ${getComputedStyle(b).backgroundColor}`), text: t(c).slice(0, 900) }; });
  console.log('Italy 1943, the first card: ' + JSON.stringify(await card())); await page.screenshot({ path: `shots/${prefix}-italy-first-card.png` });
  await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(400);
  await page.evaluate(() => document.querySelector('[data-act="end"]')?.click()); await page.waitForTimeout(600);
  for (let k = 0; k < 6; k++) { const sure = await page.evaluate(() => { const b = [...document.querySelectorAll('#card footer button')].find((x) => /end the month|end month|yes|go on/i.test(x.innerText)); if (b && document.querySelector('#veil')?.classList.contains('open')) { b.click(); return b.innerText; } return null; }); if (!sure) break; await page.waitForTimeout(500); }
  for (let k = 0; k < 90; k++) { const t = await page.evaluate(() => window.theater.game()?.turn); if (t === 48) break; await page.waitForTimeout(1000); }
  await page.waitForTimeout(2500);
  console.log('Italy, a month on: ' + JSON.stringify(await card())); await page.screenshot({ path: `shots/${prefix}-italy-second-card.png` });
  console.log('page errors: ' + JSON.stringify(errors));
  await browser.close();
})();

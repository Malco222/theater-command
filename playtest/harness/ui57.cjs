// A saved game from a headless campaign (probe88, TC_SAVE) opened on the page, to look at a moment as the player sees
// it. usage: [TC_PAGE=...] node ui57.cjs SAVE.json PREFIX ACTION...   actions: raid:Lille (the raid card for a bombing
// raid on that target), army:Riviera (take the army there in hand: the tray, the rail line), lamps (cut lines on the
// map), build:Ruhr (the BUILD tab's works for that province), card (the month card), tabs
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
    if (what === 'news') {
      const g = await page.evaluate((arg) => { const g = window.theater.game(); const re = new RegExp(arg || 'bomb|raid|railway|guns', 'i'); return g.headlines.filter((h) => h.turn >= g.turn - 1 && re.test(h.text)).map((h) => `t${h.turn} [${h.kind}] ${h.text.slice(0, 220)}`).slice(0, 14); }, arg);
      console.log('news: ' + JSON.stringify(g, null, 1));
    }
  }
  console.log('page errors: ' + JSON.stringify(errors));
  await browser.close();
})();

// The save guard's page half: one of Brian's saved campaigns opened in the built browser game the way he would open
// it (the start screen's "Continue your campaign"), then a few months ended with real presses of END THE MONTH and no
// orders given. It reads each month's card, keeps a picture of it, and collects every error the page raises.
//   node savepage.cjs <TREE> <SAVE> <OUT dir for pictures> [months]
// Prints one line a check ("ok", "WATCH" or "FAIL") and ends with "errors: [...]": the page's own errors and every
// failed check. TREE/dist/play.html must be built (chain.sh builds it). Uses the Playwright already on the machine.
const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright');
(async () => {
  const [TREE, SAVE, OUT, MONTHS = '2'] = process.argv.slice(2);
  const months = Math.max(1, Number(MONTHS) || 2);
  if (OUT) fs.mkdirSync(OUT, { recursive: true });
  const text = fs.readFileSync(SAVE, 'utf8');
  const raw = JSON.parse(text);
  const expectPath = SAVE.replace(/\.json$/, '.expect.json');
  const expect = fs.existsSync(expectPath) ? JSON.parse(fs.readFileSync(expectPath, 'utf8')) : {};
  const tag = path.basename(SAVE, '.json');
  const b = await chromium.launch();
  const page = await (await b.newContext({ viewport: { width: 1600, height: 1000 } })).newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(`page error: ${String(e).slice(0, 300)}`));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(`console: ${m.text().slice(0, 300)}`); });
  const say = (kind, what) => { console.log(`${kind === 'ok' ? 'ok   ' : kind === 'WATCH' ? 'WATCH' : 'FAIL '} ${what}`); if (kind === 'FAIL') errors.push(`FAIL ${what}`.slice(0, 300)); };
  const check = (ok, what, got) => say(ok ? 'ok' : 'FAIL', `${what}${!ok && got !== undefined ? `: ${typeof got === 'string' ? got : JSON.stringify(got)}` : ''}`);
  const BAD_TEXT = /\bundefined\b|\bNaN\b|\[object|\bnull\b|\bInfinity\b/;
  const OLD_SHIPS = /(?<! Air) Squadron\b| Escort Group\b| Submarine Flotilla\b/;
  const closeCards = async () => { for (let i = 0; i < 10; i++) { const open = await page.evaluate(() => !!document.querySelector('#veil.open #card')); if (!open) break; await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(120); } };

  await page.goto(`file://${TREE}/dist/play.html`);
  await page.waitForFunction(() => !!window.theater);
  await page.evaluate((t) => { localStorage.clear(); localStorage.setItem('theater-command-save-v1', t); }, text);
  await page.reload();
  await page.waitForFunction(() => !!window.theater);
  await page.waitForTimeout(800);
  // The start screen offers his campaign by name and date.
  const offer = await page.evaluate(() => { const el = document.querySelector('[data-act="continue"]'); return el ? el.innerText.replace(/\s+/g, ' ') : ''; });
  check(!!offer && !BAD_TEXT.test(offer), `the start screen offers it: "${offer}"`, offer || 'no Continue button');
  if (!offer) { console.log('errors:', JSON.stringify(errors)); await b.close(); return; }
  await page.click('[data-act="continue"]').catch(async () => { await page.evaluate(() => window.theater.continueGame()); });
  await page.waitForTimeout(2500);
  const at = await page.evaluate(() => { const g = window.theater.game(); return { human: g.human, turn: g.turn }; });
  check(at.human === raw.human && at.turn === raw.turn, `Continue opens it: ${at.human}, turn ${at.turn}`, at);
  await closeCards();
  if (OUT) await page.screenshot({ path: `${OUT}/${tag}-opened.png` });
  // What the page shows of his fleets: their names as the fleets list writes them.
  const fleetNames = await page.evaluate(() => { const g = window.theater.game(); return g.fleets.filter((f) => f.nat === g.human).map((f) => f.name); });
  check(fleetNames.every((n) => !OLD_SHIPS.test(n)), `his ships are fleets: ${fleetNames.join(', ') || 'none'}`, fleetNames);

  // The red row (ruling 211(c), Version 84.2.6), where the tree has it: first in the tray, and a press picks up the
  // army the save was to name (December 1940: the army round Vancouver), selects where it stands and says the cure.
  if (expect.alarm) {
    const has = await page.evaluate(() => window.theater.ui.alarmAt !== undefined);
    if (!has) console.log('      (the red row: not in this tree, passed over)');
    else {
      const first = await page.evaluate(() => { const el = document.querySelector('button.alert'); return el ? { kind: el.dataset.kind, text: el.innerText.replace(/\s+/g, ' ') } : null; });
      check(!!first && first.kind === 'home' && !BAD_TEXT.test(first.text), `the red row is first in the tray: "${first ? first.text.slice(0, 140) : ''}"`, first);
      if (first && first.kind === 'home') {
        await page.click('button.alert[data-kind="home"]').catch(() => {});
        await page.waitForTimeout(900);
        const got = await page.evaluate((army) => { const t = window.theater, g = t.game(); const ids = [...t.ui.picked]; const at = [...new Set(ids.map((id) => g.regions[g.corps.find((c) => c.id === id).reg].name))]; return { ids, at, sel: t.ui.sel >= 0 ? g.regions[t.ui.sel].name : '', msg: t.ui.msg || '', army }; }, expect.alarm.army);
        check(got.ids.length > 0 && got.at.length === 1 && got.at[0] === expect.alarm.army && got.sel === expect.alarm.army && /in hand/.test(got.msg), `a press on it picks up the army round ${expect.alarm.army}: "${got.msg}"`, got);
        if (OUT) await page.screenshot({ path: `${OUT}/${tag}-red-row-pressed.png` });
        await page.evaluate(() => { const t = window.theater; t.ui.picked.clear(); t.ui.sel = -1; t.ui.msg = ''; t.refresh(); });
      }
    }
  }

  // END THE MONTH as he presses it, sure of it, until the month's card is up.
  const endMonth = async () => {
    const t0 = await page.evaluate(() => window.theater.game().turn);
    for (let n = 0; n < 3; n++) {
      await page.evaluate(() => { const t = window.theater; t.openModal(null); const el = document.querySelector('[data-act="end"]'); if (el) { el.dataset.sure = '1'; el.click(); } });
      const on = await page.waitForFunction((t0) => window.theater.game().turn > t0 && !!document.querySelector('.mc'), t0, { timeout: 120000 }).then(() => true).catch(() => false);
      if (on) return true;
    }
    return false;
  };
  for (let m = 1; m <= months; m++) {
    const t0 = Date.now();
    const turn = await page.evaluate(() => window.theater.game().turn);
    const ok = await endMonth();
    await page.waitForTimeout(600);
    const card = await page.evaluate(() => { const c = document.querySelector('.mc'); return c ? c.innerText.replace(/\s+/g, ' ') : ''; });
    const bands = await page.evaluate(() => [...document.querySelectorAll('.mc .mc-banner')].map((el) => ({ word: (el.querySelector('.k') || {}).textContent || '', head: (el.querySelector('b') || {}).textContent || '' })));
    if (OUT) await page.screenshot({ path: `${OUT}/${tag}-month-${m}.png` });
    check(ok && card.length > 0, `month ${m}: END THE MONTH brings up the month's card (${((Date.now() - t0) / 1000).toFixed(1)} s)`, card.slice(0, 80));
    check(!BAD_TEXT.test(card), `month ${m}: the card reads whole`, (card.match(new RegExp(`.{0,60}(${BAD_TEXT.source}).{0,60}`)) || [''])[0]);
    if (OLD_SHIPS.test(card)) say('FAIL', `month ${m}: the card calls a fleet by its old name: ${(card.match(/.{0,50}(Squadron|Escort Group|Submarine Flotilla).{0,30}/) || [''])[0]}`);
    // Britain's one roll falls as the first month after loading ends: the card carries her answer in a band.
    if (m === 1 && expect.britain) {
      const has = await page.evaluate(() => typeof window.theater.game().flags === 'object' && Object.keys(window.theater.game().flags).some((k) => k.startsWith('domroll:')));
      if (!has) console.log('      (Britain and her dominions: not in this tree, passed over)');
      else {
        const want = expect.britain.answer === 'answers' ? { word: 'WAR', head: 'Britain stands by Canada' } : { word: 'NEWS', head: 'Britain stays out of Canada\'s war' };
        const band = bands.find((x) => /^Britain st/.test(x.head));
        check(!!band && band.word === want.word && band.head === want.head, `month ${m}: the card's band says what Brian was told: "${want.head}"`, bands);
      }
    }
    await closeCards();
    const turnNow = await page.evaluate(() => window.theater.game().turn);
    if (turnNow !== turn + 1) { say('FAIL', `month ${m}: the calendar went from turn ${turn} to ${turnNow}`); break; }
  }
  // The page keeps the campaign it has played on, and that save opens again.
  await page.waitForTimeout(900);
  const kept = await page.evaluate(() => { try { const t = localStorage.getItem('theater-command-save-v1'); const d = JSON.parse(t); return { turn: d.turn, human: d.human, kb: Math.round(t.length / 1024) }; } catch (e) { return { err: String(e) }; } });
  check(kept.turn === raw.turn + months && kept.human === raw.human, `the browser keeps the campaign as played on: ${kept.human}, turn ${kept.turn}, ${kept.kb} KB`, kept);
  console.log('errors:', JSON.stringify(errors));
  await b.close();
})();

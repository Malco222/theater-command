// A monkey at the controls: random clicks on every button, province, sea and army, with months ended now and then.
// Reports every page error with the last actions that led to it.
const { chromium } = require('playwright');
const [nat, seedArg, stepsArg, fromArg, levelArg, depthArg] = process.argv.slice(2);
const depth = depthArg || undefined; // Version 66: field | theatre | supreme; left out, the page's own default
const seed = Number(seedArg ?? 1), steps = Number(stepsArg ?? 800), from = Number(fromArg ?? 1), level = levelArg ?? 'colonel';
let s = seed * 2654435761 % 4294967296;
const rnd = () => { s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; return s / 4294967296; };
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
  const errors = [];
  const trail = [];
  page.on('console', (m) => { if (m.type() === 'error') errors.push({ text: `console: ${m.text()}`, trail: trail.slice(-8) }); });
  page.on('pageerror', (e) => errors.push({ text: `${e.message}\n${(e.stack || '').split('\n').slice(0, 4).join('\n')}`, trail: trail.slice(-8) }));
  await page.addInitScript(() => { try { localStorage.setItem('tc-watch', '0'); } catch {} });
  await page.goto(process.env.TC_PAGE || 'file:///home/user/theater-command/dist/play.html');
  await page.waitForTimeout(800);
  await page.evaluate(([nat, seed, from, level, depth]) => (depth ? window.theater.newGame(nat, seed, from, 'grand', level, true, false, depth) : window.theater.newGame(nat, seed, from, 'grand', level)), [nat, seed, from, level, depth ?? null]);
  await page.waitForTimeout(500);
  const map = await page.locator('#mapwrap').boundingBox();
  let months = 0;
  for (let i = 0; i < steps; i++) {
    const roll = rnd();
    try {
      if (roll < 0.5) {
        // Any visible control.
        const pick = await page.evaluate((r) => {
          const all = [...document.querySelectorAll('[data-act]')].filter((e) => e.offsetParent !== null || e.closest('svg')).filter((e) => !['new', 'menu', 'auto', 'next', 'start', 'continue', 'load', 'gcnat', 'gcyear', 'gclevel', 'scen', 'pickscen'].includes(e.getAttribute('data-act')) && !e.disabled);
          if (!all.length) return null;
          const e = all[Math.floor(r * all.length)];
          const label = `${e.getAttribute('data-act')}:${e.getAttribute('data-id') ?? ''}:${(e.textContent || '').trim().replace(/\s+/g, ' ').slice(0, 30)}`;
          e.dispatchEvent(new MouseEvent('click', { bubbles: true, cancelable: true }));
          return label;
        }, rnd());
        trail.push(`act ${pick}`);
      } else if (roll < 0.8) {
        const x = map.x + 20 + rnd() * (map.width - 40), y = map.y + 20 + rnd() * (map.height - 40);
        const right = rnd() < 0.45;
        await page.mouse.click(x, y, { button: right ? 'right' : 'left' });
        trail.push(`${right ? 'right' : 'left'}-click map ${Math.round(x)},${Math.round(y)}`);
      } else if (roll < 0.86) {
        const key = ['Escape', 'Enter', ' ', 'Tab', 'm', 'n', 'b', '1', '2', 'ArrowLeft', 'ArrowUp', 'Delete'][Math.floor(rnd() * 12)];
        await page.keyboard.press(key);
        trail.push(`key ${key}`);
      } else if (roll < 0.9) {
        await page.mouse.move(map.x + rnd() * map.width, map.y + rnd() * map.height);
        await page.mouse.wheel(0, (rnd() - 0.5) * 600);
        trail.push('wheel');
      } else if (roll < 0.93) {
        // Close whatever is open, then end the month.
        await page.keyboard.press('Escape');
        const done = await page.evaluate(() => { const b = document.querySelector('[data-act="end"]'); if (b && !b.disabled && b.offsetParent !== null) { b.dispatchEvent(new MouseEvent('click', { bubbles: true })); return true; } const n = document.querySelector('[data-act="next"]'); if (n && n.offsetParent !== null) { n.dispatchEvent(new MouseEvent('click', { bubbles: true })); return true; } return false; });
        if (done) months++;
        trail.push(`end month ${done}`);
        await page.waitForTimeout(150);
      } else {
        // A drag across the map.
        const x = map.x + 100 + rnd() * (map.width - 200), y = map.y + 100 + rnd() * (map.height - 200);
        await page.mouse.move(x, y); await page.mouse.down(); await page.mouse.move(x + (rnd() - 0.5) * 300, y + (rnd() - 0.5) * 300, { steps: 3 }); await page.mouse.up();
        trail.push('drag');
      }
    } catch (e) { errors.push({ text: `driver: ${e.message.split('\n')[0]}`, trail: trail.slice(-8) }); }
    if (i % 100 === 99) {
      const st = await page.evaluate(() => { const g = window.theater.game(); return { turn: g.turn, human: g.human, alive: g.human ? g.nations[g.human].alive : null, modal: !!document.querySelector('#modal.show, .modal.show, #veil.show'), nan: /NaN|undefined|\[object/.test(document.body.innerText) ? (document.body.innerText.match(/.{0,60}(NaN|undefined|\[object).{0,60}/) || [''])[0] : '' }; }).catch((e) => ({ err: e.message }));
      if (st.nan) errors.push({ text: `text on screen reads badly: "${st.nan.replace(/\s+/g, ' ')}"`, trail: trail.slice(-8) });
      trail.push(`[turn ${st.turn}]`);
    }
  }
  const end = await page.evaluate(() => { const g = window.theater.game(); return { turn: g.turn, human: g.human, corps: g.corps.length }; }).catch((e) => ({ err: e.message }));
  await page.screenshot({ path: `${process.argv[1].replace('monkey.cjs', 'shots')}/monkey-${nat}-${seed}.png` });
  const seen = new Map();
  for (const e of errors) { const k = e.text.split('\n')[0]; if (!seen.has(k)) seen.set(k, { ...e, n: 0 }); seen.get(k).n++; }
  console.log(`monkey ${nat} seed ${seed} from ${from}: ${steps} steps, ${months} months ended, ended at turn ${end.turn} human=${end.human}; ${errors.length} errors, ${seen.size} distinct`);
  for (const e of seen.values()) console.log(`  x${e.n} ${e.text}\n     after: ${e.trail.join(' | ')}`);
  await browser.close();
})();

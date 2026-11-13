// The start card: France in 1941 and Poland in 1940, countries that have already fallen by then.
const { chromium } = require('playwright');
const dir = __dirname;
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
  const errs = [];
  page.on('pageerror', (e) => errs.push(e.message));
  await page.addInitScript(() => { try { localStorage.setItem('tc-watch', '0'); } catch {} });
  await page.goto('file:///home/user/theater-command/dist/play.html');
  await page.waitForTimeout(800);
  const click = (sel) => page.evaluate((s) => { const e = document.querySelector(s); if (!e) return 'missing'; const d = e.disabled || e.classList.contains('off') || e.getAttribute('aria-disabled'); e.dispatchEvent(new MouseEvent('click', { bubbles: true })); return `clicked${d ? ' (marked off)' : ''} "${(e.textContent || '').trim().replace(/\s+/g, ' ').slice(0, 50)}"`; }, sel);
  const btns = () => page.evaluate(() => [...document.querySelectorAll('#card [data-act="gcnat"], #card [data-act="gcyear"]')].map((e) => `${e.getAttribute('data-act')}:${e.getAttribute('data-nat') ?? e.getAttribute('data-id') ?? e.getAttribute('data-year') ?? ''}:${e.disabled ? 'DISABLED' : ''}${e.className}`).join(' | '));
  const brief = () => page.evaluate(() => { const t = document.querySelector('#card')?.innerText.replace(/\s+/g, ' ') ?? ''; const i = t.search(/1 (SEPTEMBER|JANUARY) 19/); return t.slice(i, i + 700); });
  console.log(await btns());
  for (const [nat, yearIdx, label] of [['FRA', 17, 'France 1941'], ['POL', 5, 'Poland 1940'], ['FRA', 29, 'France 1942']]) {
    console.log('---', label);
    console.log(await click(`#card [data-act="gcnat"][data-nat="${nat}"]`));
    const ys = await page.evaluate(() => [...document.querySelectorAll('#card [data-act="gcyear"]')].map((e) => [...e.attributes].map((a) => `${a.name}=${a.value}`).join(' ') + ' :: ' + e.textContent.trim().replace(/\s+/g, ' ')));
    if (label === 'France 1941') console.log(ys.join('\n'));
    const idx = { 5: 1, 17: 2, 29: 3 }[yearIdx];
    await page.evaluate((i) => document.querySelectorAll('#card [data-act="gcyear"]')[i].dispatchEvent(new MouseEvent('click', { bubbles: true })), idx);
    await page.waitForTimeout(200);
    console.log('BRIEF:', await brief());
    const st = await page.evaluate(() => { const b = document.querySelector('#card [data-act="start"]'); return b ? `${b.textContent.trim()} disabled=${b.disabled}` : 'no start button'; });
    console.log('START:', st);
    await page.evaluate(() => document.querySelector('#card [data-act="start"]')?.scrollIntoView());
    await page.screenshot({ path: dir + `/shots/40-start-${label.replace(' ', '-')}.png` });
  }
  // Begin France 1942 and see what the player gets.
  await page.evaluate(() => document.querySelector('#card [data-act="start"]')?.dispatchEvent(new MouseEvent('click', { bubbles: true })));
  await page.waitForTimeout(3000);
  const st = await page.evaluate(() => { const g = window.theater.game(); return { turn: g.turn, human: g.human, alive: g.human ? g.nations[g.human].alive : null, top: document.body.innerText.replace(/\s+/g, ' ').slice(0, 260), card: document.querySelector('#veil')?.classList.contains('open') ? document.querySelector('#card')?.innerText.replace(/\s+/g, ' ').slice(0, 700) : '(no card)' }; });
  console.log('AFTER BEGIN:', JSON.stringify(st));
  await page.screenshot({ path: dir + '/shots/41-france-1942-begun.png' });
  console.log('errors', errs);
  await browser.close();
})();

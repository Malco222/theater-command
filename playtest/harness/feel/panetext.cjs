// panetext: plays a few presses as a new player and prints the side page's words around a phrase.
// usage: node panetext.cjs "NATION" "DEPTH" "PHRASE" step ... where a step is press:WORDS | tab:NAME | unit:NAME
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc842/dist/play.html';
const [nat, depth, phrase, ...steps] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 } })).newPage();
  await page.goto(PAGE); await page.evaluate(() => localStorage.clear()); await page.reload(); await page.waitForSelector('#card');
  const press = (words, root) => page.evaluate(({ words, root }) => { const r = document.querySelector(root); const b = r && [...r.querySelectorAll('button')].find((x) => (x.innerText || '').replace(/\s+/g, ' ').trim().toLowerCase().includes(words.toLowerCase())); if (b) b.click(); return !!b; }, { words, root });
  await press(nat, '#card'); await page.waitForTimeout(300); if (depth) { await press(depth, '#card'); await page.waitForTimeout(300); } await press('BEGIN THE CAMPAIGN', '#card'); await page.waitForTimeout(2500);
  for (const st of steps) { const [k, v] = [st.slice(0, st.indexOf(':')), st.slice(st.indexOf(':') + 1)]; let ok = true; if (k === 'tab') await page.evaluate((v) => document.querySelector(`#tabs [data-tab="${v}"]`).click(), v); else if (k === 'bar') ok = await press(v, '#barbtns'); else if (k === 'card') ok = await press(v, '#card'); else if (k === 'close') await page.evaluate(() => window.theater.openModal(null)); else ok = (await press(v, '#card')) || (await press(v, '#pane')) || (await press(v, '#tray')); await page.waitForTimeout(700); if (!ok) console.log(`   (no button "${v}")`); }
  const txt = await page.evaluate(() => (document.querySelector('#pane').innerText || '').replace(/\s+/g, ' '));
  const i = txt.toLowerCase().indexOf(phrase.toLowerCase()); console.log(i < 0 ? `"${phrase}" is not on the side page (${txt.split(' ').length} words)` : txt.slice(Math.max(0, i - 200), i + 900));
  console.log('said: ' + await page.evaluate(() => (window.theater.ui.msg || '') + ' | ' + (document.querySelector('#hint')?.innerText || '').replace(/\s+/g, ' ')));
  await browser.close();
})();

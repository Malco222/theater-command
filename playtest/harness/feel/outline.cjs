// outline: what the side page holds on a new player's first screen, heading by heading, with the words under each.
// usage: node outline.cjs "United States" [TAB ...]
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc842/dist/play.html';
const [nat = 'United States', ...tabs] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 } })).newPage();
  await page.goto(PAGE); await page.evaluate(() => localStorage.clear()); await page.reload(); await page.waitForSelector('#card');
  const press = (words, root) => page.evaluate(({ words, root }) => { const r = document.querySelector(root); const b = [...r.querySelectorAll('button')].find((x) => (x.innerText || '').replace(/\s+/g, ' ').trim().toLowerCase().startsWith(words.toLowerCase())); if (b) b.click(); return !!b; }, { words, root });
  await press(nat, '#card'); await page.waitForTimeout(400); await press('BEGIN THE CAMPAIGN', '#card'); await page.waitForTimeout(2500);
  const outline = () => page.evaluate(() => { const pane = document.querySelector('#pane'); const out = []; let cur = { h: '(top)', w: 0, b: 0 }; const walk = (el) => { for (const c of el.children) { const tag = c.tagName; const cls = c.className || ''; if (/^H[1-6]$/.test(tag) || /\b(sec|sub|head|h)\b/.test(String(cls)) && (c.innerText || '').length < 60 && !c.querySelector('button')) { out.push(cur); cur = { h: (c.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 50), w: 0, b: 0 }; } else if (c.children.length && c.querySelector('h1,h2,h3,h4,.sec,.sub,.head')) walk(c); else { cur.w += ((c.innerText || '').trim().match(/\S+/g) || []).length; cur.b += c.querySelectorAll('button').length + (tag === 'BUTTON' ? 1 : 0); } } }; walk(pane); out.push(cur); return { total: ((pane.innerText || '').trim().match(/\S+/g) || []).length, px: pane.scrollHeight, win: pane.clientHeight, out: out.filter((o) => o.w || o.b) }; });
  for (const tab of ['region', ...tabs]) {
    if (tab !== 'region') { await page.evaluate((tab) => { const b = document.querySelector(`#tabs [data-tab="${tab}"]`); if (b) b.click(); }, tab); await page.waitForTimeout(600); }
    const o = await outline(); console.log(`== ${tab}: ${o.total} words, ${o.px}px tall in a ${o.win}px window (${(o.px / o.win).toFixed(1)} screens)`); for (const s of o.out) console.log(`   ${String(s.w).padStart(5)} words ${String(s.b).padStart(3)} buttons  ${s.h}`);
  }
  console.log('tabs: ' + await page.evaluate(() => [...document.querySelectorAll('#tabs .tab')].map((b) => b.innerText.replace(/\s+/g, ' ').trim()).join(' | ')));
  console.log('bar: ' + await page.evaluate(() => [...document.querySelectorAll('#barbtns button')].map((b) => b.innerText.replace(/\s+/g, ' ').trim()).join(' | ')));
  await browser.close();
})();

// Version 76: the start card's own words for each small American seat (no campaign is begun).
// usage: node ui46.cjs NAT,NAT,...   prints the years on offer, the goals, the line under the years and the country's brief
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc76/dist/play.html';
(async () => {
  const browser = await chromium.launch(); const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 } })).newPage();
  await page.goto(PAGE); await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); }); await page.reload(); await page.waitForSelector('#card');
  for (const nat of (process.argv[2] || 'BRA,ARG,CHL,PER,COL,VEN,MEX,CAN,USA').split(',')) {
    await page.click(`.gcnat[data-nat="${nat}"]`); await page.waitForTimeout(150);
    const r = await page.evaluate(() => { const all = (document.querySelector('#card')?.innerText || '').replace(/\s+/g, ' '); const a = all.search(/CHOOSE HOW DEEP YOU PLAY/), b = all.search(/OR START AT A TURNING POINT|Fourteen shorter campaigns/); return { brief: [...document.querySelectorAll('.gcbrief, .gcline, .gcwhy, .gcnote')].map((x) => x.textContent.replace(/\s+/g, ' ').trim()), mid: all.slice(a, b > a ? b : a + 1200).slice(-650), goals: [...document.querySelectorAll('.sgoal')].length, under: document.querySelector('.gcunder')?.textContent ?? null, years: [...document.querySelectorAll('.gcyear')].filter((x) => !x.disabled).map((x) => x.textContent.trim().slice(0, 4)).join(' ') }; });
    console.log(`${nat}: years ${r.years}; goals ${r.goals}; under ${r.under}; brief ${JSON.stringify(r.brief)}; before the scenarios: ${r.mid}`);
  }
  await browser.close();
})();

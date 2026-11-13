const { chromium } = require('playwright');
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 });
  const errors = [];
  page.on('console', (m) => { if (m.type() === 'error' || m.type() === 'warning') errors.push(`${m.type()}: ${m.text()}`); });
  page.on('pageerror', (e) => errors.push(`pageerror: ${e.message}\n${e.stack}`));
  await page.goto('file:///home/user/theater-command/dist/play.html');
  await page.waitForTimeout(1500);
  await page.screenshot({ path: process.argv[2] + '/01-start.png' });
  const acts = await page.evaluate(() => [...document.querySelectorAll('[data-act]')].map((e) => `${e.getAttribute('data-act')}|${e.getAttribute('data-id') ?? ''}|${(e.textContent || '').trim().slice(0, 50)}`));
  console.log(acts.join('\n'));
  console.log('ERRORS', errors.join('\n'));
  await browser.close();
})();

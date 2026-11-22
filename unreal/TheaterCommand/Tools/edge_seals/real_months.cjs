// Writes real_months.inc, the real months' cards the fold of alike runs is checked on (edge_seals_test.cpp, TestFold):
// the rules bundle (Content/Rules/campaign-rules.js) played headless in node, twelve seats from the eleven starts that
// cover the war (seed 1939, the world on, the grand view), four months each, each month's card as the items the edge's
// seals are made from, in the card's order (EdgeNews.cpp's EdgeSealOf reads the same fields).
// Usage: node Tools/edge_seals/real_months.cjs   (about a minute)
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const here = __dirname;
const bundle = fs.readFileSync(path.join(here, '../../Content/Rules/campaign-rules.js'), 'utf8');
const seats = [['GER', 1], ['UK', 1], ['POL', 1], ['FRA', 5], ['ITA', 13], ['GRE', 14], ['SOV', 22], ['USA', 29], ['GER', 34], ['UK', 47], ['SOV', 53], ['UK', 57]];
const months = 4;
const clean = (s) => String(s == null ? '' : s).replace(/[\t\r\n]+/g, ' ');
const out = [];
for (const [human, from] of seats) {
  // (a fresh context for each seat: the rules keep their state in the bundle's own scope)
  const quiet = { log() {}, warn() {}, error() {}, info() {} };
  const ctx = { console: quiet, setTimeout, clearTimeout, Math, Date };
  ctx.globalThis = ctx;
  vm.createContext(ctx);
  vm.runInContext(bundle, ctx, { filename: 'campaign-rules.js' });
  ctx.TC.start({ human, from, seed: 1939, world: true, view: 'grand' });
  for (let month = 0; month < months; ++month) {
    ctx.TC.endMonth();
    const page = ctx.TC.page('reports', -1);
    const card = page && (page.month || page);
    if (!card || card.opening) continue;
    out.push(['M', human, clean(card.date)].join('\t'));
    const add = (part, word, tone, sign, a, b, c, text) => out.push([part, clean(word), tone, clean(sign), clean(a), clean(b), clean(c), clean(text)].join('\t'));
    for (const band of card.bands || []) add(0, band.kind, band.good ? 1 : -1, '', '', '', '', band.text);
    if (card.lead) add(1, card.lead.stamp, card.lead.tone || 0, card.lead.picture || '', '', '', '', card.lead.headline);
    for (const row of card.rows || []) add(2, row.tag, row.tone || 0, row.sign, row.place, row.join, row.joinName, row.text);
    for (const line of card.home || []) add(3, '', line.grey ? 3 : 0, line.sign, line.count, line.where, '', line.text);
    for (const line of card.staff || []) add(4, '', 0, '', '', '', '', line.text);
    for (const line of card.elsewhere || []) add(5, '', 0, '', '', '', '', line.text);
  }
}
const head = [
  '// Real months\' cards, for the fold of alike runs (edge_seals_test.cpp, TestFold). Written by real_months.cjs from the rules',
  '// bundle: do not edit by hand. A line for each month ("M", the seat, the card\'s date), then one for each of its items in',
  '// the card\'s order: its part (EdgeSeals::ESection), its word (a band\'s kind, the lead\'s stamp, a row\'s tag), its tone,',
  '// its sign (the lead\'s picture), then a row\'s place, join word and join name (home\'s count and where), and its text.',
  '// Tab-separated.',
];
fs.writeFileSync(path.join(here, 'real_months.inc'), head.join('\n') + '\nR"MONTHS(' + out.join('\n') + '\n)MONTHS"\n');
console.log(`real_months.inc: ${out.filter((l) => l.startsWith('M\t')).length} months, ${out.length} lines`);

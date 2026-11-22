// The battle schools' button, checked against the rules as the game runs them (release 2; Brian, 22 November).
//
// Runs a bundled rules file (Content/Rules/campaign-rules.js, or one built by Tools/build_rules.sh) the way the game
// does (Source/TheaterCommand/Rules/RulesHost.cpp: TC[name].apply(TC, JSON.parse(args)), its answer as JSON), starts a
// grand campaign and a history scenario, presses TC.school as the corps card would, and writes what each corps' card
// is told, a line each, for Tools/school/school_test.cpp to check against Campaign/BattleSchool.
//
// Usage: node Tools/school/probe.mjs <campaign-rules.js> <out.tsv>
import { readFileSync, writeFileSync } from 'node:fs';
import vm from 'node:vm';

const [file, out] = process.argv.slice(2);
if (!file || !out) {
  console.error('Usage: node probe.mjs <campaign-rules.js> <out.tsv>');
  process.exit(2);
}
vm.runInThisContext(readFileSync(file, 'utf8'), { filename: file });
const TC = globalThis.TC;
const call = (name, args) => {
  if (typeof TC !== 'object' || typeof TC[name] !== 'function') throw new Error('The rules have no function called ' + name);
  const r = TC[name].apply(TC, JSON.parse(JSON.stringify(args)));
  return r === undefined ? null : JSON.parse(JSON.stringify(r));
};
const fail = (why) => {
  console.error(`probe: ${why}`);
  process.exit(1);
};

const rules = typeof TC.school === 'function' ? 'r2' : 'r1';
const rows = [];
// As FCampaignSession::ReadView and the controller's card fill read them.
const facts = (view, c) => ({
  known: typeof c.school === 'boolean',
  own: c.nat === view.human && !c.ally,
  grand: view.open === true,
  fort: c.kind === 'fort',
  hisMonth: view.humanUp === true,
  atSchool: c.school === true,
  canOrder: c.canOrder === true,
  to: typeof c.schoolTo === 'string' ? c.schoolTo : '',
  why: typeof c.schoolWhy === 'string' ? c.schoolWhy : '',
});
const clean = (s) => s.replace(/[\t\n]/g, ' ');
const row = (phase, view, c) => {
  const f = facts(view, c);
  const b = (x) => (x ? '1' : '0');
  rows.push([phase, rules, b(f.known), b(f.own), b(f.grand), b(f.fort), b(f.hisMonth), b(f.atSchool), b(f.canOrder), String(c.id), clean(f.to), clean(f.why)].join('\t'));
  return f;
};
const corpsIn = (view, id) => view.corps.find((c) => c.id === id);

// A grand campaign, the player's first month open.
let view = call('start', [{ human: 'UK', from: 1, view: 'grand', seed: 1939 }]);
if (!view || !Array.isArray(view.corps)) fail('no picture of the war from start');
if (view.open !== true) fail('the grand campaign is not open play');
for (const c of view.corps) row('start', view, c);

if (rules === 'r1') {
  // Release 1: no TC.school, and no corps says 'school': the card shows no button.
  if (view.corps.some((c) => 'school' in c || 'schoolTo' in c || 'schoolWhy' in c)) fail("release 1's view speaks of the schools");
  let threw = false;
  try { call('school', [[view.corps[0].id], true]); } catch { threw = true; }
  if (!threw) fail('release 1 answered TC.school');
} else {
  if (view.humanUp !== true) fail("the player's month is not open");
  const mine = view.corps.filter((c) => { const f = facts(view, c); return f.own && !f.fort; });
  const fit = mine.find((c) => c.schoolWhy === '' && c.canOrder);
  if (!fit) fail('no corps of the player fit to go to school in the first month');
  const unfit = mine.find((c) => c.schoolWhy !== '');

  // A press of SEND TO SCHOOL.
  let r = call('school', [[fit.id], true]);
  if (!r || r.ok !== true || typeof r.note !== 'string' || !r.note || !r.view) fail(`TC.school(go) answered ${JSON.stringify(r && { ok: r.ok, note: r.note })}`);
  view = r.view;
  let f = row('sent', view, corpsIn(view, fit.id));
  if (!f.atSchool || f.canOrder) fail('a corps sent to school is not at school, or still takes orders');
  if (!corpsIn(view, fit.id).noOrders) fail('a corps at school does not say why it takes no orders');
  console.log(`sent: ${r.note}`);
  console.log(`  its card line: ${corpsIn(view, fit.id).noOrders}`);

  // A press on a corps that cannot go: refused, in the same words as its tip.
  if (unfit) {
    r = call('school', [[unfit.id], true]);
    if (r.ok !== false || r.note !== unfit.schoolWhy) fail(`a refusal's note is not the card's reason: ${JSON.stringify(r.note)} / ${JSON.stringify(unfit.schoolWhy)}`);
    view = r.view;
    row('refused', view, corpsIn(view, unfit.id));
    console.log(`refused: ${r.note}`);
  }

  // A press of BRING BACK.
  r = call('school', [[fit.id], false]);
  if (r.ok !== true || !r.view) fail('TC.school(back) was refused');
  view = r.view;
  f = row('back', view, corpsIn(view, fit.id));
  if (f.atSchool || !f.canOrder) fail('a corps called back is still at school, or takes no orders');
  console.log(`back: ${r.note}`);
}

// A history scenario: the schools are not open there, and the card shows no button.
view = call('start', [{ human: 'GER', from: 1, view: 'history', seed: 1939 }]);
if (view.open === true) fail('the history scenario is open play');
for (const c of view.corps) row('history', view, c);

writeFileSync(out, rows.join('\n') + '\n');
console.log(`${rules}: ${rows.length} corps cards written to ${out}`);

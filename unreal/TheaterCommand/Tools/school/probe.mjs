// The battle schools' button, checked against the rules as the game runs them (release 2; Brian, 22 November).
//
// Runs a bundled rules file (Content/Rules/campaign-rules.js, or one built by Tools/build_rules.sh) the way the game
// does (Source/TheaterCommand/Rules/RulesHost.cpp: TC[name].apply(TC, JSON.parse(args)), its answer as JSON), starts a
// grand campaign and a history scenario, presses TC.school as the corps card would, and writes what each corps' card
// is told, a line each, for Tools/school/school_test.cpp to check against Campaign/BattleSchool; then calls corps back
// to the army in hand four ways and writes what the controller sees at each press.
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

  handCases();
}

// BRING BACK and the army in hand (the code checker's rule of 17 November: the hand stays on one side of a town's walls).
// Four presses, each in a fresh campaign, in a province with a corps that can be ordered into its town (G) and two more
// (F1, F2): the hand held only the garrison (the reviewer's case), a garrison called back while the field army is in
// hand, a field corps called back to a field hand, and a whole army at the schools, only looked at. Each writes a 'hand'
// line of what the controller's SchoolCorps sees, for school_test.cpp to run Campaign/BattleSchool's HandOnBack on.
function handCases() {
  const fitHere = (v) => v.corps.filter((c) => { const x = facts(v, c); return x.own && !x.fort && c.canOrder && c.schoolWhy === ''; });
  const garrisonKey = (reg, id) => (JSON.stringify(call('province', [reg])).match(/garrison:\d+/g) || []).includes(`garrison:${id}`);
  let v = call('start', [{ human: 'UK', from: 1, view: 'grand', seed: 1939 }]);
  const byReg = new Map();
  for (const c of fitHere(v)) byReg.set(c.reg, [...(byReg.get(c.reg) || []), c.id]);
  let place = null;
  for (const [reg, ids] of byReg) {
    if (ids.length < 3) continue;
    const g = ids.find((id) => garrisonKey(reg, id));
    if (g !== undefined) { place = { reg, g, f: ids.filter((id) => id !== g) }; break; }
  }
  if (!place) fail('hand: no province with three corps fit for school, one of which can be ordered into its town');
  const { reg, g } = place;
  const [f1, f2] = place.f;
  // The controller's PickUpArmyIn: his corps there that take orders, those in the field first, the town's only if none.
  const pickUp = (vv) => {
    const mine = vv.corps.filter((c) => c.reg === reg && c.nat === vv.human && c.canOrder === true);
    const field = mine.filter((c) => !c.town).map((c) => c.id);
    return field.length ? field : mine.filter((c) => c.town).map((c) => c.id);
  };
  const press = (ids, go, what) => {
    const r = call('school', [ids, go]);
    if (!r || r.ok !== true || !r.view) fail(`hand: ${what} was refused: ${JSON.stringify(r && r.note)}`);
    return r.view;
  };
  const toTown = (id, what) => {
    if (!garrisonKey(reg, id)) fail(`hand: ${what}: the province offers no order into its town`);
    const r = call('work', [`garrison:${id}`, reg]);
    if (!r || r.ok !== true || !r.view) fail(`hand: ${what}: ${JSON.stringify(r && r.note)}`);
    return r.view;
  };
  const nextMonth = () => {
    let w = null;
    for (let i = 0; i < 50 && !(w && w.humanUp === true); i++) {
      const r = call('endMonth', []);
      w = r && Array.isArray(r.corps) ? r : r && r.view && Array.isArray(r.view.corps) ? r.view : call('view', []);
    }
    if (!w || w.humanUp !== true) fail("hand: the player's next month never opened");
    return w;
  };
  const b = (x) => (x ? '1' : '0');
  const line = (scenario, before, hand, back) => {
    const was = corpsIn(before, back);
    const after = press([back], false, `${scenario}: BRING BACK`);
    const now = corpsIn(after, back);
    if (!was || !now || !was.school) fail(`hand: ${scenario}: corps ${back} was not at school`);
    if (!!now.town !== !!was.town) fail(`hand: ${scenario}: calling corps ${back} back moved it through the town's walls`);
    if (now.school || now.canOrder !== true) fail(`hand: ${scenario}: corps ${back}, called back, takes no orders`);
    const inHand = hand.map((id) => corpsIn(after, id));
    const here = after.corps.filter((c) => c.reg === reg && c.nat === after.human);
    rows.push(['hand', rules, scenario, b(hand.length === 0), b(hand.length > 0), b(was.town), b(inHand.some((c) => c && c.town)),
      b(inHand.some((c) => c && !c.town)), String(back), hand.join(','), here.map((c) => `${c.id}:${b(c.town)}:${b(c.canOrder)}`).join(',')].join('\t'));
    console.log(`hand ${scenario}: in hand ${hand.join(',') || 'nobody'}; corps ${back} called back (${was.town ? 'in the town' : 'in the field'})`);
  };

  // The reviewer's case: the garrison alone in hand, the field army at the schools; one of the field army called back.
  v = toTown(g, 'the garrison order');
  v = nextMonth();
  if (!corpsIn(v, g).town || !corpsIn(v, g).canOrder) fail('hand: the garrison is not in its town taking orders in the next month');
  press([f1], true, 'garrison-in-hand: SEND TO SCHOOL');
  v = press([f2], true, 'garrison-in-hand: SEND TO SCHOOL');
  line('garrison-in-hand', v, pickUp(v), f1);

  // A garrison called back while the field army is in hand (sent to school, then ordered into the town while there).
  call('start', [{ human: 'UK', from: 1, view: 'grand', seed: 1939 }]);
  press([g], true, 'garrison-back: SEND TO SCHOOL');
  v = toTown(g, 'garrison-back: the garrison order while at school');
  if (!corpsIn(v, g).town || !corpsIn(v, g).school) fail('hand: garrison-back: the corps is not in the town and at school');
  line('garrison-back', v, pickUp(v), g);

  // A field corps called back to a field hand.
  call('start', [{ human: 'UK', from: 1, view: 'grand', seed: 1939 }]);
  v = press([f1], true, 'field-back: SEND TO SCHOOL');
  line('field-back', v, pickUp(v), f1);

  // A whole army at the schools: nothing can be picked up, it is only looked at; one corps called back.
  v = call('start', [{ human: 'UK', from: 1, view: 'grand', seed: 1939 }]);
  v = press(pickUp(v), true, 'all-at-school: SEND TO SCHOOL');
  if (pickUp(v).length) fail('hand: all-at-school: corps there still take orders');
  line('all-at-school', v, [], f1);
}

// A history scenario: the schools are not open there, and the card shows no button.
view = call('start', [{ human: 'GER', from: 1, view: 'history', seed: 1939 }]);
if (view.open === true) fail('the history scenario is open play');
for (const c of view.corps) row('history', view, c);

writeFileSync(out, rows.join('\n') + '\n');
console.log(`${rules}: ${rows.length} corps cards written to ${out}`);

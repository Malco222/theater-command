// The rules' half of the general's clipboard test (land_command_test.cpp; release 2). Built and run by command.sh.
//
//   node land-command-check.mjs setup <folder> [key] [seed]   writes the setup step as the game first hears it (the
//                                                             header with his ground, and the first piece), for the
//                                                             clipboard's placing checks.
//   node land-command-check.mjs check <folder> <setup folder> [key] [seed] [tick]
//        fights the same battle deltas.ts fought (the same orders at the same moments), gives each command the clipboard
//        wrote (commands.jsonl: the rules' time, the command) when the rules reach that time, and says which the rules
//        refused; then asks the rules about each placing it wrote (placings.jsonl) and checks that where the clipboard
//        said a block may not stand for want of room, the rules agree.
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { livePiece } from '../../../../src/battle/land/live';
import type { Command } from '../../../../src/battle/core/types';

const [mode, folder = '.', ...rest] = process.argv.slice(2);

if (mode === 'setup') {
  const [key = 'normandy-1944', seedArg] = rest;
  const L = livePiece(key, seedArg ? Number(seedArg) : undefined, { me: 'att', setup: true });
  mkdirSync(folder, { recursive: true });
  writeFileSync(join(folder, 'header.json'), JSON.stringify(L.header()));
  writeFileSync(join(folder, 'deltas.jsonl'), JSON.stringify(L.tick(0.5, 10)) + '\n');
  process.exit(0);
}

const [setupFolder = '.', key = 'normandy-1944', seedArg = '11', tickArg = '0.5'] = rest;
const seed = Number(seedArg);
let failures = 0;
const fail = (what: string): void => { failures++; if (failures <= 20) console.log(`FAIL: ${what}`); };

// The orders, at the same moments of the same battle.
const lines = readFileSync(join(folder, 'commands.jsonl'), 'utf8').split('\n').filter(Boolean).map((l) => {
  const [t, c] = l.split('\t');
  return { t: Number(t), c: JSON.parse(c) as Command & { at?: number } };
});
const L = livePiece(key, seed, { me: 'att', setup: false });
const blocks = L.header().blocks as { id: number; side: string }[];
const mine = blocks.filter((b) => b.side === 'att');
const theirs = blocks.filter((b) => b.side !== 'att');
const taken: Record<string, [number, number]> = {};
let guard = 0, next = 0;
while (!L.sim.over && guard++ < 100000 && next < lines.length) {
  const d = L.tick(Number(tickArg), 1000);
  // (deltas.ts's own orders first, as it gave them)
  if (Math.abs(d.t - 20) < 0.3) L.order({ do: 'attack', units: [mine[0].id], target: theirs[0].id });
  if (Math.abs(d.t - 25) < 0.3) L.order({ do: 'move', units: [mine[1].id], x: 1500, y: 700, run: true });
  if (Math.abs(d.t - 60) < 0.3) L.order({ do: 'land:staff', units: [mine[0].id] });
  while (next < lines.length && lines[next].t <= d.t + 1e-6) {
    const { c } = lines[next++];
    const ok = L.order(c);
    const tally = (taken[c.do] ??= [0, 0]);
    tally[ok ? 0 : 1]++;
    if (!ok) fail(`the rules refused ${JSON.stringify(c)} at ${d.t}`);
  }
}
if (next < lines.length) fail(`${lines.length - next} commands never given (the battle ended at ${L.sim.t})`);
console.log(`orders given to the rules: ${Object.entries(taken).map(([k, [ok, no]]) => `${k} ${ok} taken${no ? `, ${no} refused` : ''}`).join('; ')}`);

// The placings: where the clipboard says no for want of room, the rules say no too.
const S = livePiece(key, seed, { me: 'att', setup: true });
const places = readFileSync(join(setupFolder, 'placings.jsonl'), 'utf8').split('\n').filter(Boolean).map((l) => l.split('\t').map(Number));
let both = 0, onlyRules = 0, clipboardYesRulesNo = 0;
const why = new Map<string, number>();
for (const [id, x, y, may] of places) {
  const no = S.canPlace(id, x, y);
  if (!may && no === null) fail(`block ${id} at ${x},${y}: the clipboard said no room, the rules would place it`);
  if (may && no === null) both++;
  if (may && no !== null) { clipboardYesRulesNo++; why.set(no, (why.get(no) ?? 0) + 1); }
  if (!may && no !== null) onlyRules++;
}
console.log(`placings: ${places.length} asked; both yes ${both}, both no ${onlyRules}, the screen yes but the rules no ${clipboardYesRulesNo}${why.size ? ` (${[...why.entries()].map(([w, n]) => `${w} ${n}`).join('; ')})` : ''}`);

// Drawing up while he places his army (Brian, 26 November 2026): what the clipboard would ask for a group carried as
// one and for a line drawn on his ground (drawn.jsonl: a word, then TC.landPlaceMany's list), the rules take, all of
// it; and each formation's word (drawups.jsonl: TC.landDrawUp's arguments) draws them up.
{
  const drawn = readFileSync(join(setupFolder, 'drawn.jsonl'), 'utf8').split('\n').filter(Boolean).map((l) => { const [what, list] = l.split('\t'); return { what, list: JSON.parse(list) as number[][] }; });
  if (!drawn.some((d) => d.what === 'carry') || !drawn.some((d) => d.what === 'line')) fail('the clipboard wrote no group carry or no drawn line');
  const said: string[] = [];
  for (const d of drawn) {
    const no = S.canPlaceMany(d.list);
    said.push(`${d.what} ${d.list.length}${no ? ` refused (${no.who}: ${no.why})` : ''}`);
    // (ground the screen cannot see, water or a river's lanes, may refuse a place; two of the act's own blocks standing
    // on one another never may: the clipboard's spacing is the rules' own)
    if (no && (no.why === 'Another block stands there.' || no.why === 'Outside your ground.' || d.what === 'carry' || d.what === 'line')) fail(`the clipboard's ${d.what}: the rules refuse it (${no.who}: ${no.why})`);
  }
  const D = livePiece(key, seed, { me: 'att', setup: true });
  const ups = readFileSync(join(setupFolder, 'drawups.jsonl'), 'utf8').split('\n').filter(Boolean).map((l) => JSON.parse(l) as [string, number[]]);
  if (ups.length < 5) fail(`the clipboard wrote ${ups.length} formation words, not 5`);
  for (const [kind, ids] of ups) {
    const r = D.drawUp(kind as 'line', ids);
    said.push(`${kind}${ids.length ? ` of ${ids.length}` : ''} ${r.ok ? `drew up ${r.placed.length}` : `refused (${r.why})`}`);
    if (!r.ok || (ids.length > 0 && r.placed.length !== ids.length)) fail(`the clipboard's ${kind} of ${ids.length || 'all'}: ${r.ok ? `${r.placed.length} placed` : r.why}`);
  }
  console.log(`drawing up: ${said.join('; ')}`);
}

// The walk pressed in code for the Mac's builder (-TheaterLandScript): the two units it carries into the woods are placed
// there, and when the clock starts the rules hide them (Brian's ambushes, 22 November 04:18Z).
const woods = readFileSync(join(setupFolder, 'woods.jsonl'), 'utf8').split('\n').filter(Boolean).map((l) => l.split('\t').map(Number));
if (woods.length < 2) fail(`the walk found woods for ${woods.length} of its 2 units`);
for (const [id, x, y] of woods) {
  const no = S.canPlace(id, x, y);
  if (no !== null) fail(`the walk's block ${id} at ${x},${y}: the rules would not place it (${no})`);
  else if (!S.place(id, x, y)) fail(`the walk's block ${id} at ${x},${y}: not placed`);
}
if (!S.begin()) fail('the setup step would not end');
const after = S.tick(2);
const lastView = after.views[after.views.length - 1] as { units?: { id: number; hid?: number }[] } | undefined;
const hidden = woods.filter(([id]) => lastView?.units?.find((u) => u.id === id)?.hid === 1).length;
console.log(`the walk: ${woods.length} units carried into the woods, ${hidden} hidden when the clock started`);
if (hidden !== woods.length) fail(`only ${hidden} of the walk's ${woods.length} units in the woods were hidden`);
// Hedge cutters and smoke on call (Brian, 22 November 14:19Z), as the live battle opens them for the game: his sections
// lay a screen 150 m ahead, and with a hedge line across the field ahead of his first tank, it cuts a gap through it.
// Without the hedge the rules say why; and the enemy's blocks never take his orders.
{
  const H = livePiece(key, seed, { me: 'att', setup: false });
  const own = H.sim.units.filter((u) => u.side === 'att');
  const tank = own.find((u) => u.kind === 'arm'), foot = own.filter((u) => u.kind === 'inf');
  const foe = H.sim.units.find((u) => u.side !== 'att');
  if (!tank || !foot.length || !foe) fail('the battle has no tank and sections of his, or no enemy');
  else {
    const noHedge = H.cutWhy(tank.id, tank.x, tank.y + 50);
    if (noHedge !== 'There are no hedges on this field.') fail(`a cut with no hedge: ${noHedge}`);
    if (H.cutWhy(foe.id) !== 'Only your own troops take your orders.') fail(`the enemy took his cut: ${H.cutWhy(foe.id)}`);
    // (a hedge line across the field 60 m ahead of his tank, before the clock runs, as the 3D field hands it over)
    const [fx, fy] = [Math.cos(tank.face), Math.sin(tank.face)], ax = tank.x + fx * (tank.d / 2 + 60), ay = tank.y + fy * (tank.d / 2 + 60);
    const hedge = { kind: 'hedge', points: [[ax - fy * 400, ay + fx * 400], [ax + fy * 400, ay - fx * 400]] as [number, number][] };
    if (!H.hedges([hedge], [])) fail('the hedge would not go on the field');
    const d0 = H.tick(1, 100);
    const cutNo = H.cutWhy(tank.id, ax, ay);
    const cut = cutNo === null && H.order({ do: 'land:cut', units: [tank.id], x: ax, y: ay } as Command);
    const f = foot[0], line = { x0: f.x + Math.cos(f.face) * 150 - 50, y0: f.y + Math.sin(f.face) * 150, x1: f.x + Math.cos(f.face) * 150 + 50, y1: f.y + Math.sin(f.face) * 150 };
    const smokeNo = H.smokeWhy(f.id, line.x0, line.y0, line.x1, line.y1);
    const smoke = smokeNo === null && H.order({ do: 'land:smoke', units: [f.id], ...line } as Command);
    let cutSaid = 0, smokeSaid = 0, cutterShown = 0, smokeShown = -1, cutViews = 0, screenViews = 0, hang = -1;
    for (let i = 0; i < 400 && !H.sim.over; i++) {
      const d = i === 0 ? d0 : H.tick(0.5, 100);
      for (const e of d.events as { what?: string; unit?: number }[]) {
        if (e.what === 'land:cut' && e.unit === tank.id) cutSaid++;
        if (e.what === 'land:smoke' && e.unit === f.id) smokeSaid++;
      }
      for (const r of d.rounds as { w?: string; by?: number; hang?: number }[]) if (r.w === 'smoke' && r.by === f.id && r.hang !== undefined) hang = Math.max(hang, r.hang);
      const v = d.views[d.views.length - 1] as { units?: { id: number; cutter?: number; smoke?: number }[]; screens?: { unit?: number }[] } | undefined;
      for (const c of d.cuts ?? []) if (c.unit === tank.id) cutViews++;
      if (v?.screens?.some((c) => c.unit === f.id)) screenViews++;
      if (v?.units?.find((u) => u.id === tank.id)?.cutter) cutterShown++;
      const s = v?.units?.find((u) => u.id === f.id)?.smoke;
      if (s !== undefined) smokeShown = s;
      if (cutSaid && smokeSaid && cutViews && screenViews > 2 && hang >= 0) break;
    }
    console.log(`cutters and smoke: with no hedge "${noHedge}"; tank ${tank.id} told to cut ${cut ? 'taken' : `refused (${cutNo})`}, began ${cutSaid ? 'yes' : 'no'}, cutters shown in ${cutterShown} views; section ${f.id} told to lay smoke ${smoke ? 'taken' : `refused (${smokeNo})`}, laid ${smokeSaid ? 'yes' : 'no'}, ${smokeShown} screens left`);
    if (!cut || !cutSaid || !cutterShown) fail('his tank did not cut the gap he picked');
    if (!smoke || !smokeSaid) fail('his section did not lay the screen he dragged');
    console.log(`  the 3D field hears of them: the cut in ${cutViews} delta (once), the screen in ${screenViews} views, its smoke rounds hang ${hang} s`);
    if (cutViews !== 1 || !screenViews || hang < 0) fail('the deltas do not carry the cut once and the screen for the 3D field');
  }
}
console.log(failures ? `FAIL (${failures})` : 'the rules took every order');
process.exit(failures ? 1 : 0);

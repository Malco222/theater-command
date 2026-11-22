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
console.log(failures ? `FAIL (${failures})` : 'the rules took every order');
process.exit(failures ? 1 : 0);

// Release 2: a campaign raid recorded AS THE 3D GAME FEEDS IT. AAirBattle::Feed calls TC.battleTick every frame with
// the battle seconds it owes, so the rules step every DT (0.1 battle s) and the scene takes a view at every step, ten
// a battle second; tools/air-record/r16-live.ts ticks a whole battle second at a time, and the cloud test built on
// it never saw the flight between views laid a tenth of a second apart. This one ticks DT at a time and keeps every
// view of a window (battle seconds from..to; outside it one view a battle second, to keep the file small).
//
//   npx esbuild tools/air-record/live-steps.ts --bundle --platform=node --format=esm --outfile=dist/live-steps.mjs --log-level=warning
//   node dist/live-steps.mjs <out file> [kind] [from] [to] [nat] [start turn] [seed]

import { writeFileSync } from 'node:fs';
import { planRaid, raidTargets, type RaidKind } from '../../src/campaign/airwar';
import { loadGame } from '../../src/campaign/save';
import '../../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;
const out = process.argv[2] ?? 'live-steps.json';
const kind = (process.argv[3] ?? 'bomb') as RaidKind;
const from = Number(process.argv[4] ?? 0), to = Number(process.argv[5] ?? 1e9);
const START = { view: 'grand', human: process.argv[6] ?? 'GER', from: Number(process.argv[7] ?? 33), seed: Number(process.argv[8] ?? 11) };

TC.start(START);
const g = loadGame(TC.save().text)!;
let pick: { target: number; ids: number[]; name: string } | null = null;
for (const t of raidTargets(g, kind, START.human)) {
  const p = planRaid(g, kind, t.id, undefined, START.human);
  if (!p.picked?.length) continue;
  const look = TC.airLook(kind, t.id, p.picked);
  if (look.ticket && look.ticket.def.forces.length > 0) { pick = { target: t.id, ids: p.picked, name: g.regions[t.id].name }; break; }
}
if (!pick) throw new Error(`no ${kind} raid the field takes`);

const opened = TC.airStart(kind, pick.target, pick.ids);
TC.battleSpeed(1);
const views: unknown[] = [opened.view];
// eslint-disable-next-line @typescript-eslint/no-explicit-any
let v: any = opened.view;
let dense = 0;
for (let n = 0; n < 100000 && !v.over; n++) {
  v = TC.battleTick(0.1);
  const whole = Math.abs(v.t - Math.round(v.t)) < 1e-6;
  if ((v.t >= from && v.t <= to) || whole || v.over) { views.push(v); if (!whole) dense++; }
}
const end = TC.battleEnd();
const file = {
  what: 'RELEASE 2 LIVE PATH BY STEPS: one campaign raid through the campaign bridge, TC.battleTick(0.1) a rules step at a time as the 3D game feeds it; every step kept inside the window, one view a battle second outside it.',
  recording: true, key: `live-steps-${kind}`, seed: START.seed,
  live: true, start: START, kind, target: pick.target, place: pick.name, ids: pick.ids, window: [from, to],
  sky: opened.sky, views, end,
};
writeFileSync(out, JSON.stringify(file));
console.log(`${kind} on ${pick.name}: ${views.length} views (${dense} between whole seconds), ${v.t?.toFixed?.(1)} battle s, ${(JSON.stringify(file).length / 1e6).toFixed(2)} MB`);

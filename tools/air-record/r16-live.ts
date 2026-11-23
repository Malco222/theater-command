// Release 1.6: one real campaign day raid recorded through the campaign bridge's own calls, for the 3D thread's cloud
// test to play before any Mac run (the live path, AirBattle.cpp BeginLive, has never run against the rules). A 1942
// grand campaign (May 1942, Germany, seed 11), its first day raid the defence comes up to meet, opened with
// TC.airStart and run on the shared clock with TC.battleTick one battle second at a time (each answer is what
// TC.battleView gives), to the end, then settled with TC.battleEnd. Unlike tools/air-record's set pieces this one IS
// booked: the answers are the campaign's own.
//
//   npx esbuild tools/air-record/r16-live.ts --bundle --platform=node --format=esm --outfile=dist/r16-live.mjs --log-level=warning
//   node dist/r16-live.mjs <out file> [kind]      (kind 'bomb', the default, or 'superiority')

import { writeFileSync } from 'node:fs';
import { planRaid, raidTargets, type RaidKind } from '../../src/campaign/airwar';
import { loadGame } from '../../src/campaign/save';
import '../../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;
const out = process.argv[2] ?? 'r16-live-day-raid-1942.json';
const kind = (process.argv[3] ?? 'bomb') as RaidKind;
const START = { view: 'grand', human: 'GER', from: 33, seed: 11 };

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

const call = (name: string, args: unknown[]): unknown => ({ call: `TC.${name}`, args });
const calls: unknown[] = [];
const opened = TC.airStart(kind, pick.target, pick.ids);
calls.push(call('airStart', [kind, pick.target, pick.ids]));
TC.battleSpeed(1);
calls.push(call('battleSpeed', [1]));
const views: unknown[] = [opened.view];
// eslint-disable-next-line @typescript-eslint/no-explicit-any
let v: any = opened.view;
for (let n = 0; n < 5000 && !v.over; n++) { v = TC.battleTick(1); views.push(v); }
const view = TC.battleView();
const end = TC.battleEnd();
const file = {
  what: 'RELEASE 1.6 LIVE PATH: one campaign day raid fought through the campaign bridge (TC.airStart, then TC.battleTick(1) each battle second, whose answer is TC.battleView), to the end, then TC.battleEnd. Booked by the campaign: views[0] is airStart\'s view, each later one a tick\'s answer; end is battleEnd\'s answer (the raid report and its after card, after.lost its totals).',
  live: true, start: START, kind, target: pick.target, place: pick.name, ids: pick.ids, calls,
  sky: opened.sky, views, lastView: view, end,
};
writeFileSync(out, JSON.stringify(file));
const lost = end?.after?.lost;
console.log(`${kind} on ${pick.name} (region ${pick.target}, groups ${pick.ids.join(',')}): ${views.length} views, ${v.t?.toFixed?.(0)} battle s, over ${JSON.stringify(v.over?.level ?? v.over)}, lost ${JSON.stringify(lost)}, ok ${end?.ok}; ${(JSON.stringify(file).length / 1e6).toFixed(2)} MB`);

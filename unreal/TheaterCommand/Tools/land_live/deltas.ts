// Writes a live ground battle as the 3D game hears it, for the cloud test of the game's live readers (land_live_test.cpp):
// the header, every delta (one a line), and the same battle as one recording (the deltas put together, as the recorder
// writes a battle whole), so the test can check that a battle grown delta by delta is the battle read whole.
//
// Built and run by run.sh:  node land-live-deltas.mjs <out folder> [key] [seed] [me att|def|none] [seconds a tick] [orders]
// "orders": the player gives a few orders on the way (an attack, a run at the double, one handed back to his staff),
// so the test sees taken blocks and orders on the air.
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { LiveKeeper, livePiece, liveTicket, type LiveDelta, type LiveOpts } from '../../../../src/battle/land/live';
import { PIECES, ticketOf } from '../../../../src/battle/land/pieces';
import { ownTeamsLook } from '../../../../src/battle/land/own-teams';
import type { Side } from '../../../../src/battle/core/types';

const [folder = '.', key = 'normandy-1944', seedArg, meArg = 'att', tickArg = '0.5', ordersArg = ''] = process.argv.slice(2);
const me: Side | null = meArg === 'none' ? null : (meArg as Side);
// (LAND_LIVE_SWITCHES=blast,escort,mech,spg,td,cars,aa,life,hatches turns those Battle rules card rows on for the run (release 2). LAND_LIVE_BEARERS=0:
// stretcher bearers off, as release 1.5 ships them; unset, on, as the rules start them. Deltas already written in the
// run's folder are reused unless LAND_LIVE_FIGHT is set: set it whenever the rules or a switch changed.)
const on = new Set((process.env.LAND_LIVE_SWITCHES ?? '').split(',').filter(Boolean));
const opts: LiveOpts = { me, setup: false, ...(process.env.LAND_LIVE_BEARERS === '0' ? { bearers: false } : {}), ...(on.has('blast') ? { blast: true } : {}), ...(on.has('escort') ? { escort: true } : {}), ...(on.has('mech') ? { mech: true } : {}), ...(on.has('spg') ? { spg: true } : {}), ...(on.has('td') ? { td: true } : {}), ...(on.has('cars') ? { cars: true } : {}), ...(on.has('aa') ? { aa: true } : {}), ...(on.has('light') ? { light: true } : {}), ...(on.has('life') ? { life: true } : {}), ...(on.has('hatches') ? { hatches: true } : {}) };
// (LAND_LIVE_CAMPAIGN=1: the set piece's ticket fought as a campaign battle, no set piece: its stand-in models, and the
// self-propelled guns and tank destroyers the spg and td switches draw only there)
// (LAND_LIVE_TICKET=<file>: a campaign battle's ticket file, {ticket, live} as the campaign hands it over (its sky and
// place), fought instead of the set piece: the lorried infantry's seats, 23 November)
const piece = PIECES.find((p) => p.key === key);
const ticketFile = process.env.LAND_LIVE_TICKET ? JSON.parse(readFileSync(process.env.LAND_LIVE_TICKET, 'utf8')) : null;
// (LAND_LIVE_SWITCHES=teams, a campaign battle's only: every infantry force with its own machine-gun and mortar
// battalions, as the Battle rules card's "teams" row has the bridge make them: src/battle/land/own-teams.ts)
const own = <T extends Parameters<typeof ownTeamsLook>[0]>(t: T): T => (on.has('teams') ? (ownTeamsLook(t) as T) : t);
const withOwn = ticketFile ? own(ticketFile as { ticket: Parameters<typeof liveTicket>[0]; live?: LiveOpts }) : null;
const L = withOwn
  ? liveTicket(withOwn.ticket, { ...opts, ...(withOwn.live ?? {}) })
  : process.env.LAND_LIVE_CAMPAIGN === '1' && piece
  ? liveTicket(own(ticketOf(piece, seedArg ? Number(seedArg) : piece.seeds[0], me)), opts)
  : livePiece(key, seedArg ? Number(seedArg) : undefined, opts);
const header = L.header();
const deltas: LiveDelta[] = [];
const lines: string[] = [];
const blocks = header.blocks as { id: number; side: string; kind: string }[];
const mine = blocks.filter((b) => b.side === me);
const theirs = blocks.filter((b) => b.side !== me);
let guard = 0;
while (!L.sim.over && guard++ < 100000) {
  const d = L.tick(Number(tickArg), 1000);
  deltas.push(d);
  lines.push(JSON.stringify(d));
  if (ordersArg && me) {
    if (Math.abs(d.t - 20) < 0.3) L.order({ do: 'attack', units: [mine[0].id], target: theirs[0].id });
    if (Math.abs(d.t - 25) < 0.3) L.order({ do: 'move', units: [mine[1].id], x: 1500, y: 700, run: true });
    if (Math.abs(d.t - 60) < 0.3) L.order({ do: 'land:staff', units: [mine[0].id] });
  }
}
mkdirSync(folder, { recursive: true });
writeFileSync(join(folder, 'header.json'), JSON.stringify(header));
writeFileSync(join(folder, 'deltas.jsonl'), lines.join('\n') + '\n');
// The battle whole: the header with the deltas put together.
const whole = { ...header, live: false };
whole.men = LiveKeeper.assemble(deltas);
whole.rounds = deltas.flatMap((d) => d.rounds);
whole.events = deltas.flatMap((d) => d.events);
whole.views = deltas.flatMap((d) => d.views);
const phases = new Map<number, unknown[]>();
for (const d of deltas) for (const p of d.phases) phases.set(p.b, [...(phases.get(p.b) ?? []), ...p.ph]);
whole.phases = [...phases.entries()].map(([b, ph]) => ({ b, ph }));
const last = deltas[deltas.length - 1];
whole.contact = deltas.find((d) => d.contact !== null)?.contact ?? null;
whole.over = last.over ?? null;
whole.end = last.end ?? last.t;
writeFileSync(join(folder, 'whole.json'), JSON.stringify(whole));
console.log(`${key}: ${deltas.length} deltas to t=${last.t}, ${(lines.join('').length / 1e6).toFixed(1)} MB; ${(whole.men as unknown[]).length} tracks, ${(whole.rounds as unknown[]).length} rounds`);

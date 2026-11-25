// Men in lorries' cloud test (release 1.5): a campaign ticket with lorried infantry (LAND_LIVE_TICKET, a {ticket, live}
// file) fought live, with the Battle rules card's Men in lorries row on or off (LAND_LIVE_TRUCKS=1), written as
// deltas.ts writes a battle: header.json, deltas.jsonl and whole.json in the folder.
// Usage: esbuild-bundled, node trucks-deltas.mjs <folder> (or the folder in LAND_LIVE_OUT)
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { LiveKeeper, liveTicket, type LiveDelta } from '../../../../src/battle/land/live';

const folder = process.argv[2] ?? process.env.LAND_LIVE_OUT ?? '.';
const file = JSON.parse(readFileSync(process.env.LAND_LIVE_TICKET ?? '', 'utf8'));
const L = liveTicket(file.ticket, { me: 'att', setup: false, ...(file.live ?? {}), ...(process.env.LAND_LIVE_TRUCKS === '1' ? { trucks: true } : {}) });
const header = L.header();
const deltas: LiveDelta[] = [];
const lines: string[] = [];
let guard = 0;
while (!L.sim.over && guard++ < 100000) {
  const d = L.tick(0.5, 1000);
  deltas.push(d);
  lines.push(JSON.stringify(d));
}
mkdirSync(folder, { recursive: true });
writeFileSync(join(folder, 'header.json'), JSON.stringify(header));
writeFileSync(join(folder, 'deltas.jsonl'), lines.join('\n') + '\n');
const whole = { ...header, live: false } as Record<string, unknown>;
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
console.log(`${deltas.length} deltas to t=${last.t}, ${(lines.join('').length / 1e6).toFixed(1)} MB; ${(whole.men as unknown[]).length} tracks, ${(whole.rounds as unknown[]).length} rounds`);

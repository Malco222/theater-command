// Neighbours join the fight (release 2), the 3D game's half: a live battle whose ticket carries a corps coming up from
// next door on each side (the rules' SideTicket.joiners, the tactics thread's part (b)) is written as the 3D game hears
// it, and checked: the header names the joiners from the start ('joins'), the views say where and when the player's
// own corps (and an enemy one his staff knows of) is coming ('coming'), each corps walks on together at its time with
// a 'land:joined' event the dispatches speak, and its men are drawn from then on. The test ticket is the Normandy set
// piece's own, with a slice of each side's power moved into its joiner, so the card's A and D stand as they were; the
// game never fakes a joiner on a practice battle (the production lead, 08:03Z): only this test builds one.
// Hold until relieved and the rider ahead (release 2, the tactics thread's 3bda466e): early on, his first block on foot
// that may wait for help is told to (land:wait, as the general's HOLD TILL RELIEVED gives it, after asking waitWhy as the
// screen does); its views carry waits (his own only) with the due second the check said, and his corps' despatch rider
// rides in (riders, his side only) and leaves the views when his corps comes up.
//
// Built and run by command.sh (its join folder then read by land_command_test):  node land-live-join.mjs <out folder> [key] [seed]
import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { LiveBattle, type LiveDelta } from '../../../../src/battle/land/live';
import { PIECES, ticketOf } from '../../../../src/battle/land/pieces';
import type { ForceTicket, SideTicket } from '../../../../src/battle/core/types';

const [folder = '.', key = 'normandy-1944', seedArg = '11'] = process.argv.slice(2);
const piece = PIECES.find((p) => p.key === key);
if (!piece) throw new Error(`no set piece ${key}`);
const ticket = structuredClone(ticketOf(piece, Number(seedArg), 'att'));
let failures = 0;
const fail = (what: string): void => { failures++; if (failures <= 20) console.log(`FAIL: ${what}`); };

/** A side's first division-sized force, copied as a corps from next door with `part` of the side's power moved into it. */
function joinerFor(t: SideTicket, corps: number, part: number, joined: number, arrive: ForceTicket['arrive'], toward: number): void {
  const base = t.forces.find((f) => f.kind === 'arm') ?? t.forces[0];
  for (const f of t.forces) f.share *= 1 - part;
  t.joiners = [{ ...structuredClone(base), corps, name: `${base.name} (next door)`, share: part, divs: 1, joined, from: 0, arrive, toward }];
}
// (his own by the south edge, behind his line, so his despatch rider beats it there; the enemy's on its flank)
joinerFor(ticket.att, 9001, 0.15, 0.3, 'late', Math.PI);
joinerFor(ticket.def, 9002, 0.15, 0.6, 'midway', -Math.PI / 2);

const L = new LiveBattle(ticket, piece, { me: 'att', setup: false });
const header = L.header();
const blocks = header.blocks as { id: number; side: string; joins?: boolean; corps: number }[];
const joiners = blocks.filter((b) => b.joins);
const idsOf = (side: string): Set<number> => new Set(joiners.filter((b) => b.side === side).map((b) => b.id));
const own = idsOf('att'), foe = idsOf('def');
if (!own.size || !foe.size) fail(`the header names joiners on both sides (att ${own.size}, def ${foe.size})`);
const most = Math.max(...blocks.filter((b) => !b.joins).map((b) => b.id));
if (joiners.some((b) => b.id <= most)) fail('the joiners are numbered after both armies');

const lines: string[] = [];
const comingSeen = { att: 0, def: 0 }, joinedAt: Record<string, number> = {};
let firstUnits: Record<string, number> = {}, tracks: Record<string, number> = { att: 0, def: 0 };
let guard = 0, last: LiveDelta | null = null;
let waiter = -1, waitDue: number | null = null, firstDue = -1, waitsSeen = 0, waitsWrong = 0, ridersSeen = 0, riderLastAt = -1, riderSays = '';
const waitEvents: string[] = [];
while (!L.sim.over && guard++ < 100000) {
  const d = L.tick(0.5, 1000);
  lines.push(JSON.stringify(d));
  last = d;
  if (waiter < 0 && L.sim.t >= 5) {
    // (HOLD TILL RELIEVED: the first of his blocks on the field the rules let wait, as the screen asks before it offers it)
    for (const u of L.sim.units) {
      if (u.side !== 'att' || own.has(u.id)) continue;
      const w = L.waitWhy(u.id);
      if (w.why !== null) continue;
      waiter = u.id;
      waitDue = w.due;
      if (!L.order({ do: 'land:wait', units: [u.id], at: L.sim.t } as never)) fail(`the wait order for block ${u.id} is taken`);
      break;
    }
    if (waiter < 0) fail('one of his blocks on foot may wait for his corps coming up');
    const foeOne = L.sim.units.find((u) => u.side === 'def');
    if (foeOne && L.waitWhy(foeOne.id).why === null) fail('an enemy block is never his to hold');
  }
  for (const e of d.events as { what: string }[]) if (e.what === 'land:waits' || e.what === 'land:relieved' || e.what === 'land:late') waitEvents.push(e.what);
  for (const v of d.views as { t: number; units: { id: number; side: string; waits?: { due: number; late?: boolean; free?: boolean } }[]; riders?: { corps: number; side: string; says: string }[] }[]) {
    for (const u of v.units) {
      if (!u.waits) continue;
      if (u.side !== 'att' || u.id !== waiter) waitsWrong++;
      else {
        // (the rules reckon it again as the order lands, after the radio's few seconds: near what the check said, then fixed)
        waitsSeen++;
        if (waitDue !== null && Math.abs(u.waits.due - waitDue) > 10) fail(`its waits' due ${u.waits.due} near the second the check said, ${waitDue}`);
        if (firstDue < 0) firstDue = u.waits.due;
        else if (u.waits.due !== firstDue) fail(`its due stays put (${firstDue}, then ${u.waits.due})`);
      }
    }
    for (const r of v.riders ?? []) {
      if (r.side !== 'att' || r.corps !== 9001) fail(`a rider of his own corps only (${r.side} ${r.corps})`);
      ridersSeen++;
      riderLastAt = v.t;
      riderSays = r.says;
    }
  }
  for (const v of d.views as { t: number; units: { id: number }[]; coming?: { id: number; side: string; at: number }[] }[]) {
    for (const c of v.coming ?? []) {
      comingSeen[c.side as 'att' | 'def']++;
      if (v.units.some((u) => u.id === c.id)) fail(`block ${c.id} is both coming and on the field at ${v.t}`);
      if (c.at < v.t - 1) fail(`block ${c.id} still coming at ${v.t}, past its time ${c.at}`);
    }
    for (const u of v.units) {
      const side = own.has(u.id) ? 'att' : foe.has(u.id) ? 'def' : null;
      if (side && firstUnits[side] === undefined) firstUnits[side] = v.t;
    }
  }
  for (const e of d.events as { what: string; side: string; text?: string; t?: number }[]) {
    if (e.what === 'land:joined' && joinedAt[e.side] === undefined) { joinedAt[e.side] = d.t; console.log(`joined at ${d.t}: ${e.text}`); }
  }
  for (const m of d.men as { b?: number }[]) {
    if (m.b !== undefined && own.has(m.b)) tracks.att++;
    if (m.b !== undefined && foe.has(m.b)) tracks.def++;
  }
}
mkdirSync(folder, { recursive: true });
writeFileSync(join(folder, 'header.json'), JSON.stringify(header));
writeFileSync(join(folder, 'deltas.jsonl'), lines.join('\n') + '\n');

console.log(`joiners: att ${own.size} blocks, def ${foe.size}; coming said ${comingSeen.att} times for his own, ${comingSeen.def} for the enemy's; ` +
  `joined at att ${joinedAt.att ?? 'never'}, def ${joinedAt.def ?? 'never'}; on the field from att ${firstUnits.att ?? 'never'}, def ${firstUnits.def ?? 'never'}; ` +
  `their men's tracks att ${tracks.att}, def ${tracks.def}; the battle to ${last?.t}`);
if (!comingSeen.att) fail('his own corps coming is said before it comes');
if (joinedAt.att === undefined || firstUnits.att === undefined) fail('his own corps walks on, its coming told');
if (joinedAt.att !== undefined && firstUnits.att !== undefined && Math.abs(joinedAt.att - firstUnits.att) > 1.01) fail('his corps is on the field the second its coming is told');
if (firstUnits.att !== undefined && !tracks.att) fail('his corps\' men are drawn once it is on');
console.log(`hold until relieved: block ${waiter} told to wait (help due at ${waitDue}, then ${firstDue} as it landed), in ${waitsSeen} views; events ${waitEvents.join(', ') || 'none'}; ` +
  `the rider in ${ridersSeen} views, last at ${riderLastAt}: "${riderSays}"`);
if (!waitsSeen || waitsWrong) fail(`his waiting block's views carry waits, and no other block's (${waitsSeen}, wrong ${waitsWrong})`);
if (!waitEvents.includes('land:waits') || !waitEvents.includes('land:relieved')) fail('the rules say it digs in, and that help reached it');
if (!ridersSeen) fail('his corps\' despatch rider rides in ahead of it');
if (riderLastAt >= 0 && firstUnits.att !== undefined && riderLastAt > firstUnits.att) fail('the rider leaves the views when his corps comes up');
console.log(failures ? `FAIL (${failures})` : 'the joiners came as the card priced them');
process.exit(failures ? 1 : 0);

// A campaign battle opened live from its ticket (release 2, the production lead's (a)): the 3D game's half. The live
// battle the game opens from a FieldTicket (TC.landStartTicket, liveTicket) is the same battle as the set piece's own
// when the ticket is that piece's: every delta the same, byte for byte, to the end. Without a piece (a campaign battle)
// it opens and fights to its end all the same, no model named. Under the card's fog (fog, seen) the enemy's blocks the
// card did not identify show only where they stand until met, and its result (over) is what goes back to the campaign's
// TC.battleSettleLand.
//
// Built and run by command.sh:  node land-live-ticket.mjs [key] [seed]
import { liveTicket, livePiece, type LiveBattle, type LiveOpts } from '../../../../src/battle/land/live';
import { PIECES, ticketOf } from '../../../../src/battle/land/pieces';
import '../../Rules/land-bridge';

const [key = 'normandy-1944', seedArg = '11'] = process.argv.slice(2);
const piece = PIECES.find((p) => p.key === key);
if (!piece) throw new Error(`no set piece ${key}`);
const seed = Number(seedArg);
let failures = 0;
const fail = (what: string): void => { failures++; if (failures <= 20) console.log(`FAIL: ${what}`); };

/** Fights it to the end a piece at a time, as the worker does; each delta's JSON. */
function fight(L: LiveBattle): string[] {
  const out: string[] = [];
  for (let guard = 0; !L.sim.over && guard < 100000; guard++) out.push(JSON.stringify(L.tick(0.5, 1000)));
  return out;
}

const opts: LiveOpts = { me: 'att', setup: false };
// 1. The set piece's own ticket, opened as the campaign's would be: the same battle as the piece by key.
const byKey = livePiece(key, seed, opts), byTicket = liveTicket(structuredClone(ticketOf(piece, seed, 'att')), opts, key);
const h1 = JSON.stringify(byKey.header()), h2 = JSON.stringify(byTicket.header());
if (h1 !== h2) fail('the header from the ticket is the piece\'s own');
const d1 = fight(byKey), d2 = fight(byTicket);
let same = d1.length === d2.length ? 0 : -1;
if (same === 0) for (let i = 0; i < d1.length; i++) if (d1[i] !== d2[i]) { same = i + 1; break; }
if (same !== 0) fail(`every delta the same (${d1.length} against ${d2.length}, first apart ${same})`);
console.log(`${key} by its ticket: ${d2.length} deltas, the same as by its key: ${same === 0 ? 'yes' : 'no'}; over ${JSON.stringify(byTicket.sim.over?.level)} R ${byTicket.sim.over?.R}`);

// 2. A campaign battle: no piece, under the card's fog with none identified, and a design year for each side's armour.
const t = structuredClone(ticketOf(piece, seed, 'att'));
const camp = liveTicket(t, { ...opts, fog: true, seen: [], designYears: { [t.att.nat]: 1943, [t.def.nat]: 1944 } });
const head = camp.header() as { key: string; blocks: { side: string; kind: string; team?: unknown; model: string | null }[]; field: { place: unknown } };
// (the 3D game's tank and gun models so far: Content/Models/Battle)
const HAVE = new Set(['Sherman', 'PanzerIV', 'Tiger', 'StuG3', 'T34', 'LightTank', 'Howitzer105', 'leFH18', 'ZiS3']);
const drawn = head.blocks.filter((b) => b.kind === 'arm' || (b.kind === 'art' && !b.team));
if (head.key !== 'field') fail('a campaign battle names no set piece');
if (drawn.some((b) => !b.model || !HAVE.has(b.model))) fail('each of its tank and gun blocks names a model the 3D game has');
// (stand-ins are looks only: the rules weigh none of them, so the battle is the one the campaign priced)
if ((camp.sim as unknown as { models: Map<number, string> }).models.size) fail('a stand-in model is looks only, never weighed by the rules');
console.log(`a campaign battle's models: ${drawn.map((b) => `${b.side} ${b.model}`).join(', ')}`);
if (!head.field.place) fail('its header carries the ticket\'s place');
let unknownSeen = 0, knownFoe = 0, lines = 0;
for (const text of fight(camp)) {
  lines++;
  const d = JSON.parse(text) as { views: { units: { side: string; known?: boolean }[] }[] };
  for (const v of d.views) for (const u of v.units) {
    if (u.side !== 'def') continue;
    if (u.known === false) unknownSeen++;
    else knownFoe++;
  }
}
console.log(`a campaign battle from its ticket: ${lines} deltas to ${camp.sim.t.toFixed(1)} s, over ${JSON.stringify(camp.sim.over?.level)}; under the card's fog the enemy unknown ${unknownSeen} times, known ${knownFoe}`);
if (!camp.sim.over) fail('it fights to its end');
if (!unknownSeen) fail('under the card\'s fog an enemy block not identified shows only where it stands');
if (camp.sim.over && typeof camp.sim.over.R !== 'number') fail('its result carries R for the campaign\'s settle');
const sky = (head as unknown as { sky?: { cover: number; kind: string; base: number } }).sky;
if (!sky || !(sky.cover >= 0 && sky.cover <= 1) || !sky.base) fail('a campaign battle\'s header carries a sky from its ticket');

// 3. Through the bridge as the 3D game calls it: the campaign's TC.battleLook answer as it stands, fought to its end,
// and TC.landResult the very result the last delta carried (what goes back to the campaign's TC.battleSettleLand).
type Calls = Record<string, (...a: unknown[]) => unknown>;
const TC = (globalThis as unknown as { TC: Calls }).TC;
const look = { ticket: structuredClone(ticketOf(piece, seed, 'att')), live: { fog: true, seen: [], designYears: {}, sky: { cover: 0.8, base: 700, kind: 'deck', latLon: [51.9, 5.9], hour: 8 } } };
const bh = TC.landStartTicket(look, { me: 'att', setup: false }) as { key: string; sky?: { cover: number; latLon?: number[]; hour?: number } };
if (bh.key !== 'field' || bh.sky?.cover !== 0.8 || bh.sky?.latLon?.[0] !== 51.9 || bh.sky?.hour !== 8) fail('the bridge opens the campaign\'s look answer with its sky');
let lastOver: unknown = null, n = 0;
for (let guard = 0; guard < 100000; guard++) {
  const d = TC.landTick(0.5, 1000) as { end?: number; over?: unknown };
  n++;
  if (d.over) { lastOver = d.over; break; }
}
const res = (TC.landResult() as { over: unknown }).over;
const sameOver = !!lastOver && JSON.stringify(res) === JSON.stringify(lastOver);
console.log(`through the bridge: ${n} deltas; TC.landResult the last delta's over: ${sameOver ? 'yes' : 'no'} (${JSON.stringify((res as { level?: string } | null)?.level)})`);
if (!sameOver) fail('TC.landResult is the result the last delta carried');
TC.landEnd();
console.log(failures ? `FAIL (${failures})` : 'a ticket opens the battle the campaign priced');
process.exit(failures ? 1 : 0);

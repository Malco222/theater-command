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
const head = camp.header() as { key: string; blocks: { side: string; model: string | null }[]; field: { place: unknown } };
if (head.key !== 'field' || head.blocks.some((b) => b.model !== null)) fail('a campaign battle names no set piece and no model');
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
console.log(failures ? `FAIL (${failures})` : 'a ticket opens the battle the campaign priced');
process.exit(failures ? 1 : 0);

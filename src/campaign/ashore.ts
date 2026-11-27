// The test of strength (Version 83; the design director's rulings 89, 92 and 94). Before a force is put across the
// sea, onto a held coast or into a friendly harbour the enemy can reach, a staff reckons what the defender can bring
// to that place within three months and what it will have there itself, and sails only with a force it expects to
// be still ashore after them. A player is never held to it: his landing card shows him the same reckoning as one
// more line, on what his side knows (`ashoreSays`).
//
// The reckoning is a count of strength by land round the place (open.json, 'descent.strength'):
//   theirs   the enemy's field corps within three marches of it, as they would attack (one already in the line
//            against an army farther off counts for a share: it cannot all be brought); and a share of his reserves
//            farther off, out of the line, on ground of his in one piece with ground he holds within those marches:
//            his railways bring them within the time (a bigger share where the place is within those marches of
//            his own country: he defends his own door with more of what he has free). Only the holder of the
//            place brings them from his own country: an ally's army standing in its own home stays there, for its
//            own coasts (ruling 233(s): the divisions that reached Normandy were Germany's; a place in the hands
//            of the force's own side has no such holder, and there it is whoever holds ground within the marches). Across ground he does not
//            hold he must march, so a harbour more than three marches from any ground of his is out of his reach;
//   ours     the force as it would defend the place, the field corps of its side already within those marches, and
//            the corps standing ready that the ships can bring in after it within the time.
// The chance is ours / (ours + theirs). Nothing here changes the game: no flag is written and no dice are thrown.
import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import openData from '../../data/campaign/open.json';
import { allied, atWar, corpsBase, roomless } from './game';
import { corpsKnown, fogOn } from './intel';
import { fieldCtrl } from './siege';
import { fitWave, islandOf, roomLeft } from './islands';
import { KIND, TERRAIN_DEF, type Corps, type Game, type Region } from './types';

/** The numbers of the reckoning (open.json, 'descent.strength'). */
export interface StrengthRules {
  /** The months the reckoning looks ahead; they are also how many marches from the place an army counts in full. */
  months: number;
  /** The share that counts of an enemy corps within those marches that is in the line against an army farther off. */
  line: number;
  /** The share that counts of his field corps farther off, out of the line, that his railways could bring. */
  reserve: number;
  /** That share where the place is within the marches of his own home country and he holds it: he defends his own door. */
  door: number;
  /** True: only the holder of the place brings reserves from his own country; an ally's corps standing in its own home is no reserve for this place (ruling 233(s)). */
  own?: boolean;
  /** The chance a staff asks for before it sails. */
  sure: number;
  /** The chance a bold staff sails on. */
  bold: number;
  /** None sails on less: an even chance. */
  even: number;
  /** The most corps a staff puts into one first wave when its usual force does not pass (shipping and the quay allowing). */
  wave: number;
  /** The main front (ruling 98(e)): for the staffs named, a harbour of a front their side holds on this home ground comes before every other. */
  first?: { for: string[]; ground: string[] };
}
export const STRENGTH = (openData as unknown as { descent?: { strength?: StrengthRules } }).descent?.strength ?? null;

/** A corps as it would attack, and as it would defend a province. */
export const strikes = (g: Game, c: Corps): number => corpsBase(g, c) * KIND[c.kind].atk;
export const stands = (g: Game, c: Corps, r: Region): number => corpsBase(g, c) * KIND[c.kind].def * TERRAIN_DEF[r.terrain];
/** A corps of the field army: no fortress, and no town's standing garrison (neither marches anywhere). */
const field = (c: Corps): boolean => !c.dead && c.kind !== 'fort' && !c.town;

/** What a staff reckons before it puts a force down at a place. */
export interface Reckoning {
  /** What the defender can bring within the months, as it would attack: his corps within the marches, and the share of his reserves farther off. */
  theirs: number;
  near: number;
  reserve: number;
  /** How many of his field corps stand within the marches, and how many farther off were counted as reserves. */
  corps: number;
  behind: number;
  /** What the staff will have there, as it would defend: the force, the rows of the same plan landing beside it (a plan's test: `reckon`), its side's corps already within the marches, and what the ships bring in after it. */
  ours: number;
  force: number;
  beside: number;
  /** The rows counted beside the force, by name: those of `beside` whose beach lies on the ground theirs is counted from. */
  rows: string[];
  friends: number;
  follow: number;
  /** ours / (ours + theirs): 1 where nothing of his can come. */
  chance: number;
  /** The same two sides in divisions at their present strength, with the same shares: what a card can say in a number a player knows. */
  divs: { theirs: number; ours: number };
}

/**
 * The reckoning for a force put down at a place. `follow` is the strength the ships can bring in after it within the
 * months, as it would defend the place (the caller knows the quay and the shipping: `followUp`). `seen`, when given,
 * says how much of an enemy corps the reckoning may count (0 for one its side knows nothing of): a player's card
 * counts what his side knows, a computer's staff counts them all. `also`: countries the staff means to go to war
 * with, counted as its enemies already (the readiness test, readiness.ts). `beside`: the rows of the same plan that
 * land on its other beaches, each with its beach (rider b642 to ruling 234: one ground, both sides; `theirs` counts
 * every enemy corps within the marches of the place, those the plan's other landings are aimed at with them, so `ours`
 * counts every row of the plan that lands within the ground theirs is counted from: in full on a beach within the
 * marches, at the reserve's share on his ground beyond in one piece with it, as it would defend its own beach).
 * Null where the rules have no such test.
 */
export function reckon(g: Game, nat: string, at: Region, force: Corps[], follow = 0, seen?: (c: Corps) => number, also?: ReadonlySet<string>, beside?: { c: Corps; at: Region }[]): Reckoning | null {
  const S = STRENGTH;
  if (!S) return null;
  const N = g.regions.length, M = S.months;
  const ours = (c: string): boolean => c === nat || allied(g, nat, c);
  const foe = (c: string): boolean => atWar(g, nat, c) || !!also?.has(c);
  const held = g.regions.map((r) => fieldCtrl(g, r));
  // Marches from the place, by land, over ground either side holds (neutral ground stops an army).
  const far = new Int16Array(N).fill(-1);
  const q = [at.id];
  far[at.id] = 0;
  for (let i = 0; i < q.length; i++) {
    if (far[q[i]] >= M) continue;
    for (const j of g.regions[q[i]].nb) if (far[j] < 0 && (ours(held[j]) || foe(held[j]))) { far[j] = far[q[i]] + 1; q.push(j); }
  }
  // His ground in one piece with ground he holds within those marches: where his trains can bring a reserve from.
  const rail = new Uint8Array(N);
  const w = q.filter((id) => foe(held[id]));
  for (const id of w) rail[id] = 1;
  // Whose own country lies within the marches, still in his hands: his reserves come to his own door in greater number.
  const door = new Set<string>();
  for (const id of w) { const r = g.regions[id]; if (!r.overseas && r.owner === held[id]) door.add(r.owner); }
  // Who brings reserves from his own country (ruling 233(s)): the holder of the place, alone. Where the place is in
  // the hands of the force's own side there is no such holder: then whoever holds ground within the marches (it is his front).
  const host = new Set<string>();
  if (foe(held[at.id])) host.add(held[at.id]);
  else for (const id of w) host.add(held[id]);
  for (let i = 0; i < w.length; i++) for (const j of g.regions[w[i]].nb) if (!rail[j] && foe(held[j])) { rail[j] = 1; w.push(j); }
  const mine = new Set(force.map((c) => c.id));
  const out: Reckoning = { theirs: 0, near: 0, reserve: 0, corps: 0, behind: 0, ours: 0, force: 0, beside: 0, rows: [], friends: 0, follow, chance: 1, divs: { theirs: 0, ours: 0 } };
  for (const c of force) { out.force += stands(g, c, at); out.divs.ours += c.divs * c.str; }
  for (const x of beside ?? []) {
    const share = far[x.at.id] >= 0 ? 1 : rail[x.at.id] ? S.reserve : 0;
    if (share <= 0 || mine.has(x.c.id)) continue;
    mine.add(x.c.id);
    out.rows.push(x.c.name);
    out.beside += share * stands(g, x.c, x.at);
    out.divs.ours += share * x.c.divs * x.c.str;
  }
  // A corps is in the line where an enemy of its own holds ground next to it or stands in its province: `near` says
  // whether any of that ground lies within the marches (then it is this place's own front, and it counts in full).
  const inLine = (c: Corps, r: Region): { tied: boolean; here: boolean } => {
    let tied = r.units.some((u) => !u.dead && atWar(g, c.nat, u.nat)), here = tied;
    for (const j of r.nb) if (atWar(g, c.nat, held[j])) { tied = true; if (far[j] >= 0) here = true; }
    return { tied, here: here || r.id === at.id };
  };
  for (let id = 0; id < N; id++) {
    if (far[id] < 0 && !rail[id]) continue;
    const r = g.regions[id];
    for (const u of r.units) {
      if (!field(u) || mine.has(u.id)) continue;
      if (ours(u.nat)) { if (far[id] >= 0) { out.friends += stands(g, u, r); out.divs.ours += u.divs * u.str; } continue; }
      if (!foe(u.nat)) continue;
      const share = seen ? seen(u) : 1;
      if (share <= 0) continue;
      const p = strikes(g, u) * share, L = inLine(u, r);
      const atHome = !!S.own && !r.overseas && r.owner === u.nat && !host.has(u.nat);
      const counts = far[id] >= 0 ? (L.tied && !L.here ? S.line : 1) : L.tied || atHome ? 0 : door.has(u.nat) ? S.door ?? S.reserve : S.reserve;
      if (far[id] >= 0) { out.near += counts * p; out.corps++; }
      else if (counts > 0) { out.reserve += counts * p; out.behind++; }
      out.divs.theirs += counts * share * u.divs * u.str;
    }
  }
  out.theirs = out.near + out.reserve;
  out.ours = out.force + out.beside + out.friends + out.follow;
  out.chance = out.theirs > 0 ? out.ours / (out.ours + out.theirs) : 1;
  return out;
}

/**
 * What the ships can bring in after a first wave within the reckoning's months, as it would defend the place: the
 * fit corps of the nation standing ready at the quay beside the wave, or a march from the quay on quiet ground of
 * its side (they are on the quay the month the wave has left it: a quay shared with an ally's corps holds no more
 * than a wave), as many as its shipping lifts in the months that are left and as the place has room for (and no more
 * than `most`, where a share of the shipping does the work). `counted`: corps the reckoning counts already (the rows
 * of a plan that land beside the wave), which are not counted a second time here.
 */
export function followUp(g: Game, nat: string, quay: Region, wave: Corps[], at: Region, most = Infinity, counted: Corps[] = []): number {
  const S = STRENGTH;
  if (!S) return 0;
  const gone = new Set([...wave, ...counted].map((c) => c.id));
  const there = at.units.filter((u) => !u.dead && (u.nat === nat || allied(g, nat, u.nat)) && !roomless(g, u)).length;
  // (An island of the Pacific takes the follow-up corps that still fit in men beside the wave and its side's corps
  // there, the strongest first: islands.ts `fitWave`. Other ground: five corps, as it always was.)
  const isle = islandOf(g, at) !== null;
  const room = Math.max(0, Math.min((S.months - 1) * g.nations[nat].sealift, isle ? Infinity : roomLeft(g, at, nat, wave.length + there), most));
  if (room <= 0) return 0;
  const ours = (c: string): boolean => c === nat || allied(g, nat, c);
  const quiet = (r: Region): boolean => ours(r.ctrl) && !r.units.some((u) => !u.dead && atWar(g, nat, u.nat)) && !r.nb.some((j) => atWar(g, nat, fieldCtrl(g, g.regions[j])));
  const ranked = [quay, ...quay.nb.map((j) => g.regions[j]).filter(quiet)]
    .flatMap((r) => r.units)
    .filter((u) => u.nat === nat && field(u) && u.kind !== 'eng' && !u.airborne && u.joint === undefined && u.str >= 0.6 && !gone.has(u.id))
    .map((u) => ({ u, s: stands(g, u, at) }))
    .sort((a, b) => b.s - a.s);
  const goes = isle ? new Set<Corps>(fitWave(g, at, nat, [...wave, ...ranked.map((x) => x.u)])) : null;
  return ranked
    .filter((x) => !goes || goes.has(x.u))
    .slice(0, room)
    .reduce((s, x) => s + x.s, 0);
}

/** How much of an enemy corps a nation's side knows of: all of one identified or reported, nothing of one never seen. */
export const knownTo = (g: Game, viewer: string | null) => (c: Corps): number => (corpsKnown(g, c, viewer).level === 'none' ? 0 : 1);

/** The reckoning as a player's landing card says it. */
export interface AshoreSaid {
  /** The small head of the line: 'Three months on'. */
  head: string;
  line: string;
  /** How the staff would call it: it would expect to hold, an even thing, or it would not expect the force to be still ashore. */
  tone: 'holds' | 'even' | 'lost';
  chance: number;
  /** The divisions the line names: the enemy's within the months, and his own there. */
  divs: { ours: number; theirs: number };
  /**
   * Where his staff would not expect the force to be still ashore, the card's red headline (ruling 134(d); ruling
   * 24(b): the headline is never more cheerful than the worst ticket). It is third in the order, after the fight's own
   * and 'Ashore, then cut off'. `text` is the line under it; `also` says the same as a second line under another headline.
   */
  warn?: { head: string; text: string; also: string };
}

/**
 * The test of strength as one line of a player's landing card (ruling 89(f)): the reckoning his rivals' staffs make
 * before they sail, for his own force at that beach, on what his side knows of the enemy. It counts the force and
 * the corps of his side already within the marches, and nothing that might follow: what follows is his to order.
 * It never stops him. Null where the rules have no such test.
 */
export function ashoreSays(g: Game, cs: Corps[], to: Region, viewer: string | null = g.human): AshoreSaid | null {
  const S = STRENGTH;
  if (!S || !cs.length) return null;
  const fog = fogOn(g, viewer);
  const R = reckon(g, cs[0].nat, to, cs, 0, fog ? knownTo(g, viewer) : undefined);
  if (!R) return null;
  const months = ['no', 'one', 'two', 'three', 'four', 'five', 'six'][S.months] ?? String(S.months);
  const head = `${months.charAt(0).toUpperCase()}${months.slice(1)} months on`;
  const reports = fog ? ', by your reports' : '';
  const count = (x: number): number => Math.max(1, Math.round(x));
  const n = (x: number): string => { const k = count(x); return `${k} division${k === 1 ? '' : 's'}`; };
  if (R.theirs <= 0) return { head, line: `No enemy army can reach ${to.name} by land within ${months} months${reports}.`, tone: 'holds', chance: 1, divs: { ours: count(R.divs.ours), theirs: 0 } };
  const tone = R.chance >= S.sure ? 'holds' : R.chance >= S.even ? 'even' : 'lost';
  const says = tone === 'holds' ? 'Your staff would expect to hold it.' : tone === 'even' ? 'Your staff calls it an even thing.' : 'Your staff would not expect this force to be still ashore by then.';
  const said: AshoreSaid = { head, line: `Within ${months} months the enemy can bring about ${n(R.divs.theirs)} against ${to.name}${reports}; you would have about ${n(R.divs.ours)} there. ${says}`, tone, chance: R.chance, divs: { ours: count(R.divs.ours), theirs: count(R.divs.theirs) } };
  if (tone === 'lost') {
    said.warn = {
      head: 'Ashore, then outnumbered',
      text: `The force can land, but within ${months} months the enemy can bring about ${n(R.divs.theirs)} against it${reports}${fog ? ',' : ''} and you would have about ${count(R.divs.ours)} there. Your staff would not expect it to be still ashore.`,
      also: `And a force that did get ashore would be outnumbered: within ${months} months the enemy can bring about ${n(R.divs.theirs)} against it${reports}${fog ? ',' : ''} and you would have about ${count(R.divs.ours)} there.`,
    };
  }
  return said;
}

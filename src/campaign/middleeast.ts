/**
 * Britain's army between home and the Middle East (ruling 229, part 1; data/campaign/middle-east.json). The floor its
 * staff keeps at home, from the record, which every departure by sea from the homeland respects; and the weighing of
 * a theatre's threat against home's, by which its spare field corps sail to Egypt (ai.ts, `reinforce`). The grand
 * campaign only, and only for a country the computer runs: a player's army goes where he sends it.
 */
import data from '../../data/campaign/middle-east.json';
import type { Corps, Game, Region } from './types';
import { allied, atWar, voyagesOf } from './game';
import { NAVY, canPass } from './navy';
import { stopOf } from './history';

interface Floor { from: number; to: number; share: number }
interface Reinforce { theatre: string; to: string[]; stop: number; risk: number; cover: number; waves: number; by: string[]; cape: number }

export const MIDDLE_EAST = data as unknown as { floor: Record<string, Floor[]>; reinforce: Record<string, Reinforce> };

/** A corps of the field army: not a fortress, not engineers, and not a command that holds in place. */
const field = (c: Corps, nat: string): boolean => c.nat === nat && !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && !c.hold;
/** Ground of the homeland: the country's own, and not a colony overseas. */
export const homeland = (r: Region, nat: string): boolean => r.owner === nat && !r.overseas;

/** The floor's share this month, or null where none is set (another country, the player's, a history campaign, outside its months). */
function floorShare(g: Game, nat: string): number | null {
  if (g.flags.open !== 1 || nat === g.human) return null;
  return MIDDLE_EAST.floor[nat]?.find((f) => g.turn >= f.from && g.turn <= f.to)?.share ?? null;
}

/** The size of a country's field army, as its staff counted it when its month began (`countField`), or now: with the corps at sea on a long passage, which belong to it wherever they are. */
const counted = new WeakMap<Game, Record<string, [number, number]>>();
const fieldNow = (g: Game, nat: string): number => g.corps.filter((c) => field(c, nat)).length + voyagesOf(g, nat).filter((v) => field(v.corps, nat)).length;
const fieldSize = (g: Game, nat: string): number => {
  const m = counted.get(g)?.[nat];
  return m && m[0] === g.turn ? m[1] : fieldNow(g, nat);
};

/**
 * Counts a country's field army for the floor as its staff's month begins (ai.ts): while the main army moves, the
 * staff marks the corps of a side theatre as holding in place, and they still count in the army the floor is
 * measured against.
 */
export function countField(g: Game, nat: string): void {
  if (floorShare(g, nat) === null) return;
  const m = counted.get(g) ?? {};
  m[nat] = [g.turn, fieldNow(g, nat)];
  counted.set(g, m);
}

/** The field corps a computer staff keeps at home this month (ruling 229, 1b): 0 where no floor is set. */
export function homeFloor(g: Game, nat: string): number {
  const share = floorShare(g, nat);
  if (share === null) return 0;
  return Math.ceil(share * fieldSize(g, nat) - 1e-6);
}

/** How many more field corps may leave the homeland by sea this month: Infinity where no floor binds. */
export function mayLeave(g: Game, nat: string): number {
  if (floorShare(g, nat) === null) return Infinity;
  const home = g.corps.filter((c) => field(c, nat) && homeland(g.regions[c.reg], nat)).length;
  return Math.max(0, home - homeFloor(g, nat));
}

/** True where a corps standing here would be leaving the homeland if it sailed. */
export const leavesHome = (g: Game, nat: string, from: Region): boolean => homeland(from, nat) && floorShare(g, nat) !== null;

/** The strength of a force, in divisions at full strength. */
const weight = (us: Corps[]): number => us.reduce((s, u) => s + u.divs * u.str, 0);

/**
 * A theatre's ground as its side holds it, joined by land to the first of its harbours that is in friendly hands; with
 * the threat on it (the strength of the enemy's corps in it or next to it) and the strength its side stands there with.
 * Null when its side holds none of the harbours.
 */
export function theatreFront(g: Game, nat: string, ground: Set<number>, ports: Region[]): { at: Region; on: Set<number>; threat: number; held: number } | null {
  const at = ports.find((p) => allied(g, nat, p.ctrl));
  if (!at) return null;
  const on = new Set([at.id]);
  const list = [at.id];
  for (let i = 0; i < list.length; i++) for (const j of g.regions[list[i]].nb) if (!on.has(j) && ground.has(j) && allied(g, nat, g.regions[j].ctrl)) { on.add(j); list.push(j); }
  const near = new Set(on);
  for (const id of on) for (const j of g.regions[id].nb) near.add(j);
  let threat = 0, held = 0;
  for (const id of near) for (const u of g.regions[id].units) {
    if (u.dead || u.kind === 'eng') continue;
    if (atWar(g, u.nat, nat) && u.kind !== 'fort') threat += u.divs * u.str;
    else if (on.has(id) && allied(g, u.nat, nat)) held += u.divs * u.str;
  }
  return { at, on, threat, held };
}

/**
 * What can really cross the sea to a country's homeland: for each enemy at war with it that has shipping, the
 * strongest of his field corps in the harbours he holds within a landing's range of its coast, as many as `waves`
 * months of his shipping carry; with the strength it stands at home with. Nothing where its coast is out of reach.
 */
export function homeThreat(g: Game, nat: string, waves: number): { threat: number; held: number } {
  const coast = new Uint8Array(g.seas.length);
  let held = 0;
  for (const r of g.regions) {
    if (!homeland(r, nat)) continue;
    for (const u of r.units) if (!u.dead && u.kind !== 'eng' && allied(g, u.nat, nat)) held += u.divs * u.str;
    if (r.port) for (const z of r.seas) coast[z] = 1;
  }
  // Seas within a landing's range of the coast, by the ways an enemy's ships could pass.
  let threat = 0;
  for (const e of [...g.nations[nat].atWar].sort()) {
    const x = g.nations[e];
    if (!x?.alive || x.dormant || x.sealift <= 0) continue;
    const d = new Int16Array(g.seas.length).fill(-1);
    const q: number[] = [];
    for (let z = 0; z < g.seas.length; z++) if (coast[z]) { d[z] = 1; q.push(z); }
    for (let i = 0; i < q.length; i++) {
      if (d[q[i]] >= NAVY.landing.range) continue;
      for (const j of g.seas[q[i]].adj) if (d[j] < 0 && canPass(g, j, q[i], e)) { d[j] = d[q[i]] + 1; q.push(j); }
    }
    const ready: Corps[] = [];
    for (const r of g.regions) {
      if (!r.port || r.ctrl !== e || !r.seas.some((z) => d[z] > 0)) continue;
      for (const u of r.units) if (u.nat === e && !u.dead && u.kind !== 'fort' && u.kind !== 'eng' && !u.town) ready.push(u);
    }
    ready.sort((a, b) => b.divs * b.str - a.divs * a.str || a.id - b.id);
    threat += weight(ready.slice(0, Math.max(0, Math.round(waves * x.sealift))));
  }
  return { threat, held };
}

/** The reinforcement rules for this country at this stop of the history dial, or null (ruling 229, 1a). */
export function reinforceRules(g: Game, nat: string): Reinforce | null {
  const R = MIDDLE_EAST.reinforce[nat];
  if (!R || g.flags.open !== 1 || nat === g.human || stopOf(g) < R.stop) return null;
  return R;
}

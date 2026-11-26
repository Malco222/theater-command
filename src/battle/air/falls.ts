// How a lost aircraft is seen to go down: Brian's ask of 2026-11-17 ("if a plane gets shot down it explodes,
// catches on fire on the engines and goes down and the crew can bail out some of the times").
//
// Cosmetic only. By the time a fall is drawn the group's count has already dropped by one; this picks how the 3D
// side shows it. The one number that matters to the rules is whether a fighter pilot comes back, and that is drawn
// at the rules' own rate (AIR.pilots.home over his own ground, away over the enemy's), so the parachutes the player
// sees and the pilots the rules give back are the same people.

import type { Rng } from '../../core/rng';
import { SKY } from './book';
import { aCraftOf, upFirst } from './names';
import type { AirKind, Fall } from './types';

const pick = (rng: Rng, w: number[]): number => {
  let x = rng.next() * w.reduce((s, v) => s + v, 0);
  for (let i = 0; i < w.length; i++) { x -= w[i]; if (x < 0) return i; }
  return w.length - 1;
};

/**
 * One fall. `over`: whose ground it is over. `saved`: the rules' chance that a fighter pilot downed there flies again.
 */
export function drawFall(rng: Rng, kind: AirKind, over: Fall['over'], saved: number): Fall {
  const F = SKY.fall;
  if (kind === 'bmr') {
    const k = (['explode', 'burn', 'spin'] as const)[pick(rng, F.bomber)];
    let chutes = 0;
    if (k !== 'explode') for (let i = 0; i < F.crew; i++) if (rng.chance(k === 'burn' ? F.bomberOut[0] : F.bomberOut[1])) chutes++;
    return { kind: k, chutes, over, back: false, captured: over === 'enemy' ? chutes : 0 };
  }
  const back = rng.chance(saved);
  if (back) {
    const k = (['burn', 'spin'] as const)[pick(rng, F.saved)];
    return { kind: k, chutes: 1, over, back: true, captured: 0 };
  }
  const k = (['explode', 'burn', 'spin'] as const)[pick(rng, F.lost)];
  // A pilot who is not coming back may still be seen to get out over enemy ground: he is taken prisoner.
  const out = k !== 'explode' && over === 'enemy' && rng.chance(k === 'burn' ? F.captured[0] : F.captured[1]);
  return { kind: k, chutes: out ? 1 : 0, over, back: false, captured: out ? 1 : 0 };
}

/**
 * The words for the dispatch strip. The group's own name and the nation word the line adds ('' for one's own), which
 * sits on the aircraft (the UI director, 14:26Z): 'An aircraft of KG 2', 'An American aircraft of the 15th Pursuit
 * Group', 'An aircraft of the Home Fleet's strike wing', 'One of the Japanese Shokaku and Zuikaku dive bombers'
 * (names.ts aCraftOf). The ways down passed as written (ui-director/words-air-battle-dispatches.md, 20 November 15:02Z).
 */
export function fallWords(name: string, adj: string, f: Fall): string {
  const how = f.how ? howWords(f) : f.kind === 'explode' ? 'blows up in the air' : f.kind === 'burn' ? 'goes down burning' : 'spins down';
  return `${upFirst(aCraftOf(name, 'aircraft', adj))} ${how}${endsOf(f.chutes, f.out ?? 0)}.`;
}

/** The endings: ', one parachute', ', 3 parachutes'; ', one man climbing out', ', 4 men climbing out' (passed, 15:02Z). */
function endsOf(chutes: number, out: number): string {
  const c = chutes === 0 ? '' : chutes === 1 ? ', one parachute' : `, ${chutes} parachutes`;
  const o = !out ? '' : out === 1 ? ', one man climbing out' : `, ${out} men climbing out`;
  return c + o;
}

/**
 * A collision downs two, so one line says both, each named as the dispatch names it ('a bomber of the 8th Bomb Group',
 * 'a German fighter of JG 26'), with the endings of the two together (the UI director, 20 November 15:02Z): 'A bomber
 * of the 8th Bomb Group and a fighter of JG 26 collide, and both go down.' A ram on purpose names the one who rams
 * first ('A Soviet fighter of 6 IAK rams a German bomber of KG 26, and both go down.'; passed 15:38Z, and "rams" alone
 * says it was meant). Two of the same group are named once, by `b` left empty and `a` as names.ts twoOf gives it: 'Two
 * bombers of the 8th Bomb Group collide, and both go down.' (15:38Z).
 */
export function collideWords(a: string, b: string, chutes: number, out: number, meant = false): string {
  const ends = `, and both go down${endsOf(chutes, out)}.`;
  if (!b) return `${upFirst(a)} collide${ends}`;
  return meant ? `${upFirst(a)} rams ${b}${ends}` : `${upFirst(a)} and ${b} collide${ends}`;
}

/** The way down in words (fates.ts). A collision is told by collideWords. */
function howWords(f: Fall): string {
  const apart = f.boom !== undefined ? ' and blows apart' : '';
  switch (f.how) {
    case 'explode': return 'blows up in the air';
    case 'flamer': return `goes down in flames${apart}`;
    case 'burn': return `goes down burning${apart}`;
    case 'wing': return 'loses a wing and goes down';
    case 'tail': return 'has its tail shot away and goes down';
    case 'pilot': return `noses over and dives into the ${f.end === 'sea' ? 'sea' : 'ground'}`;
    case 'glide': return f.ditch ? 'glides down and ditches in the sea' : f.out ? 'glides down and crash-lands' : 'glides down and crashes';
    case 'breakup': return 'breaks up in the air';
    case 'struck': return f.cause === 'debris' ? 'is hit by flying wreckage and goes down' : 'is struck by a falling aircraft and goes down';
    case 'collide': return 'goes down in a collision';
    default: return 'spins down';
  }
}

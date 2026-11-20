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
 * The words for the dispatch strip. Placeholders until the UI director and the design director pass them. The group's
 * own name and the nation word the line adds ('' for one's own), which sits on the aircraft (the UI director, 14:26Z):
 * 'An aircraft of KG 2', 'An American aircraft of the 15th Pursuit Group', 'An aircraft of the Home Fleet's strike
 * wing', 'One of the Japanese Shokaku and Zuikaku dive bombers' (names.ts aCraftOf).
 */
export function fallWords(name: string, adj: string, f: Fall, other = ''): string {
  const how = f.how ? howWords(f, other) : f.kind === 'explode' ? 'blows up in the air' : f.kind === 'burn' ? 'goes down burning' : 'spins down';
  const chutes = f.chutes === 0 ? '' : f.chutes === 1 ? ', one parachute' : `, ${f.chutes} parachutes`;
  const out = !f.out ? '' : f.out === 1 ? ', one man climbing out' : `, ${f.out} men climbing out`;
  return `${upFirst(aCraftOf(name, 'aircraft', adj))} ${how}${chutes}${out}.`;
}

/**
 * The way down in words (fates.ts; placeholders for the UI director). 'other': the aircraft it collided with, as the
 * dispatch names it ('a bomber of the 8th Bomb Group').
 */
function howWords(f: Fall, other: string): string {
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
    case 'collide': return other ? `collides with ${other} and goes down` : 'goes down in a collision';
    default: return 'spins down';
  }
}

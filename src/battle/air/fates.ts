// How an aircraft that is hit is seen to go, from the situation it is in (Brian, 20 November 2026, after the Pearl
// Harbor film: "i want many different ways a plan can go down, a another plane falling hitting another one, a win
// gets blown off ect. not all planes that get hit go down"; "i just dont want it to be the same visual effects each
// time or it will get boring"; "it should be variables and different situations").
//
// Shown, never counted (ruling 237). By the time a fate is drawn the count has already dropped and the rules have
// drawn whether a fighter pilot comes back (falls.ts drawFall, from its own stream, unchanged). Everything here comes
// from the battle's own stream for looks, so no count, pilot or bomb moves when a look changes.
//
// The situation weighs each way down: what hit it (a fighter's rifle-calibre guns, heavy machine guns or cannon; the
// guns round the target, light, medium or heavy), the way the fighter came at it (head-on, from astern, from below),
// its height, whether it still carries its bombs, whether it already has a dead engine, whether its tanks were
// protected, whether a night fighter came at it from below, and what is close by: a bomber that falls out of a tight
// box moments after another may be struck by it or by its wreckage, and a fighter lost in the same moment as the
// bomber it was attacking may have collided with it, or rammed it on purpose.
// The same way down is never shown twice running, and each carries a look number so the 3D side can vary even that.

import type { Rng } from '../../core/rng';
import type { Band } from './book';
import { SKY } from './book';
import type { Pass } from './doctrine';
import type { GunKind } from './guns';
import type { AirKind, Fall, FallHow } from './types';

/** The ways down the weights are kept for, in the order of SKY.fate.base. 'struck' and 'collide' come from what is close by. */
export const HOWS = ['explode', 'flamer', 'burn', 'wing', 'tail', 'pilot', 'spin', 'glide', 'breakup'] as const;
type Drawn = (typeof HOWS)[number];
type Mult = Partial<Record<Drawn, number>>;

/** Everything about one loss that the way it goes may turn on. */
export interface Scene {
  air: AirKind;
  /** Engines on the aircraft (a fighter: one, or two for the twin-engined fighters). */
  engines: number;
  /** The aircraft's nation and model, and the month. */
  nat: string;
  model: string;
  when: { year: number; month: number };
  /** What brought it down: a fighter's heaviest gun, or the guns round the target and the heaviest of them firing at its height. */
  gun: GunKind | null;
  flak: 'light' | 'medium' | 'heavy' | null;
  /** How the fighter that brought it down came at it, when it was a bomber box. */
  pass: Pass | null;
  band: Band;
  /** Metres up when it is hit. */
  alt: number;
  /** A bomber with its bombs still aboard. */
  loaded: boolean;
  /** The engines already dead on a straggler, or null. */
  strag: number[] | null;
  /** Over the sea. */
  sea: boolean;
  /** A night battle: a bomber a fighter brings down was mostly hit from below and astern, in the tanks. */
  night: boolean;
  /** The rules' fighter pilot who flies again, and whose ground it is over. */
  back: boolean;
  over: Fall['over'];
  /** Crew aboard (bombers: crewOf, by the model and the month), who may be seen to get out. */
  crew: number;
  /** A loss of its own tight box moments before, close by, or a fighter that was attacking the box ('foe'), that may strike it. */
  near: { fid: number; how: FallHow; foe?: boolean } | null;
}

/** What the battle remembers between falls, so a look is not shown twice running. */
export interface FateMemory { last: FallHow[]; looks: Partial<Record<FallHow, number>>; roll?: 1 | -1; side?: 'port' | 'starboard' }
export const freshMemory = (): FateMemory => ({ last: [], looks: {} });

/** The fate drawn for one loss; the sim folds it into the event's Fall. */
export type Fate = Required<Pick<Fall, 'how' | 'hit' | 'dur' | 'end' | 'roll' | 'look'>> &
  Pick<Fall, 'side' | 'engineAt' | 'fire' | 'boom' | 'with' | 'cause' | 'ditch' | 'out' | 'chuteAt'> & { chutes: number };

/** Japan's navy land bombers, the G4M and the G3M (the plain Japanese bomber is drawn as a G4M). */
const japanTwin = (model: string): boolean => /^(G4M|G3M|Betty|Nell|Bomber:(JAP|JPN))$/.test(model);
/**
 * Japan flew without self-sealing tanks or armour for most of the war: hit, its aircraft burned and blew apart far
 * more often. 'bare' until mid-1944, and the G4M until November 1944, when the G4M3 came (the historian, 20 November,
 * turn 63); 'part' after, for the A6M5b's CO2 bottles and armoured glass and a little armour came, but self-sealing
 * tanks only with the A6M6c from November 1944, and few were built, so most flew unprotected to the end.
 */
export function tanksOf(nat: string, model: string, w: { year: number; month: number }): 'bare' | 'part' | null {
  if (nat !== 'JAP' && nat !== 'JPN') return null;
  const d = w.year + w.month / 12;
  return d < 1944 + 6 / 12 || (japanTwin(model) && d < 1944 + 10 / 12) ? 'bare' : 'part';
}

/**
 * The chance a fighter lost in the same moment as the bomber it was firing on rammed it on purpose (book.ts
 * SKY.fate.ram.meant; the historian, 20 November): Soviet pilots, most in 1941 and 1942; Japan's against the American
 * four-engined bombers from mid-1944 (the B-29s), its ramming flights from November; Germany's Sturm groups closing
 * on the heavies from astern in 1944 and to April 1945. `month` counts from 0.
 */
export function ramShare(nat: string, w: { year: number; month: number }, engines: number, pass?: string): number {
  const d = w.year + w.month / 12, n = nat === 'JPN' ? 'JAP' : nat;
  const r = SKY.fate.ram.meant.find((m) => m.nat === n && d >= m.from && d < m.to && (!m.four || engines === 4) && (!m.pass || m.pass === pass));
  return r ? r.p : 0;
}

/**
 * The crew aboard a bomber, for who is seen to get out (book.ts SKY.fall.crews; the historian): two in a dive bomber,
 * three in a torpedo bomber, five in a He 111 (four before 1941), seven in a G4M or a Lancaster, ten in a B-17 (nine
 * from mid-1944). `month` counts from 0.
 */
export function crewOf(model: string, w: { year: number; month: number }): number {
  const F = SKY.fall, d = w.year + w.month / 12;
  const dated = F.crewsDated.find((c) => c.model === model && d >= (c.from ?? -Infinity) && d < (c.to ?? Infinity));
  return dated?.crew ?? F.crews[model] ?? F.crews[model.split(':')[0]] ?? F.crews.Bomber;
}

/**
 * How the engine is cooled, for the shade of what it trails when hit (the art director, 20 November: white glycol
 * vapour from a liquid-cooled engine, blue-grey oil smoke from a radial). By the model the 3D game draws; a nation's
 * plain fighter or bomber by what it mostly flew (France's Morane and Dewoitine and the Soviet SB in-line, the rest
 * radials).
 */
export function coolOf(model: string): 'liquid' | 'radial' {
  if (/^(Bf109|Bf110|Me410|Spitfire|Hurricane|Mustang|Warhawk|P38|P40|P51|Yak|Ki61|D4Y|Barracuda|Stuka|Ju87|Ju88|He111|Lancaster|Mosquito|MC202|C202|D520|Il2|Pe2|Whitley)/.test(model)) return 'liquid';
  if (/^(Fighter:FRA|Bomber:SOV)$/.test(model)) return 'liquid';
  return 'radial';
}
/** The twin-engined fighters. */
export const fighterEngines = (model: string): number => (/^(Bf110|Me410|P38|Beaufighter|Mosquito|Ki45|J1N|Pe3|Potez63)/.test(model) ? 2 : 1);

const mul = (w: number[], m: Mult | undefined, k = 1): void => {
  if (!m) return;
  HOWS.forEach((h, i) => { const f = m[h]; if (f !== undefined) w[i] *= Math.pow(f, k); });
};

const pick = (rng: Rng, w: number[]): number => {
  const sum = w.reduce((s, v) => s + v, 0);
  let x = rng.next() * sum;
  for (let i = 0; i < w.length; i++) { x -= w[i]; if (x < 0) return i; }
  return w.length - 1;
};

/** The weights of each way down for this scene, before what is close by: the base row times every factor that applies. */
export function weightsOf(sc: Scene, mem: FateMemory): number[] {
  const F = SKY.fate;
  const w = [...F.base[sc.air]];
  if (sc.gun) mul(w, F.gun[sc.gun]);
  if (sc.flak) mul(w, F.flak[sc.flak]);
  if (sc.pass) mul(w, F.pass[sc.pass]);
  if (sc.air === 'bmr') mul(w, sc.loaded ? F.loaded : F.dropped);
  const tanks = tanksOf(sc.nat, sc.model, sc.when);
  if (tanks) mul(w, tanks === 'bare' ? F.fragile : F.frail);
  // By night a fighter came at a bomber from below into its tanks; by day the heavies came down under control.
  if (sc.air === 'bmr' && sc.night && sc.gun) mul(w, F.night);
  else if (sc.air === 'bmr' && sc.engines === 4) mul(w, F.sturdy);
  if (sc.strag) mul(w, F.strag);
  mul(w, F.band[sc.band]);
  // A saved pilot was seen under a parachute: only the ways down he can get out of.
  if (sc.back) HOWS.forEach((h, i) => { if (!(F.saved as readonly string[]).includes(h)) w[i] = 0; });
  // Never the same twice running unless it is the only way left, and less often the closer it was shown (the rule the
  // ships and ground thread uses too, so the three kinds of battle feel alike).
  const was = [...w];
  mem.last.forEach((h, k) => { const i = (HOWS as readonly string[]).indexOf(h); if (i >= 0) w[i] *= F.again[k] ?? 1; });
  return w.some((v) => v > 0) ? w : was;
}

const range = (rng: Rng, [a, b]: readonly [number, number] | [number, number]): number => a + rng.next() * (b - a);

/** One fate. 'pair' forces a collision with the loss it names (the sim pairs a fighter with the bomber it attacked). */
export function fateOf(rng: Rng, sc: Scene, mem: FateMemory, pair?: { fid: number; cause: 'ram' }): Fate {
  const F = SKY.fate;
  let how: FallHow, cause: Fall['cause'], wth: number | undefined;
  if (pair) { how = 'collide'; cause = pair.cause; wth = pair.fid; }
  else {
    const n = sc.near;
    const struck = n && !sc.back && sc.air === 'bmr'
      ? rng.chance((n.foe ? F.near.foe : 1) * (n.how === 'explode' || n.how === 'breakup' ? F.near.debris : n.how === 'wing' || n.how === 'tail' || n.how === 'spin' ? F.near.falling : n.how === 'burn' || n.how === 'flamer' || n.how === 'pilot' ? F.near.burning : 0))
      : false;
    if (struck && n) { how = 'struck'; cause = n.how === 'explode' || n.how === 'breakup' ? 'debris' : 'falling'; wth = n.fid; }
    else how = HOWS[pick(rng, weightsOf(sc, mem))];
  }
  // Where it was hit, which side, which engine. A side or a roll the same as the last is drawn again once.
  let side: 'port' | 'starboard' = rng.chance(0.5) ? 'port' : 'starboard';
  if (side === mem.side) side = rng.chance(0.5) ? 'port' : 'starboard';
  // Which engine, as a place across the aircraft from port (0) to starboard (1), so the same draw reads the same on any
  // model; a straggler's is the engine it already lost (the event's 'eng').
  const engOf = (): number | undefined => (sc.strag?.length ? undefined : Math.round(rng.next() * 1000) / 1000);
  const hit: Fall['hit'] = how === 'burn' || how === 'glide' ? 'engine'
    : how === 'flamer' ? (sc.air === 'bmr' && rng.chance(0.5) ? 'wing' : 'tank')
    : how === 'wing' ? 'wing' : how === 'tail' ? 'tail' : how === 'pilot' ? 'cockpit'
    : how === 'explode' ? (sc.air === 'bmr' && sc.loaded ? 'bay' : 'tank')
    : how === 'spin' ? (rng.chance(0.5) ? 'tail' : 'wing') : 'fuselage';
  const eng = hit === 'engine' ? engOf() : undefined;
  // How fast it comes down, and so how long it is seen to fall.
  const v = range(rng, F.fallV[how]);
  const end: Fall['end'] = how === 'explode' ? 'air' : sc.sea ? 'sea' : 'land';
  const dur = Math.round((Math.max(200, sc.alt) / v) * 10) / 10;
  // A burning one may blow apart on the way down; sooner with its bombs aboard.
  const bp = how === 'burn' ? F.boom.burn : how === 'flamer' ? F.boom.flamer : 0;
  const tanks = tanksOf(sc.nat, sc.model, sc.when);
  const boom = bp > 0 && rng.chance(Math.min(0.9, bp * (sc.air === 'bmr' && sc.loaded ? F.boom.loaded : 1) * (tanks ? F.boomBare[tanks] : 1)))
    ? Math.round(dur * range(rng, F.boom.at) * 10) / 10 : undefined;
  const fire = how === 'burn' ? Math.round(range(rng, F.fire.burn) * 10) / 10 : how === 'flamer' ? Math.round(range(rng, F.fire.flamer) * 10) / 10 : undefined;
  // Who is seen to get out, and when each canopy opens (never after the end, never in a blast).
  //
  // A bomber's crew is by its type (crewOf), and the crew aboard must never move another look: the same battle flown
  // by two-seaters shows what the twins' shows but for who gets out (tests/battle-air-single-engine.test.ts). So this
  // stream throws the same dice whatever the crew: five men (SKY.fall.crew) and the canopies of as many of the five
  // as get out, as it did when every bomber carried five. A smaller crew is the first of those men, and its canopies
  // the first of theirs; a sixth man and after, and their canopies, are thrown on a stream of their own (`more`).
  const upTo = boom ?? dur;
  const DICE = SKY.fall.crew;
  // 'thrown': the canopies this stream times; 'ours' of them are seen; 'extra' more are the men past the fifth.
  let chutes = 0, out = 0, thrown = 0, ours = 0, extra = 0;
  let more: Rng | undefined;
  if (sc.air === 'ftr') {
    if (sc.back) chutes = 1;
    else if (sc.over === 'enemy' && (F.saved as readonly string[]).includes(how)) chutes = rng.chance(F.captured[how as keyof typeof F.captured] ?? 0.5) ? 1 : 0;
    if (how === 'glide' && !sc.back && sc.over === 'enemy') out = rng.chance(0.6) ? 1 : 0;
    thrown = ours = chutes;
  } else if (how === 'glide') {
    out = sc.crew - rng.int(Math.max(1, Math.ceil(sc.crew / 3)));
  } else {
    const p = Math.min(0.95, (F.out[how] ?? 0) * F.outBand[sc.band]);
    if (sc.crew > DICE) more = rng.fork(53);
    for (let i = 0; i < DICE; i++) if (rng.chance(p)) { thrown++; if (i < sc.crew) ours++; }
    for (let i = DICE; i < sc.crew; i++) if (more!.chance(p)) extra++;
    chutes = ours + extra;
  }
  const chuteAt: number[] = [];
  if (thrown || extra) {
    const first = how === 'spin' || how === 'wing' || how === 'tail' ? F.chute.spin : F.chute.first;
    let at = range(thrown ? rng : more!, first);
    // Low down there is no time: a canopy that would open after the end opens just before it.
    const open = (): void => { chuteAt.push(Math.round(Math.min(at, Math.max(0.5, upTo - 0.5)) * 10) / 10); };
    for (let i = 0; i < thrown; i++) { if (i < ours) open(); at += range(rng, F.chute.gap); }
    for (let i = 0; i < extra; i++) { open(); at += range(more!, F.chute.gap); }
  }
  // Which way it rolls, and a look number that is not the last one shown for this way down.
  let roll: 1 | -1 = rng.chance(0.5) ? 1 : -1;
  if (roll === mem.roll) roll = rng.chance(0.5) ? 1 : -1;
  mem.roll = roll;
  if (hit === 'wing' || how === 'wing' || how === 'spin') mem.side = side;
  let look = rng.int(F.looks);
  if (mem.looks[how] === look) look = (look + 1 + rng.int(F.looks - 1)) % F.looks;
  mem.looks[how] = look;
  mem.last = [how, ...mem.last].slice(0, F.again.length);
  return {
    how, hit, dur, end, roll, look, chutes,
    ...(hit === 'wing' || how === 'wing' || how === 'spin' ? { side } : {}),
    ...(eng !== undefined ? { engineAt: eng } : {}),
    ...(fire !== undefined ? { fire } : {}),
    ...(boom !== undefined ? { boom } : {}),
    ...(wth !== undefined ? { with: wth, cause } : {}),
    ...(how === 'glide' && sc.sea ? { ditch: true } : {}),
    ...(out ? { out } : {}),
    ...(chuteAt.length ? { chuteAt } : {}),
  };
}

/** Whether a board point is over the sea: off every land ring of the ground round the target (ground.ts). No ground, or an inland board: land. */
export function seaAt(ground: { land: [number, number][][]; inland: boolean } | undefined, x: number, y: number): boolean {
  if (!ground || ground.inland) return false;
  for (const ring of ground.land) {
    let inside = false;
    for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
      const [xi, yi] = ring[i], [xj, yj] = ring[j];
      if (yi > y !== yj > y && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) inside = !inside;
    }
    if (inside) return false;
  }
  return true;
}

/** The old three looks, for anything that reads only 'kind' (the browser page, a 3D build before this). */
export const kindOf = (how: FallHow): Fall['kind'] =>
  how === 'explode' || how === 'breakup' ? 'explode' : how === 'burn' || how === 'flamer' || how === 'glide' ? 'burn' : 'spin';

/** A hit that does not bring the aircraft down (shown only): which, from the battle's stream for looks. */
export function hitOf(rng: Rng, air: AirKind): { mark: 'holes' | 'smoke' | 'pieces' | 'fireout'; engineAt?: number; side: 'port' | 'starboard'; for: number } {
  const H = SKY.hit;
  const rows = H.marks[air];
  const k = pick(rng, rows.map((r) => r[1]));
  const mark = rows[k][0];
  const side = rng.chance(0.5) ? 'port' : 'starboard';
  const len = Math.round(range(rng, H.for[mark]) * 10) / 10;
  // The engine as a place across the aircraft, port 0 to starboard 1 (as fateOf).
  return { mark, ...(mark === 'smoke' || mark === 'fireout' ? { engineAt: Math.round(rng.next() * 1000) / 1000 } : {}), side, for: len };
}

// What moved near the player in one computer nation's month (release 2; Brian, 24 November 2026: "when the AI nations
// are doing their turn, show movements around my territories or if my units are attacked so i know whats going on").
//
// The month's steps (campaign-bridge.ts stepOne) already say what each nation took and fought. This adds, for the 3D
// table's watch of the enemy's month, the moves the player would want to see drawn: every foreign army that came up
// to his ground or left it, every attack on his troops (from where it came, and how it ended), and his own corps that
// fell back. Only what his reports can see is told (the fog of war). Nothing here changes the war: it only reads it.
//
// (Brian, 24 November 2026 18:01Z, "yes to all": his allies' battles near him are shown too, in their own colours and
// in a sentence of their own, which the table shows only with its switch "Allies' battles near you" on.)
import { allied, atWar } from '../../../src/campaign/game';
import type { BattleReport, Game } from '../../../src/campaign/types';

/** One arrow on the table. 'move': a foreign army came up to (or into, or away from) his ground. 'attack': his troops were attacked. 'fell': his own corps fell back. */
export interface NearArrow {
  kind: 'move' | 'attack' | 'fell';
  /** The province it came from; -1 where that is not known (a landing from the sea). */
  from: number;
  to: number;
  nat: string;
  /** At war with the player (red on the table); false for a nation at peace with him (amber: watch it). */
  war: boolean;
  corps: number;
  /** Divisions, where his reports see them whole; null where only glimpsed. */
  divs: number | null;
  /** (attack) The place fell; or the field was won but the town holds (siege); or his troops gave ground without a fight (yielded). */
  taken?: boolean;
  siege?: boolean;
  yielded?: boolean;
  /** (attack) Men lost: theirs and his side's. */
  lost?: [number, number];
  /** (move) The army went away from his ground: drawn fainter. */
  away?: boolean;
  /** (attack) A battle of his allies near him, not his own: drawn in their colours, told in allyNote. */
  ally?: boolean;
  /** (ally) His side attacked (an ally's attack); otherwise an ally was attacked. */
  ours?: boolean;
  /** (ally, attacked) The ally that was attacked. */
  def?: string;
}

export interface NearSight {
  near: boolean;
  arrows: NearArrow[];
  /** A sentence or two of what happened near him, his side's words: the attacks first, then the moves. Empty when nothing did. */
  nearNote: string;
  /** (allies' battles near him) There were some: the watch stops here when its switch is on. */
  nearAllies?: boolean;
  /** What they were, in a sentence or two of their own. */
  allyNote?: string;
}

const NONE: NearSight = { near: false, arrows: [], nearNote: '' };
/** No more arrows than this on the table at once: the attacks first, then the biggest moves. */
export const NEAR_ARROWS = 8;
/** And no more than this of his allies' battles, besides. */
export const NEAR_ALLY_ARROWS = 4;

const num = (n: number): string => n.toLocaleString('en');
/** "A" or "An" before a word ("An Italian attack"). */
const an = (word: string): string => (/^[AEIOU]/i.test(word) ? 'An' : 'A');
const some = (names: string[]): string => (names.length <= 2 ? names.join(' and ') : `${names.slice(0, 2).join(', ')} and ${names.length - 2} more`);

/**
 * What one computer nation's month did near the player.
 *  - stood: where every corps stood before the step (by corps id).
 *  - ctrl0: who controlled each province before the step.
 *  - reps: the battle reports the step wrote.
 *  - seen: the player's sight after the step (intel.ts sightOf: 0 none, 1 glimpsed, 2 seen whole), or a call that gives it:
 *    then it is asked for only where a battle or a march near him needs it (the bridge's step works it out only then).
 *  - foes0: who was at war with him before the step (a country beaten in the step makes peace in it, and its last moves were still a war's).
 */
export function nearMoves(s: Game, nat: string, stood: Map<number, number>, ctrl0: string[], reps: BattleReport[], seen: Uint8Array | (() => Uint8Array), foes0: ReadonlySet<string> = new Set()): NearSight {
  const me = s.human;
  if (!me || !s.nations[me] || nat === me) return NONE;
  try {
    const ours = (n: string): boolean => n === me || allied(s, n, me);
    // (an ally's month: only its battles near him are told, in their own sentence)
    const friend = ours(nat);
    // His ground: what he held before the step and holds now, and wherever his own corps stood or stand.
    const his = new Set<number>();
    s.regions.forEach((r, i) => { if (ctrl0[i] === me || r.ctrl === me) his.add(r.id); });
    for (const c of s.corps) {
      if (c.nat !== me) continue;
      if (!c.dead) his.add(c.reg);
      const was = stood.get(c.id);
      if (was !== undefined) his.add(was);
    }
    if (!his.size) return NONE;
    const nearTo = (id: number): boolean => id >= 0 && !!s.regions[id] && (his.has(id) || s.regions[id].nb.some((j) => his.has(j)));
    const sight = (): Uint8Array => (typeof seen === 'function' ? seen() : seen);
    const sees = (id: number): boolean => id >= 0 && (sight()[id] ?? 0) > 0;
    const whole = (id: number): boolean => id >= 0 && (sight()[id] ?? 0) >= 2;
    // A long road (by rail, or a march of more than one province in the month) is drawn one province long, at the end
    // that touches him: as a staff officer's arrow shows from which side an army came, not every mile of its road. Far:
    // the neighbour of At that lies nearest Far (Far itself where they are neighbours already), held by Side or its
    // allies where one is (an arrow never seems to come out of ground the other side holds).
    const dist2 = (a: number, b: number): number => (s.regions[a].x - s.regions[b].x) ** 2 + (s.regions[a].y - s.regions[b].y) ** 2;
    const toward = (at: number, far: number, side: string): number => {
      if (far < 0 || at < 0 || far === at || !s.regions[at] || !s.regions[far] || s.regions[at].nb.includes(far)) return far;
      const all = s.regions[at].nb.filter((j) => !!s.regions[j]);
      const held = all.filter((j) => s.regions[j].ctrl === side || allied(s, s.regions[j].ctrl, side));
      let best = far, d = Infinity;
      for (const j of held.length ? held : all) if (dist2(j, far) < d) { d = dist2(j, far); best = j; }
      return best;
    };

    const arrows: NearArrow[] = [];
    const arrived = new Set<string>(); // "corps id" of attackers already drawn as an attack, so that their advance is not drawn again as a move
    // An attack, from where the attackers stood, and how it ended; two fights for one place in a month (the field, then
    // the town's assault) are one arrow: the last word on how it ended, the men lost in both.
    const attackOn = (into: NearArrow[], r: BattleReport, extra: Partial<NearArrow>): void => {
      const names = new Set(r.attackers.filter((u) => !u.guard).map((u) => `${u.nat}:${u.name}`));
      const counts = new Map<number, number>();
      let corps = 0;
      for (const c of s.corps) {
        if (!names.has(`${c.nat}:${c.name}`)) continue;
        const was = stood.get(c.id);
        if (was === undefined) continue;
        corps++;
        arrived.add(String(c.id));
        if (was !== r.region) counts.set(was, (counts.get(was) ?? 0) + 1);
      }
      let from = -1, best = 0;
      for (const [reg, n] of counts) if (n > best || (n === best && reg < from)) { from = reg; best = n; }
      from = toward(r.region, from, r.attNat);
      const divs = r.attackers.filter((u) => !u.guard).reduce((t, u) => t + u.divs, 0);
      const fresh: NearArrow = {
        kind: 'attack', from, to: r.region, nat: r.attNat, war: true, corps: Math.max(corps, r.attackers.filter((u) => !u.guard).length), divs,
        taken: !!r.taken && !r.siege, ...(r.siege ? { siege: true } : {}), ...(r.yielded ? { yielded: true } : {}), lost: [r.attLost, r.defLost], ...extra,
      };
      const again = into.find((a) => a.kind === 'attack' && a.to === r.region && a.nat === r.attNat);
      if (!again) { into.push(fresh); return; }
      again.from = again.from >= 0 ? again.from : fresh.from;
      again.corps = Math.max(again.corps, fresh.corps);
      again.divs = Math.max(again.divs ?? 0, fresh.divs ?? 0);
      again.lost = [(again.lost?.[0] ?? 0) + r.attLost, (again.lost?.[1] ?? 0) + r.defLost];
      again.taken = fresh.taken;
      if (fresh.siege) again.siege = true; else delete again.siege;
      if (fresh.yielded) again.yielded = true; else delete again.yielded;
    };
    // The attacks on his troops (or on any troops holding his ground).
    const attacked = friend ? [] : reps.filter((r) => r.defNat === me || r.defenders.some((u) => u.nat === me) || ctrl0[r.region] === me);
    for (const r of attacked) attackOn(arrows, r, {});
    // His allies' battles near his ground, that his reports see: an ally attacked, or an ally attacking.
    const allies: NearArrow[] = [];
    for (const r of reps) {
      if (attacked.includes(r) || !nearTo(r.region) || !sees(r.region)) continue;
      const allyDef = r.defNat !== me && (ours(r.defNat) || r.defenders.some((u) => u.nat !== me && ours(u.nat)));
      const allyAtt = r.attNat !== me && ours(r.attNat);
      if (allyAtt === allyDef) continue; // (neither side his, or a quarrel among his own)
      attackOn(allies, r, allyAtt ? { ally: true, ours: true, war: false } : { ally: true, def: ours(r.defNat) ? r.defNat : r.defenders.find((u) => ours(u.nat))!.nat });
    }
    // Foreign armies that came up to his ground, into it, or away from it: grouped by nation and road, seen ones only.
    const moves = new Map<string, NearArrow>();
    const fell = new Map<string, NearArrow>();
    for (const c of s.corps) {
      if (c.dead) continue;
      const from = stood.get(c.id);
      if (from === undefined || from === c.reg) continue;
      if (c.nat === me) {
        const to = toward(from, c.reg, me);
        const key = `${from}>${to}`;
        const a = fell.get(key) ?? { kind: 'fell' as const, from, to, nat: me, war: false, corps: 0, divs: 0 };
        a.corps++;
        a.divs = (a.divs ?? 0) + c.divs;
        fell.set(key, a);
        continue;
      }
      if (friend || ours(c.nat) || arrived.has(String(c.id))) continue;
      if (!(sees(from) || sees(c.reg)) || !(nearTo(from) || nearTo(c.reg))) continue;
      const away = !nearTo(c.reg);
      const [a0, a1] = away ? [from, toward(from, c.reg, c.nat)] : [toward(c.reg, from, c.nat), c.reg];
      const key = `${c.nat}:${a0}>${a1}`;
      const a = moves.get(key) ?? { kind: 'move' as const, from: a0, to: a1, nat: c.nat, war: foes0.has(c.nat) || atWar(s, c.nat, me), corps: 0, divs: whole(c.reg) ? 0 : null, ...(away ? { away: true } : {}) };
      a.corps++;
      if (a.divs !== null) a.divs += c.divs;
      moves.set(key, a);
    }
    // His own corps that moved in an enemy's month only did so because they were pushed: drawn where they fell back after an attack.
    const pushed = [...fell.values()].filter((a) => attacked.some((r) => r.region === a.from));
    const byWeight = (a: NearArrow, b: NearArrow): number => b.corps - a.corps || a.to - b.to || a.from - b.from;
    // (toward him before away from him, and the armies at war with him before those at peace)
    arrows.push(...pushed.sort(byWeight), ...[...moves.values()].sort((a, b) => Number(!!a.away) - Number(!!b.away) || Number(b.war) - Number(a.war) || byWeight(a, b)));
    const kept = arrows.slice(0, NEAR_ARROWS);
    // (his allies' battles: an ally's losses first, then its holds, then its own attacks)
    const allyWorst = (a: NearArrow): number => (a.ours ? (a.taken ? 4 : 3) : a.taken ? 0 : a.siege ? 1 : 2);
    const told = allies.sort((a, b) => allyWorst(a) - allyWorst(b) || b.corps - a.corps).slice(0, NEAR_ALLY_ARROWS);
    if (!kept.length && !told.length) return NONE;
    // (armies only going away from him are drawn where the month is watched anyway, but the watch does not stop for them)
    const own: NearSight = { near: kept.some((a) => !a.away), arrows: kept, nearNote: kept.length ? noteOf(s, me, kept) : '' };
    // (after a sentence of his own attacks, the allies' get one battle told and a count of the rest)
    return told.length ? { ...own, arrows: [...kept, ...told], nearAllies: true, allyNote: allyNoteOf(s, told, kept.some((a) => a.kind === 'attack') ? 1 : 2) } : own;
  } catch {
    return NONE; // (the war goes on whatever: a watch that cannot be read is not shown)
  }
}

/** The sentence under the heading while the table shows these arrows: the attacks first (the worst first), then the moves. */
function noteOf(s: Game, me: string, arrows: NearArrow[]): string {
  const adj = (n: string): string => s.nations[n]?.adj ?? n;
  const place = (id: number): string => s.regions[id]?.name ?? '';
  const said: string[] = [];
  // (the worst first: a place lost, then a siege, then an attack held)
  const worst = (a: NearArrow): number => (a.taken ? 0 : a.siege ? 1 : a.yielded ? 2 : 3);
  const attacks = arrows.filter((a) => a.kind === 'attack').sort((a, b) => worst(a) - worst(b));
  for (const a of attacks.slice(0, 2)) {
    const where = place(a.to);
    const [theirs, ours] = a.lost ?? [0, 0];
    const cost = theirs && ours ? ` They lost ${num(theirs)} men, you ${num(ours)}.` : theirs ? ` They lost ${num(theirs)} men.` : ours ? ` You lost ${num(ours)} men.` : '';
    if (a.yielded) said.push(`Your troops at ${where} fall back before a ${adj(a.nat)} attack, as ordered.`);
    else if (a.taken) said.push(`The ${adj(a.nat)} army attacks ${where} and takes it.${cost}`);
    else if (a.siege) said.push(`The ${adj(a.nat)} army wins the field at ${where}; the town holds out under siege.${cost}`);
    else said.push(`${an(adj(a.nat))} ${adj(a.nat)} attack on ${where} is thrown back.${cost}`);
  }
  if (attacks.length > 2) said.push(attacks.length === 3 ? 'One more of your positions was attacked.' : `${attacks.length - 2} more of your positions were attacked.`);
  const moves = arrows.filter((a) => a.kind === 'move');
  const nats = [...new Set(moves.map((a) => a.nat))];
  for (const n of nats.slice(0, attacks.length ? 1 : 2)) {
    const mine = moves.filter((a) => a.nat === n);
    // (into his land, else up to it, else away from it: the corps counted are only those the sentence speaks of)
    const into = mine.filter((a) => !a.away && s.regions[a.to]?.ctrl === me);
    const up = mine.filter((a) => !a.away && s.regions[a.to]?.ctrl !== me);
    const away = mine.filter((a) => a.away);
    const told = into.length ? into : up.length ? up : away;
    const corps = told.reduce((t, a) => t + a.corps, 0);
    const where = some([...new Set(told.map((a) => place(told === away ? a.from : a.to)))]);
    const one = corps === 1;
    const army = one ? `${an(adj(n))} ${adj(n)} corps` : `${adj(n)} armies (${corps} corps)`;
    const peace = told[0].war ? '' : ', at peace with you,';
    if (told === into) said.push(`${army}${peace} ${one ? 'marches' : 'march'} into ${where}.`);
    else if (told === up) said.push(`${army}${peace} ${one ? 'moves' : 'move'} up to ${where}, next to your land.`);
    else said.push(`${army}${peace} ${one ? 'pulls' : 'pull'} back from ${where}.`);
  }
  return said.join(' ');
}

/** His allies' battles near him, in a sentence or two of their own: an ally's losses first, then its holds, then its attacks. */
function allyNoteOf(s: Game, arrows: NearArrow[], told = 2): string {
  const adj = (n: string): string => s.nations[n]?.adj ?? n;
  const place = (id: number): string => s.regions[id]?.name ?? '';
  const said: string[] = [];
  for (const a of arrows.slice(0, told)) {
    const where = place(a.to);
    if (a.ours) {
      if (a.taken) said.push(`${adj(a.nat)} troops attack ${where} and take it.`);
      else if (a.siege) said.push(`${adj(a.nat)} troops win the field at ${where}; the town holds out under siege.`);
      else said.push(`${an(adj(a.nat))} ${adj(a.nat)} attack on ${where} is thrown back.`);
    } else {
      const def = adj(a.def ?? '');
      if (a.taken) said.push(`The ${adj(a.nat)} army attacks ${def} troops at ${where} and takes it.`);
      else if (a.siege) said.push(`The ${adj(a.nat)} army wins the field at ${where}; the ${def} garrison holds out under siege.`);
      else if (a.yielded) said.push(`${def} troops at ${where} fall back before ${an(adj(a.nat)).toLowerCase()} ${adj(a.nat)} attack.`);
      else said.push(`${def} troops at ${where} throw back ${an(adj(a.nat)).toLowerCase()} ${adj(a.nat)} attack.`);
    }
  }
  const rest = arrows.length - Math.min(told, arrows.length);
  if (rest > 0) said.push(rest === 1 ? 'One more allied battle near you.' : `${rest} more allied battles near you.`);
  return said.join(' ');
}

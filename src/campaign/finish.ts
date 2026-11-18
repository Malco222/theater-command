/**
 * Finishing the war together (rulings 111 and 115). The grand campaign only, and never in its last act, where every
 * power fights for itself. A war is not won while an enemy great power stands.
 *
 * Built once, for every alliance:
 *
 *  - When an alliance's chief enemy falls (the leader of the other alliance, diplomacy.json 'leaders': Germany for
 *    the Allies), every computer-run great power that was at war with it turns on the great powers its allies are
 *    still fighting and it is at peace with: it gives notice at the end of the month of the fall, and is at war so
 *    many months after the fall (the Soviet Union went to war with Japan three months after Germany's surrender, as
 *    it had promised its allies). The same holds for a great power that has no enemy great power of its own left
 *    standing, whoever it was that fell: its own war is won and its allies' is not. A great power that still has
 *    one of its own to beat, and has not seen the chief enemy fall, goes on as it was. It is an alliance finishing
 *    its own war and not a country straying, so it holds at every stop of the history dial. It never binds the
 *    player's own country; a player it is aimed at is warned on a card of his own.
 *
 *  - The staffs press what is left: on the chief enemy's fall the great powers still standing that they are at war
 *    with already are named an offensive at once, and the war newly declared is pressed from its first month.
 *
 *  - Vichy does not outlive Germany, nor a France that is free again in its own capital (it ended so in August
 *    1944, with Germany standing). At that month's end its ground at home and overseas is France's and its troops
 *    answer to France, unless a player is playing it.
 *
 *  - A freed country takes up the wars it was in when it fell, beside those who freed it, as it returns
 *    (liberation.ts).
 *
 * (The dial 'before' in the data, set false, makes Vichy's end and a freed country's old wars wait for the chief
 * enemy's fall, so that a war nobody plays goes as it did until then. The design director ruled for the letter, b498:
 * read on twenty-four wars nobody plays it ends three a month or two earlier and leaves the middle war where it was.)
 * The numbers are in data/campaign/liberation.json, 'finish'.
 */
import { turnToDate } from '../core/ticket';
import { airOnFall } from './air';
import { DIPLOMACY, majorsOf } from './diplomacy';
import { allied, atWar, bloc, declareWar, headline, theName, warning } from './game';
import { LIB, handBack, recordFall, rejoinWars, warsAtFall } from './liberation';
import { fleetsOnFall } from './navy';
import { dropSieges } from './siege';
import { launch } from './staff';
import type { Game } from './types';

/** True where an alliance finishes its war together: the grand campaign, until its last act dissolves the alliances. */
export const finishRules = (g: Game): boolean => g.flags.open === 1 && !g.flags.dom && !!LIB.finish;

const noticeKey = (who: string, on: string): string => `fin:${who}:${on}`;
const sideOf = (g: Game, id: string): 'allies' | 'axis' | null => { const b = g.nations[id] ? bloc(g.nations[id]) : null; return b === 'allies' || b === 'axis' ? b : null; };

/**
 * True while one great power owes its allies a war with another: both stand, it stands in an alliance, the two are at
 * peace and not of one side, and a member of its alliance is still at war with the other.
 */
export function owesWar(g: Game, who: string, on: string): boolean {
  const G = g.nations[who], E = g.nations[on];
  if (!G?.alive || !E?.alive || G.dormant || E.dormant || who === on || !sideOf(g, who) || allied(g, who, on) || atWar(g, who, on)) return false;
  return Object.values(g.nations).some((A) => A.alive && A.id !== who && allied(g, A.id, who) && A.atWar.includes(on));
}

/** A notice given and not yet acted on: who will declare war on whom, the month it was given, and the month the war comes. */
export interface WarNotice { who: string; on: string; given: number; due: number }

/** Every notice of war that stands, soonest first. */
export function warNotices(g: Game): WarNotice[] {
  const out: WarNotice[] = [];
  for (const k of Object.keys(g.flags)) {
    if (!k.startsWith('fin:')) continue;
    const [, who, on] = k.split(':');
    if (g.nations[who] && g.nations[on]) out.push({ who, on, given: g.flags[k], due: g.flags[k] + LIB.finish.notice });
  }
  return out.sort((a, b) => a.due - b.due || (a.who + a.on < b.who + b.on ? -1 : 1));
}

/**
 * The month's end, after the fallen have returned: Vichy's end, and the notices and new offensives that follow a great
 * power's fall this month.
 */
export function finishMonthEnd(g: Game): void {
  if (!finishRules(g)) return;
  const week = g.week;
  g.week = 4;
  endVichy(g);
  const majors = majorsOf(g);
  for (const x of majors) {
    const X = g.nations[x];
    if (!X || X.alive || X.fellOn?.turn !== g.turn) continue;
    // A country freed before its liberators' chief enemy fell takes up now any old war it has not taken up already
    // (ruling 115(c): all of them, where the data made them wait for this month).
    for (const N of Object.values(g.nations)) {
      if (!N.alive || !N.freedOn || N.id === g.human || !sideOf(g, N.id) || DIPLOMACY.leaders[sideOf(g, N.id) === 'allies' ? 'axis' : 'allies'] !== x) continue;
      const on = rejoinWars(g, N.id, N.freedOn.by);
      if (!on.length) continue;
      const names = on.map((e) => theName(g.nations[e]));
      const list = names.length > 1 ? `${names.slice(0, -1).join(', ')} and ${names[names.length - 1]}` : names[0];
      const said = (whom: string): string => `With ${theName(X)} beaten, ${theName(N)} takes up its old war with ${whom} again, beside those who freed it.`;
      if (g.human && on.includes(g.human)) warning(g, 'An old enemy returns', said('you'), g.human, undefined, N.id);
      else headline(g, 'event', said(list), N.id);
    }
    for (const who of warsAtFall(g, x)) {
      const G = g.nations[who], side = sideOf(g, who);
      if (!majors.includes(who) || who === g.human || !G?.alive || G.dormant || !side) continue;
      // The great powers still standing that it is at war with already. Unless it was its alliance's chief enemy
      // that fell, a great power with one of them still to beat goes on with the war it has.
      const left = majors.filter((e) => g.nations[e]?.alive && atWar(g, who, e));
      const chief = DIPLOMACY.leaders[side === 'allies' ? 'axis' : 'allies'] === x;
      if (!chief && left.length) continue;
      // Its staff makes them its business now.
      if (left.length) launch(g, who, left, LIB.finish.push);
      // And those its allies are fighting without it: notice now, war when the months are up.
      for (const on of majors) {
        if (!owesWar(g, who, on) || g.flags[noticeKey(who, on)]) continue;
        g.flags[noticeKey(who, on)] = g.turn;
        const when = turnToDate(g.turn + LIB.finish.notice).label;
        const said = `With ${theName(X)} beaten, ${theName(G)} stands by its allies`;
        if (on === g.human) warning(g, 'War is coming', `${said}, who are still at war with you: it gives notice that it will declare war on you in ${when}, unless you are at peace with them by then.`, on, undefined, who);
        else headline(g, 'event', `${said}: it gives ${theName(g.nations[on])} notice that it will be at war with it in ${when}.`, who);
      }
    }
  }
  g.week = week;
}

/** The month's opening: the wars that were given notice of and have come due are declared, where they are still owed. */
export function finishMonthOpen(g: Game): void {
  if (!finishRules(g)) return;
  for (const n of warNotices(g)) {
    const key = noticeKey(n.who, n.on);
    // (A notice lapses when the war it stood for is over, when the two have come to one side, or when a player has taken the country in hand.)
    if (!owesWar(g, n.who, n.on) || n.who === g.human) { delete g.flags[key]; continue; }
    if (g.turn < n.due) continue;
    delete g.flags[key];
    declareWar(g, n.who, n.on);
    if (atWar(g, n.who, n.on)) launch(g, n.who, [n.on], LIB.finish.push);
  }
}

/**
 * Vichy's end (ruling 115(b)): a government and not a country. Once France is free again and holds its own capital,
 * what Vichy holds at home and overseas is France's, its troops answer to France, its wars are over, and it is
 * struck off as the fallen are. Never while a player is playing it.
 */
function endVichy(g: Game): void {
  const V = g.nations.VIC, F = g.nations.FRA;
  if (!V?.alive || V.dormant || g.human === 'VIC' || !F?.alive || !F.freedOn || F.capital < 0 || g.regions[F.capital].ctrl !== 'FRA') return;
  // (Where the data makes it wait for the chief enemy's fall: not while Germany stands.)
  if (!LIB.finish.before && g.nations[DIPLOMACY.leaders.axis]?.alive) return;
  V.alive = false;
  V.fellOn = { turn: g.turn, week: g.week, to: 'FRA', how: 'annexed' };
  recordFall(g, 'VIC', 'FRA', 'annexed');
  for (const c of g.corps) if (c.nat === 'VIC' && !c.dead) { c.nat = 'FRA'; c.hold = false; }
  fleetsOnFall(g, 'VIC');
  airOnFall(g, 'VIC');
  dropSieges(g, 'VIC');
  for (const r of g.regions) {
    if (r.owner === 'VIC') { r.owner = 'FRA'; r.cap = false; }
    if (r.ctrl === 'VIC') r.ctrl = 'FRA';
  }
  for (const n of Object.values(g.nations)) n.atWar = n.atWar.filter((e) => e !== 'VIC');
  V.atWar = [];
  handBack(g, F);
  g.supplyDirty = true;
  const G = g.nations.GER;
  headline(g, 'event', `The government at Vichy is no more. ${G && !G.alive ? `With ${theName(G)} beaten, its` : 'France is free again in Paris, and its'} ground at home and overseas is France's again, and its troops answer to France.`, 'FRA', F.capital, { arm: 'land', won: 'FRA' });
}

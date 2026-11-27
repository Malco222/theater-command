// The air battle page: choose a raid the campaign is about to fly, fight it over its target, and see what the campaign
// makes of it. The air's first slice of the played battles (ruling 217): a browser page of its own, beside the land and
// sea pages, with no press on the campaign screen until the play version ships and the Playtester finds it easy.
//
// Mouse: left picks (drag for a box, Shift adds), right orders your fighters (on an enemy group to attack it, on the
// sky to patrol there). Keys: Space pause, 1 2 3 speed, H hold, R break off, Tab next group, Esc drops the choice or
// answers a question no. A plain left click never gives an order, and Enter does nothing. Until the formations first
// meet, and once the fighting is done and the last head for home, the clock runs fast on its own (ruling 226, mark 1);
// a question on the field stops it (b358). The words are the UI director's (3d-handoff.md section 4, ruling 226).

import { afterCard, peopleOf, type AfterCard } from '../../campaign/airfought';
import { launchRaid, planRaid, raidTargets, type RaidKind } from '../../campaign/airwar';
import { createGame, setWar } from '../../campaign/game';
import { startOpen } from '../../campaign/staff';
import type { Game } from '../../campaign/types';
import { Clock, type Speed } from '../core/clock';
import { unitAt, unitsIn } from '../core/select';
import type { Command, FieldEvent, FieldTicket, Side } from '../core/types';
import { SKY } from '../air/book';
import { AirSim } from '../air/sim';
import { airTicket, escortPair, foughtAir, raidGroups, type EscortPair } from '../air/ticket';
import type { AirPlace, AirUnit } from '../air/types';
import { BAND_WORD, SOUTH, drawAir, live, planeSvg, shown, takeEvents, type AirScene } from './air-draw';
import { toField, toScreen } from './draw';

const KEY_GUARD = 0.5;
const esc = (s: string): string => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]!);
const mmss = (t: number): string => `${Math.floor(t / 60)}:${String(Math.floor(t % 60)).padStart(2, '0')}`;
/** A row of facts with the kit's dots between them, wrapping between facts and never inside one. */
const facts = (bits: string[]): string => `<span class="facts"><span>${bits.map((b) => `<span class="fact">${b}</span>`).join('')}</span></span>`;
/**
 * Places English names with "the" (the UI director, 21:25Z: "the Ruhr"), wherever the name runs in a sentence: "The
 * air battle over the Ruhr", "3% of the Ruhr's output". The map's label stays RUHR. A mend in the page until the
 * campaign carries its own flag for such places.
 */
const THE = ['Ruhr', 'Rhineland', 'Saar', 'Palatinate', 'Crimea', 'Caucasus', 'Ukraine', 'Balkans', 'Netherlands'];
const THE_RX = new RegExp(`(^|[^\\w])(?<![Tt]he )(${THE.join('|')})\\b`, 'g');
const withThe = (text: string): string => text.replace(THE_RX, (_m, pre: string, name: string) => `${pre}${pre === '' ? 'The' : 'the'} ${name}`);
/** A number never parts from its noun (the UI director, air review 11). */
const NB = '\u00a0';
/** How fast the clock runs on its own while nothing can be decided: before the formations meet, and after the fighting. */
const FAST = 10;

/** A raid the campaign is about to fly: the game it comes from, and what the raid card picked. */
interface Raid { g: Game; nat: string; kind: RaidKind; target: number; ids: number[]; ticket: FieldTicket<AirPlace> }

interface State {
  screen: 'choose' | 'battle' | 'after';
  sim: AirSim | null;
  raid: Raid | null;
  clock: Clock;
  scene: AirScene;
  guardUntil: number;
  dispatch: FieldEvent | null;
  /** The question standing on the field, if any: HAND IT TO THE STAFF's or BREAK OFF ALL's. The clock stops while it stands. */
  asking: 'staff' | 'withdraw' | null;
  /** A passing tag at the top of the field ("RUNNING ×2"), until a real time; and how the clock ran itself at the last frame. */
  tag: { text: string; until: number } | null;
  rushed: '' | 'meet' | 'home';
  last: number;
  trayAt: number;
  /** After the campaign books the raid: each of the player's groups' aircraft as booked, which the tray then shows. */
  booked: Map<number, number> | null;
  /** The balance bar's pale mark: his side's chance on the raid card, where it stood when the formations set out. */
  mark: number;
  /** The staff's chance now, for the balance bar's top line, and when it was last reckoned (real seconds). */
  now: { p: number; at: number } | null;
}

const app = document.getElementById('app')!;
const S: State = {
  screen: 'choose', sim: null, raid: null, clock: new Clock(),
  scene: { cam: { x: SKY.w / 2, y: SKY.h / 2, z: 0.008 }, W: 800, H: 600, me: 'att', sel: new Set(), hover: null, box: null, line: null, now: 0, ping: null, marks: [], at: null, fan: new Map() },
  guardUntil: 0, dispatch: null, asking: null, tag: null, rushed: '', last: 0, trayAt: 0, booked: null, mark: 0.5, now: null,
};
const now = (): number => performance.now() / 1000;
const guard = (): void => { S.guardUntil = now() + KEY_GUARD; };

// ---- The raids on offer ----------------------------------------------------------------------------------------------

/**
 * The raids the first slice can fight, from the grand campaign's opening in September 1939, as its raid card picks them.
 * A night raid is not among them: the staff settles it (the design director, b384(c)).
 */
const OFFERS: { nat: string; wars: string[]; kind: RaidKind; name: string; pair?: true }[] = [
  { nat: 'GER', wars: ['UK', 'FRA'], kind: 'bomb', name: 'Alsace' },
  { nat: 'GER', wars: ['UK', 'FRA'], kind: 'superiority', name: 'Alsace' },
  // The third raid: France's, on the nearest industrial target its escort can reach (the design director, b401 as ruled
  // 07:38Z on 22 November): its fighters reach 220 km in September 1939, and no French field is that near the Ruhr
  // (Lorraine, the nearest, is 268 km off), so it is the Saar, 65 km from Lorraine. Its choice is the escort itself (b401
  // as re-ruled 08:48Z and 08:50Z): the same raid offered alone and escorted, side by side, both cards from one
  // reckoning (escortPair), and the escort's worth as the difference between them. Both cards are foregone, on purpose:
  // day bombers without fighters overhead were cut to pieces in 1939 and 1940, and an escort changed that. The battles
  // before it carry the uncertain outcome (the raid on Alsace, about 4 in 10).
  { nat: 'FRA', wars: ['GER'], kind: 'bomb', name: 'Saar', pair: true },
];

/**
 * Losses carry (the design director, b384(i)): each air group's strength after the last battle fought here, by its
 * campaign id, goes into every raid offered after it; and what changed, for the chooser's SINCE YOUR LAST BATTLE.
 */
const carried = new Map<number, { ftr: number; bmr: number }>();
/**
 * One line per group that lost aircraft: whose, what it had and lost, what is left, and, for an enemy group, its
 * campaign id and its fighters left.
 */
let since: { who: string; had: string; left: string; foe: number | null; n: number }[] = [];

/** Battles fought on this page so far: each one is the campaign's month played again, with its own luck. */
let battles = 0;

/** The i-th offer's September: the game it is flown from, with this battle's luck and the losses carried, and its target. */
function openingOf(i: number): { g: Game; target: number } | null {
  const o = OFFERS[i];
  const g = startOpen(createGame(7, o.nat));
  for (const w of o.wars) setWar(g, o.nat, w);
  // Each battle has luck of its own (the design director, b397): the same September every time, but not the same throws.
  g.seed = (7 + 7919 * battles) >>> 0;
  g.flags.airBattles = battles;
  for (const f of g.airFleets) { const c = carried.get(f.id); if (c) { f.ftr = c.ftr; f.bmr = c.bmr; } }
  const t = raidTargets(g, o.kind, o.nat).find((x) => planRaid(g, o.kind, x.id, undefined, o.nat).place === o.name);
  return t ? { g, target: t.id } : null;
}

function raidOf(i: number): Raid | null {
  const o = OFFERS[i], at = openingOf(i);
  if (!at) return null;
  const { g, target } = at;
  // The staff's pick and every fighter group that can go with it by day, from the nation's own airfields (b397).
  const ids = raidGroups(g, o.kind, target, o.nat);
  const { ticket } = airTicket(g, o.kind, target, ids, o.nat);
  return ticket ? { g, nat: o.nat, kind: o.kind, target, ids, ticket } : null;
}

/** The same raid alone and escorted (b401 as re-ruled): one game, both cards from one reckoning, each a raid to fly. */
interface Pair { alone: Raid; escorted: Raid; worth: EscortPair['worth'] }

function pairOf(i: number): Pair | null {
  const o = OFFERS[i], at = openingOf(i);
  if (!at) return null;
  const { g, target } = at;
  const p = escortPair(g, o.kind, target, o.nat);
  if (!p) return null;
  const raid = (x: EscortPair['alone']): Raid => ({ g, nat: o.nat, kind: o.kind, target, ids: x.ids, ticket: x.ticket });
  sameAim.set(p.alone.ticket, p.escorted.ticket);
  return { alone: raid(p.alone), escorted: raid(p.escorted), worth: p.worth };
}

/** The bombers a raid sends up. */
const bombersOf = (t: FieldTicket<AirPlace>): number => t.att.forces.filter((f) => f.kind === 'bmr').reduce((s, f) => s + f.men, 0);

/** "300 bombers and 96 fighters": what a side sends up. */
function forceWords(t: FieldTicket<AirPlace>, side: Side): string {
  const n = (k: string): number => t[side].forces.filter((f) => (k === 'bmr' ? f.kind === 'bmr' : f.kind !== 'bmr')).reduce((s, f) => s + f.men, 0);
  const f = n('ftr'), b = n('bmr');
  return [b ? `${b}${NB}bombers` : '', f ? `${f}${NB}fighters` : ''].filter(Boolean).join(' and ') || 'nothing';
}

/** What a side flies, in the raid card's words: "the German raid", "the British fighters". */
function sideWords(t: FieldTicket<AirPlace>, side: Side): string {
  return `the ${t[side].adj} ${side === 'att' && t.place.mission !== 'superiority' ? 'raid' : 'fighters'}`;
}

/** The staff's chance on a raid card, for the side the player flies (the UI director, newcomer review 4). */
function chanceWords(p: number): string {
  if (p >= 0.995) return 'the staff is sure of it';
  if (p <= 0.005) return 'the staff gives it no chance';
  return `the staff gives it about ${Math.round(p * 100)}%`;
}

/**
 * What the raid is for and what makes it count, named on its card before the fight (the design director, b394): a
 * raid is judged by what it was sent to do, so a raid that bombs well and misses what mattered is no surprise.
 */
function aimWords(t: FieldTicket<AirPlace>): string {
  const P = t.place, them = peopleOf(t.def.adj), aim = aimOf(t);
  if (aim === 'sky') return `The aim: the sky over ${P.name}. The fight counts if your fighters end it stronger than what the ${them} can put up.`;
  if (aim === 'factories') return `The aim: the factories of ${P.name}. The raid counts if its bombs do more harm than it costs you in aircraft.`;
  if (aim === 'rail') return `The aim: the rail yards of ${P.name}. The raid counts if the line is cut, or if the ${them} lose more aircraft than you.`;
  return `The aim: ${P.name}, which has nothing the bombs can harm. The raid counts only if the ${them} lose more aircraft than you.`;
}

/**
 * A pair's alone card names the aim its escorted card does (b401 as re-ruled: the same raid). What a raid is sent to do
 * does not change with its escort, though the harm its card expects from the bombs that get through does: read from the
 * alone card's own reckoning, the Saar's factories would turn into its rail yards.
 */
const sameAim = new WeakMap<FieldTicket<AirPlace>, FieldTicket<AirPlace>>();

/** What a raid is sent to do: the sky, the factories, the rail yards, or nothing the bombs can harm. */
function aimOf(t: FieldTicket<AirPlace>): 'sky' | 'factories' | 'rail' | 'none' {
  const P = (sameAim.get(t) ?? t).place;
  if (P.mission === 'superiority') return 'sky';
  if (P.reckon.value >= 1 || P.reckon.ruin > 0) return 'factories';
  return P.reckon.rail > 0 ? 'rail' : 'none';
}

/**
 * The aim slip at the field's top left, under the tag (the UI director, 20:53Z; the design director, b397: the field
 * shows the score the stamp will use): what the raid is for, and how it stands on the very counts the stamp judges.
 */
function aimSlip(sim: AirSim): [string, string, string] {
  const t = sim.ticket, P = t.place, me = S.scene.me, foe: Side = me === 'att' ? 'def' : 'att', sc = sim.score(), them = t[foe].adj;
  const lost = `Aircraft lost: yours ${sc.lost[me]}, ${them} ${sc.lost[foe]}`;
  switch (aimOf(t)) {
    case 'sky': return [`The sky over ${P.name}`, `Fighters in the air: yours ${sc.up[me]}, ${them} ${sc.up[foe]}`, ''];
    case 'factories': return [`The factories of ${P.name}`, sim.droppedNow() <= 0 ? 'No bombs have fallen yet'
      : sc.ratio >= 1 ? 'So far its bombs have done more than the raid has cost' : 'So far the raid has cost more than its bombs have done', ''];
    case 'rail': return [`The rail yards of ${P.name}`, sc.cut ? 'The line is cut' : 'The line still runs', lost];
    default: return [P.name, lost, ''];
  }
}

/**
 * The title, for the top bar: where it must wrap, it wraps before "over", never inside either half (the UI director,
 * 21:18Z): "The fight for the sky / over Alsace".
 */
function titleWords(title: string): string {
  const at = title.lastIndexOf(' over ');
  if (at < 0) return esc(title);
  return `<span class="nw">${esc(title.slice(0, at))}</span> <span class="nw">${esc(title.slice(at + 1))}</span>`;
}

/** What kind of battle it is, for the top bar's subtitle: "an escorted day raid", "a day raid without escort", "a fight for the sky". */
function kindWords(t: FieldTicket<AirPlace>): string {
  if (t.place.mission === 'superiority') return 'a fight for the sky';
  if (t.place.night) return 'a night raid';
  return t.att.forces.some((f) => f.kind !== 'bmr') ? 'an escorted day raid' : 'a day raid without escort';
}

function showChooser(): void {
  S.screen = 'choose'; S.sim = null; S.raid = null;
  guard();
  const offered = OFFERS.map((o, i): Raid | Pair | null => (o.pair ? pairOf(i) : raidOf(i)));
  const isPair = (x: Raid | Pair): x is Pair => 'worth' in x;
  // Every raid that can be flown from here, in the order shown: a pair's alone card, then its escorted one.
  const raids: Raid[] = offered.flatMap((x) => (!x ? [] : isPair(x) ? [x.alone, x.escorted] : [x]));
  // Each card: the title and the staff's chance; which side he flies and what each side sends up; when and what kind
  // (the UI director, newcomer review 4).
  const offer = (r: Raid, i: number): string => {
    const t = r.ticket, me: Side = t.att.human ? 'att' : 'def', foe: Side = me === 'att' ? 'def' : 'att';
    const p = me === 'att' ? t.place.chance : 1 - t.place.chance;
    const kind = t.place.mission === 'superiority' ? 'a fight for the sky' : t.att.forces.some((f) => f.kind !== 'bmr') ? 'an escorted raid' : 'a raid without escort';
    return `<button class="tc-card is-press offer" data-raid="${i}">
        <span class="otop"><b class="tc-title-3">${esc(withThe(t.title))}</b><span class="tc-data ochance">${esc(chanceWords(p))}</span></span>
        <span class="tc-data omid">${facts([`You fly ${esc(sideWords(t, me))}`, `${esc(forceWords(t, me))} against ${esc(forceWords(t, foe))}`])}</span>
        <span class="oaim">${esc(withThe(aimWords(t)))}</span>
        <span class="tc-data olow">${facts([t.place.night ? 'By night' : 'By day', kind])}</span>${lost(t, me)}</button>`;
  };
  // On a pair's cards, the bombers each card expects lost: the very number the escort's worth is read from.
  const lost = (t: FieldTicket<AirPlace>, me: Side): string => {
    if (!pairs.has(t) || me !== 'att') return '';
    const n = bombersOf(t), x = Math.round(t.place.forecast.bmr);
    return `<span class="tc-data olost">The staff expects about ${x}${NB}of the ${n}${NB}bombers lost</span>`;
  };
  const pairs = new Set(offered.flatMap((x) => (x && isPair(x) ? [x.alone.ticket, x.escorted.ticket] : [])));
  /**
   * The same raid alone and escorted, side by side, and the escort's worth under them: the escorted card less the alone
   * one, in the numbers the two cards show, so that the sum can be checked by eye.
   */
  const pairHtml = (p: Pair): string => {
    const a = p.alone.ticket, e = p.escorted.ticket, ia = raids.indexOf(p.alone), ie = raids.indexOf(p.escorted);
    const pa = Math.round(a.place.chance * 100), pe = Math.round(e.place.chance * 100);
    const la = Math.round(a.place.forecast.bmr), le = Math.round(e.place.forecast.bmr);
    const chance = pe === pa ? 'leaves the staff\'s chance where it was' : `takes the staff's chance from about ${pa}% to about ${pe}%, ${Math.abs(pe - pa)}${NB}points ${pe > pa ? 'up' : 'down'}`;
    const saved = la === le ? 'saves no bombers' : la > le ? `saves about ${la - le}${NB}of the ${la}${NB}bombers the raid alone would lose` : `costs about ${le - la}${NB}bombers more`;
    return `<section class="pair">
        <div class="tc-label phead">${esc(withThe(`${a.place.name}: the same raid, alone or escorted`))}</div>
        <div class="offers ptwo">${offer(p.alone, ia)}${offer(p.escorted, ie)}</div>
        <p class="pworth"><b>What the escort is worth</b>, the one card against the other: it ${chance}, and ${saved}.</p>
      </section>`;
  };
  // An enemy group's line says how many of what it has left rise against the raids offered now (the UI director, b384
  // review 6, passed 20:38Z): "French Groupe de Chasse 3: 223 fighters left; 156 of them rise to meet you."
  const rise = (id: number): number[] => raids.flatMap((r) => {
    const t = r.ticket, foe: Side = t.att.human ? 'def' : 'att';
    const n = t[foe].forces.filter((f) => f.corps === id).reduce((a, f) => a + f.men, 0);
    return n > 0 ? [n] : [];
  });
  // A count that adds up (the UI director, 20:53Z): "French Groupe de Chasse 3: 250 fighters, 27 lost; 223 left, and 156
  // of them rise to meet you." His own: "Jagdgeschwader 4: 200 fighters, 17 lost; 183 left."
  const sinceLine = (x: (typeof since)[number]): string => {
    const ns = x.foe === null ? [] : rise(x.foe), most = ns.length ? Math.max(...ns) : 0;
    const tail = !most ? '' : most >= x.n ? ', and all of them rise to meet you' : `, and ${ns.every((n) => n === most) ? '' : 'up to '}${most} of them rise to meet you`;
    return `<b>${esc(x.who)}</b>: ${x.had}; ${x.left} left${tail}.`;
  };
  app.innerHTML = `<div class="choose tc air">
    <header class="chead"><div><div class="tc-label">Air battles · first slice</div><h1 class="tc-title-1">Choose a raid to fight</h1></div></header>
    <div class="cbody">
      <section class="tc-paper intro"><p><b>The campaign settles every raid from its raid card until you choose to fly one here.</b> Each air group is a formation that fights as one; the raid comes on as one stream and the defenders rise to meet it. Height is yours to choose: a group above its enemy dives on it and strikes first. What happens here goes back to the campaign as the aircraft each side lost and the bombers that bombed, and you can hand the rest to the staff at any moment.</p></section>
      <h2 class="tc-label shead">From the campaign · September 1939</h2>
      ${since.length ? `<section class="tc-paper since"><div class="tc-label">Since your last battle</div><ul>${since.map((l) => `<li>${sinceLine(l)}</li>`).join('')}</ul></section>` : ''}
      <div class="offers">${offered.map((x) => (!x || isPair(x) ? '' : offer(x, raids.indexOf(x)))).join('')}</div>
      ${offered.map((x) => (x && isPair(x) ? pairHtml(x) : '')).join('')}
    </div></div>`;
  app.querySelectorAll<HTMLElement>('[data-raid]').forEach((el) => el.addEventListener('click', () => { const r = raids[Number(el.dataset.raid)]; if (r) startBattle(r); }));
}

// ---- The battle ------------------------------------------------------------------------------------------------------

let canvas: HTMLCanvasElement;
let ctx: CanvasRenderingContext2D;

const STATE_WORD: Record<string, string> = { steady: 'Steady', wavering: 'Wavering', routing: 'Breaking off', gone: 'Lost', withdrawn: 'Out of the fight' };
const ACT_WORD: Record<string, string> = { idle: 'on the ground', march: 'flying', fight: 'in the fight', rout: 'breaking for home', down: '' };

function startBattle(r: Raid): void {
  const t = r.ticket;
  // Shown, never counted (ruling 237): the stragglers and escorts the UI director worded reach the dispatches.
  // The page draws every model on a flat map: it keeps its own budget (SKY.look.page), apart from the 3D game's.
  const sim = new AirSim(t, { show: true, look: SKY.look.page });
  S.sim = sim; S.raid = r; S.screen = 'battle';
  const sc = S.scene;
  sc.me = t.att.human ? 'att' : 'def';
  sc.sel = new Set(); sc.hover = null; sc.box = null; sc.line = null; sc.ping = null; sc.marks = []; sc.at = null;
  S.clock = new Clock(); S.clock.set(1); S.clock.toggle();
  S.asking = null; S.tag = null; S.rushed = 'meet';
  S.booked = null;
  sc.fan = new Map();
  // One sentence (the UI director, air review 6).
  S.dispatch = { t: 0, what: 'contact', text: `${t.place.mission === 'superiority' ? 'Your fighters are formed up' : 'Your raid is formed up'} and heading for ${t.place.name}: give your orders, then press Space to begin.` };
  guard();
  const mine = t[sc.me], theirs = t[sc.me === 'att' ? 'def' : 'att'];
  app.innerHTML = `<div class="battle tc air">
    <header class="bbar">
      <div class="btitle"><b class="tc-title-3">${titleWords(withThe(t.title))}</b><span class="tc-data">${facts([esc(t.date), esc(kindWords(t))])}</span></div>
      <div class="bclock"><span class="tc-label" id="cstate">Paused</span><span class="tc-data" id="time">0:00</span></div>
      <div class="bspeed" role="group" aria-label="Clock">
        <button class="tc-btn tc-btn--sm" data-clock="0" title="Pause (Space)"><span id="pword">Paused</span> <span class="tc-key">Space</span></button>
        <button class="tc-btn tc-btn--sm" data-clock="1" title="Normal speed (1)">×1 <span class="tc-key">1</span></button>
        <button class="tc-btn tc-btn--sm" data-clock="2" title="Twice as fast (2)">×2 <span class="tc-key">2</span></button>
        <button class="tc-btn tc-btn--sm" data-clock="4" title="Four times as fast (3)">×4 <span class="tc-key">3</span></button>
      </div>
      <div class="bbal" title="The staff's chance, as the fight stands now. The pale mark is where it stood when the formations set out.">
        <div class="bends"><span>${esc(mine.adj)}</span><span id="ratio" class="tc-data"></span><span>${esc(theirs.adj)}</span></div>
        <div class="btrack"><i id="balMe" style="background:${mine.color}"></i><i id="balThem" style="background:${theirs.color}"></i><b id="balMark"></b></div>
        <div class="bbreak" title="A bar fills as a side runs short of ammunition or nerve; when it is full, that side breaks off and heads for home. Who breaks off first does not decide the raid: its aim does."><span class="bbar2"><i id="nearMe" style="background:${mine.color}"></i></span><span class="tc-label">How near to breaking off</span><span class="bbar2 is-theirs"><i id="nearThem" style="background:${theirs.color}"></i></span></div>
      </div>
      <div class="bacts">
        <button class="tc-btn" data-act="dice" title="The staff settles the rest at once, from where the battle stands.">Hand it to the staff</button>
        <button class="tc-btn tc-btn--grave" data-act="withdraw">Break off all</button>
        <div class="wask tc-paper" id="ask" hidden role="alertdialog"><p id="askq"></p><div class="wbtns"><button class="tc-btn" data-act="ago" id="ago"></button><button class="tc-btn tc-btn--primary" data-act="astay">Keep fighting</button></div></div>
      </div>
    </header>
    <div class="bfield"><canvas id="field"></canvas>
      <div class="dispatch tc-paper" id="dispatch"></div>
      <div class="ftop"><div class="fwarn" id="fwarn" hidden></div></div>
      <div class="ftag" id="ftag" hidden></div>
      <div class="faim tc-paper" id="faim"><div class="tc-label">The aim</div><b id="aimWhat"></b><span id="aimNow"></span><span id="aimLost"></span></div>
      <div class="zoom"><button class="tc-btn tc-btn--sm" data-zoom="1" aria-label="Closer">+</button><button class="tc-btn tc-btn--sm" data-zoom="-1" aria-label="Further">−</button><button class="tc-btn tc-btn--sm" data-zoom="0" title="The whole sky">Whole sky</button></div>
      <div class="airorders tc-paper" id="airorders" hidden>
        <span class="tc-label">Height</span><span class="tc-seg" id="band">${BAND_WORD.map((w, i) => `<button data-band="${i}">${w}</button>`).join('')}</span>
        <span class="tc-seg" id="escort"><button data-escort="1">Stay close</button><button data-escort="0">Range ahead</button></span>
        <button class="tc-btn tc-btn--sm" data-act="hunt" id="hunt">Hunt</button>
        <button class="tc-btn tc-btn--sm tc-btn--grave" id="jett" data-act="jettison">Jettison the bombs</button>
      </div>
    </div>
    <div class="keys tc-data">${keysLine()}</div>
    <footer class="tray" id="tray">${sim.units.filter((u) => u.side === sc.me).map((u) => card(u, mine.color)).join('')}</footer>
  </div>`;
  const el = (id: string): HTMLElement => document.getElementById(id)!;
  S.mark = sc.me === 'att' ? t.place.chance : 1 - t.place.chance;
  S.now = null;
  canvas = el('field') as HTMLCanvasElement;
  ctx = canvas.getContext('2d')!;
  fit(true);
  wireBattle();
  updateTop(true);
  updateTray(true);
  S.last = now();
}

/**
 * The controls line (the UI director's ruling 226 words, item 9): one form of "Right-click", and the last part about
 * looking round. Under 1440 it folds once, before "Arrows", and the dot at the fold goes (the fold rule).
 */
function keysLine(): string {
  const k = (c: string): string => `<span class="tc-key">${c}</span>`;
  const bits = ['Click or drag a box: pick up', 'Right-click an enemy group: attack it', 'Right-click the sky: patrol there', `${k('H')} hold`, `${k('R')} break off`, `${k('Tab')} next group`, `${k('Esc')} put down`];
  return `<span class="facts"><span>${bits.map((b) => `<span class="fact">${b}</span>`).join('')}<span class="kfold"></span><span class="fact">Arrows or two-finger drag: look round</span></span></span>`;
}

function card(u: AirUnit, color: string): string {
  return `<button class="tc-card is-press ucard" data-unit="${u.id}" title="${esc(u.name)}">
    <span class="uhead">${planeSvg(u.air === 'bmr', color)}<span class="uwhat"><b>${esc(u.name)}</b><small>${u.air === 'bmr' ? 'Bombers' : 'Fighters'}</small></span></span>
    <span class="tc-meter" data-m="str"><span class="tc-label">Aircraft</span><i><b></b></i><span></span></span>
    <span class="tc-meter is-rest" data-m="mor"><span class="tc-label">Morale</span><i><b></b></i><span></span></span>
    <span class="ustate tc-data"></span></button>`;
}

const orderable = (u: AirUnit): boolean => live(u) && u.state !== 'routing';
const unit = (id: number): AirUnit | undefined => S.sim!.units.find((u) => u.id === id);

function updateTray(force = false): void {
  const t = now();
  if (!force && t - S.trayAt < 0.15) return;
  S.trayAt = t;
  for (const el of document.querySelectorAll<HTMLElement>('#tray [data-unit]')) {
    const u = unit(Number(el.dataset.unit));
    if (!u) continue;
    el.classList.toggle('is-on', S.scene.sel.has(u.id));
    el.classList.toggle('is-off', !live(u));
    const set = (m: string, v: number, text: string, cls: string): void => {
      const box = el.querySelector<HTMLElement>(`[data-m="${m}"]`)!;
      box.className = `tc-meter ${cls}`;
      (box.querySelector('i b') as HTMLElement).style.width = `${Math.round(Math.max(0, Math.min(1, v)) * 100)}%`;
      (box.querySelector('span:last-child') as HTMLElement).textContent = text;
    };
    // Once the campaign has booked the raid, the tray shows what it booked (the UI director, air review 4).
    const n = S.booked?.get(u.id) ?? u.n;
    const share = n / Math.max(1, u.n0);
    set('str', share, `${n}`, share < 0.35 ? 'is-low' : share < 0.65 ? 'is-mid' : '');
    set('mor', u.mor, `${Math.round(u.mor * 100)}`, 'is-rest');
    const st = el.querySelector<HTMLElement>('.ustate')!;
    const doing = live(u) ? ACT_WORD[u.act] ?? '' : '';
    const height = live(u) && u.act !== 'idle' ? ` · ${BAND_WORD[u.band]}${u.to !== u.band ? ` to ${BAND_WORD[u.to]}` : ''}` : '';
    const bombs = u.air === 'bmr' ? (u.dropped ? ' · bombs gone' : u.jettisoned ? ' · jettisoned' : '') : u.fuel < u.fuel0 * 0.2 && live(u) ? ' · short of fuel' : '';
    st.innerHTML = `<b>${STATE_WORD[u.state]}</b>${doing ? ` · ${doing}` : ''}${height}${bombs}`;
    st.className = `ustate tc-data${u.state === 'routing' ? ' bad' : u.state === 'wavering' ? ' warn' : ''}`;
  }
}

/** Before the formations first meet, and once the fighting is done and the last head home, nothing waits on the player. */
function rush(): '' | 'meet' | 'home' {
  const sim = S.sim!;
  return sim.over ? '' : !sim.fired ? 'meet' : sim.winding() ? 'home' : '';
}

/** HAND IT TO THE STAFF's question, with the staff's chance now (the UI director's ruling 226 words, item 5). */
function staffQuestion(): string {
  const sim = S.sim!, what = sim.place.mission === 'superiority' ? 'fight' : 'raid';
  const p = S.scene.me === 'att' ? sim.staffChance() : 1 - sim.staffChance();
  const odds = p >= 0.995 ? 'They are sure of it.' : p <= 0.005 ? 'They give it no chance.' : `They give you about ${Math.round(p * 100)}% to win it.`;
  return sim.fired ? `Hand the ${what} to the staff? They settle the rest at once, from where it stands now. ${odds}`
    : `Hand the ${what} to the staff? No shot has been fired: they settle the whole ${what} at once. ${odds}`;
}

function updateTop(force = false): void {
  const sim = S.sim!;
  const el = (id: string): HTMLElement => document.getElementById(id)!;
  // Paused means paused (ruling 226, mark 8): the clock box says which, and the first speed segment says it when lit.
  const stopped = S.clock.paused || !!S.asking;
  el('time').textContent = mmss(sim.t);
  el('cstate').textContent = stopped ? 'Paused' : 'Running';
  el('cstate').classList.toggle('is-run', !stopped);
  el('pword').textContent = S.clock.paused ? 'Paused' : 'Pause';
  document.querySelectorAll<HTMLElement>('[data-clock]').forEach((x) => x.classList.toggle('is-lit', Number(x.dataset.clock) === S.clock.speed));
  // The top bar in land's form (the UI director, b384 review 2 and 20:47Z): over the bar the card's chance and the
  // staff's now, reckoned as the hand-over question reckons it, twice a second and always when stopped; the bar fills
  // with that same chance from his end, the pale mark at the card's; under it how near each side is to breaking off,
  // his from the left, no numbers (mark 3).
  const me = S.scene.me, foe: Side = me === 'att' ? 'def' : 'att';
  const mineOf = (p: number): number => (me === 'att' ? p : 1 - p);
  if (!S.now || stopped || force || now() - S.now.at >= 0.5) S.now = { p: mineOf(sim.staffChance()), at: now() };
  el('balMe').style.width = `${(S.now.p * 100).toFixed(1)}%`;
  el('balThem').style.width = `${((1 - S.now.p) * 100).toFixed(1)}%`;
  el('balMark').style.left = `${(S.mark * 100).toFixed(1)}%`;
  // The bar speaks in numbers at its ends too (the UI director, 21:18Z); the cards and the question keep "sure of it".
  const pc = (p: number): string => (p >= 0.995 ? 'over 99%' : p <= 0.005 ? 'under 1%' : `about ${Math.round(p * 100)}%`);
  const line = `Card ${pc(mineOf(sim.place.chance))} · now ${pc(S.now.p)}`;
  if (el('ratio').textContent !== line) el('ratio').textContent = line;
  el('nearMe').style.width = `${(100 * Math.min(1, sim.nearBreak(me))).toFixed(1)}%`;
  el('nearThem').style.width = `${(100 * Math.min(1, sim.nearBreak(foe))).toFixed(1)}%`;
  // His own side three quarters of the way there: an amber line that stays until it passes.
  const warn = el('fwarn');
  const near = !sim.over && sim.nearBreak(me) >= SKY.warn;
  warn.hidden = !near;
  if (near) warn.textContent = `Your ${me === 'att' && sim.place.mission !== 'superiority' ? 'raid is' : 'fighters are'} near breaking off: send the worn home, or hand it to the staff.`;
  // The question, in place; the clock stands while it does.
  const ask = el('ask');
  ask.hidden = !S.asking;
  if (S.asking) {
    const q = S.asking === 'staff' ? staffQuestion() : 'Break off every air group? The battle ends here.';
    if (el('askq').textContent !== q) el('askq').textContent = q;
    el('ago').textContent = S.asking === 'staff' ? 'Hand it over' : 'Break off';
    ask.setAttribute('aria-label', q);
  }
  // The tag at the top of the field: PAUSED while stopped (not while a question stands), the clock running itself fast,
  // or a passing word as the clock starts or drops back.
  // The warning sits under the dispatch, however many lines it takes; the tag at the field's top left (the UI director,
  // b384 review 1), at the zoom group's height, as on land and at sea.
  const d = el('dispatch');
  (app.querySelector('.ftop') as HTMLElement).style.top = `${(d.textContent ? d.offsetTop + d.offsetHeight : 10) + 8}px`;
  const tag = el('ftag'), r = rush();
  const words = S.asking ? '' : S.clock.paused ? `PAUSED <span class="tc-key">Space</span>`
    : r === 'meet' ? `×${FAST} UNTIL THE FORMATIONS MEET` : r === 'home' ? `×${FAST} UNTIL THE LAST ARE HOME`
    : S.tag && now() < S.tag.until ? S.tag.text : '';
  tag.hidden = !words || !!sim.over;
  if (tag.innerHTML !== words) tag.innerHTML = words;
  // The aim slip under it: the score the stamp will judge, as it stands.
  el('faim').hidden = !!sim.over;
  if (!sim.over) {
    const slip = aimSlip(sim);
    (['aimWhat', 'aimNow', 'aimLost'] as const).forEach((id, i) => { const x = el(id), w = withThe(slip[i]); if (x.textContent !== w) x.textContent = w; x.hidden = !w; });
  }
  // The orders strip shows only what the groups in hand can take (the UI director, newcomer review 1): height for all,
  // the escort's for fighters while his bombers fly, the hunt for fighters, the bombs for bombers still carrying them.
  const ids = chosen();
  const us = ids.map((id) => unit(id)!);
  const strip = el('airorders');
  strip.hidden = us.length === 0;
  const bands = new Set(us.map((u) => u.to));
  strip.querySelectorAll<HTMLElement>('[data-band]').forEach((b) => b.classList.toggle('is-on', bands.size === 1 && bands.has(Number(b.dataset.band) as 0 | 1 | 2)));
  const ftr = us.filter((u) => u.air === 'ftr'), bmr = us.filter((u) => u.air === 'bmr' && !u.dropped && !u.jettisoned);
  el('escort').hidden = ftr.length === 0 || !sim.units.some((u) => u.side === me && u.air === 'bmr' && live(u));
  el('hunt').hidden = ftr.length === 0;
  const ways = new Set<string>(ftr.map((u) => (u.job.do === 'escort' ? (u.job.close ? '1' : u.job.top ? 'top' : '0') : u.job.do === 'hunt' ? 'h' : '')));
  strip.querySelectorAll<HTMLElement>('[data-escort]').forEach((b) => b.classList.toggle('is-on', ways.size === 1 && ways.has(b.dataset.escort!)));
  el('hunt').classList.toggle('is-lit', ways.size === 1 && ways.has('h'));
  el('jett').hidden = bmr.length === 0;
  if (force || S.dispatch) {
    const d = S.dispatch;
    el('dispatch').innerHTML = d ? `<span class="tc-label">${mmss(d.t)}</span><span>${esc(withThe(d.text))}</span>` : '';
  }
}

function fit(first = false): void {
  const r = canvas.getBoundingClientRect();
  const dpr = Math.min(2, window.devicePixelRatio || 1);
  canvas.width = Math.round(r.width * dpr); canvas.height = Math.round(r.height * dpr);
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  S.scene.W = r.width; S.scene.H = r.height;
  if (first) whole();
}
/** The sky the camera keeps in view: the board, and under its southern edge the raid forming up and where it comes from. */
/**
 * The field's free part, in pixels: under the dispatch at the top; and at the foot, the board's southern edge this far
 * above the field's, so the raid forming up under it, fanned out and ringed when picked up, stays clear of the orders
 * strip (some 56 pixels) with room to spare.
 */
const FREE = { top: 56, foot: 190 };
// The whole sky in the field's free part, the raid and FROM ... AIRFIELDS above the orders strip, so the strip never
// covers the player's own groups (the design director, b384(f); the UI director, newcomer review 2).
function whole(): void {
  const sc = S.scene;
  const z = Math.min((sc.W * 0.97) / SKY.w, (sc.H - FREE.top - FREE.foot) / SKY.h);
  sc.cam = { x: SKY.w / 2, y: (sc.H / 2 - FREE.foot) / z, z };
}
function minZoom(): number { return Math.min(S.scene.W / SKY.w, S.scene.H / SKY.h) * 0.8; }
function zoomAt(px: number, py: number, k: number): void {
  const c = S.scene.cam;
  const [fx, fy] = toField(S.scene, px, py);
  c.z = Math.max(minZoom(), Math.min(0.12, c.z * k));
  const [nx, ny] = toField(S.scene, px, py);
  c.x += fx - nx; c.y += fy - ny;
  clampCam();
}
function clampCam(): void {
  const c = S.scene.cam;
  const hw = S.scene.W / 2 / c.z, hh = S.scene.H / 2 / c.z;
  // Wider than the sky, the view keeps all of it; narrower, it stays over it.
  const keep = (v: number, lo: number, hi: number, h: number): number => (h * 2 >= hi - lo ? Math.min(lo + h, Math.max(hi - h, v)) : Math.max(lo + h, Math.min(hi - h, v)));
  c.x = keep(c.x, 0, SKY.w, hw);
  c.y = keep(c.y, SOUTH, SKY.h, hh);
}

// ---- Orders -----------------------------------------------------------------------------------------------------------

function chosen(): number[] { return [...S.scene.sel].filter((id) => { const u = unit(id); return !!u && orderable(u); }); }
/** Groups where they are drawn (fanned out of a stack), for picking: the pointer takes what the player sees. */
const drawn = (us: AirUnit[]): AirUnit[] => us.map((u) => { const [x, y] = shown(S.scene, u); return { ...u, x, y }; });
/** On the board: off it, the sky is no place to patrol (the UI director, newcomer review 6). */
const onBoard = (fx: number, fy: number): boolean => fx >= 0 && fx <= SKY.w && fy >= 0 && fy <= SKY.h;
const give = (c: Command): boolean => S.sim!.command(c, S.scene.me);
/** How near the pointer must be to a group to take it, in metres: a few pixels at any zoom. */
const reachPx = (): number => Math.max(1_500, 22 / S.scene.cam.z);
/** The enemy groups the player can see; the view hands back its events, so they are kept for the frame. */
function visible(): AirUnit[] {
  const sim = S.sim!;
  return sim.units.filter((u) => u.side !== S.scene.me && live(u) && u.act !== 'idle' && sim.sees(S.scene.me, u));
}

/** What a right click there would do, in the player's words, or '' where it does nothing. */
function orderWords(fx: number, fy: number): string {
  const ftr = chosen().filter((id) => unit(id)!.air === 'ftr');
  if (!ftr.length) return '';
  const foe = unitAt(drawn(visible()), fx, fy, reachPx());
  return foe ? `Attack ${S.sim!.called(foe)}` : onBoard(fx, fy) ? 'Patrol here' : '';
}

function wireBattle(): void {
  const sc = S.scene;
  let down: { x: number; y: number; button: number; touch: boolean; moved: boolean; px: number; py: number } | null = null;
  let hold: number | undefined;
  const pos = (e: PointerEvent | WheelEvent | MouseEvent): [number, number] => { const r = canvas.getBoundingClientRect(); return [e.clientX - r.left, e.clientY - r.top]; };
  canvas.addEventListener('contextmenu', (e) => e.preventDefault());
  canvas.addEventListener('pointerdown', (e) => {
    if (S.screen !== 'battle') return;
    const [px, py] = pos(e);
    canvas.setPointerCapture(e.pointerId);
    const touch = e.pointerType === 'touch';
    down = { x: px, y: py, button: e.button, touch, moved: false, px, py };
    if (touch) hold = window.setTimeout(() => { if (down && !down.moved) { orderAt(down.x, down.y); down = null; } }, 450);
  });
  canvas.addEventListener('pointermove', (e) => {
    const sim = S.sim;
    if (!sim) return;
    const [px, py] = pos(e);
    const [fx, fy] = toField(sc, px, py);
    sc.at = [fx, fy];
    if (!down) {
      const u = unitAt(drawn([...sim.units.filter((x) => x.side === sc.me && live(x)), ...visible()]), fx, fy, reachPx(), sc.me);
      sc.hover = u ? u.id : null;
      canvas.style.cursor = u && u.side !== sc.me && sc.sel.size ? 'crosshair' : u && u.side === sc.me ? 'pointer' : 'default';
      canvas.title = orderWords(fx, fy);
      return;
    }
    if (Math.hypot(px - down.x, py - down.y) > 6) down.moved = true;
    if (!down.moved) return;
    if (down.touch || down.button === 1 || down.button === 2) { sc.cam.x -= (px - down.px) / sc.cam.z; sc.cam.y += (py - down.py) / sc.cam.z; clampCam(); }
    else if (down.button === 0) { const [ax, ay] = toField(sc, down.x, down.y); sc.box = [ax, ay, fx, fy]; }
    down.px = px; down.py = py;
  });
  canvas.addEventListener('pointerleave', () => { sc.at = null; });
  const up = (e: PointerEvent): void => {
    window.clearTimeout(hold);
    const sim = S.sim;
    if (!down || !sim) { down = null; return; }
    const [px, py] = pos(e);
    const d = down;
    down = null;
    if (d.touch) { if (!d.moved) pickAt(px, py, false); return; }
    if (d.button === 0) {
      if (sc.box) {
        const [x0, y0, x1, y1] = sc.box;
        const got = unitsIn(drawn(sim.units), sc.me, x0, y0, x1, y1).filter(orderable);
        if (!e.shiftKey) sc.sel.clear();
        for (const u of got) sc.sel.add(u.id);
        sc.box = null;
      } else pickAt(px, py, e.shiftKey);
    } else if (d.button === 2 && !d.moved) orderAt(px, py);
    updateTray(true); updateTop();
  };
  canvas.addEventListener('pointerup', up);
  canvas.addEventListener('pointercancel', () => { down = null; sc.box = null; });
  canvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    const [px, py] = pos(e);
    const notched = e.deltaMode !== 0 || (e.deltaX === 0 && Math.abs(e.deltaY) >= 40 && Number.isInteger(e.deltaY));
    if (e.ctrlKey || notched) zoomAt(px, py, Math.exp(-e.deltaY * (e.ctrlKey ? 0.01 : 0.0015)));
    else { sc.cam.x += e.deltaX / sc.cam.z; sc.cam.y -= e.deltaY / sc.cam.z; clampCam(); }
  }, { passive: false });
  window.onresize = (): void => { if (S.screen !== 'choose') fit(); };
  app.querySelectorAll<HTMLElement>('[data-clock]').forEach((el) => el.addEventListener('click', () => { setSpeed(Number(el.dataset.clock) as Speed); updateTop(); }));
  app.querySelectorAll<HTMLElement>('[data-zoom]').forEach((el) => el.addEventListener('click', () => {
    const k = Number(el.dataset.zoom);
    if (k === 0) whole(); else zoomAt(sc.W / 2, sc.H / 2, k > 0 ? 1.4 : 1 / 1.4);
  }));
  app.querySelectorAll<HTMLElement>('[data-band]').forEach((el) => el.addEventListener('click', () => { give({ do: 'air:band', units: chosen(), band: Number(el.dataset.band) }); updateTop(); updateTray(true); }));
  app.querySelectorAll<HTMLElement>('[data-escort]').forEach((el) => el.addEventListener('click', () => {
    // Escort the bombers they are already with, or the nearest of the player's own still going in.
    const ftr = chosen().map((id) => unit(id)!).filter((u) => u.air === 'ftr');
    const boxes = S.sim!.units.filter((u) => u.side === sc.me && u.air === 'bmr' && live(u));
    for (const f of ftr) {
      const on = f.job.do === 'escort' ? f.job.on : boxes.sort((a, b) => Math.hypot(a.x - f.x, a.y - f.y) - Math.hypot(b.x - f.x, b.y - f.y))[0]?.id;
      if (on !== undefined) give({ do: 'air:escort', units: [f.id], target: on, close: el.dataset.escort === '1' });
    }
    updateTop(); updateTray(true);
  }));
  app.querySelector('[data-act="hunt"]')!.addEventListener('click', () => { give({ do: 'air:hunt', units: chosen() }); updateTop(); });
  app.querySelector('[data-act="jettison"]')!.addEventListener('click', () => { give({ do: 'air:jettison', units: chosen() }); updateTop(); updateTray(true); });
  // Both orders that end the battle ask once, in place, and KEEP FIGHTING is the default (ruling 226, mark 5).
  const ask = (q: 'staff' | 'withdraw'): void => { S.asking = q; guard(); updateTop(); (app.querySelector('[data-act="astay"]') as HTMLElement).focus(); };
  app.querySelector('[data-act="dice"]')!.addEventListener('click', () => ask('staff'));
  app.querySelector('[data-act="withdraw"]')!.addEventListener('click', () => ask('withdraw'));
  app.querySelector('[data-act="astay"]')!.addEventListener('click', () => { S.asking = null; updateTop(); });
  app.querySelector('[data-act="ago"]')!.addEventListener('click', () => {
    if (now() < S.guardUntil || !S.asking) return;
    const q = S.asking;
    S.asking = null;
    if (q === 'staff') give({ do: 'dice' }); else give({ do: 'withdrawAll', side: sc.me });
  });
  const tray = document.getElementById('tray')!;
  tray.addEventListener('click', (e) => {
    const el = (e.target as HTMLElement).closest<HTMLElement>('[data-unit]');
    if (!el) return;
    const id = Number(el.dataset.unit), u = unit(id);
    if (!u || !orderable(u)) return;
    if (e.shiftKey) { if (sc.sel.has(id)) sc.sel.delete(id); else sc.sel.add(id); } else { sc.sel.clear(); sc.sel.add(id); }
    updateTray(true); updateTop();
  });
  tray.addEventListener('dblclick', (e) => {
    const el = (e.target as HTMLElement).closest<HTMLElement>('[data-unit]');
    const u = el ? unit(Number(el.dataset.unit)) : undefined;
    if (u) { sc.cam.x = u.x; sc.cam.y = u.y; sc.cam.z = Math.max(sc.cam.z, 0.02); clampCam(); }
  });
}

function pickAt(px: number, py: number, add: boolean): void {
  const sim = S.sim!, sc = S.scene;
  const [fx, fy] = toField(sc, px, py);
  const u = unitAt(drawn(sim.units.filter((x) => x.side === sc.me && orderable(x))), fx, fy, reachPx(), sc.me);
  if (!u) { if (!add) sc.sel.clear(); return; }
  if (add) { if (sc.sel.has(u.id)) sc.sel.delete(u.id); else sc.sel.add(u.id); } else { sc.sel.clear(); sc.sel.add(u.id); }
}

function orderAt(px: number, py: number): void {
  const sc = S.scene;
  const ids = chosen();
  if (!ids.length) return;
  const [fx, fy] = toField(sc, px, py);
  const foe = unitAt(drawn(visible()), fx, fy, reachPx());
  if (foe) { if (give({ do: 'attack', units: ids, target: foe.id })) sc.ping = { x: foe.x, y: foe.y, at: now(), kind: 'attack' }; }
  else if (onBoard(fx, fy) && give({ do: 'move', units: ids, x: fx, y: fy })) sc.ping = { x: fx, y: fy, at: now(), kind: 'move' };
}

window.addEventListener('keydown', (e) => {
  if (e.key === 'Enter') { e.preventDefault(); return; }
  if (now() < S.guardUntil) { if (e.key === 'Tab' || e.key === ' ') e.preventDefault(); return; }
  if (S.screen !== 'battle' || !S.sim) return;
  const sc = S.scene, sim = S.sim;
  const key = e.key.length === 1 ? e.key.toLowerCase() : e.key;
  const pan = 120 / sc.cam.z;
  // While a question stands it is the one voice: Esc answers it no, and nothing else runs the clock.
  if (S.asking && key !== 'Escape') { if (key === ' ' || key === 'Tab') e.preventDefault(); return; }
  switch (key) {
    case ' ': e.preventDefault(); setSpeed(S.clock.paused ? -1 : 0); break;
    case '1': setSpeed(1); break;
    case '2': setSpeed(2); break;
    case '3': setSpeed(4); break;
    case 'h': give({ do: 'hold', units: chosen() }); break;
    case 'r': give({ do: 'withdraw', units: chosen() }); break;
    case 'Escape': if (S.asking) S.asking = null; else { sc.sel.clear(); sc.box = null; } break;
    case 'Tab': {
      e.preventDefault();
      const mine = sim.units.filter((u) => u.side === sc.me && orderable(u));
      if (!mine.length) break;
      const cur = [...sc.sel][0];
      const i = mine.findIndex((u) => u.id === cur);
      const next = mine[(i + (e.shiftKey ? mine.length - 1 : 1)) % mine.length];
      sc.sel.clear(); sc.sel.add(next.id);
      [sc.cam.x, sc.cam.y] = shown(sc, next); clampCam();
      break;
    }
    case 'ArrowLeft': case 'a': sc.cam.x -= pan; clampCam(); break;
    case 'ArrowRight': case 'd': sc.cam.x += pan; clampCam(); break;
    case 'ArrowUp': case 'w': sc.cam.y += pan; clampCam(); break;
    case 'ArrowDown': case 's': sc.cam.y -= pan; clampCam(); break;
    case '+': case '=': zoomAt(sc.W / 2, sc.H / 2, 1.25); break;
    case '-': zoomAt(sc.W / 2, sc.H / 2, 0.8); break;
    default: return;
  }
  updateTop(); updateTray(true);
});

/**
 * Sets the clock (0 stops it; -1 starts it again at the speed before). Started from stopped, the tag says so for about
 * two seconds: "RUNNING ×2" (ruling 226, mark 8); a change of speed while running needs none.
 */
function setSpeed(v: Speed | -1): void {
  const was = S.clock.paused;
  if (v === -1) S.clock.toggle(); else S.clock.set(v);
  if (was && !S.clock.paused) S.tag = { text: `RUNNING ×${S.clock.speed}`, until: now() + 2 };
}

// ---- The loop -----------------------------------------------------------------------------------------------------------

function frame(): void {
  requestAnimationFrame(frame);
  if (S.screen === 'choose' || !S.sim) return;
  const t = now();
  const dt = Math.min(0.25, t - S.last);
  S.last = t;
  S.scene.now = t;
  const sim = S.sim;
  // A question stops the clock (b358). Before the formations meet, and once the fighting is done, it runs itself fast;
  // at the first contact it drops back to the player's own speed and says so (ruling 226, mark 1).
  if (!S.asking) {
    const fast = rush() && S.clock.speed > 0 && S.clock.speed < FAST ? FAST / S.clock.speed : 1;
    S.clock.advance(sim, dt * fast);
  }
  const r = rush();
  if (S.rushed === 'meet' && r !== 'meet' && !sim.over) S.tag = { text: `THE FORMATIONS MEET · RUNNING ×${S.clock.speed || 1}`, until: t + 3 };
  S.rushed = r;
  const v = sim.view(S.scene.me);
  takeEvents(S.scene, sim, v.events, t);
  if (sim.t > 0 && S.dispatch?.t === 0 && !S.dispatch.unit) S.dispatch = { t: sim.t, what: 'contact', text: 'The raid is on its way in.', unit: -1 };
  // His own side near breaking off is the amber line, which stays; the enemy's is one dispatch.
  const own = (e: FieldEvent): boolean => e.what === 'air:near' && (e.extra as { side?: Side } | undefined)?.side === S.scene.me;
  for (const e of v.events) if (e.text && e.what !== 'air:lost' && !own(e)) S.dispatch = e;
  for (const id of [...S.scene.sel]) { const u = unit(id); if (!u || !orderable(u)) S.scene.sel.delete(id); }
  drawAir(ctx, sim, v, S.scene);
  // What a right click would do, by the pointer (the UI director's order names).
  const sc = S.scene;
  if (sc.at && S.screen === 'battle') {
    const words = orderWords(sc.at[0], sc.at[1]);
    if (words) {
      const [a, b] = toScreen(sc, sc.at[0], sc.at[1]);
      ctx.font = '700 12px "Courier Prime", monospace'; ctx.textAlign = 'left'; ctx.textBaseline = 'top';
      ctx.lineWidth = 3; ctx.strokeStyle = 'rgba(14,11,8,.85)'; ctx.strokeText(words, a + 14, b + 12);
      ctx.fillStyle = '#e0b872'; ctx.fillText(words, a + 14, b + 12);
    }
  }
  updateTop();
  updateTray();
  if (v.over && S.screen === 'battle') showAfter();
}

// ---- After the battle: what the campaign makes of it -------------------------------------------------------------------

/** Never "0.0%" (the UI director, ruled 19:51Z): "less than 0.1% of German production", until the rules print it so. */
const never00 = (text: string): string => text.replace(/\b0\.0% of (\S+) production/g, 'less than 0.1% of $1 production');

function showAfter(): void {
  const sim = S.sim!, r = sim.over!, raid = S.raid!;
  S.screen = 'after';
  guard();
  const t = sim.ticket;
  // The campaign flies the raid with the battle's result in place of its dice (or, handed to the staff, its dice on the rest).
  const fought = foughtAir(sim.outcome());
  const rep = launchRaid(raid.g, raid.kind, raid.target, raid.ids, raid.nat, fought);
  const card: AfterCard | null = rep ? afterCard(raid.g, rep, fought) : null;
  if (rep) battles++;
  // The battle's verdict is the one the campaign booked: the field's own (b331, b384(a)), or, handed to the staff, theirs.
  if (rep) { r.winner = rep.won ? 'att' : 'def'; r.level = rep.level; }
  const lines = card ? card.lines : r.lines;
  const level = card ? card.level : r.level;
  const won = level.includes('victory');
  const sky = sim.place.mission === 'superiority';
  // The after card is the one voice: the dispatch strip steps aside (the UI director, air review 5).
  S.dispatch = null;
  document.getElementById('dispatch')!.innerHTML = '';
  // The tray takes the booked counts: each group's booked losses shared over its formations by what each sent up.
  if (rep) {
    S.booked = new Map();
    for (const x of [...rep.att, ...rep.def]) {
      for (const kind of ['ftr', 'bmr'] as const) {
        const us = sim.units.filter((u) => u.corps === x.id && u.air === kind);
        const up = us.reduce((s, u) => s + u.n0, 0);
        let left = kind === 'ftr' ? x.lostFtr : x.lostBmr;
        us.forEach((u, i) => {
          const share = i === us.length - 1 ? left : Math.round(((kind === 'ftr' ? x.lostFtr : x.lostBmr) * u.n0) / Math.max(1, up));
          left -= share;
          S.booked!.set(u.id, Math.max(0, u.n0 - share));
        });
      }
    }
    updateTray(true);
    // Losses carry into the raids offered after this one, and the chooser says what changed (the design director,
    // b384(i); the UI director, newcomer review 9): his own groups first, then the enemy's with their country word.
    since = [];
    for (const side of [S.scene.me, S.scene.me === 'att' ? 'def' : 'att'] as Side[]) {
      for (const x of side === 'att' ? rep.att : rep.def) {
        if (x.id === undefined) continue;
        const f = raid.g.airFleets.find((y) => y.id === x.id);
        if (f) carried.set(f.id, { ftr: f.ftr, bmr: f.bmr });
        if (!x.lostFtr && !x.lostBmr) continue;
        const kinds = (['bmr', 'ftr'] as const).filter((k) => x[k] > 0);
        const lostOf = (k: 'bmr' | 'ftr'): number => (k === 'bmr' ? x.lostBmr : x.lostFtr);
        const word = (k: 'bmr' | 'ftr', n: number): string => `${n}${NB}${k === 'bmr' ? 'bomber' : 'fighter'}${n === 1 ? '' : 's'}`;
        const had = kinds.map((k) => `${word(k, x[k])}, ${lostOf(k)} lost`).join(', and ');
        const left = kinds.length === 1 ? `${x[kinds[0]] - lostOf(kinds[0])}` : kinds.map((k) => word(k, x[k] - lostOf(k))).join(' and ');
        const who = side === S.scene.me ? x.fleet : `${t[side].adj} ${x.fleet}`;
        since.push({ who, had, left, foe: side === S.scene.me ? null : x.id, n: x.ftr - x.lostFtr });
      }
    }
  }
  // The roster: what each group lost of what it sent up, the same form on both sides (the design director, 19:29Z).
  const sides = card ? `<div class="aside2">${(['att', 'def'] as const).map((sd) => `<div><h3 class="tc-label">${esc(t[sd].adj)} ${sd === 'att' ? (sky ? 'fighters' : 'raid') : 'fighters'}</h3><ul>${card.groups.filter((x) => x.side === sd).map((x) =>
    `<li><b>${esc(x.name)}</b> <span>${esc(x.words)}</span></li>`).join('')}</ul></div>`).join('')}</div>` : '';
  const veil = document.createElement('div');
  veil.className = 'veil';
  veil.innerHTML = `<div class="after tc-paper" role="dialog" aria-label="Raid result">
    <div class="tc-label kick">${sky ? 'Battle result' : 'Raid result'} · ${esc(t.date)}</div>
    <h2 class="tc-title-2">${esc(withThe(t.title))}</h2>
    <div class="verdict"><span class="tc-stamp ${won ? 'tc-stamp--won' : ''}">${esc(level)}</span></div>
    ${rep && rep.result && (sky || (rep.dropped ?? 0) > 0) ? `<p class="told"><b>${esc(withThe(never00(rep.result)))}</b></p>` : ''}
    <h3 class="tc-label">In the air</h3><ul class="flines">${lines.map((l) => `<li>${esc(withThe(l))}</li>`).join('')}</ul>
    ${card?.reckoning ? `<h3 class="tc-label">Against the staff's reckoning</h3><p class="dice">${esc(card.reckoning)}</p>` : ''}
    ${sides}
    <div class="afoot"><button class="tc-btn tc-btn--primary tc-btn--lg" data-act="continue">Continue</button></div>
  </div>`;
  document.body.appendChild(veil);
  veil.querySelector('[data-act="continue"]')!.addEventListener('click', () => { if (now() < S.guardUntil) return; veil.remove(); showChooser(); });
}

// A handle for the page's own checks (screenshots, the playtester's harness). Not used by the page itself.
(window as unknown as { __air: unknown }).__air = { state: S, give, showChooser, startBattle, raidOf, pairOf, OFFERS, toScreen };

showChooser();
requestAnimationFrame(frame);

// The campaign screen: a staff map of Europe, the armies on it, and the paperwork a
// commander would see. All of the rules live in ../game, ../ai and ../turn; this file
// only draws the state of the game and turns clicks into orders.

import { flagDefs, flagIcon, flagUse } from './flags';
import grandData from '../../../data/campaign/grand.json';
import {
  DESERT, allied, atWar, collapseScore, convoyDrag, createGame, ensureSupply, fieldArmy, frozen, hostileUnits, inDesert, limitAttackers, lineStrength, men, railReach, refuge, weather,
} from '../game';
import {
  beginEve, beginHuman, canOrder, candidates, declarable, endEve, endHuman, eveAttacker, evePlan, humanUp, landTargets, onEve, orderAttack, orderBuildCorps, orderBuildFort, orderEvePlan,
  orderAir, orderAirBase, orderAssign, orderBureau, orderCancelBuild, orderCourt, orderDeclare, orderEvacuate, orderFleet, orderLand, orderMove, orderPromote, orderRail, orderRebase, orderRelieve, orderSack, orderShip, orderSkill, orderStance, orderTeams, postureOf,
  previewAirOrder, previewAttack, previewLanding, reach, runTurnAI, shipTargets, stepAI, type Reach,
  orderCash, orderDrafts, orderSeen, arsenalOffers, orderArsenal,
} from '../turn';
import { ARSENAL, canStation, factoriesBuilt, workLevel, yardLevel, type Hull, type Offer } from '../arsenal';
import {
  AIR, MISSION, airBases, airDeliveries, airFleetsOf, airIncomeLoss, airPlanFor, airPool, airRange, airTargets, cover, crowding, forecastAir, isOffensive,
  km as airKm, seaKm, skyOver, whyNotBase, whyNotMission, type AirOrder,
} from '../air';
import {
  NAVY, SEA_BORDERS, airborne as byAir, assaultStrength, atSea, baseTargets, canPass, closedTo, fleetPower, fleetReach, fleetsOf, liftLeft, oreRoute, seaAt, seaRisk, seaSide, shipCount, shipList, surfaceShips,
} from '../navy';
import { phaseFor, theatrePhases } from '../ai';
import { GOALS, GOAL_KEYS, goalChecks, goalState, goalWhen, judgeGoals, openGoals, weeksLasted } from '../goals';
import { fronts, isOpen, startOpen } from '../staff';
import { BUDGET, PRODUCTION, budgeted, canRaise, corpsUpkeep, fortCost, isHome, ledger, needsOf, provinceYield, raiseSite, specOf, type Ledger, type LedgerLine } from '../economy';
import { FIELDS, MAX_TEAMS, RESEARCH, arrival, bureauCost, doctrineOf, inService, lead, monthsAhead, nextStep, researches, stepsOf, teamsFree, teamsTotal } from '../research';
import { loadGame, peekSave, saveGame, staleSave } from '../save';
import { COMMAND, SKILLS, corpsOf, fit, generalOf, popular, rankOf, serving, vacant, waiting, whyNotAssign, whyNotPromote, xpToNext } from '../commanders';
import { STORIES, awardOf, awardTitle, cashWorth, citation, designation, diaryText, fallenOf, fameOf, grade, honourDef, honoursOf, isGuards, raisedXp, revivable, rivalryOf, stat, takeOver, training, unitOf, unseen, whyNotCash } from '../stories';
import { DIPLOMACY, TRACK_LABEL, TRACK_PHRASE, canCourt, courtable, momentum, pcIncome, receptive, standing, step, tradePartners, type Standing } from '../diplomacy';
import { bloc } from '../game';
import { KIND, type AirFleet, type AirMission, type AirReport, type AirSide, type Award, type BattleReport, type Cash, type Corps, type DiaryLine, type Fallen, type Storied, type UnitKind, type Field, type Fleet, type Game, type General, type Kind, type Nation, type Posture, type Region, type SeaReport, type SeaSide, type Skill, type Stance } from '../types';
import { turnToDate } from '../../core/ticket';

let g: Game;
/** The month after the last one the historical plans cover: February 1945, when the Bulge has been closed. */
const STORY_ENDS = 66;
/** The first month in which only some fronts still follow history: January 1944. */
const STORY_THINS = 53;
/** A free-play war has no story to run out of: left on Play, it is stopped at the end of 1947 so that it does not run for ever. */
const FREE_ENDS = 100;
const ui = {
  sel: -1,
  picked: new Set<number>(),
  rail: false,
  ship: false,
  /** Choosing an enemy coast for the picked corps to land on. */
  land: false,
  /** The sea being looked at, when no region is. */
  sea: -1,
  /** The fleet being given orders under the Navy tab. */
  fleet: -1,
  stance: 'patrol' as Stance,
  /** Choosing a new home port for that fleet. */
  rebase: false,
  seenSea: 0,
  /** The air fleet being given orders under the Air tab. */
  air: -1,
  /** What a click on the map means for that air fleet: a new base, or a target for a mission. */
  airMode: '' as '' | 'base' | AirMission,
  seenAir: 0,
  /** A one-line answer to the last click, shown where the hint is. */
  msg: '',
  /** False until a game has been started or resumed, so the title screen never overwrites a saved game. */
  live: false,
  tab: 'region' as 'region' | 'build' | 'research' | 'generals' | 'honours' | 'navy' | 'air' | 'reel' | 'battles' | 'powers',
  view: { x: 380, y: 300, w: 620 },
  unread: 0,
  seenReports: 0,
  flash: [] as number[],
  auto: 0 as number,
  modal: null as null | { kind: string; [k: string]: any },
  /** The start screen: the grand campaign's front door, or the list of historical scenarios behind it. */
  menu: 'grand' as 'grand' | 'scenarios',
  /** Which shelf of the arsenal the Build tab shows in the grand campaign. */
  arm: 'army' as 'army' | 'air' | 'navy' | 'works',
  /** The country and the year picked for a grand campaign. An empty country watches. */
  gcNat: 'GER',
  gcYear: 1939,
};

const $ = (s: string): HTMLElement => document.querySelector(s) as HTMLElement;
const esc = (s: string): string => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]!));
const num = (n: number): string => Math.round(n).toLocaleString('en-US');
/** Money as the interface shows it: a price or what is in hand, rounded down so nothing is promised that is not there. */
const money = (pp: number): string => Math.floor(pp * BUDGET.scale + 1e-6).toLocaleString('en-US');
/** A flow of money in the books, rounded to the nearest whole number, with the sign written out when asked. */
const cash = (pp: number, signed = false): string => { const v = Math.round(pp * BUDGET.scale); return `${v < 0 ? '−' : signed && v > 0 ? '+' : ''}${Math.abs(v).toLocaleString('en-US')}`; };
/** What is in the treasury: rounded down in credit, so it never shows more than can be spent, and as a debt when it is one. */
const inHand = (pp: number): string => (pp >= 0 ? money(pp) : cash(pp));
const natName = (id: string): string => g.nations[id]?.name ?? id;
/** A nation's name as it reads inside a sentence: "the United Kingdom", "Germany". */
const theName = (id: string): string => { const nm = natName(id); return /^(United|Soviet|Netherlands)/.test(nm) ? `the ${nm}` : nm; };
const upFirst = (s: string): string => (s ? s[0].toUpperCase() + s.slice(1) : s);
/** "Germany", "Germany and Italy", "Germany, Italy, Hungary and 2 more". */
function nameList(ids: string[], most = 3): string {
  const names = ids.slice(0, most).map(theName);
  if (ids.length > most) names.push(`${ids.length - most} more`);
  return names.length <= 1 ? names.join('') : `${names.slice(0, -1).join(', ')} and ${names[names.length - 1]}`;
}

/** The nation the player is running, while it is still in the war: the one whose friends and enemies the map is read by. */
const viewer = (): string | null => (g.human && g.nations[g.human]?.alive ? g.human : null);

/**
 * What a nation is to the player: one word for the badge, and the class that colours it. With nobody at the
 * controls there are no friends or enemies, so the word is the side the nation is on.
 */
function relationWord(nat: string): { kind: Standing | 'axis' | 'allies'; word: string } {
  const me = viewer();
  if (me) {
    const kind = standing(g, me, nat);
    return { kind, word: kind === 'own' ? 'Yours' : kind === 'ally' ? 'Ally' : kind === 'enemy' ? 'Enemy' : 'Neutral' };
  }
  const side = bloc(g.nations[nat]);
  return side ? { kind: side, word: side === 'axis' ? 'Axis' : 'Allies' } : { kind: 'neutral', word: 'Neutral' };
}

/** The banner at the head of the region panel: whose ground this is to the player, in a word and a sentence. */
function standingNote(r: Region): string {
  const me = viewer();
  const h = r.ctrl, o = r.owner, n = g.nations[h];
  const { kind, word } = relationWord(h);
  const N = upFirst(theName(h));
  const foes = n.atWar.filter((x) => x !== me && g.nations[x]?.alive);
  const wars = foes.length ? `At war with ${nameList(foes)}.` : 'Not at war with anyone.';
  let line: string;
  if (!me) line = `${N} is ${TRACK_PHRASE[n.align]}. ${wars}`;
  else if (kind === 'own') line = o === me ? 'Your own ground.' : 'Held by your troops.';
  else if (kind === 'enemy') line = `${N} is at war with you.${foes.length ? ` Also at war with ${nameList(foes)}.` : ''}`;
  else if (kind === 'ally') line = n.atWar.some((x) => g.nations[me].atWar.includes(x)) ? `${N} is on your side and fighting the same enemy.` : `${N} is on your side. ${wars}`;
  else if (bloc(n)) line = foes.length ? `${N} is ${TRACK_PHRASE[n.align]}, but not at war with you. ${wars}` : `${N} is ${TRACK_PHRASE[n.align]}, but is not at war with anyone.`;
  else line = `${N} is ${TRACK_PHRASE[n.align]}. ${wars}`;
  // Occupied ground: say where the nation it was taken from stands as well.
  if (o !== h && g.nations[o]) {
    const O = upFirst(theName(o));
    if (!g.nations[o].alive) line += ` ${O}, whose ground this was, has fallen.`;
    else if (me && o !== me) { const k = standing(g, me, o); line += ` ${O}, whose ground this is, is ${k === 'ally' ? 'your ally' : k === 'enemy' ? 'your enemy too' : 'neutral'}.`; }
    else if (me) line += ' This is your own ground, in their hands.';
    else line += ` The ground is ${g.nations[o].adj}.`;
  }
  return `<div class="rel ${kind}" style="--side:${n.color}"><b>${word}</b><span>${esc(line)}</span></div>`;
}
const date = (t: number): string => turnToDate(t).label;
const capital = (t: string): string => (t ? t[0].toUpperCase() + t.slice(1) : t);
/** Enemy coasts the picked corps could land on, worked out once each time the screen is drawn. */
let lands: Region[] = [];
/** What each picked corps can reach this month, kept from the last redraw for drawing its route under the cursor. */
let reaches: { from: number; ap: number; map: Map<number, Reach> }[] = [];
/** The army whose general and units the tray shows. */
let trayArmy: { reg: number; nat: string } | null = null;
/** The piece kept off the map while a copy of it is still being slid along its road, and the march being drawn. */
let marchHide: { reg: number; nat: string } | null = null;
let marchToken = 0;
/** The region whose route is drawn under the cursor. */
let routeShown = -1;

function mix(a: string, b: string, t: number): string {
  const p = (h: string, i: number) => parseInt(h.slice(1 + 2 * i, 3 + 2 * i), 16);
  const c = [0, 1, 2].map((i) => Math.round(p(a, i) * (1 - t) + p(b, i) * t));
  return `rgb(${c[0]},${c[1]},${c[2]})`;
}

function pathOf(polys: [number, number][][][]): string {
  let d = '';
  for (const poly of polys) for (const ring of poly) d += 'M' + ring.map((p) => `${p[0]} ${p[1]}`).join('L') + 'Z';
  return d;
}

// ---- Unit symbols (standard military map symbols, drawn in a w x h box) -------------------

function symbol(kind: Kind, w: number, h: number): string {
  const X = `<path class="sym" d="M0 0L${w} ${h}M0 ${h}L${w} 0"/>`;
  switch (kind) {
    case 'inf': return X;
    case 'mot': return X + `<circle class="symf" cx="${w * 0.3}" cy="${h + 1.7}" r="1.3"/><circle class="symf" cx="${w * 0.7}" cy="${h + 1.7}" r="1.3"/>`;
    case 'arm': return `<ellipse class="sym" cx="${w / 2}" cy="${h / 2}" rx="${w * 0.33}" ry="${h * 0.27}"/>`;
    case 'cav': return `<path class="sym" d="M0 ${h}L${w} 0"/>`;
    case 'mtn': return X + `<path class="symf" d="M${w / 2 - 3} ${h}L${w / 2} ${h - 4.5}L${w / 2 + 3} ${h}Z"/>`;
    case 'fort': return `<path class="sym" d="M${w * 0.3} ${h * 0.75}V${h * 0.3}H${w * 0.42}V${h * 0.45}H${w * 0.58}V${h * 0.3}H${w * 0.7}V${h * 0.75}Z"/>`;
    case 'art': return `<circle class="symf" cx="${w / 2}" cy="${h / 2}" r="${h * 0.24}"/>`;
  }
}

/**
 * The branch of an army's strongest corps, as a small plate on the corner of its flag. Infantry, the usual
 * case, carries none, so that the flag is seen whole.
 */
function badge(kind: Kind): string {
  const w = 9, h = 6.4;
  let s = '';
  switch (kind) {
    case 'inf': return '';
    case 'arm': s = `<ellipse class="bsym" cx="${w / 2}" cy="${h / 2}" rx="2.7" ry="1.6"/>`; break;
    case 'mot': s = `<path class="bsym" d="M1.5 1.2L${w - 1.5} ${h - 2.2}M1.5 ${h - 2.2}L${w - 1.5} 1.2"/><circle class="bsymf" cx="2.9" cy="${h - 1}" r=".7"/><circle class="bsymf" cx="${w - 2.9}" cy="${h - 1}" r=".7"/>`; break;
    case 'cav': s = `<path class="bsym" d="M1.5 ${h - 1.3}L${w - 1.5} 1.3"/>`; break;
    case 'mtn': s = `<path class="bsymf" d="M${w / 2 - 2.6} ${h - 1.2}L${w / 2} 1.2L${w / 2 + 2.6} ${h - 1.2}Z"/>`; break;
    case 'fort': s = `<path class="bsym" d="M2 ${h - 1.3}V1.7H3.6V2.9H5.4V1.7H7V${h - 1.3}Z"/>`; break;
    case 'art': s = `<circle class="bsymf" cx="${w / 2}" cy="${h / 2}" r="1.7"/>`; break;
  }
  return `<g class="badge" transform="translate(14.4 8.2)"><rect width="${w}" height="${h}" rx="1"/>${s}</g>`;
}

/** An army's standard: its nation's flag, edged, with the branch plate on its corner. */
const standard = (nat: string, kind: Kind): string => `${flagUse(nat, g.nations[nat].color)}<rect class="box" width="22" height="14" rx=".7" fill="none"/>${badge(kind)}`;

/** A nation's flag beside its name in a panel. */
const chip = (nat: string): string => flagIcon(nat, g.nations[nat]?.color ?? '#888', 20, 'chip');

/** The general an army on the map marches under: the most senior of those commanding its corps. */
function leaderOf(cs: Corps[]): General | null {
  let best: General | null = null, size = 0;
  for (const c of cs) {
    const x = generalOf(g, c);
    if (!x) continue;
    const s = c.divs * c.str;
    if (!best || x.level > best.level || (x.level === best.level && s > size)) { best = x; size = s; }
  }
  return best;
}

/** The name a general goes by on the map: his surname. */
const surname = (name: string): string => name.replace(/\s*\(.*\)\s*$/, '').trim().split(/\s+/).pop() ?? name;
/** The share of its full strength an army still has. */
const fitness = (cs: Corps[]): number => { const full = cs.reduce((n, c) => n + c.divs, 0); return full > 0 ? sumDivs(cs) / full : 0; };

function icon(c: Corps): string {
  return `<svg width="34" height="24" viewBox="-1 -1 26 19" class="ctr"><rect class="box" x="0" y="0" width="24" height="15" rx="1.5" fill="${g.nations[c.nat].color}"/>${symbol(c.kind, 24, 15)}</svg>`;
}

// ---- Fleet symbols: a warship in profile, or a submarine for a flotilla with no surface ships

const HULL = 'M-12 .5L-10 4H10L12.5 .5H6V-2H3V-4.5H-1V-2H-6V.5Z';
const SUB = 'M-11 2.2Q-11 4.2-8 4.2H8Q11.5 4.2 11.5 2.2Q11.5 .2 8 .2H2.5V-3H-1.5V.2H-8Q-11 .2-11 2.2Z';

function hullIcon(f: Fleet): string {
  return `<svg width="34" height="24" viewBox="-14 -9 28 18" class="ctr"><path class="hull" d="${surfaceShips(f) ? HULL : SUB}" fill="${g.nations[f.nat].color}"/></svg>`;
}

/** The fleets on the map: one marker for each navy in each sea, and a smaller one for ships lying in harbour. */
function fleetMarks(): string {
  const groups = new Map<string, Fleet[]>();
  for (const f of g.fleets) {
    if (!shipCount(f.ships)) continue;
    const key = atSea(f) ? `s:${f.zone}:${f.nat}:${f.stance}` : f.base >= 0 ? `p:${f.base}:${f.nat}` : `h:${f.home}:${f.nat}`;
    (groups.get(key) ?? groups.set(key, []).get(key)!).push(f);
  }
  const rows = new Map<string, number>();
  for (const key of groups.keys()) { const k = key.split(':').slice(0, 2).join(':'); rows.set(k, (rows.get(k) ?? 0) + 1); }
  const used = new Map<string, number>();
  let out = '';
  for (const [key, fs] of groups) {
    const [where, idText] = key.split(':');
    const id = Number(idText), slot = `${where}:${id}`;
    const i = used.get(slot) ?? 0;
    used.set(slot, i + 1);
    const n = fs.reduce((t, f) => t + shipCount(f.ships), 0);
    const shape = fs.some((f) => surfaceShips(f) > 0) ? HULL : SUB;
    const picked = fs.some((f) => f.id === ui.fleet) && ui.tab === 'navy';
    let x: number, y: number, small = false;
    if (where === 'p') { const r = g.regions[id]; x = r.x + 16 + i * 12; y = r.y + 12; small = true; }
    else { const z = g.seas[id]; x = z.x + (i - (rows.get(slot)! - 1) / 2) * 30; y = z.y + (where === 'h' ? 37 : 19); small = where === 'h'; }
    out += `<g class="ctr flt${fs[0].stance === 'raid' ? ' raid' : ''}${small ? ' port' : ''}" transform="translate(${x.toFixed(1)} ${y.toFixed(1)})${small ? ' scale(.68)' : ''}">${picked ? '<circle class="fsel" r="17"/>' : ''}<path class="hull" d="${shape}" fill="${g.nations[fs[0].nat].color}"/><g class="jack" transform="translate(-5 -12.4)">${flagUse(fs[0].nat, g.nations[fs[0].nat].color, 10, 6.4)}<rect width="10" height="6.4" fill="none"/></g><text x="0" y="11.5">${n}</text></g>`;
  }
  return out;
}

// ---- Air fleets: an aircraft seen from above, beside the region it is based in ---------------

const PLANE = 'M0-8.5L1.6-3 9.5 1.2V3.2L1.6 1.6 1.1 6 3.6 8V9.2L0 8.2-3.6 9.2V8L-1.1 6-1.6 1.6-9.5 3.2V1.2L-1.6-3Z';

function planeIcon(f: AirFleet): string {
  return `<svg width="34" height="24" viewBox="-14 -10.5 28 21" class="ctr"><path class="hull" d="${PLANE}" fill="${g.nations[f.nat].color}"/></svg>`;
}

/** Air fleets at their bases, and a line from each one that is flying a mission to where it is flying it. */
function airBaseMarks(): string {
  let out = '';
  for (const f of g.airFleets) {
    if (!isOffensive(f.mission) || f.ftr + f.bmr <= 0) continue;
    const b = g.regions[f.base];
    const t = f.mission === 'strike' ? g.seas[f.target] : g.regions[f.target];
    if (!t) continue;
    const ty = f.mission === 'strike' ? t.y + 9 : t.y;
    const c = g.nations[f.nat].color;
    out += `<path class="airline under" d="M${b.x} ${b.y}L${t.x} ${ty}"/><path class="airline${f.mission === 'night' ? ' night' : ''}" d="M${b.x} ${b.y}L${t.x} ${ty}" stroke="${c}"/><circle class="airhit" cx="${t.x}" cy="${ty}" r="3.4" fill="${c}"/>`;
  }
  const groups = new Map<string, AirFleet[]>();
  for (const f of g.airFleets) {
    if (f.ftr + f.bmr <= 0) continue;
    const key = `${f.base}:${f.nat}`;
    (groups.get(key) ?? groups.set(key, []).get(key)!).push(f);
  }
  const used = new Map<number, number>();
  for (const [key, fs] of groups) {
    const base = Number(key.split(':')[0]);
    const r = g.regions[base];
    const i = used.get(base) ?? 0;
    used.set(base, i + 1);
    const n = fs.reduce((t, f) => t + f.ftr + f.bmr, 0);
    const picked = ui.tab === 'air' && fs.some((f) => f.id === ui.air);
    out += `<g class="ctr flt air" transform="translate(${(r.x - 17 - i * 13).toFixed(1)} ${(r.y + 11).toFixed(1)}) scale(.62)">${picked ? '<circle class="fsel" r="17"/>' : ''}<path class="hull" d="${PLANE}" fill="${g.nations[fs[0].nat].color}"/><g class="jack" transform="translate(4.5 -12)">${flagUse(fs[0].nat, g.nations[fs[0].nat].color, 10, 6.4)}<rect width="10" height="6.4" fill="none"/></g><text x="0" y="17.5">${n}</text></g>`;
  }
  return out;
}

/** The reach of the picked air fleet, and where its next order could send it. */
function airMarks(f: AirFleet): string {
  const b = g.regions[f.base];
  const R = airRange(f);
  const u = AIR.kmPerUnit;
  let out = '';
  if (ui.airMode === 'base') for (const r of airBases(g, f)) out += `<path class="mk rail" d="${pathOf(r.polys)}"/>`;
  else if (ui.airMode === 'strike') for (const z of airTargets(g, f, 'strike')) out += `<circle class="mk sea${f.mission === 'strike' && f.target === z ? ' at' : ''}" cx="${g.seas[z].x}" cy="${g.seas[z].y + 9}" r="29"/>`;
  else if (ui.airMode) for (const id of airTargets(g, f, ui.airMode)) out += `<path class="mk hit" d="${pathOf(g.regions[id].polys)}"/>`;
  if (f.bmr > 0) out += `<circle class="mk ring bmr" cx="${b.x}" cy="${b.y}" r="${(R.bmr / u).toFixed(1)}"/>`;
  if (f.ftr > 0) out += `<circle class="mk ring ftr" cx="${b.x}" cy="${b.y}" r="${(R.ftr / u).toFixed(1)}"/><circle class="mk ring full" cx="${b.x}" cy="${b.y}" r="${((R.ftr * AIR.range.fullAt) / u).toFixed(1)}"/>`;
  return out;
}

// ---- The page skeleton --------------------------------------------------------------------

function mount(): void {
  $('#app').innerHTML = `
    <div class="bar">
      <div class="brand">Theater Command<small>1939 to 1945 · campaign prototype</small></div>
      <div class="when" id="when"></div>
      <span id="purse"></span>
      <span class="spacer"></span>
      <span id="barbtns" style="display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end"></span>
    </div>
    <div class="main">
      <div class="mapwrap" id="mapwrap">
        <svg id="map" preserveAspectRatio="xMidYMid slice">
          <defs>${flagDefs()}<pattern id="hatch" width="5" height="5" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><rect width="1.6" height="5" fill="rgba(20,22,24,.3)"/></pattern></defs>
          <g id="seas"></g><g id="land"></g><g id="regs"></g><g id="occ"></g><g id="fx"></g><g id="marks"></g><g id="labels"></g><g id="fleets"></g><g id="units"></g><g id="route"></g><g id="ghost"></g>
        </svg>
        <div class="hint" id="hint"></div>
        <div class="passing" id="passing"></div>
        <div class="alerts" id="alerts"></div>
        <div class="tray" id="tray"></div>
        <div class="legend"><span><i style="background:rgba(92,170,96,.5);border-color:#2f6b39"></i>Can march here</span><span><i style="background:rgba(207,90,65,.55);border-color:#a22d18"></i>Can attack</span><span><i style="background:repeating-linear-gradient(45deg,#8a8a80 0 2px,#cfc9b8 2px 4px);border-color:#555"></i>Occupied</span><span><svg width="22" height="10" viewBox="-13 -5.5 26 10.5" style="vertical-align:-1px;margin-right:4px"><path d="${HULL}" fill="#8f98a0" stroke="#15181b" stroke-width=".8"/></svg>Fleet at sea</span><span><svg width="13" height="12" viewBox="-10.5 -9.5 21 19.5" style="vertical-align:-2px;margin-right:4px"><path d="${PLANE}" fill="#8f98a0" stroke="#15181b" stroke-width=".8"/></svg>Air fleet</span></div>
        <div class="zoom"><button class="btn" data-act="zin" aria-label="Zoom in">+</button><button class="btn" data-act="zout" aria-label="Zoom out">−</button></div>
        <div class="tip" id="tip"></div>
      </div>
      <div class="side">
        <div class="tabs" id="tabs"></div>
        <div class="pane" id="pane"></div>
      </div>
    </div>
    <div class="veil" id="veil"><div class="card" id="card"></div></div>`;
  document.addEventListener('click', onClick);
  document.addEventListener('keydown', onKey);
  wireMap();
  window.addEventListener('resize', applyView);
}

function drawStatic(): void {
  $('#seas').innerHTML = SEA_BORDERS.map((b) => `<path class="seaborder" d="M${b.map((p) => `${p[0]} ${p[1]}`).join('L')}"/>`).join('')
    + g.seas.map((z) => {
      // Long names, and names in tight water, go on two lines.
      const cut = z.narrow || z.name.length > 12 ? z.name.lastIndexOf(' ') : -1;
      const lines = cut > 0 ? [z.name.slice(0, cut), z.name.slice(cut + 1)] : [z.name];
      const step = z.narrow ? 9 : 11;
      return `<text class="sealbl${z.narrow ? ' narrow' : ''}" y="${z.y - (lines.length - 1) * step / 2}">${lines.map((t, i) => `<tspan x="${z.x}" dy="${i ? step : 0}">${esc(t)}</tspan>`).join('')}</text>`;
    }).join('');
  $('#land').innerHTML = `<path class="land" d="${pathOf(g.land)}"/>`;
  $('#regs').innerHTML = g.regions.map((r) => `<path class="reg" data-id="${r.id}" d="${pathOf(r.polys)}"/>`).join('');
}

function drawLabels(): void {
  $('#labels').innerHTML = g.regions.map((r) => {
    const busy = r.units.length > 0;
    const star = r.cap ? `<path class="capmark" transform="translate(${r.x - (busy ? 16.5 : 0)} ${r.y - (busy ? 3 : 7)}) scale(1.15)" d="M0-3.2L.9-1 3.2-1 1.4.5 2 2.8 0 1.4-2 2.8-1.4.5-3.2-1-.9-1Z"/>` : '';
    const fort = r.fort ? `<path class="fortmark" transform="translate(${r.x + (busy ? 17 : 0)} ${r.y + (busy ? 3 : -7)})" d="M-3 2.5V-2.5H-1.2V-.8H1.2V-2.5H3V2.5"/>` : '';
    return `${star}${fort}<text class="lbl${r.cap ? ' cap' : ''}" x="${r.x}" y="${r.y + (busy ? 26.5 : 3)}">${esc(r.name)}</text>`;
  }).join('');
}

// ---- Drawing the changing parts -----------------------------------------------------------

function refresh(): void {
  ensureSupply(g);
  const me = g.human;
  const myTurn = humanUp(g);
  // Region colours: who holds it.
  const regs = $('#regs').children;
  let occ = '';
  for (const r of g.regions) {
    const n = g.nations[r.ctrl];
    (regs[r.id] as SVGPathElement).setAttribute('fill', mix(n.color, '#e9e2cf', n.atWar.length ? 0.4 : 0.63));
    if (r.ctrl !== r.owner) occ += `<path class="occ" d="${pathOf(r.polys)}"/>`;
  }
  $('#occ').innerHTML = occ;
  // Where an air force has won the sky, the region is ringed in its colour.
  let skies = '';
  for (const k of Object.keys(g.sky)) { const who = skyOver(g, +k); if (who) skies += `<path class="skywon" d="${pathOf(g.regions[+k].polys)}" stroke="${g.nations[who].color}"/>`; }
  $('#fx').innerHTML = ui.flash.map((id) => `<path class="flash" d="${pathOf(g.regions[id].polys)}"/>`).join('') + skies;

  // Where the picked corps can go.
  let marks = '';
  const picked = pickedCorps();
  reaches = [];
  $('#route').innerHTML = '';
  routeShown = -1;
  trayArmy = armyShown();
  lands = picked.length && (g.regions[picked[0].reg].port || byAir(picked)) && g.nations[picked[0].nat].atWar.length ? landTargets(g, picked) : [];
  if (!lands.length) ui.land = false;
  const fl = ui.tab === 'navy' ? myFleet() : null;
  if (!fl) ui.rebase = false;
  const af = ui.tab === 'air' ? myAir() : null;
  if (!af) ui.airMode = '';
  if (af) marks += airMarks(af);
  else if (fl) {
    if (ui.rebase) for (const r of baseTargets(g, fl)) marks += `<path class="mk rail" d="${pathOf(r.polys)}"/>`;
    else for (const z of fleetReach(g, fl)) marks += `<circle class="mk sea${atSea(fl) && fl.zone === z ? ' at' : ''}" cx="${g.seas[z].x}" cy="${g.seas[z].y + 9}" r="29"/>`;
  } else if (ui.rail && picked.length) {
    for (const r of railTargets(picked[0])) marks += `<path class="mk rail" d="${pathOf(r.polys)}"/>`;
  } else if (ui.land && picked.length) {
    for (const r of lands) marks += `<path class="mk hit" d="${pathOf(r.polys)}"/>`;
  } else if (ui.ship && picked.length) {
    const ports = new Set<number>();
    for (const c of picked) for (const r of shipTargets(g, c)) ports.add(r.id);
    for (const id of ports) marks += `<path class="mk rail" d="${pathOf(g.regions[id].polys)}"/>`;
  } else {
    const goes = new Map<number, boolean>();
    for (const c of picked) { const m = reach(g, c); reaches.push({ from: c.reg, ap: c.ap, map: m }); for (const [id, rc] of m) if (id !== c.reg) goes.set(id, (goes.get(id) ?? false) || rc.attack); }
    for (const [id, attack] of goes) marks += `<path class="mk ${attack ? 'hit' : 'move'}" d="${pathOf(g.regions[id].polys)}"/>`;
  }
  if (ui.sel >= 0) marks += `<path class="mk sel" d="${pathOf(g.regions[ui.sel].polys)}"/>`;
  else if (ui.sea >= 0 && ui.tab === 'region') marks += `<circle class="mk seasel" cx="${g.seas[ui.sea].x}" cy="${g.seas[ui.sea].y + 9}" r="33"/>`;
  $('#marks').innerHTML = marks;
  $('#fleets').innerHTML = airBaseMarks() + fleetMarks();

  // Armies. Each nation's force in a region is one piece on the map: its colours on a standard with the branch
  // of its strongest corps, its commander's name over it, and under it a bar for the strength it has left.
  let units = '';
  for (const r of g.regions) {
    if (!r.units.length) continue;
    const by = new Map<string, Corps[]>();
    for (const c of r.units) (by.get(c.nat) ?? by.set(c.nat, []).get(c.nat)!).push(c);
    const groups = [...by.entries()].sort((a, b) => sumDivs(a[1]) - sumDivs(b[1]));
    groups.forEach(([nat, cs], i) => {
      const k = groups.length - 1 - i;
      const lead = [...cs].sort((a, b) => KIND[b.kind].atk * b.divs - KIND[a.kind].atk * a.divs)[0];
      const ready = myTurn && nat === me && cs.some((c) => c.ap > 0 && c.kind !== 'fort');
      const spent = myTurn && nat === me && !ready;
      const chosen = r.id === ui.sel && (cs.some((c) => ui.picked.has(c.id)) || (trayArmy?.reg === r.id && trayArmy.nat === nat));
      const who = leaderOf(cs);
      const fit = fitness(cs);
      const x = r.x - 11 + k * 4.5, y = r.y - 7 - k * 4;
      units += `<g class="ctr army${ready ? ' ready' : ''}${spent ? ' spent' : ''}${chosen ? ' chosen' : ''}${who ? ' led' : ''}" data-reg="${r.id}" data-nat="${nat}" transform="translate(${x} ${y})"${marchHide && marchHide.reg === r.id && marchHide.nat === nat ? ' style="visibility:hidden"' : ''}>
        <ellipse class="base" cx="11" cy="16.2" rx="13" ry="3.4" fill="${g.nations[nat].color}"/><path class="pole" d="M-.6 -1.5V16.2"/>
        ${standard(nat, lead.kind)}
        ${k === 0 ? `<rect class="strbg" x="0" y="15.2" width="22" height="2.2" rx=".6"/><rect class="str${fit < 0.5 ? ' low' : fit < 0.75 ? ' mid' : ''}" x="0" y="15.2" width="${(22 * Math.min(1, fit)).toFixed(1)}" height="2.2" rx=".6"/>
        <text x="11" y="24.6">${num(sumDivs(cs))}</text><text class="size" x="11" y="-1.6">${cs.length > 1 ? 'XXXX' : 'XXX'}</text>${who ? `<text class="cmdr" x="11" y="-2">${esc(surname(who.name))}</text>` : ''}` : ''}
        ${k === 0 && r.units.length > 1 ? `<circle class="more" cx="22" cy="0" r="4"/><text class="moret" x="22" y="2">${r.units.length}</text>` : ''}</g>`;
    });
  }
  $('#units').innerHTML = units;
  drawLabels();

  drawBar();
  drawTabs();
  drawPane();
  drawTray();
  $('#hint').textContent = hintText();
  $('#alerts').innerHTML = alertButtons();
  const pass = $('#passing');
  if (passing) {
    const n = passing.nat ? g.nations[passing.nat] : null;
    pass.innerHTML = `<div class="prow">${n ? `${chip(passing.nat)}<b>${esc(natName(passing.nat))}</b>` : '<b>The month goes on</b>'}<span class="grow">${esc(passing.note || 'The other nations give their orders.')}</span><button class="btn small" data-act="skip">Skip ▸▸</button></div><div class="pbar"><i style="width:${Math.round(100 * Math.min(1, passing.done / Math.max(1, passing.total)))}%"></i></div>`;
    pass.classList.add('open');
  } else { pass.classList.remove('open'); pass.innerHTML = ''; }
  queueSave();
}

// ---- Saved games: the campaign is kept in this browser so it can be picked up later -------

const SAVE_KEY = 'theater-command-save-v1';
let saveTimer = 0;

function queueSave(): void {
  if (!ui.live || saveTimer) return;
  saveTimer = window.setTimeout(() => {
    saveTimer = 0;
    try { localStorage.setItem(SAVE_KEY, saveGame(g)); } catch { /* storage is off or full: the game still plays */ }
  }, 500);
}

function readSave(): string | null {
  try { return localStorage.getItem(SAVE_KEY); } catch { return null; }
}

const sumDivs = (cs: Corps[]): number => cs.reduce((s, c) => s + c.divs * c.str, 0);

function pickedCorps(): Corps[] {
  if (!humanUp(g)) return [];
  return g.corps.filter((c) => ui.picked.has(c.id) && canOrder(g, c) && c.ap > 0);
}

function railTargets(c: Corps): Region[] {
  return [...railReach(g, c)].map((id) => g.regions[id]);
}

function hintText(): string {
  if (!g.human) return '';
  if (!humanUp(g)) return '';
  if (ui.msg) return ui.msg;
  if (onEve(g)) return `The eve of war. ${natName(eveAttacker(g) ?? 'GER')} strikes before your first month comes round: place your army now, by road or by rail, give each army its orders for the day it is attacked, then finish. No month passes.`;
  if (ui.tab === 'build') return budgeted(g) ? 'The arsenal: pick Army, Air, Navy or Works, and click one of your own provinces on the map to raise, station or build there.' : 'Planning production: click one of your own home regions to raise troops or build works there.';
  if (ui.tab === 'research') return g.human && g.nations[g.human].lab ? 'Research: use − and + to move design teams between branches. The new pace starts next month.' : '';
  if (ui.tab === 'navy') {
    const f = myFleet();
    if (f && ui.rebase) return `New base for the ${f.name}: click any blue port. The fleet spends the month in harbour there.`;
    return f ? `Click a ringed sea to send the ${f.name} there.` : 'Pick one of your fleets in the list to give it orders.';
  }
  if (ui.tab === 'air') {
    const f = myAir();
    if (!f) return 'Pick one of your air fleets in the list to give it orders.';
    if (ui.airMode === 'base') return `New airfields for ${f.name}: click any blue region.`;
    if (ui.airMode === 'strike') return `Click a ringed sea to send ${f.name} against the shipping there.`;
    if (ui.airMode) return `${MISSION[ui.airMode].label}: click a red region to see the odds.`;
    return `The rings show how far ${f.name} reaches: fighters inside the gold ring, bombers inside the pale one.`;
  }
  if (ui.land) return byAir(pickedCorps()) ? 'Airborne assault: click a red coast to see the odds.' : 'Landing from the sea: click a red coast to see the odds.';
  if (ui.rail) return 'Rail move: click any blue region. It takes the whole month.';
  if (ui.ship) return 'Sea transport: click any blue port. It takes the whole month.';
  if (pickedCorps().length) return 'Right-click a green region to march there, or a red one to plan an attack. On a touch screen, tap it, or press and hold.';
  const ready = g.corps.some((c) => c.nat === g.human && c.ap > 0 && c.kind !== 'fort');
  return ready ? 'Click one of your armies (gold outline) to pick it up. End the month when you are done.' : 'All of your armies have used their month. End the month to continue.';
}

/** The treasury, always in sight on the top bar: what is in hand and what the month will add or take. Click for the ledger. */
function purse(me: string): string {
  const L = ledger(g, me);
  const red = L.treasury < 0;
  const tip = L.budget
    ? `Income ${cash(L.gross)} a month, upkeep ${cash(L.upkeepTotal)}. ${red ? 'The treasury is in debt: no replacements, no new orders, and the armies suffer for it. ' : ''}Click to open the ledger.`
    : `Income ${cash(L.gross)} a month. Click to open the ledger.`;
  return `<button class="purse${red ? ' debt' : ''}" data-act="ledger" title="${esc(tip)}"><span class="pl">Treasury</span><b>${inHand(L.treasury)}</b><i class="${L.net >= 0 ? 'up' : 'down'}">${cash(L.net, true)} a month</i></button>`;
}

function drawBar(): void {
  const me = g.human;
  const w = weather(g);
  const eve = onEve(g);
  const who = passing ? 'The other nations are moving' : me ? (g.nations[me].alive ? (eve ? `The eve of war: place the army of ${natName(me)}` : `Your orders: ${natName(me)}${isOpen(g) ? ' · free play' : ''}`) : `${natName(me)} has fallen`) : isOpen(g) ? 'Watching a free-play war: every nation decides for itself' : 'Watching history unfold: every nation on its historical plan';
  const when = eve ? '31 August 1939' : me || g.turn === 1 ? (g.turn === 1 && !me ? '1 September 1939' : date(g.turn)) : `End of ${date(g.turn - 1)}`;
  $('#when').innerHTML = `${when}<small>${esc(who)}${me ? ` · ${esc(w.text)}` : ''}</small>`;
  $('#purse').innerHTML = me && g.nations[me].alive && !eve ? purse(me) : '';
  let b = '';
  if (passing) b += `<button class="btn go" data-act="skip" title="Play the rest of the month at once">Skip ▸▸</button>`;
  else if (me && humanUp(g)) {
    if (declarable(g).length) b += `<button class="btn warn" data-act="declare">Declare war</button>`;
    b += `<button class="btn go" data-act="end">${eve ? 'Finish deployment ▸' : 'End month ▸'}</button>`;
  } else {
    b += `<button class="btn" data-act="auto">${ui.auto ? 'Pause' : 'Play ▸▸'}</button><button class="btn go" data-act="next">Next month ▸</button>`;
  }
  b += `<button class="btn" data-act="new">New game</button>`;
  $('#barbtns').innerHTML = b;
}

function drawTabs(): void {
  const newB = g.reports.length - ui.seenReports + Math.max(0, g.seaReports.length - ui.seenSea) + Math.max(0, g.airReports.length - ui.seenAir);
  const t = (id: string, label: string, n = 0) => `<button class="tab${ui.tab === id ? ' on' : ''}" data-act="tab" data-tab="${id}">${label}${n ? `<span class="n">${n}</span>` : ''}</button>`;
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  const build = me ? t('build', 'Build') : '';
  // Generals with no corps, when there is a corps for them: a decision waiting for the player.
  const idle = me && vacant(g, me).length ? waiting(g, me).length : 0;
  $('#tabs').innerHTML = t('region', 'Region') + build + t('research', 'Research') + t('generals', 'Generals', idle) + t('honours', 'Honours', unseen(g).length) + t('navy', 'Navy') + t('air', 'Air') + t('reel', 'News', ui.unread) + t('battles', 'Battles', newB) + t('powers', 'Powers');
}

/** Free play: nobody hands the player a plan. The note says so, and gives his own staff's reading of each front. */
function freeAdvice(me: string): string {
  const word = (r: number): string => (r >= 1.6 ? 'strongly in your favour' : r >= 1.15 ? 'in your favour' : r >= 0.85 ? 'about even' : r >= 0.55 ? 'against you' : 'heavily against you');
  const rows = fronts(g, me).sort((a, b) => b.ratio - a.ratio).slice(0, 4).map((f) => {
    const who = f.enemies.slice(0, 3).map(natName).join(', ') + (f.enemies.length > 3 ? ' and others' : '');
    return `<li>Against ${esc(who)}: the odds for an offensive are ${word(f.ratio)}, about ${f.ratio.toFixed(1)} to 1.</li>`;
  });
  const body = rows.length
    ? `Your staff's reading of the fronts, counting your attack against their defence near the line with forts, terrain and season weighed in:<ul>${rows.join('')}</ul>Other nations' staffs open an offensive at about 1 to 1 if they are bold and 2 to 1 if they are cautious.`
    : g.nations[me].atWar.length ? 'You are at war, but no enemy army stands on your borders.' : 'You are at peace. Build up, court your neighbours, and declare war when you choose, or never.';
  return `<div class="note"><b>Free play: there is no plan to follow.</b> You decide who to fight and when. So does every other nation: its staff weighs its fronts each month and makes up its own mind, so this war will not go the way history did. ${body}</div>`;
}

/** The staff's advice on the eve of war: what to do with the army before the blow falls, and a button that does the placing. */
function eveAdvice(): string {
  const plan = evePlan(g);
  if (!plan) return '';
  const where = plan.place.map(([name, n]) => `${n} corps at ${name}`);
  return `<div class="note eve"><b>Your staff's advice: ${esc(plan.title)}.</b><ul>${plan.advice.map((t) => `<li>${esc(t)}</li>`).join('')}</ul>
    <button class="btn small" data-act="eveplan" title="Sends the army by rail, strongest corps first: ${esc(where.join(', '))}. Corps you have already moved stay where you put them. The orders each army is given are left to you.">Place the army as the staff advises</button></div>`;
}

function advice(): string {
  const me = g.human;
  if (!me || !g.nations[me].alive) return '';
  if (onEve(g)) return eveAdvice();
  if (isOpen(g)) return freeAdvice(me);
  const list = (a?: string[]) => (a ?? []).slice(0, 7).join(', ');
  // Only what is still to do: wars not yet declared, objectives still in enemy hands.
  const todo = (a?: string[]) => (a ?? []).filter((nm) => { const r = g.byName[nm]; return r && atWar(g, me, r.ctrl); });
  // The plan's standing orders beyond its objectives: forces to gather or keep in place, and sectors left to allies.
  // An army overseas has no railway and a port to fall back on, so its orders are worded for that.
  const orders = (ph: NonNullable<ReturnType<typeof phaseFor>>, base?: string | null): string => {
    const away = base !== undefined;
    const out: string[] = [];
    const held = (nm: string): boolean => { const r = g.byName[nm]; return !!r && allied(g, me, r.ctrl); };
    const fast = Object.entries(ph.concentrate ?? {}).filter(([nm]) => held(nm));
    if (fast.length) out.push(away
      ? `Bring your tanks and lorried troops up for the stroke to come: ${esc(fast.map(([nm, k]) => `${k} corps at ${nm}`).join(', '))}. March infantry out if there is no room.`
      : `Take the tanks out of the quiet parts of the line and gather them by rail for the summer: ${esc(fast.map(([nm, k]) => `${k} corps at ${nm}`).join(', '))}.`);
    const keep = ph.posture === 'redeploy' ? [] : Object.entries(ph.staging ?? {}).filter(([nm]) => held(nm));
    if (keep.length) {
      const each = keep.map(([nm, k]) => `${k} corps at ${nm}`);
      out.push(`Keep ${esc(each.length <= 2 ? each.join(' and ') : `${each.slice(0, -1).join(', ')} and ${each[each.length - 1]}`)}.`);
    }
    const lent = Object.entries(ph.entrust ?? {}).flatMap(([ally, places]) => places.filter((nm) => held(nm) && g.byName[nm].units.filter((u) => u.nat === ally).length >= 2));
    if (lent.length) out.push(`Your allies now hold ${esc(list(lent))}: history left one corps to stiffen each and sent the rest on.`);
    const out2 = (ph.abandon ?? []).filter((nm) => held(nm) && g.byName[nm].units.some((u) => u.nat === me && u.kind !== 'fort'));
    if (out2.length) out.push(`Give up ${esc(list(out2))}: march the corps there back toward ${away && base ? esc(base) : 'home'} before the road behind them is cut.`);
    const dear = Object.entries(ph.oddsFor ?? {}).filter(([nm, o]) => o < (ph.minOdds ?? 1.3) && !held(nm)).map(([nm]) => nm);
    if (dear.length) out.push(`History threw its army at ${esc(list(dear))} against the odds. You need not.`);
    if (ph.posture === 'attack' && ph.renew === false) out.push('History made this assault once and did not send the army in again when it was held.');
    return out.length ? ` ${out.join(' ')}` : '';
  };
  const main = (): string => {
    const ph = phaseFor(g, me);
    if (!ph) return '';
    if (ph.posture === 'attack') {
      const declare = (ph.declare ?? []).filter((e) => g.nations[e].alive && !atWar(g, me, e));
      // Once the wars the plan calls for are on, only the objectives still in enemy hands are left to name.
      const left = declare.length ? ph.objectives ?? [] : todo(ph.objectives);
      if (!left.length && !declare.length) return `<div class="note"><b>History's plan this month${ph.name ? `: ${esc(ph.name)}` : ''}.</b> The plan at home is carried out. Hold what you have.${orders(ph)}</div>`;
      const aims = left.length ? `Objectives, in order: ${esc(list(left))}.` : '';
      return `<div class="note"><b>History's plan this month${ph.name ? `: ${esc(ph.name)}` : ''}.</b> ${declare.length ? `Declare war on ${declare.map(natName).join(', ')}. ` : ''}${aims}${ph.landings?.length ? ` From the sea: ${esc(ph.landings.map((l) => `${l.corps} to ${l.to[0]}`).join(', '))}.` : ''}${orders(ph)} You are free to do something else.</div>`;
    }
    if (ph.posture === 'redeploy') return `<div class="note"><b>History's plan this month.</b> Move the army west by rail and wait for spring. Assembly areas: ${esc(list(ph.assembly))}.</div>`;
    return `<div class="note"><b>History's plan this month${ph.name ? `: ${esc(ph.name)}` : ''}.</b> ${ph.quiet ? 'Stand on the defensive and risk nothing: the army is being saved for a later stroke.' : 'Hold the line and keep a reserve near the capital.'}${ph.landings?.length ? ` By sea: ${esc(ph.landings.map((l) => `${l.corps} to ${l.to[0]}`).join(', '))}.` : ''}${orders(ph)}</div>`;
  };
  // An army overseas has a plan of its own.
  const away = theatrePhases(g, me).map(({ theatre, phase: ph, base }) => {
    const head = `<b>History's plan in ${esc(theatre)}${ph?.name ? `: ${esc(ph.name)}` : ''}.</b>`;
    if (ph?.posture === 'attack') {
      const declare = (ph.declare ?? []).filter((e) => g.nations[e].alive && !atWar(g, me, e));
      // Until the wars the plan calls for are declared, every objective is still to be won.
      const left = declare.length ? (ph.objectives ?? []).filter((nm) => { const r = g.byName[nm]; return r && !allied(g, me, r.ctrl); }) : todo(ph.objectives);
      const drops = (ph.landings ?? []).filter((l) => l.to.some((nm) => { const r = g.byName[nm]; return r && !allied(g, me, r.ctrl); }));
      return `<div class="note">${head} ${declare.length ? `Declare war on ${declare.map(natName).join(', ')}. ` : ''}${left.length ? `Objectives, in order: ${esc(list(left))}.` : 'Its objectives are taken.'}${ph.siege?.length ? ` Do not storm ${esc(list(ph.siege))}: starve it out.` : ''}${drops.length ? ` Across the water: ${esc(drops.map((l) => `${l.corps}${l.with?.length ? ` with ${list(l.with)}` : ''} to ${l.to[0]}${l.to.length > 1 ? ` (or ${l.to.slice(1).join(' or ')} if that shore is too strongly held)` : ''}`).join('; '))}.` : ''}${orders(ph, base)} You are free to do something else.</div>`;
    }
    if (ph?.withdraw) {
      const lost = ph.withdraw.when.filter((nm) => { const r = g.byName[nm]; return r && atWar(g, me, r.ctrl); });
      return `<div class="note">${head} ${lost.length ? `${esc(list(lost))} has fallen. Get the expedition to a port and lift it off before it is lost.` : `Stand beside your ally, and be ready to leave: if ${esc(list(ph.withdraw.when))} falls, get the expedition to a port and lift it off.`}</div>`;
    }
    return `<div class="note">${head} ${ph?.quiet ? 'Stand on the defensive and risk nothing: the army is being saved for a later stroke.' : 'Stand on the defensive and build up.'}${ph ? orders(ph, base) : ''}</div>`;
  }).join('');
  return main() + away;
}

function scopeNote(): string {
  if (isOpen(g)) return g.human ? '' : `<div class="note"><b>Free play.</b> No nation is following history's plan. Each one's staff weighs its fronts every month and decides for itself whether to attack, hold or wait, so every war you watch comes out differently.</div>`;
  if (g.turn < STORY_THINS) return '';
  if (g.turn < STORY_ENDS) return `<div class="note"><b>The last winter is still being written.</b> History's plans cover every front to January 1945: Italy to the Gothic Line, the West to the Ardennes, and the eastern front from Leningrad to the siege of Budapest. After that the computer's nations only hold what they have.</div>`;
  return `<div class="note"><b>This is as far as the prototype's story goes.</b> History's plans cover the land war from September 1939 to the end of 1943, and Italy and the West to the end of 1944. You can play on, but the computer's nations will only hold what they have. Shipbuilding and the later campaigns come in later phases.</div>`;
}

function corpsRow(c: Corps, mine: boolean): string {
  const k = KIND[c.kind];
  const pick = ui.picked.has(c.id);
  const tags: string[] = [];
  if (!c.supplied) tags.push('<span class="tag bad">Cut off</span>');
  if (c.fought) tags.push('<span class="tag">Fought this month</span>');
  const line = lineStrength(g, c);
  if (line.mul < 1) tags.push(inDesert(g.regions[c.reg])
    ? `<span class="tag bad" title="Counted from the nearest working port, a step for every region of desert. Take a port nearer the front, or fall back toward one.">${Math.round(line.length)} regions from port: attacks at ${Math.round(line.mul * 100)}%</span>`
    : `<span class="tag bad">Long supply line: attacks at ${Math.round(line.mul * 100)}%</span>`);
  if (frozen(g, c.nat, g.regions[c.reg])) tags.push('<span class="tag bad">No winter kit</span>');
  const gen = generalOf(g, c);
  for (const t of gen?.traits ?? []) {
    const d = COMMAND.traits[t];
    if (d) tags.push(`<span class="tag ${d.good ? 'good' : 'bad'}" title="${esc(d.effect)}">${esc(d.label)}</span>`);
  }
  tags.push(...storyTags(c));
  if (c.airborne) tags.push(`<span class="tag good" title="Parachute and glider troops. Once a month they can be flown from a friendly coast to an enemy one across a single sea, with no ships and whoever holds the water. They fight there at ${Math.round(NAVY.landing.airborne * 100)}%, without heavy weapons, and not under a sky the enemy's fighters hold.">Airborne</span>`);
  if (c.hold && !mine) tags.push('<span class="tag">Garrison</span>');
  if (c.yields && c.nat === g.human) tags.push(`<span class="tag" title="${esc(STANCE_BACK)}">Falls back if attacked</span>`);
  if (c.dug) tags.push(`<span class="tag good" title="${esc(STANCE_DIG)}">Dug in</span>`);
  else if (c.digs && c.nat === g.human) tags.push(`<span class="tag" title="${esc(STANCE_DIG)}">Digging in: ready next month if it stays put</span>`);
  const pips = c.kind === 'fort' ? '' : `<span class="pips" title="Moves left this month">${Array.from({ length: Math.max(c.apMax, c.ap) }, (_, i) => `<i class="${i < c.ap ? '' : 'off'}"></i>`).join('')}</span>`;
  const el = mine && c.kind !== 'fort' ? 'button' : 'div';
  return `<div class="unit"><${el} class="corps${pick ? ' pick' : ''}" ${el === 'button' ? `data-act="pick" data-id="${c.id}"` : ''}>
    ${icon(c)}<span class="nm">${esc(c.name)}${c.nick ? ` <i class="nick">${esc(c.nick)}</i>` : ''}${gen ? ` <span style="font-weight:400;color:var(--ink-2)">· ${esc(gen.name)} <span class="lvl" title="Level ${gen.level} general">${gen.level}</span></span>` : ''}</span>${pips}
    <span class="meta">${g.nations[c.nat].adj} ${k.label.toLowerCase()} · ${c.divs} division${c.divs > 1 ? 's' : ''} · ${num(men(c))} men</span>
    <span class="bars"><span>Strength</span><span class="meter"><i class="${c.str < 0.5 ? 'low' : ''}" style="width:${Math.round(c.str * 100)}%"></i></span><span>Cohesion</span><span class="meter"><i class="org${c.org < 0.4 ? ' low' : ''}" style="width:${Math.round(c.org * 100)}%"></i></span></span>
    ${tags.length ? `<span class="tags">${tags.join('')}</span>` : ''}</${el}>${recStrip('corps', c)}</div>`;
}

// ---- The army tray: the general and his units, laid out as cards along the foot of the map ----

const STANCE_STAND = 'Stand and fight wherever they are attacked.';
const STANCE_DIG = 'Entrench. An army that spends a whole month where it stands, without marching or attacking, fights from trenches from then on and defends 25% stronger. It keeps them for as long as it stays put, and loses them the moment it leaves the ground.';
const STANCE_BACK = 'Give ground instead of fighting when the enemy comes on with odds of 1.2 to 1 or better, which is what it takes to carry a position. The troops slip away whole to the ground behind them, at a cost in cohesion and the region itself. Caught again in the same month, they stand. Fortress troops always stand, and nobody gives up their own capital without a fight.';

/** The army the tray shows for the selected region: the player's own if he has one there, otherwise the largest. */
function armyShown(): { reg: number; nat: string } | null {
  if (ui.sel < 0 || ui.tab !== 'region') return null;
  const r = g.regions[ui.sel];
  if (!r.units.length) return null;
  const size = new Map<string, number>();
  for (const c of r.units) size.set(c.nat, (size.get(c.nat) ?? 0) + c.divs * c.str);
  const me = g.human;
  return { reg: r.id, nat: me && size.has(me) ? me : [...size].sort((a, b) => b[1] - a[1])[0][0] };
}

/** One corps of the army as a card. The player's own are buttons: a click leaves the corps behind or takes it along. */
function unitCard(c: Corps, mine: boolean): string {
  const can = mine && c.kind !== 'fort';
  const el = can ? 'button' : 'div';
  const pips = c.kind === 'fort' ? '' : `<span class="pips" title="Moves left this month">${Array.from({ length: Math.max(c.apMax, c.ap) }, (_, i) => `<i class="${i < c.ap ? '' : 'off'}"></i>`).join('')}</span>`;
  const gen = generalOf(g, c);
  const flags = `${c.supplied ? '' : '<span class="tag bad">Cut off</span>'}${c.fought ? '<span class="tag">Fought</span>' : ''}${c.yields && c.nat === g.human ? '<span class="tag" title="Under orders to give ground when the enemy attacks in strength">Falls back</span>' : ''}${c.dug ? '<span class="tag good" title="Entrenched: defends 25% stronger until it leaves this ground">Dug in</span>' : c.digs && c.nat === g.human ? '<span class="tag" title="Under orders to entrench: the trenches are ready next month if it stays put">Digging</span>' : ''}`;
  return `<${el} class="ucard${ui.picked.has(c.id) ? ' pick' : ''}${can && c.ap <= 0 ? ' spent' : ''}" ${can ? `data-act="pick" data-id="${c.id}"` : ''} title="${esc(c.name)}${c.nick ? `, ${esc(c.nick)}` : ''}${gen ? ` · ${esc(gen.name)}` : ''}">
    <span class="utop">${icon(c)}${pips}</span><b>${esc(c.name)}</b>
    <span class="um">${c.divs} ${KIND[c.kind].label.toLowerCase()} div${c.divs > 1 ? 's' : ''}${gen ? ` · ${esc(surname(gen.name))}` : ''}</span>
    <span class="um vet">${esc(grade(c.xp))}${c.leave !== undefined ? ' · at home' : ''} ${ribbons(c, 5)}</span>
    <span class="meter" title="Strength ${Math.round(c.str * 100)}%"><i class="${c.str < 0.5 ? 'low' : ''}" style="width:${Math.round(c.str * 100)}%"></i></span>
    <span class="meter" title="Cohesion ${Math.round(c.org * 100)}%"><i class="org${c.org < 0.4 ? ' low' : ''}" style="width:${Math.round(c.org * 100)}%"></i></span>${flags ? `<span class="tags">${flags}</span>` : ''}</${el}>`;
}

function drawTray(): void {
  const el = $('#tray');
  const a = trayArmy;
  if (!a) { el.classList.remove('open'); el.innerHTML = ''; return; }
  const r = g.regions[a.reg];
  const last = (c: Corps): number => (c.kind === 'fort' ? 1 : 0);
  const cs = r.units.filter((c) => c.nat === a.nat).sort((x, y) => last(x) - last(y) || y.divs * y.str - x.divs * x.str);
  const mine = a.nat === g.human && humanUp(g);
  const who = leaderOf(cs);
  const n = g.nations[a.nat];
  const initials = who ? initialsOf(who.name) : '';
  const others = [...new Set(r.units.filter((c) => c.nat !== a.nat).map((c) => c.nat))];
  const head = who
    ? `<button class="tgen" data-act="general" data-id="${who.id}" title="Open this general's record"><span class="seal" style="background:${n.color}">${esc(initials)}</span><span class="grow"><b>${esc(who.name)}</b><small>${esc(n.adj)} army at ${esc(r.name)} · ${esc(rankOf(who))}, level ${who.level}</small>${who.traits.length ? `<span class="tags">${traitTags(who)}</span>` : ''}</span></button>`
    : `<div class="tgen"><span class="seal none" style="background:${n.color}"></span><span class="grow"><b>${esc(n.adj)} army at ${esc(r.name)}</b><small>No general in command</small></span></div>`;
  const tally = `<div class="ttally"><b>${num(sumDivs(cs))}</b><small>divisions</small><b>${Math.round(fitness(cs) * 100)}%</b><small>strength</small>${others.length ? `<small class="also">Also here: ${others.map((o) => esc(g.nations[o].adj)).join(', ')}</small>` : ''}</div>`;
  // The standing order for the day the enemy comes: the whole army in the tray takes it together.
  const field = cs.filter((c) => c.kind !== 'fort');
  const under = new Set(field.map(postureOf));
  const all = (p: Posture): string => (under.size === 1 && under.has(p) ? 'on' : '');
  const stance = mine && field.length
    ? `<div class="tstance" role="group" aria-label="Orders if this army is attacked"><small>If attacked${under.size > 1 ? ' · mixed' : ''}</small><button class="${all('stand')}" data-act="ifattacked" data-v="stand" title="${esc(STANCE_STAND)}">Stand</button><button class="${all('dig')}" data-act="ifattacked" data-v="dig" title="${esc(STANCE_DIG)}">Dig in</button><button class="${all('back')}" data-act="ifattacked" data-v="back" title="${esc(STANCE_BACK)}">Fall back</button></div>`
    : '';
  el.innerHTML = `${head}${tally}${stance}<div class="cards">${cs.map((c) => unitCard(c, mine)).join('')}</div>`;
  el.classList.add('open');
}

// ---- Orders on the map: the road an army would take, and the march along it ------------------

/** The road the picked army would take to a region: the centres of the regions on the way, from where it stands. */
function routeTo(id: number): { pts: [number, number][]; cost: number; ap: number; attack: boolean } | null {
  let best: { pts: [number, number][]; cost: number; ap: number; attack: boolean } | null = null;
  for (const rc of reaches) {
    const end = rc.map.get(id);
    if (!end || id === rc.from) continue;
    const ids: number[] = [];
    for (let at = id; at >= 0 && at !== rc.from; at = rc.map.get(at)?.prev ?? -1) ids.unshift(at);
    ids.unshift(rc.from);
    // The slowest corps sets the pace: its road is the one shown.
    if (!best || end.cost > best.cost) best = { pts: ids.map((k) => [g.regions[k].x, g.regions[k].y]), cost: end.cost, ap: rc.ap, attack: end.attack };
  }
  return best;
}

/** Draws that road on the map while the cursor is over a region the picked army can reach. */
function drawRoute(id: number): void {
  if (id === routeShown) return;
  routeShown = id;
  const way = id >= 0 && !ui.rail && !ui.ship && !ui.land && ui.tab === 'region' ? routeTo(id) : null;
  if (!way) { $('#route').innerHTML = ''; return; }
  const [ax, ay] = way.pts[way.pts.length - 2], [bx, by] = way.pts[way.pts.length - 1];
  const turn = Math.atan2(by - ay, bx - ax) * 180 / Math.PI;
  const cls = way.attack ? ' strike' : '';
  $('#route').innerHTML = `<path class="road under" d="M${way.pts.map((p) => `${p[0]} ${p[1]}`).join('L')}"/><path class="road${cls}" d="M${way.pts.map((p) => `${p[0]} ${p[1]}`).join('L')}"/>
    <path class="roadhead${cls}" transform="translate(${bx} ${by}) rotate(${turn.toFixed(1)})" d="M4 0L-5 -4.6L-2.6 0L-5 4.6Z"/>`;
}

/** Slides a copy of the piece along the road it took, so the eye can follow the move. */
function march(pts: [number, number][], nat: string, kind: Kind, to: number): void {
  const ghost = $('#ghost');
  const done = (): void => {
    ghost.innerHTML = '';
    marchHide = null;
    const real = document.querySelector<SVGGElement>(`#units .army[data-reg="${to}"][data-nat="${nat}"]`);
    if (real) real.style.visibility = '';
  };
  if (pts.length < 2 || typeof requestAnimationFrame !== 'function') { done(); return; }
  ghost.innerHTML = `<g class="ctr army chosen"><ellipse class="base" cx="11" cy="16.2" rx="13" ry="3.4" fill="${g.nations[nat].color}"/><path class="pole" d="M-.6 -1.5V16.2"/>${standard(nat, kind)}</g>`;
  const piece = ghost.firstElementChild as SVGGElement;
  const legs = pts.length - 1, total = Math.min(720, 190 * legs + 140);
  const t0 = performance.now();
  const token = ++marchToken;
  const step = (now: number): void => {
    if (token !== marchToken) return;
    const f = Math.min(1, (now - t0) / total);
    const e = f < 0.5 ? 2 * f * f : 1 - 2 * (1 - f) * (1 - f);
    const at = e * legs, i = Math.min(legs - 1, Math.floor(at)), u = at - i;
    piece.setAttribute('transform', `translate(${(pts[i][0] + (pts[i + 1][0] - pts[i][0]) * u - 11).toFixed(1)} ${(pts[i][1] + (pts[i + 1][1] - pts[i][1]) * u - 7).toFixed(1)})`);
    if (f < 1) requestAnimationFrame(step); else done();
  };
  step(t0);
  requestAnimationFrame(step);
}

/**
 * Gives the picked corps their order at a region: a march there, or the plan of an attack on it.
 * False if there is no such order to give, and the click means something else.
 */
function orderAt(id: number): boolean {
  const picked = pickedCorps();
  if (!picked.length || id === ui.sel || ui.land || ui.rail || ui.ship || ui.tab === 'build' || ui.tab === 'navy' || ui.tab === 'air') return false;
  const r = g.regions[id];
  if (hostileUnits(g, r, g.human!).length) {
    const near = picked.filter((c) => g.regions[c.reg].nb.includes(id));
    if (!near.length) return false;
    openModal({ kind: 'attack', target: id, chosen: new Set(near.map((c) => c.id)) });
    return true;
  }
  if (!atWar(g, g.human!, r.ctrl) && !allied(g, g.human!, r.ctrl)) return false;
  const h = g.headlines.length;
  const before = g.regions.map((x) => x.ctrl);
  const way = routeTo(id);
  const lead = [...picked].sort((a, b) => KIND[b.kind].atk * b.divs - KIND[a.kind].atk * a.divs)[0];
  const nat = lead.nat, kind = lead.kind;
  let moved = 0;
  for (const c of picked) if (orderMove(g, c, id) > 0) moved++;
  if (!moved) return false;
  ui.unread += g.headlines.length - h;
  ui.flash = g.regions.filter((x, i) => x.ctrl !== before[i]).map((x) => x.id);
  ui.sel = id;
  ui.picked = new Set(picked.filter((c) => c.reg === id && c.ap > 0).map((c) => c.id));
  ui.tab = 'region';
  const arrived = picked.some((c) => c.reg === id);
  marchHide = arrived && way ? { reg: id, nat } : null;
  refresh();
  if (arrived && way) march(way.pts, nat, kind, id);
  return true;
}

let paneKey = '';

function drawPane(): void {
  const pane = $('#pane');
  // A new subject starts at the top of the panel; a redraw of the same one keeps its place.
  const key = `${ui.tab}:${ui.tab === 'region' ? `${ui.sel}:${ui.sea}` : ''}`;
  if (key !== paneKey) { paneKey = key; pane.scrollTop = 0; }
  if (ui.tab === 'build' && !(g.human && g.nations[g.human].alive)) ui.tab = 'region';
  if (ui.tab === 'region') pane.innerHTML = regionPane();
  else if (ui.tab === 'build') pane.innerHTML = buildPane();
  else if (ui.tab === 'research') pane.innerHTML = researchPane();
  else if (ui.tab === 'generals') pane.innerHTML = generalsPane();
  else if (ui.tab === 'honours') pane.innerHTML = honoursPane();
  else if (ui.tab === 'navy') pane.innerHTML = navyPane();
  else if (ui.tab === 'air') pane.innerHTML = airPane();
  else if (ui.tab === 'reel') pane.innerHTML = reelPane();
  else if (ui.tab === 'battles') pane.innerHTML = battlesPane();
  else pane.innerHTML = powersPane();
}

// ---- What wants the player's attention this month --------------------------------------------

interface Alert { kind: string; text: string; /** Regions to visit in turn, for an alert about places. */ regs?: number[]; tab?: string }

function alertList(): Alert[] {
  const me = g.human;
  if (!me || !humanUp(g) || onEve(g) || passing) return [];
  const n = g.nations[me];
  const out: Alert[] = [];
  const cut = [...new Set(g.corps.filter((c) => c.nat === me && !c.dead && !c.supplied).map((c) => c.reg))];
  if (cut.length) out.push({ kind: 'cut', text: `${cut.length} ${cut.length === 1 ? 'army is' : 'armies are'} cut off from supply`, regs: cut });
  if (n.atWar.length) {
    // An army under orders to dig in is doing its work by staying where it is.
    const idle = g.regions.filter((r) => r.units.some((c) => canOrder(g, c) && c.ap >= c.apMax && !c.fought && !c.digs) && r.nb.some((j) => atWar(g, me, g.regions[j].ctrl))).map((r) => r.id);
    if (idle.length) out.push({ kind: 'front', text: `${idle.length} ${idle.length === 1 ? 'army' : 'armies'} at the front ${idle.length === 1 ? 'has' : 'have'} not moved`, regs: idle });
  }
  const cheapest = Math.min(...Object.values(PRODUCTION.builds).map((b) => b.cost));
  if (n.pp < 0) out.push({ kind: 'debt', text: 'The treasury is in debt: open the ledger' });
  if (Math.floor(n.pp) >= cheapest) out.push({ kind: 'build', text: `${money(n.pp)} in the treasury to spend`, tab: 'build' });
  const stars = g.generals.filter((x) => x.nat === me && x.points > 0 && serving(x)).length;
  if (stars) out.push({ kind: 'generals', text: `${stars} general${stars === 1 ? ' has' : 's have'} skill points to spend`, tab: 'generals' });
  if (n.lab && teamsFree(n.lab) > 0) out.push({ kind: 'research', text: 'A design team has no work', tab: 'research' });
  const cited = unseen(g).length;
  if (cited) out.push({ kind: 'honours', text: `${cited} honour${cited === 1 ? '' : 's'} earned: read the citation${cited === 1 ? '' : 's'}` });
  return out;
}

const alertButtons = (): string => alertList().map((a) => `<button class="alert ${a.kind}" data-act="alert" data-kind="${a.kind}">${esc(a.text)}</button>`).join('');

/** Goes where an alert points: the next place on its list, or the tab that deals with it. */
function followAlert(kind: string): void {
  const a = alertList().find((x) => x.kind === kind);
  if (!a) return;
  if (kind === 'honours') { const next = unseen(g)[0]; if (next) openModal({ kind: 'award', id: next.id }); return; }
  if (kind === 'debt') { openModal({ kind: 'ledger' }); return; }
  if (a.tab) { ui.tab = a.tab as typeof ui.tab; ui.rail = ui.ship = ui.land = ui.rebase = false; refresh(); return; }
  const regs = a.regs ?? [];
  if (!regs.length) return;
  const at = regs.indexOf(ui.sel);
  const id = regs[(at + 1) % regs.length];
  ui.tab = 'region';
  selectRegion(id);
  refresh();
  const svg = $('#map');
  const tall = svg.clientHeight / Math.max(1, svg.clientWidth);
  const w = Math.min(ui.view.w, 700);
  glide({ x: g.regions[id].x - w / 2, y: g.regions[id].y - (w * tall) / 2, w });
}

/** The player's short and long campaign, with what each one asks for ticked off as it is done. */
function goalsBlock(): string {
  const me = g.human;
  if (!me || !GOALS[me]) return '';
  const rows = GOAL_KEYS.map((key) => {
    const goal = GOALS[me]![key];
    const st = goalState(g, key);
    if (!goal || st === 'out') return '';
    const when = goalWhen(goal, date);
    return `<div class="goal ${st}"><span class="gk">${key === 'short' ? 'Short campaign' : 'Long campaign'} · ${when}</span><b>${esc(goal.title)}${st === 'won' ? ' <span class="tag good">Won</span>' : st === 'lost' ? ' <span class="tag bad">Lost</span>' : ''}</b><small>${esc(goal.text)}</small><span class="gchecks">${goalChecks(g, me, goal).map((c) => `<span class="${c.done ? 'done' : ''}">${c.done ? '✓' : '○'} ${esc(c.label)}</span>`).join('')}</span></div>`;
  }).join('');
  return rows ? `<h3>Campaign goals</h3>${rows}` : '';
}

// ---- The province: what a region holds, laid out as slots, and what can be raised or built there ----

const GLYPH = {
  factory: '<path d="M1.5 16.5V8l5.5 3V8l5.5 3V8l5 2.6V2.5h3.5v14Z"/>',
  works: '<path d="M3 16.5V5h4v3.2h3V5h4v3.2h3V5h4v11.5Z"/>',
  harbour: '<path class="line" d="M12 6.2V16M7.5 9.3h9M4.5 11.5c.8 3.3 4 4.6 7.5 4.6s6.7-1.3 7.5-4.6"/><circle class="line" cx="12" cy="4.2" r="1.9"/>',
};
/** An anti-aircraft gun on its mount, barrel raised. */
const GUN = '<path d="M4.5 16.5h15v-2.2h-4.6l-1-2.7 7.1-7.5-1.5-1.5-7.7 6.9-2.6-.5-1.6 5.3H4.5Z"/>';
const slotIcon = (body: string, box = '0 0 24 18'): string => `<svg class="sglyph" width="30" height="22" viewBox="${box}">${body}</svg>`;
const pipsOf = (have: number, coming: number, of: number): string => `<span class="lvpips">${Array.from({ length: of }, (_, i) => `<i class="${i < have ? 'on' : i < have + coming ? 'due' : ''}"></i>`).join('')}</span>`;

// ---- The arsenal as the player sees it: one drawing of an offer for the Build tab, one for the province ----

const BOMBER = 'M0-9L1.7-4 11.5-.6V1.6L1.7.9 1.2 6.2 4 8.2V9.4L0 8.4-4 9.4V8.2L-1.2 6.2-1.7.9-11.5 1.6V-.6L-1.7-4ZM-6.6-3.6h1.7v3.2h-1.7ZM4.9-3.6h1.7v3.2H4.9Z';
const LINER = 'M-12.5.6L-10.5 4H10.5L12.5.6H8V-2.2H-7.5V.6ZM-3.6-5.8h2.3v3.6h-2.3ZM1.3-5.8h2.3v3.6H1.3Z';
const FLATTOP = 'M-13-.8L-11.2 4H10.8L13-.8ZM2.6-4h3.8v3.2H2.6Z';
const HULL_OF: Record<string, [string, number]> = { dd: [HULL, 0.78], ca: [HULL, 0.9], bb: [HULL, 1.08], cv: [FLATTOP, 1], ss: [SUB, 0.9], tp: [LINER, 0.95] };
const workGlyph = (item: string): [string, string] => (item === 'factory' ? [GLYPH.factory, '0 0 24 18'] : item === 'yard' ? [GLYPH.harbour, '0 0 24 18'] : item === 'aa' ? [GUN, '0 0 24 18'] : item === 'field' ? [`<path d="${PLANE}"/>`, '-10.5 -9.5 21 19.5'] : [GLYPH.works, '0 0 24 18']);
const months = (n: number): string => `${n} month${n === 1 ? '' : 's'}`;

function offerIcon(o: Offer): string {
  const me = g.human!;
  const c = g.nations[me].color;
  const [group, item] = o.key.split(':');
  if (group === 'corps') return icon({ nat: me, kind: item } as Corps);
  // Aircraft and ships are drawn like the corps: a plate in the nation's colour with the silhouette on it.
  const plate = (d: string, k: number, dy = 0): string => `<svg width="34" height="24" viewBox="-1 -1 26 19" class="ctr"><rect class="box" x="0" y="0" width="24" height="15" rx="1.5" fill="${c}"/><path class="symf" d="${d}" transform="translate(12 ${7.5 + dy}) scale(${k})"/></svg>`;
  if (group === 'air') return plate(item === 'bmr' ? BOMBER : PLANE, item === 'bmr' ? 0.68 : 0.7, -0.2);
  if (group === 'ship') { const [d, k] = HULL_OF[item]; return plate(d, k * 0.8, 0.6); }
  const [body, box] = workGlyph(item);
  return `<svg class="wglyph" width="34" height="24" viewBox="${box}">${body}</svg>`;
}

const offerLabel = (o: Offer): string => (o.key === 'corps:arm' ? `${PRODUCTION.armourName[g.human!] ?? PRODUCTION.armourName._default} corps` : o.label);
/** What an offer would cost to keep each month as things stand: forces cost half at peace, works the same always. */
const offerKeep = (o: Offer): number => o.upkeep * (o.group !== 'works' && g.human && !g.nations[g.human].atWar.length ? BUDGET.upkeep.peace : 1);

/** An offer as a full-width row in the Build tab: what it is, what it does, where it goes, and why not when it cannot be ordered. */
function offerButton(o: Offer, quiet = false): string {
  const ok = humanUp(g) && o.why === null;
  const keep = offerKeep(o);
  const line = [o.size, `ready in ${months(o.months)}`, keep > 0 ? `upkeep ${cash(keep)} a month` : ''].filter(Boolean).join(' · ');
  const facts = line[0].toUpperCase() + line.slice(1);
  return `<button class="buy offer" data-act="arsenal" data-key="${o.key}" ${ok ? '' : 'disabled'}>${offerIcon(o)}<span class="grow"><b>${esc(offerLabel(o))}</b><small>${facts}</small><small>${esc(o.blurb)}</small>${o.note ? `<small class="where">${esc(o.note)}</small>` : ''}${o.why && !o.poor && !quiet ? `<small class="whyno">${esc(o.why)}</small>` : ''}</span><span class="cost${o.poor ? ' short' : ''}">${money(o.cost)}</span></button>`;
}

/** An offer as a small card in a province: it can be ordered there only if this is where it would go. */
function offerCard(o: Offer, r: Region): string {
  const here = o.at === r.id;
  const ok = humanUp(g) && here && o.why === null;
  const [group, item] = o.key.split(':');
  const need = here ? '' : group === 'corps' ? `Needs ${needsOf(g, item)} factor${needsOf(g, item) === 1 ? 'y' : 'ies'} here` : group === 'ship' ? `Needs a level ${ARSENAL.ships[item as Hull].yard} shipyard` : o.at < 0 ? 'No aircraft industry' : 'No airfields here';
  const keep = offerKeep(o);
  const count = group === 'ship' && item !== 'tp' ? ARSENAL.ships[item as Hull].count : 0;
  // A card is narrow: the count goes in as a multiple, and the title on hover says the rest.
  const size = group === 'corps' ? `${specOf(g, item)!.divs} divs` : group === 'ship' ? (count > 1 ? `×${count}` : '') : `×${ARSENAL.air[item as 'ftr' | 'bmr'].count}`;
  const name = offerLabel(o).replace(/ corps$/i, '').replace(/ (wing|flotilla)$/i, 's').replace(/^Aircraft carrier$/, 'Carrier');
  const why = !here ? need : o.why && !o.poor ? o.why : '';
  return `<button class="rcard" data-act="arsenal" data-key="${o.key}" ${ok ? '' : 'disabled'} title="${esc(offerLabel(o))}, ${esc(o.size)}: ${esc(o.blurb)}${keep > 0 ? ` Upkeep ${cash(keep)} a month.` : ''}${why ? ` ${esc(why)}.` : ''}">${offerIcon(o)}<b>${esc(name)}</b><small>${why ? esc(why) : [size, months(o.months)].filter(Boolean).join(' · ')}</small><span class="cost${here && !o.poor && o.why === null ? '' : ' short'}">${money(o.cost)}</span></button>`;
}

/** A region's holdings as slots: industry, works, harbour, airfields and air defence. In the grand campaign each slot the player can build on is a button. */
function provinceSlots(r: Region): string {
  const me = g.human;
  const mine = !!me && humanUp(g) && isHome(g, me, r);
  const occupied = r.ctrl !== r.owner;
  const facs = r.fac ? `${r.fac} factor${r.fac === 1 ? 'y' : 'ies'}${occupied ? `, worth ${Math.round(PRODUCTION.occupiedIncome * 100)}% under occupation` : ''}` : 'No industry';
  const lv = r.fort?.lv ?? 0;
  const queued = me ? g.nations[me].queue.filter((b) => b.what === 'fort' && b.region === r.id).length : 0;
  const state = `${lv ? `Level ${lv}` : 'No works'}${queued ? `, level ${lv + queued} building` : ''}`;
  const sea = r.port ? 'A port: troops and supplies can come and go by sea' : r.seas.length ? 'A coast with no port' : 'Inland';
  const wings = g.airFleets.filter((f) => f.base === r.id && f.ftr + f.bmr > 0);
  const flying = wings.length ? `${wings.length} air fleet${wings.length > 1 ? 's' : ''}, ${num(wings.reduce((t, f) => t + f.ftr + f.bmr, 0))} aircraft` : 'No air fleet based here';
  const plane = slotIcon(`<path d="${PLANE}"/>`, '-10.5 -9.5 21 19.5');
  if (!budgeted(g)) {
    const industry = `<div class="slot${r.fac ? ' has' : ''}">${slotIcon(GLYPH.factory)}<b>Industry</b><small>${facs}</small></div>`;
    const cost = mine ? fortCost(g, me!, r) : null;
    const have = me ? Math.floor(g.nations[me].pp) : 0;
    const works = cost !== null
      ? `<button class="slot act${lv ? ' has' : ''}" data-act="fortify" ${have >= cost ? '' : 'disabled'} title="Defenders behind works fight far harder. Ready in ${PRODUCTION.fort.months} months, and the works face every direction.">${slotIcon(GLYPH.works)}<b>Works</b>${pipsOf(lv, queued, 3)}<small>${state}</small><small class="do">Build level ${lv + queued + 1} · ${money(cost)}</small></button>`
      : `<div class="slot${lv ? ' has' : ''}">${slotIcon(GLYPH.works)}<b>Works</b>${pipsOf(lv, queued, 3)}<small>${state}${mine && lv + queued >= 3 ? ', as strong as they can be' : ''}</small></div>`;
    const harbour = `<div class="slot${r.port ? ' has' : ''}">${slotIcon(GLYPH.harbour)}<b>Harbour</b><small>${sea}</small></div>`;
    const field = `<div class="slot${wings.length ? ' has' : ''}">${plane}<b>Airfields</b><small>${flying}</small></div>`;
    return `<h3>The province</h3><div class="slots">${industry}${works}${harbour}${field}</div>`;
  }
  // The grand campaign: what the treasury can build here.
  const list = me && g.nations[me].alive && r.ctrl === me ? arsenalOffers(g, r.id) : [];
  const coming = (item: string): number => (me ? g.nations[me].queue.filter((b) => b.what === 'work' && b.item === item && b.region === r.id).length : 0);
  const slot = (key: string, glyph: string, title: string, has: boolean, pips: string, text: string, verb: string, wide = false): string => {
    const o = list.find((x) => x.key === key);
    const cls = `slot${has ? ' has' : ''}${wide ? ' wide' : ''}`;
    if (o && o.at === r.id && (o.why === null || o.poor)) {
      return `<button class="${cls} act" data-act="arsenal" data-key="${key}" ${humanUp(g) && o.why === null ? '' : 'disabled'} title="${esc(o.blurb)} Ready in ${months(o.months)}.${o.upkeep > 0 ? ` Each level costs ${cash(o.upkeep)} a month to man.` : ''}">${glyph}<b>${title}</b>${pips}<small>${text}</small><small class="do">${verb} · ${money(o.cost)}</small></button>`;
    }
    return `<div class="${cls}"${o?.why ? ` title="${esc(o.why)}"` : ''}>${glyph}<b>${title}</b>${pips}<small>${text}</small></div>`;
  };
  const built = factoriesBuilt(g, r.id), facComing = coming('factory');
  const room = r.terrain === 'urban' || r.cap ? ARSENAL.works.factory.most.city : ARSENAL.works.factory.most.town;
  const city = list.some((o) => o.key === 'work:factory' && (o.why === null || o.poor)) || built + facComing > 0;
  const industry = slot('work:factory', slotIcon(GLYPH.factory), 'Industry', r.fac > 0, city ? pipsOf(built, facComing, room) : '', `${facs}${facComing ? `, ${facComing} more building` : ''}`, 'Build a factory', true);
  const works = slot('work:fort', slotIcon(GLYPH.works), 'Works', lv > 0, pipsOf(lv, queued, 3), state, `Build level ${lv + queued + 1}`);
  const yard = yardLevel(g, r), yardComing = coming('yard');
  const YARD = ['', 'builds destroyers, submarines and troopships', 'builds every ship up to cruisers', 'builds every class of ship'];
  const harbour = slot('work:yard', slotIcon(GLYPH.harbour), 'Harbour', r.port, r.port ? pipsOf(yard, yardComing, 3) : '', r.port ? (yard ? `Shipyard level ${yard}: ${YARD[yard]}` : 'A port with no shipyard') : sea, `${yard + yardComing ? 'Enlarge' : 'Build'} the shipyard`);
  const fl = workLevel(g, 'field', r.id), flComing = coming('field');
  const field = slot('work:field', plane, 'Airfields', wings.length > 0 || fl > 0, pipsOf(fl, flComing, 3), `${flying}${fl ? `. Level ${fl} fields: room for ${num(AIR.airfields.holds + fl * ARSENAL.works.field.holds)} aircraft` : ''}`, `Build level ${fl + flComing + 1}`);
  const aa = workLevel(g, 'aa', r.id), aaComing = coming('aa');
  const guns = slot('work:aa', slotIcon(GUN), 'Air defence', aa > 0, pipsOf(aa, aaComing, 3), aa ? `Level ${aa} guns: they stop ${Math.round(aa * ARSENAL.works.aa.cut * 100)}% of any bombing here` : 'No anti-aircraft guns', `Build level ${aa + aaComing + 1}`);
  return `<h3>The province</h3><div class="slots">${industry}${works}${harbour}${field}${guns}</div>`;
}

/** What the player can raise, station or lay down in this region, and what is already on order there. */
function provinceMuster(r: Region): string {
  const me = g.human;
  if (!me || !g.nations[me].alive) return '';
  const n = g.nations[me];
  const mine = humanUp(g);
  const list = arsenalOffers(g, r.id);
  const shelf = (k: Offer['group']): string => `<div class="recruit">${list.filter((o) => o.group === k).map((o) => offerCard(o, r)).join('')}</div>`;
  let first = true;
  const head = (title: string): string => { const h = `<h3>${title}${first ? ` <span class="inhand">${money(n.pp)} in the treasury</span>` : ''}</h3>`; first = false; return h; };
  let out = '';
  if (canRaise(g, me, r)) out += head('Raise troops here') + shelf('army');
  if (budgeted(g)) {
    if (canStation(g, me, r)) out += head('Station aircraft here') + shelf('air');
    if (isHome(g, me, r) && yardLevel(g, r) > 0) out += head('Lay down ships here') + shelf('navy');
  }
  const here = n.queue.filter((b) => b.region === r.id).sort((a, c) => a.eta - c.eta);
  if (here.length) {
    out += `<h3>${budgeted(g) ? 'On order here' : 'Forming here'}</h3>`;
    for (const b of here) {
      const left = b.eta - g.turn;
      out += `<div class="order"><span class="grow"><b>${esc(b.name)}</b><small>Ready ${date(b.eta)}${left > 0 ? ` (${left} month${left > 1 ? 's' : ''})` : ''}</small></span>${b.ordered === g.turn && mine ? `<button class="btn small" data-act="cancelbuild" data-id="${b.id}">Cancel</button>` : `<span class="cost">${money(b.cost)}<small>paid</small></span>`}</div>`;
    }
  }
  return out;
}

/** What a new corps will go on costing once it is raised, for the buttons that order one. Nothing in a historical scenario. */
const keepNote = (kind: Kind, divs: number): string => (budgeted(g) ? ` Upkeep ${cash(corpsUpkeep(kind, divs) * (g.human && !g.nations[g.human].atWar.length ? BUDGET.upkeep.peace : 1))} a month.` : '');

/** What a province is worth each month to whoever holds it, and what it would be worth to the player. */
function provinceWorth(r: Region): string {
  const me = g.human;
  const y = provinceYield(g, r);
  if (y.total <= 0) return '';
  const parts: string[] = [];
  if (y.land) parts.push(`land ${cash(y.land)}`);
  if (y.port) parts.push(`harbour ${cash(y.port)}`);
  if (y.oil) parts.push(`oil ${cash(y.oil)}`);
  if (y.fac) parts.push(`${r.fac} factor${r.fac === 1 ? 'y' : 'ies'} ${cash(y.fac)}`);
  if (y.capital) parts.push(`the capital ${cash(y.capital)}`);
  const mine = !!me && r.ctrl === me;
  const taken = me && !mine && g.nations[me].alive ? provinceYield(g, r, me) : null;
  const less = r.ctrl !== r.owner ? 'Occupied ground pays half.' : r.overseas && budgeted(g) ? 'Overseas ground pays half.' : '';
  return `<button class="worth" data-act="ledger" data-nat="${r.ctrl}" title="Open the ledger of ${esc(natName(r.ctrl))}"><span>${mine ? 'Pays into your treasury each month' : `Worth to ${esc(natName(r.ctrl))} each month`}</span><b>${cash(y.total)}</b><small>${parts.join(' · ')}${less ? ` · ${less}` : ''}${taken && taken.total > 0 ? ` In your hands it would pay ${cash(taken.total)}.` : ''}</small></button>`;
}

function regionPane(): string {
  if (ui.sel < 0 && ui.sea >= 0) return seaPane();
  if (ui.sel < 0) {
    return `<h2>${g.human ? 'Your orders' : 'The map'}</h2><p class="sub">${g.human ? 'Click a region to see who holds it and which armies are there.' : 'Click any region to inspect it. Use Next month or Play to watch the war unfold.'}</p>${alertList().length ? `<div class="alerts inpane">${alertButtons()}</div>` : ''}${scopeNote()}${advice()}${goalsBlock()}${g.human ? `<p class="sub pref">${watchPref() ? 'When you end the month, the map follows the other nations as they move.' : 'When you end the month, the other nations move at once.'} <button class="btn small" data-act="watchpref">${watchPref() ? 'Play them at once' : 'Show their moves'}</button></p>` : ''}
      <h3>How a month works</h3><ol class="steps"><li>Each army gets four moves a month, shown as gold dots. Tanks and motorized troops in a fresh offensive get one or two more.</li><li>Marching through friendly ground costs one move. Entering enemy ground costs foot infantry two.</li><li>An attack is settled at once from the Battle Ticket: forces, terrain, forts, supply, air, commanders.</li><li>A nation asks for terms when it has lost too much of its capital, its territory and its army.</li><li>Fleets hold the seas. A sea the enemy commands is closed to troopships and supply ships. Click any sea to see who holds it.</li><li>Air fleets fly from airfields. Left alone they support the armies and defend their own sky. Given a mission they fight for the sky, bomb industry or strike at shipping, as far as their range allows.</li>${g.turn >= 9 ? '<li>In the desert an army is only as strong as the port behind it. Every region of desert between a corps and its nearest working port weakens its attacks and slows its advance, and cargoes sunk on the way to Libya lengthen the Axis line. Infantry on foot in open desert is easy meat for tanks unless it stands in a fortress.</li>' : ''}${g.corps.some((c) => c.airborne) ? '<li>Airborne troops need no ships. Once a month a corps marked Airborne can be dropped on an enemy coast across one sea, whoever holds the water, as long as the enemy\'s fighters do not hold the sky there. It fights without heavy weapons, so pick a weakly held target.</li>' : ''}${g.turn >= 20 ? '<li>Deep inside the Soviet Union an invader outruns his supplies: attacks weaken and marches slow until the railways behind are relaid. Mud and winter make it worse.</li>' : ''}${g.human ? '<li>Every province you hold pays into your treasury each month: factories most, then cities, farmland, harbours and oilfields. Your armies, fleets and air fleets draw their upkeep from it, and what is left buys ' + (budgeted(g) ? 'the arsenal: corps of every kind, aircraft by the wing, warships, and works for your provinces' : 'new corps and works') + '. The treasury is on the top bar: click it for the ledger. Click one of your own regions to raise corps or build works there, or open <b>Build</b> to see everything on order.</li>' + (budgeted(g) ? '<li>Heavy equipment needs industry. Tanks, artillery and motorized troops are raised only in a province with factories; aircraft need an aircraft industry and airfields to fly from; ships need a shipyard in a home port. Build a factory in a city and it also pays into the treasury every month. Ring your industry with anti-aircraft guns against bombing, and build airfields where you want fighter patrols.</li>' : '') + '<li>Neutral countries drift toward whoever is winning. Open <b>Powers</b> to court them with political capital.</li><li>Generals gain experience in battle and earn traits from how they fight. Open <b>Generals</b> to give them commands and spend their skill points.</li><li>Every unit keeps a record, and one that has done enough is honoured: a breakthrough, a stand against the odds, a capital entered, a full tour of missions. Keep the honour at the front and the unit fights harder, or send its heroes home to sell war bonds or train the next drafts. Open <b>Honours</b>, or the <b>Record</b> strip under any unit.</li><li>Open <b>Navy</b> to send your fleets out. A corps standing in a port can be shipped to a friendly port or landed on an enemy coast.</li><li>Open <b>Air</b> to move your air fleets and give them missions. The month\'s air campaigns are flown when you end the month.</li><li>Weapons improve on history\'s clock. Open <b>Research</b> to put extra design teams on the branches you want early; a branch left without a team falls behind.</li>' : ''}</ol>`;
  }
  const r = g.regions[ui.sel];
  const me = g.human;
  const held = r.ctrl === r.owner ? `${g.nations[r.owner].adj} territory` : `${g.nations[r.owner].adj} territory, held by ${natName(r.ctrl)}`;
  const tags: string[] = [];
  if (r.cap) tags.push('<span class="tag good">Capital</span>');
  if (r.port) tags.push('<span class="tag">Port</span>');
  if (r.seas.length) tags.push(`<span class="tag">Coast: ${r.seas.map((z) => esc(g.seas[z].name)).join(', ')}</span>`);
  if (r.fort) tags.push(`<span class="tag">Fortified, level ${r.fort.lv}, facing ${r.fort.vs.map((v) => g.nations[v]?.adj ?? v).join(' and ')} ground</span>`);
  if (me === 'POL' && r.sov && r.ctrl === 'POL' && !g.flags.sovietMove && g.turn <= 3) tags.push('<span class="tag bad" title="Once Poland\'s front breaks, the Red Army crosses the border under its pact with Germany and takes this province. Troops left here are lost.">East of the pact line</span>');
  if (me && g.nations[me].alive && allied(g, me, r.ctrl)) tags.push(g.supply[me]?.[r.id] ? '<span class="tag good">In your supply net</span>' : '<span class="tag bad">No supply line to here</span>');
  const sky = skyOver(g, r.id);
  if (sky) tags.push(`<span class="tag ${me && allied(g, me, sky) ? 'good' : 'bad'}">The ${g.nations[sky].adj} air force holds the sky here</span>`);
  const based = g.airFleets.filter((f) => f.base === r.id && f.ftr + f.bmr > 0);
  const mineHere = r.units.filter((c) => canOrder(g, c));
  let acts = '';
  const picked = pickedCorps();
  if (picked.length) {
    const canRail = picked.some((c) => railReach(g, c).size > 0);
    const canShip = picked.some((c) => shipTargets(g, c).length > 0);
    const canEvac = picked.some((c) => g.nations[c.nat].seaSupply && r.port && r.owner !== c.nat);
    const evacTo = canEvac ? refuge(g, picked[0], r) : null;
    acts = `<div class="acts">${canRail ? `<button class="btn small${ui.rail ? ' on' : ''}" data-act="rail">Rail move</button>` : ''}${canShip ? `<button class="btn small${ui.ship ? ' on' : ''}" data-act="ship">Ship by sea</button>` : ''}${lands.length ? (byAir(picked) ? `<button class="btn small${ui.land ? ' on' : ''}" data-act="land" title="Drop these airborne troops on an enemy coast across one sea. They need no ships, and fight without heavy weapons.">Airborne assault</button>` : `<button class="btn small${ui.land ? ' on' : ''}" data-act="land" title="Land these corps on an enemy coast.">Invade by sea</button>`) : ''}${canEvac ? `<button class="btn small" data-act="evac" title="An emergency lift ${evacTo && !evacTo.cap ? `to ${esc(evacTo.name)}, the nearest haven still in friendly hands` : 'home'}. The corps leaves a quarter of its strength behind.">Evacuate by sea</button>` : ''}${picked.length === 1 ? (picked[0].general !== null ? `<button class="btn small" data-act="general" data-id="${picked[0].general}">General's record</button>` : `<button class="btn small" data-act="appoint" data-id="${picked[0].id}">Appoint a general</button>`) : ''}<button class="btn small" data-act="unpick">Stand down</button></div>`;
  } else if (mineHere.some((c) => c.ap > 0)) {
    acts = `<div class="acts"><button class="btn small" data-act="pickall">Select all here</button></div>`;
  }
  const units = r.units.length ? [...r.units].sort((a, b) => (a.nat === me ? 0 : 1) - (b.nat === me ? 0 : 1)).map((c) => corpsRow(c, canOrder(g, c))).join('') : '<div class="empty">No armies here.</div>';
  return `<h2>${esc(r.name)}</h2><div class="sub">${held}</div>${standingNote(r)}
    <div class="facts"><div class="fact"><span>Terrain</span><b>${r.terrain[0].toUpperCase() + r.terrain.slice(1)}</b></div><div class="fact"><span>Victory pts</span><b>${r.vp}</b></div><div class="fact"><span>Industry</span><b>${r.fac}</b></div></div>
    ${tags.length ? `<div class="tags" style="display:flex;gap:5px;flex-wrap:wrap;margin-top:8px">${tags.join('')}</div>` : ''}
    ${provinceWorth(r)}${provinceSlots(r)}
    <h3>Forces here</h3>${acts}${units}
    ${based.length ? `<h3>On the airfields</h3>${based.map((f) => airRow(f, false, true)).join('')}` : ''}${provinceMuster(r)}${ui.sel >= 0 && me ? advice() : ''}`;
}

const SHELF = { army: 'Army', air: 'Air force', navy: 'Navy', works: 'Works' } as const;

function buildPane(): string {
  const me = g.human!;
  const n = g.nations[me];
  const mine = humanUp(g);
  const have = Math.floor(n.pp);
  const L = ledger(g, me);
  const list = arsenalOffers(g, ui.sel);
  const sel = ui.sel >= 0 ? g.regions[ui.sel] : null;
  const at = (text: string): string => `<b style="color:var(--ink)">${esc(text)}</b>`;
  const perDiv = cash(PRODUCTION.replacementCost), depots = Math.round(PRODUCTION.manpower * 100);
  let out = `<h2>${L.budget ? 'The arsenal' : 'War production'}</h2><p class="sub">${L.budget
    ? `Every province you hold pays into the treasury, and every corps, ship and aircraft draws its upkeep from it. Losses in the field are made good next, at ${perDiv} a division, as fast as the depots can train men: about ${depots}% of the army a month. What is left is yours to spend.`
    : `Your factories pay into the treasury every month. Losses in the field are made good first, at ${perDiv} a division, as fast as the depots can train men: about ${depots}% of the army a month. What is left is yours to spend.`}</p>
    <div class="facts"><div class="fact"><span>In the treasury</span><b${L.treasury < 0 ? ' style="color:var(--red)"' : ''}>${inHand(L.treasury)}</b></div><div class="fact"><span>${L.budget ? 'After upkeep, a month' : 'Each month'}</span><b${L.net < 0 ? ' style="color:var(--red)"' : ''}>${cash(L.net, true)}</b></div><div class="fact"><span>On order</span><b>${n.queue.length}</b></div></div>
    <div class="acts"><button class="btn small" data-act="ledger">Open the ledger</button></div>`;
  if (L.treasury < 0) out += `<div class="note"><b>The treasury is in debt.</b> Nothing new can be ordered and no replacements go out until the books balance. Unpaid troops rest badly and men drift away.</div>`;
  if (!n.atWar.length) out += `<div class="note"><b>At peace.</b> Industry runs at half speed until the nation is at war${L.budget ? ', and the forces cost half as much to keep' : ''}.</div>`;

  const shelf = (k: Offer['group'], quiet = false): string => list.filter((o) => o.group === k).map((o) => offerButton(o, quiet)).join('');
  const army = (): string => {
    let s = `<h3>Raise a new corps</h3>`;
    const any = list.filter((o) => o.group === 'army');
    if (!any.some((o) => o.at >= 0)) return `${s}<div class="empty">There is no home ground left to raise troops on.</div>`;
    s += `<p class="sub" style="margin:0 0 7px">${L.budget ? 'Infantry and mountain troops can be raised in any home province. Heavier equipment is built where there is industry: motorized troops and artillery need a factory in the province, tanks need two. ' : ''}${sel && canRaise(g, me, sel) ? `Raising at ${at(sel.name)} where it can be.` : 'Click one of your own home regions on the map to raise troops there.'}</p>${shelf('army')}`;
    const lostOnes = any.flatMap((o) => revivable(g, me, o.key.split(':')[1])).sort((a, b) => b.turn - a.turn).slice(0, 6);
    if (lostOnes.length) {
      s += `<h3>Raise a lost corps again</h3><p class="sub" style="margin:0 0 7px">A formation on the memorial wall can be raised again under its old name. It costs what any new corps costs, and it carries the old colours, the name the army gave it and part of what the old corps knew.</p>`;
      for (const f of lostOnes) {
        const b = specOf(g, f.kind)!;
        const site = raiseSite(g, me, f.kind, ui.sel);
        if (site < 0) continue;
        s += `<button class="buy" data-act="revive" data-id="${f.id}" ${mine && have >= b.cost ? '' : 'disabled'}>${icon({ nat: me, kind: f.kind } as Corps)}<span class="grow"><b>${esc(f.name)}${f.nick ? ` <i class="nick">${esc(f.nick)}</i>` : ''}</b><small>${fallenLine(f)} ${ribbons(f)}</small><small>${b.divs} divisions · ready in ${b.months} months · forms up at ${esc(g.regions[site].name)}</small></span><span class="cost${have >= b.cost ? '' : ' short'}">${money(b.cost)}</span></button>`;
      }
    }
    return s;
  };
  const fortify = (): string => {
    // A historical scenario builds one kind of works: fortifications, on home ground.
    const s = `<h3>Fortify</h3>`;
    if (!sel || !isHome(g, me, sel)) return `${s}<div class="empty">Click one of your own home regions on the map to fortify it.</div>`;
    return s + (shelf('works') || `<div class="empty">${esc(sel.name)} is already fortified as heavily as it can be.</div>`);
  };
  if (!L.budget) out += army() + fortify();
  else {
    out += `<div class="seg shelves">${(Object.keys(SHELF) as (keyof typeof SHELF)[]).map((k) => `<button class="btn small${ui.arm === k ? ' on' : ''}" data-act="arm" data-arm="${k}">${SHELF[k]}</button>`).join('')}</div>`;
    if (ui.arm === 'army') out += army();
    else if (ui.arm === 'air') {
      out += `<h3>Order aircraft</h3><p class="sub" style="margin:0 0 7px">Aircraft come by the wing from the nation's aircraft industry, and are stationed on airfields: a province where one of your air fleets is based, or one where you have built airfields. ${sel && canStation(g, me, sel) ? `Stationing at ${at(sel.name)}.` : 'Click such a province on the map to station them there.'}</p>${shelf('air')}`;
    } else if (ui.arm === 'navy') {
      out += `<h3>Lay down ships</h3><p class="sub" style="margin:0 0 7px">Ships are laid down in a home port whose shipyard is big enough, and they take a long time. A level 1 yard builds destroyers, submarines and troopships, level 2 cruisers, level 3 carriers and battleships. ${sel && isHome(g, me, sel) && yardLevel(g, sel) > 0 ? `Building at ${at(sel.name)} where its yard will do.` : 'Click one of your ports on the map to build there.'}</p>${shelf('navy')}`;
    } else {
      const own = !!sel && sel.ctrl === me;
      out += `<h3>Build works</h3><p class="sub" style="margin:0 0 7px">${own ? `Building at ${at(sel!.name)}.` : '<b style="color:var(--brass)">Click one of your own provinces on the map to build there.</b>'} Factories go up in cities and add to your income every month. Shipyards go in ports. Anti-aircraft guns and airfields can go on any ground you hold.</p>${shelf('works', !own)}`;
    }
  }

  out += `<h3>On order</h3>`;
  if (!n.queue.length) out += `<div class="empty">Nothing on order.</div>`;
  for (const b of [...n.queue].sort((a, c) => a.eta - c.eta)) {
    const left = b.eta - g.turn;
    const where = g.regions[b.region]?.name ?? '';
    out += `<div class="order"><span class="grow"><b>${esc(b.name)}</b><small>${b.what === 'corps' ? `Forming at ${esc(where)} · ready` : 'Ready'} ${date(b.eta)}${left > 0 ? ` (${left} month${left > 1 ? 's' : ''})` : ''}</small></span>${b.ordered === g.turn && mine ? `<button class="btn small" data-act="cancelbuild" data-id="${b.id}">Cancel</button>` : `<span class="cost">${money(b.cost)}<small>paid</small></span>`}</div>`;
  }
  return out;
}

// ---- Research -----------------------------------------------------------------------------

/** The last month the research timelines run to: December 1945. */
const WAR_ENDS = 76;
const PACE = ['Nobody at work: this branch stands still', 'One team: keeps to history\'s schedule', 'Two teams: half as fast again', 'Three teams: nearly twice the pace'];

/** One branch as a strip of the whole war: history's dates as marks, the designers' progress as a gold bar, today as a white line. */
function labTrack(nat: string, f: Field): string {
  const lab = g.nations[nat].lab!;
  const pos = (t: number): string => `${(Math.max(0, Math.min(WAR_ENDS, t)) / WAR_ENDS * 100).toFixed(1)}%`;
  const ticks = stepsOf(nat, f).filter((st) => st.at > 0).map((st) => `<i class="${st.at <= lab.clock[f] ? 'got' : ''}" style="left:${pos(st.at)}" title="${esc(st.name)}: ${date(st.at)} in history"></i>`).join('');
  return `<div class="labtrack"><span class="fill" style="width:${pos(lab.clock[f])}"></span>${ticks}<span class="now" style="left:${pos(lab.months)}" title="History today: ${date(Math.max(1, lab.months))}"></span></div>`;
}

function labRow(nat: string, f: Field, mine: boolean): string {
  const lab = g.nations[nat].lab!;
  const spec = RESEARCH.fields[f];
  const cur = inService(nat, lab, f), next = nextStep(nat, lab, f);
  const ahead = Math.round(monthsAhead(lab, f));
  const tag = ahead > 0 ? `<span class="tag good">${ahead} month${ahead === 1 ? '' : 's'} ahead</span>` : ahead < 0 ? `<span class="tag bad">${-ahead} month${ahead === -1 ? '' : 's'} behind</span>` : `<span class="tag">On history's schedule</span>`;
  const eta = arrival(g, nat, f);
  const k = lead(g, nat, f);
  const pct = Math.round(spec.per * k * 100);
  const n = lab.teams[f];
  const free = teamsFree(lab);
  let out = `<div class="lab"><div class="lab-h"><b>${esc(f === 'doctrine' ? `${spec.label}: ${doctrineOf(nat)}` : spec.label)}</b>${tag}</div>
    <div class="lab-now">In service: <b>${esc(cur?.name ?? 'nothing yet')}</b></div>
    <div class="lab-next">${next ? `Next: ${esc(next.name)}. History had it in ${date(next.at)}; ${eta === null ? 'at this pace it never comes' : `yours is due ${date(eta)}`}.` : 'The designers have nothing further on the drawing board.'}</div>
    ${labTrack(nat, f)}`;
  if (k) out += `<div class="lab-fx ${k > 0 ? 'good' : 'bad'}">${esc(spec.who)}: ${pct > 0 ? '+' : ''}${pct}% while these designs are ${Math.abs(k) === 1 ? 'a generation' : `${Math.abs(k)} generations`} ${k > 0 ? 'ahead of' : 'behind'} history.</div>`;
  out += `<div class="lab-t"><button class="btn small" data-act="teams" data-field="${f}" data-n="${n - 1}" ${mine && n > 0 ? '' : 'disabled'} aria-label="One team fewer on ${esc(spec.label)}">−</button><span class="teampips">${Array.from({ length: MAX_TEAMS }, (_, i) => `<i class="${i < n ? 'on' : ''}"></i>`).join('')}</span><button class="btn small" data-act="teams" data-field="${f}" data-n="${n + 1}" ${mine && n < MAX_TEAMS && free > 0 ? '' : 'disabled'} aria-label="One team more on ${esc(spec.label)}">+</button><small>${PACE[n]}</small></div></div>`;
  return out;
}

/** What one power has in service, branch by branch. */
function labSummary(nat: string): string {
  const lab = g.nations[nat].lab!;
  return `<table class="ticket">${FIELDS.map((f) => `<tr><td>${esc(RESEARCH.fields[f].label)}</td><td>${esc(inService(nat, lab, f)?.name ?? '')}</td></tr>`).join('')}</table>`;
}

function researchPane(): string {
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  const majors = Object.values(g.nations).filter((n) => n.lab && n.alive && n.id !== me).sort((a, b) => fieldArmy(g, b.id) - fieldArmy(g, a.id));
  const others = majors.map((n) => `<h3>${esc(n.name)}${doctrineOf(n.id) ? ` · ${esc(doctrineOf(n.id))}` : ''}</h3>${labSummary(n.id)}`).join('');
  if (!me || !g.nations[me].lab) {
    return `<h2>Research</h2><p class="sub">The weapons of every great power improve as the war goes on, each new design arriving in the month history brought it into service. ${me ? `${esc(natName(me))} has no design offices of its own and buys its weapons abroad.` : 'A power that pushes one branch hard gets its designs early; one that neglects a branch falls behind.'}</p>${others}`;
  }
  const n = g.nations[me], lab = n.lab!;
  const mine = humanUp(g);
  const free = teamsFree(lab);
  const cost = bureauCost(n);
  let out = `<h2>Research</h2><p class="sub">Every design team keeps one branch of the armed forces on history's schedule. Put a second team on a branch and its next designs arrive early; take the team away and the branch falls behind. In battle a design counts for as long as it is ahead of its time.</p>
    <div class="facts"><div class="fact"><span>Design teams</span><b>${teamsTotal(lab)}</b></div><div class="fact"><span>Unassigned</span><b>${free}</b></div><div class="fact"><span>Doctrine</span><b style="font-size:13px">${esc(doctrineOf(me))}</b></div></div>
    <div class="trackkey"><span><i class="k fill"></i>Your designers</span><span><i class="k now"></i>History today</span><span><i class="k tick"></i>A design's date in history</span></div>`;
  if (free > 0) out += `<div class="note"><b>${free} design team${free === 1 ? ' is' : 's are'} idle.</b> Put ${free === 1 ? 'it' : 'them'} to work with a + button below.</div>`;
  for (const f of FIELDS) out += labRow(me, f, mine);
  out += `<h3>More design teams</h3>`;
  out += cost === null ? `<div class="empty">The nation has all the design teams its engineers can staff.</div>`
    : `<button class="buy" data-act="bureau" ${mine && n.pp >= cost ? '' : 'disabled'}><span class="grow"><b>Found a new design team</b><small>One more team to put on any branch, for the rest of the war.</small><small>Paid for from the treasury: ${money(n.pp)} in hand.</small></span><span class="cost${n.pp >= cost ? '' : ' short'}">${money(cost)}</span></button>`;
  if (others) out += `<h3 style="margin-top:20px">What the other powers have in service</h3>${others.replace(/<h3>/g, '<h3 style="margin-top:10px">')}`;
  return out;
}

// ---- Generals -----------------------------------------------------------------------------

const GONE: Record<string, string> = { captured: 'Prisoner of war', dead: 'Killed in action', sacked: 'Dismissed' };

function genWhere(x: General): string {
  if (x.status === 'wounded') return `Wounded. Fit again in ${date(x.backOn)}`;
  if (x.status !== 'active') return GONE[x.status];
  const c = corpsOf(g, x);
  return c ? `${esc(c.name)} · ${KIND[c.kind].label.toLowerCase()} · ${esc(g.regions[c.reg].name)}` : 'Waiting for a command';
}

function traitTags(x: General): string {
  return x.traits.map((t) => {
    const d = COMMAND.traits[t];
    return d ? `<span class="tag ${d.good ? 'good' : 'bad'}" title="${esc(d.effect)}">${esc(d.label)}</span>` : '';
  }).join('');
}

function genRow(x: General, act = 'general', extra = '', off = ''): string {
  const pts = x.nat === g.human && x.points > 0 && serving(x) ? `<span class="tag pts">${x.points} skill point${x.points > 1 ? 's' : ''}</span>` : '';
  const whose = x.nat === g.human ? '' : `${g.nations[x.nat].adj} · `;
  return `<button class="gen${serving(x) ? '' : ' gone'}" data-act="${act}" data-id="${x.id}" ${extra} ${off ? `disabled title="${esc(off)}"` : ''}><span class="lv">${x.level}</span><span class="grow"><b>${esc(x.name)}</b><small>${whose}${esc(rankOf(x))} · ${genWhere(x)}</small>${x.traits.length || pts ? `<span class="tags">${pts}${traitTags(x)}</span>` : ''}</span></button>`;
}

function generalsPane(): string {
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  const rank = (a: General, b: General): number => b.level - a.level || b.rec.won - a.rec.won || a.id - b.id;
  let out = '';
  if (me) {
    const mine = g.generals.filter((x) => x.nat === me);
    const free = waiting(g, me);
    const led = mine.filter((x) => x.status === 'active' && x.corps >= 0).sort(rank);
    const gone = mine.filter((x) => x.status !== 'active').sort((a, b) => (serving(b) ? 1 : 0) - (serving(a) ? 1 : 0) || rank(a, b));
    out += `<h2>Your generals</h2><p class="sub">Generals gain experience in battle, and every level brings a skill point to spend. Traits are earned from how a general actually fights. Open one to see his service record or give him a command.</p>
      <div class="facts"><div class="fact"><span>In command</span><b>${led.length}</b></div><div class="fact"><span>Waiting</span><b>${free.length}</b></div><div class="fact"><span>Corps without</span><b>${vacant(g, me).length}</b></div></div>`;
    if (free.length) out += `<h3>Waiting for a command</h3>${free.map((x) => genRow(x)).join('')}`;
    out += `<h3>In command</h3>${led.length ? led.map((x) => genRow(x)).join('') : '<div class="empty">No corps has a general yet.</div>'}`;
    if (gone.length) out += `<h3>Out of action</h3>${gone.map((x) => genRow(x)).join('')}`;
    out += `<div class="note"><b>A corps with no general fights by the book.</b> Pick one of your corps on the map and press <b>Appoint a general</b> to give it one who is waiting, or to promote an unknown officer from its ranks for ${COMMAND.promote.pc} political capital.</div>`;
  }
  const others = g.generals.filter((x) => x.nat !== me && serving(x) && (x.rec.battles > 0 || x.level >= 3)).sort(rank).slice(0, me ? 8 : 40);
  out += me ? '<h3>Other commanders of the war</h3>' : '<h2>Commanders of the war</h2><p class="sub">The generals who have made a name so far. They gain experience in battle, earn traits from how they fight, and can be wounded, captured or killed. Open one to see his service record.</p><div style="height:8px"></div>';
  out += others.length ? others.map((x) => genRow(x)).join('') : '<div class="empty">No one has made a name yet.</div>';
  return out;
}

/** A general's service record, and the decisions the player can take about him. */
function generalCard(x: General): string {
  const m = ui.modal!;
  const n = g.nations[x.nat];
  const mine = x.nat === g.human && humanUp(g);
  const c = corpsOf(g, x);
  const need = xpToNext(x);
  const times = (k: number): string => (k === 1 ? 'once' : k === 2 ? 'twice' : `${k} times`);
  const traits = x.traits.map((t) => {
    const d = COMMAND.traits[t];
    return d ? `<li class="${d.good ? 'plus' : 'minus'}"><b>${d.good ? '+' : '−'} ${esc(d.label)}</b><span>${esc(d.effect)}</span><small>${esc(d.earned)}</small></li>` : '';
  }).join('');
  const skills = SKILLS.map((k) => {
    const d = COMMAND.skills[k], v = x.skills[k];
    const can = mine && serving(x) && x.points > 0 && v < COMMAND.skillMax;
    const dots = Array.from({ length: COMMAND.skillMax }, (_, i) => `<i class="${i < v ? '' : 'off'}"></i>`).join('');
    return `<div class="skill"><b>${d.label}</b><span class="dots" title="${v} of ${COMMAND.skillMax}">${dots}</span>${can ? `<button class="btn small" data-act="skill" data-id="${x.id}" data-skill="${k}" title="Spend a skill point on ${d.label.toLowerCase()}">+ 1</button>` : '<span></span>'}<small>${esc(d.blurb)}</small></div>`;
  }).join('');
  let orders = '';
  if (mine && x.status === 'active') {
    const one = pickedCorps().length === 1 ? pickedCorps()[0] : null;
    const open = vacant(g, x.nat).sort((a, b) => fit(g, x, b) - fit(g, x, a));
    const list = [...(one && one.id !== x.corps && !open.includes(one) ? [one] : []), ...open].slice(0, 6);
    const row = (t: Corps): string => {
      const why = whyNotAssign(g, x, t);
      const sitting = generalOf(g, t);
      return `<button class="pickrow" data-act="assign" data-id="${x.id}" data-corps="${t.id}" ${why ? `disabled title="${esc(why)}"` : ''}>${icon(t)}<span class="grow"><b>${esc(t.name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${KIND[t.kind].label} · ${t.divs} division${t.divs > 1 ? 's' : ''} · at ${esc(g.regions[t.reg].name)}${sitting ? ` · takes over from ${esc(sitting.name)}` : ''}</span></span></button>`;
    };
    if (x.moved === g.turn) orders = `<h3>${c ? 'Another command' : 'A command'}</h3><div class="empty">He took up a new command this month. He can be moved again next month.</div>`;
    else orders = `<h3>${c ? 'Give him another command' : 'Give him a command'}</h3>${list.length ? list.map(row).join('') : '<div class="empty">Every corps has a general. Pick one corps on the map first to put him in another man\'s place.</div>'}`;
  }
  const state = x.status === 'active' ? (c ? `Assigned: ${esc(c.name)}, ${esc(g.regions[c.reg].name)}` : 'Waiting for a command') : genWhere(x);
  const confirm = m.confirm ? `<div class="note" style="border-left-color:var(--red)"><b>Dismiss ${esc(x.name)} for good?</b> ${popular(x) ? `The army thinks well of him. ${c ? 'His corps will lose cohesion, and it' : 'It'} will cost ${COMMAND.sack.pc} political capital.` : 'He will not come back.'}</div>` : '';
  const foot = m.confirm
    ? `<button class="btn" data-act="keep" data-id="${x.id}">Keep him</button><button class="btn go" data-act="sack" data-id="${x.id}">Dismiss him</button>`
    : `${mine && serving(x) ? `<button class="btn" data-act="sack" data-id="${x.id}">Dismiss</button>` : ''}${mine && x.status === 'active' && c ? `<button class="btn" data-act="relieve" data-id="${x.id}" title="Take him off his corps and put him back on the list">Relieve of command</button>` : ''}${c ? `<button class="btn" data-act="showmap" data-id="${c.reg}">Show on map</button>` : ''}<button class="btn go" data-act="close">Close</button>`;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">${n.adj} army · ${esc(rankOf(x))} · ${x.historical ? 'Historical' : 'Rose from the ranks'}</div><h2>${esc(x.name)}</h2></header>
    <div class="body">
      <div class="record"><div class="lvbig"><b>${x.level}</b><span>LEVEL</span></div><div class="who"><b>${state}</b>
        <div class="xp"><i style="width:${need === Infinity ? 100 : Math.round((x.xp / need) * 100)}%"></i></div>
        <small>${need === Infinity ? 'At the highest level' : `XP ${num(x.xp)} / ${num(need)} · Next level brings a skill point`}${x.points > 0 && serving(x) ? ` · <b style="color:var(--brass)">${x.points} to spend</b>` : ''}</small></div></div>
      <h3>Traits</h3>${traits ? `<ul class="traits">${traits}</ul>` : '<div class="empty">None yet. Traits are earned from how a general fights: closing pockets, holding against the odds, costly assaults.</div>'}
      <h3>Skills</h3>${skills}
      <div class="tally">Battles ${x.rec.battles} · Won ${x.rec.won}${x.rec.wounds ? ` · Wounded ${times(x.rec.wounds)}` : ''}</div>
      ${rankLadder(x)}
      ${orders}${confirm}
    </div><footer>${foot}</footer>`;
}

/** Choosing a general for one corps: a man from the waiting list, or an unknown officer from its own ranks. */
function appointCard(c: Corps): string {
  const free = waiting(g, c.nat).sort((a, b) => fit(g, b, c) - fit(g, a, c) || b.level - a.level);
  const why = whyNotPromote(g, c);
  const pc = Math.floor(g.nations[c.nat].pc);
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Command</div><h2>A general for ${esc(c.name)}</h2></header>
    <div class="body"><p class="lead">${g.nations[c.nat].adj} ${KIND[c.kind].label.toLowerCase()}, ${c.divs} division${c.divs > 1 ? 's' : ''}, at ${esc(g.regions[c.reg].name)}. A general adds his skills and traits to every battle this corps fights.</p>
      <h3>Generals waiting for a command</h3>
      ${free.length ? free.map((x) => genRow(x, 'assign', `data-corps="${c.id}"`, whyNotAssign(g, x, c))).join('') : '<div class="empty">No general is waiting for a command.</div>'}
      <h3>Promote from the ranks</h3>
      <button class="buy" data-act="promote" data-id="${c.id}" ${why ? `disabled title="${esc(why)}"` : ''}><span class="grow"><b>Promote an officer of this corps</b><small>An unknown officer takes command at level 1. He is a rising star and learns faster than the old guard.</small><small>You have ${pc} political capital.</small></span><span class="cost${why ? ' short' : ''}">${COMMAND.promote.pc}<small>capital</small></span></button>
    </div><footer><button class="btn go" data-act="close">Close</button></footer>`;
}

// ---- Unit stories: ribbons, service records, honours and the memorial wall -------------------

type AnyUnit = (Corps | AirFleet | Fleet) & Storied;

const andList = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const serviceOf = (kind: UnitKind): string => (kind === 'corps' ? 'army' : kind === 'air' ? 'air force' : 'navy');
const placeOf = (kind: UnitKind, at: number): string => (at < 0 ? '' : kind === 'fleet' ? g.seas[at]?.name ?? '' : g.regions[at]?.name ?? '');

/** What has become of an honour, in a few words. */
function awardState(a: Award): string {
  if (a.old) return 'carried from the corps that bore the name before';
  if (a.cash === 'bonds') return `sent home on a bond tour, ${date(a.cashed ?? a.turn)}`;
  if (a.cash === 'school') return `its veterans went to the schools, ${date(a.cashed ?? a.turn)}`;
  return 'carried at the front';
}

/** One honour as a ribbon: two colours, as on a medal bar. Faded once the fame has been spent at home. */
function ribbonOf(a: Award, big = false): string {
  const d = honourDef(a.hon);
  if (!d) return '';
  return `<i class="rib${big ? ' big' : ''}${a.old ? ' old' : a.cash ? ' home' : ''}" style="--a:${d.ribbon[0]};--b:${d.ribbon[1]}" title="${esc(awardTitle(g, a))}, ${date(a.turn)}: ${esc(awardState(a))}"></i>`;
}

/** A unit's ribbons in a row, newest last. */
function ribbons(u: Storied, most = 8): string {
  const hs = honoursOf(g, u);
  if (!hs.length) return '';
  return `<span class="ribs">${hs.slice(-most).map((a) => ribbonOf(a)).join('')}${hs.length > most ? `<small>+${hs.length - most}</small>` : ''}</span>`;
}

/** What an honour adds in battle while it is carried at the front. */
function edgeText(a: Award): string {
  const e = honourDef(a.hon)?.edge;
  const pc = (v: number): number => Math.round(v * 100);
  const parts: string[] = [];
  if (e?.attack) parts.push(`attacks ${pc(e.attack)}% stronger`);
  if (e?.defence) parts.push(`defends ${pc(e.defence)}% stronger`);
  if (e?.all) parts.push(`fights ${pc(e.all)}% stronger`);
  return parts.length ? andList(parts) : '';
}

/** The tags a corps wears for its story: its designation, leave at home, drafts refused, an old enemy, a home in enemy hands. */
function storyTags(c: Corps): string[] {
  const out: string[] = [];
  const mine = c.nat === g.human;
  if (isGuards(g, c)) out.push(`<span class="tag hon" title="Named in the order of the day: the highest honour a formation can earn.">${esc(designation(c.nat))}</span>`);
  if (c.leave !== undefined) out.push(`<span class="tag" title="At home with its fame. It cannot be given orders until it is back.">At home until ${date(c.leave)}</span>`);
  if (c.lean && mine) out.push('<span class="tag" title="This corps takes no drafts. It stays as small as its losses leave it, and keeps what it knows.">Veterans only</span>');
  const r = rivalryOf(g, c.rival);
  if (r && !r.over && mine) out.push(`<span class="tag hon" title="The two have met ${r.met} times. This corps fights ${Math.round(STORIES.rivals.edge * 100)}% harder in any battle its old enemy is in.">Old enemy: ${esc(r.theirsName)}</span>`);
  if (mine && c.rec?.exile && c.home !== undefined) out.push(`<span class="tag bad" title="The place this corps was raised in is in enemy hands.">${esc(g.regions[c.home].name)} lost</span>`);
  return out;
}

/** A unit's record in one line. */
function statLine(kind: UnitKind, u: AnyUnit): string {
  const v = (k: string): number => u.rec?.[k] ?? 0;
  if (kind === 'corps') {
    const c = u as Corps;
    if (c.kind === 'fort') return v('battles') ? `${v('battles')} ${v('battles') === 1 ? 'battle' : 'battles'}, ${v('won')} won` : 'No battles yet';
    return `${grade(c.xp)} · ${v('battles') ? `${v('battles')} ${v('battles') === 1 ? 'battle' : 'battles'}, ${v('won')} won` : 'no battles yet'}`;
  }
  if (kind === 'air') {
    const ops = v('flown') + v('defended');
    return ops ? `${ops} ${ops === 1 ? 'operation' : 'operations'}${v('missions') ? ` · ${v('missions')} bombing missions` : ''}` : 'No operations yet';
  }
  const parts: string[] = [];
  if (v('actions')) parts.push(`${v('actions')} ${v('actions') === 1 ? 'action' : 'actions'} won`);
  if (v('tons')) parts.push(`${num(v('tons'))} tons sunk`);
  if (v('escort')) parts.push(`${v('escort')} months on escort`);
  return parts.length ? parts.join(' · ') : 'No actions yet';
}

/** The strip under a unit's row: its ribbons, its record in a line, and the way in to the whole of it. */
function recStrip(kind: UnitKind, u: AnyUnit): string {
  return `<button class="recstrip" data-act="record" data-type="${kind}" data-id="${u.id}" title="Open the service record of ${esc(u.name)}">${ribbons(u, 6)}<span class="grow">${esc(statLine(kind, u))}</span><b>Record ›</b></button>`;
}

/** How and where a formation on the memorial wall was lost. */
function fallenLine(f: Fallen): string {
  const how = f.how === 'destroyed' ? 'Destroyed' : f.how === 'surrendered' ? 'Laid down its arms' : 'Starved out';
  return `${how} at ${esc(g.regions[f.reg]?.name ?? 'the front')}, ${date(f.turn)}`;
}

/** The ranks of a general's army, with the one he holds picked out. */
function rankLadder(x: General): string {
  const rows = COMMAND.ranks[x.nat] ?? COMMAND.ranks._default;
  return `<h3>Rank</h3><div class="ladder">${rows.map(([from, title], i) => {
    const next = rows[i + 1]?.[0] ?? Infinity;
    return `<span class="${x.level >= from ? 'got' : ''}${x.level >= from && x.level < next ? ' here' : ''}"><b>${esc(title)}</b><small>${x.level >= from ? 'Held' : `At level ${from}`}</small></span>`;
  }).join('')}</div>`;
}

/** One honour set out in full on a record: ribbon, name, when and where, and what became of it. */
function honourBlock(a: Award, open: boolean): string {
  const d = honourDef(a.hon);
  const el = open ? 'button' : 'div';
  const todo = open && a.nat === g.human && !a.cash && !a.old && !d?.last;
  const edge = !a.cash && !a.old ? edgeText(a) : '';
  return `<${el} class="hon" ${open ? `data-act="award" data-id="${a.id}"` : ''}>${ribbonOf(a, true)}<span class="grow"><b>${esc(awardTitle(g, a))}</b><small>${esc(d?.name ?? '')} · ${date(a.turn)}${a.first ? ` · first of the ${esc(g.nations[a.nat].adj)} ${serviceOf(a.kind)}` : ''}</small><small>${capital(esc(awardState(a)))}${edge ? `: ${edge}` : ''}</small></span>${todo ? '<span class="go">Decide ›</span>' : open ? '<span class="go">Citation ›</span>' : ''}</${el}>`;
}

const STAT_LABELS: Record<UnitKind, [string, string][]> = {
  corps: [['battles', 'Battles'], ['won', 'Won'], ['taken', 'Regions taken'], ['stormed', 'Breakthroughs'], ['stood', 'Stood against odds'], ['pris', 'Prisoners taken'], ['lost', 'Men lost'], ['caps', 'Capitals entered'], ['landings', 'Landings'], ['drops', 'Airborne drops'], ['cut', 'Months cut off'], ['winter', 'Winter battles'], ['desert', 'Desert battles'], ['scores', 'Old scores settled']],
  air: [['flown', 'Operations flown'], ['missions', 'Bombing missions'], ['nights', 'Night raids'], ['skies', 'Skies won'], ['kills', 'Enemy aircraft downed'], ['sunk', 'Ships sunk'], ['defended', 'Raids met'], ['saved', 'Raiders shot down']],
  fleet: [['actions', 'Actions won'], ['capital', 'Capital ships sunk'], ['tons', 'Tons of shipping sunk'], ['escort', 'Months on escort']],
};

function statGrid(kind: UnitKind, u: Storied, always: number): string {
  const cells = STAT_LABELS[kind].filter(([k], i) => i < always || (u.rec?.[k] ?? 0) > 0).map(([k, label]) => `<div class="fact"><span>${label}</span><b>${num(u.rec?.[k] ?? 0)}</b></div>`);
  return cells.length ? `<div class="facts">${cells.join('')}</div>` : '';
}

function diaryBlock(u: { log?: DiaryLine[] }, shape: { kind?: unknown; ships?: unknown; ftr?: unknown }): string {
  const log = u.log ?? [];
  if (!log.length) return '<div class="empty">Nothing written yet.</div>';
  let out = '<div class="reel diary">', t = -1;
  for (let i = log.length - 1; i >= 0; i--) {
    const line = log[i];
    if (line[0] !== t) { t = line[0]; out += `<div class="mo">${date(t)}</div>`; }
    out += `<p class="${line[1] === 'honour' ? 'honour' : ''}">${esc(diaryText(g, shape, line))}</p>`;
  }
  return out + '</div>';
}

/** The letters written about a unit. Air fleets and fleets are filed under their own numbers. */
function lettersOf(kind: UnitKind, id: number): Game['headlines'] {
  const key = kind === 'corps' ? id : kind === 'air' ? -id : -1000 - id;
  return g.headlines.filter((h) => h.kind === 'letter' && h.unit === key);
}

/** A unit's whole story: what it is, what it has earned, what it has done, and what the player can decide about it. */
function unitRecordCard(kind: UnitKind, u: AnyUnit): string {
  const n = g.nations[u.nat];
  const mine = u.nat === g.human;
  const up = mine && humanUp(g);
  const hs = honoursOf(g, u);
  const v = (k: string): number => u.rec?.[k] ?? 0;
  let badge: string, who: string, extra = '', where = -1;
  if (kind === 'corps') {
    const c = u as Corps;
    where = c.reg;
    const gen = generalOf(g, c);
    const guards = isGuards(g, c);
    badge = `<div class="lvbig"><b>${Math.round(c.xp * 100)}</b><span>${esc(grade(c.xp).toUpperCase())}</span></div>`;
    who = `<b>${n.adj} ${KIND[c.kind].label.toLowerCase()}${guards ? ` · ${esc(designation(c.nat))}` : ''} · ${c.divs} division${c.divs > 1 ? 's' : ''}</b>
      <small>${c.leave !== undefined ? `At home in ${esc(g.regions[c.reg].name)} until ${date(c.leave)}` : `At ${esc(g.regions[c.reg].name)}`}${gen ? ` · ${esc(rankOf(gen))} ${esc(gen.name)}` : ''}</small>
      <small>Raised ${c.heir !== undefined ? 'again ' : ''}${date(c.born ?? 1)}${c.home !== undefined ? ` at ${esc(g.regions[c.home].name)}` : ''} · ${stat(g, c, 'service')} months in service</small>`;
    // Home ground.
    if (c.home !== undefined && mine && c.kind !== 'fort') {
      const home = g.regions[c.home];
      extra += `<div class="note"><b>Home: ${esc(home.name)}.</b> ${c.rec?.exile ? `It is in enemy hands. The corps attacks ${Math.round(STORIES.home.attack * 100)}% stronger to win it back, and will take heart the day it is freed.` : `The corps defends it ${Math.round(STORIES.home.defence * 100)}% stronger than any other ground. If it falls to the enemy the corps loses heart.`}</div>`;
    }
    // An old enemy.
    const all = g.stories.rivals.filter((r) => r.mine === c.id);
    for (const r of all.slice(-2)) {
      const adj = g.nations[r.theirsNat]?.adj ?? '';
      extra += r.over
        ? `<div class="note"><b>Old enemy: ${esc(adj)} ${esc(r.theirsName)}.</b> ${r.over === 'won' ? `Destroyed by this corps in ${date(r.ended ?? r.since)}, after ${r.met} meetings.` : r.over === 'lost' ? `It had the better of this corps in the end, ${date(r.ended ?? r.since)}.` : `Lost elsewhere in ${date(r.ended ?? r.since)}. The score was never settled.`}</div>`
        : `<div class="note"><b>Old enemy: ${esc(adj)} ${esc(r.theirsName)}.</b> They first met at ${esc(g.regions[r.first]?.name ?? 'the front')} and have met ${r.met} times${r.won + r.lost ? `; since the men began to keep count, ${r.won} won and ${r.lost} lost` : ''}. This corps fights ${Math.round(STORIES.rivals.edge * 100)}% harder in any battle the other is in. Destroy it in battle and the corps gains cohesion, experience, an honour, and ${STORIES.rivals.pc} political capital for you.</div>`;
    }
    // Replacements.
    if (mine && c.kind !== 'fort') {
      const t = Math.round(training(g, c.nat) * 100);
      extra += `<h3>Replacements</h3><p class="sub" style="margin:0 0 7px">Drafts bring a corps back to strength, but men who have never been under fire thin out what it knows. Drafts reach the front at experience ${t} (${esc(grade(t / 100).toLowerCase())}); this corps stands at ${Math.round(c.xp * 100)}. A corps kept to veterans only stays as small as its losses leave it and keeps its edge.</p>
        <div class="seg"><button class="btn small${c.lean ? '' : ' on'}" data-act="drafts" data-id="${c.id}" data-v="take" ${up ? '' : 'disabled'}>Take drafts</button><button class="btn small${c.lean ? ' on' : ''}" data-act="drafts" data-id="${c.id}" data-v="lean" ${up ? '' : 'disabled'}>Veterans only</button></div>`;
    }
  } else if (kind === 'air') {
    const f = u as AirFleet;
    where = f.base;
    const ops = v('flown') + v('defended');
    badge = `<div class="lvbig"><b>${ops}</b><span>${ops === 1 ? 'OPERATION' : 'OPERATIONS'}</span></div>`;
    const need = honourDef('tour')?.needs[1] ?? 25;
    who = `<b>${n.adj} air fleet · ${esc(craft(f.ftr, f.bmr))}</b><small>Based at ${esc(g.regions[f.base].name)}${f.leader ? ` · ${esc(f.leader.name)}` : ''}</small>
      ${f.bmr > 0 && !hs.some((a) => a.hon === 'tour') ? `<small>Tour of duty: ${Math.min(need, v('missions'))} of ${need} bombing missions flown</small><div class="xp"><i style="width:${Math.min(100, Math.round((v('missions') / need) * 100))}%"></i></div>` : ''}`;
  } else {
    const f = u as Fleet;
    badge = `<div class="lvbig"><b>${v('actions')}</b><span>${v('actions') === 1 ? 'ACTION WON' : 'ACTIONS WON'}</span></div>`;
    who = `<b>${n.adj} fleet · ${esc(shipList(f.ships))}</b><small>${whereFleet(f)}${f.admiral ? ` · ${esc(f.admiral.name)}` : ''}</small>`;
  }
  const letters = lettersOf(kind, u.id);
  const waiting = hs.filter((a) => mine && !a.cash && !a.old && !honourDef(a.hon)?.last).length;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">${n.adj} ${serviceOf(kind)} · Service record</div><h2>${esc(u.name)}</h2>${u.nick ? `<div class="aka">${esc(capital(u.nick))}</div>` : ''}</header>
    <div class="body">
      <div class="record">${badge}<div class="who">${who}</div></div>
      <h3>Honours</h3>${hs.length ? `${hs.map((a) => honourBlock(a, true)).join('')}${waiting ? `<p class="sub">An honour carried at the front makes the unit fight harder. Open one to send its heroes home instead: on a bond tour, or to the schools as instructors.</p>` : ''}` : `<div class="empty">None yet. Honours are earned from the record: ${kind === 'corps' ? 'breakthroughs, stands against the odds, capitals entered, sieges outlasted, long service' : kind === 'air' ? 'a full tour of twenty-five bombing missions, a sky won, raids broken up, ships sunk' : 'a great ship sunk in a fleet action, a year on convoy escort, tonnage sunk on the trade routes'}.</div>`}
      <h3>Record</h3>${statGrid(kind, u, kind === 'corps' ? 3 : 1)}
      ${extra}
      <h3>Diary</h3>${diaryBlock(u, u)}
      ${letters.length ? `<h3>Letters</h3>${letters.slice(-4).reverse().map(letterBlock).join('')}` : ''}
    </div><footer>${where >= 0 ? `<button class="btn" data-act="showmap" data-id="${where}">Show on map</button>` : ''}<button class="btn go" data-act="close">Close</button></footer>`;
}

/** A formation on the memorial wall. */
function fallenCard(f: Fallen): string {
  const n = g.nations[f.nat];
  const mine = f.nat === g.human;
  const hs = honoursOf(g, f);
  const by = g.nations[f.by];
  const heir = f.heir !== undefined ? g.corps.find((c) => c.id === f.heir) : undefined;
  const queued = n.queue.some((b) => b.revive === f.id);
  const site = mine ? raiseSite(g, f.nat, f.kind, ui.sel) : -1;
  const at = site >= 0 ? g.regions[site] : null;
  const b = specOf(g, f.kind) ?? PRODUCTION.builds.inf;
  const have = Math.floor(n.pp);
  let again = '';
  if (heir) again = `<div class="note"><b>Raised again.</b> A new ${esc(f.name)} carries the old colours. <button class="link" data-act="record" data-type="corps" data-id="${heir.id}">Its record ›</button></div>`;
  else if (f.heir !== undefined) again = '<div class="note"><b>Raised again, and lost again.</b> The name has been carried twice.</div>';
  else if (queued) again = '<div class="note"><b>Being raised again.</b> A new corps is forming under the old name.</div>';
  else if (mine && b && at) again = `<h3>Raise it again</h3><button class="buy" data-act="revive" data-id="${f.id}" ${humanUp(g) && have >= b.cost ? '' : 'disabled'}>${icon({ nat: f.nat, kind: f.kind } as Corps)}<span class="grow"><b>Raise ${esc(f.name)} again</b><small>${b.divs} divisions · ready in ${b.months} months · forms up at ${esc(at.name)}</small><small>The new corps carries the old colours${f.nick ? ', the name the army gave it' : ''} and ${Math.round(STORIES.legacy.xp * 100)}% of what the old corps knew above a green one.</small></span><span class="cost${have >= b.cost ? '' : ' short'}">${money(b.cost)}</span></button>`;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Memorial wall · ${n.adj} army</div><h2>${esc(f.name)}</h2>${f.nick ? `<div class="aka">${esc(capital(f.nick))}</div>` : ''}</header>
    <div class="body">
      <p class="lead"><b>${fallenLine(f)}</b>${by && by.id !== f.nat ? `, ${f.how === 'destroyed' ? 'by' : f.how === 'surrendered' ? 'to' : 'cut off by'} the ${esc(by.adj)} army` : ''}. ${f.divs} ${KIND[f.kind].label.toLowerCase()} division${f.divs > 1 ? 's' : ''}, raised ${date(f.born)}${f.home >= 0 ? ` at ${esc(g.regions[f.home].name)}` : ''}; ${esc(grade(f.xp).toLowerCase())} troops at the end.${f.general ? ` ${esc(f.general)} was with it.` : ''}</p>
      ${hs.length ? `<h3>Honours</h3>${hs.map((a) => honourBlock(a, true)).join('')}` : ''}
      <h3>Record</h3>${statGrid('corps', f, 3)}
      ${again}
      ${f.log?.length ? `<h3>Diary</h3>${diaryBlock(f, { kind: f.kind })}` : ''}
    </div><footer><button class="btn" data-act="showmap" data-id="${f.reg}">Show on map</button><button class="btn go" data-act="close">Close</button></footer>`;
}

/** The citation for an honour, and the choice that comes with it: keep the fame at the front, or spend it at home. */
function awardCard(a: Award): string {
  const d = honourDef(a.hon);
  const n = g.nations[a.nat];
  const mine = a.nat === g.human;
  const u = unitOf(g, a.kind, a.unit);
  const up = humanUp(g);
  const C = STORIES.cash, fame = fameOf(a);
  const edge = edgeText(a);
  const where = a.kind === 'fleet' ? -1 : a.at >= 0 ? a.at : a.kind === 'corps' && u ? (u as Corps).reg : a.kind === 'air' && u ? (u as AirFleet).base : -1;
  const away = a.kind === 'corps'
    ? `The corps goes home for ${C.months} months and cannot be given orders.`
    : `Its best crews go home, and the ${a.kind === 'air' ? 'air fleet' : 'fleet'} loses ${Math.round(C.ready * 100)} points of readiness.`;
  let choice = '';
  if (d?.last) choice = '<div class="note"><b>A last honour.</b> It is carried on the memorial wall, and by any corps raised again in the name.</div>';
  else if (a.old) choice = '<div class="note"><b>Carried from the old corps.</b> The formation that bore this name before earned it. The new one wears the ribbon, and has its own name to make.</div>';
  else if (!mine) choice = '';
  else if (!u) choice = '<div class="note"><b>The unit that earned this honour is gone.</b> It is written on the memorial wall.</div>';
  else if (a.cash) choice = `<div class="note"><b>${a.cash === 'bonds' ? 'Sent home on a bond tour' : 'Its veterans went to the schools'}, ${date(a.cashed ?? a.turn)}.</b> The ribbon stays on the colours. The edge it gave at the front has been spent at home.</div>`;
  else {
    const opt = (how: Cash, head: string, blurb: string): string => {
      const why = whyNotCash(g, a, how) || (up ? '' : 'Wait for your turn.');
      return `<button class="buy choice" data-act="cash" data-id="${a.id}" data-how="${how}" ${why ? `disabled title="${esc(why)}"` : ''}><span class="grow"><b>${head}</b><small class="gain">${esc(capital(cashWorth(g, a, how)))}</small><small>${blurb}</small>${why && up ? `<small class="why">${esc(why)}</small>` : ''}</span></button>`;
    };
    const lose = a.kind === 'corps' ? ` It also gives up ${Math.round(C.schoolXp * fame * 100)} points of its own experience with the men who leave.` : '';
    choice = `<h3>What will you do with the fame?</h3>
      <div class="buy choice pick"><span class="grow"><b>Keep it at the front</b><small class="gain">${edge ? capital(`${esc(u.name)} ${edge}`) : 'The honour stays on the colours'}</small><small>This is how things stand now, and you can change your mind in any later month. Honours add up, to at most ${Math.round(STORIES.edgeCap * 100)}% for one unit.</small></span></div>
      ${opt('bonds', 'Send it home on a bond tour', `The heroes are paraded through the cities and the country digs into its pockets. ${away} The honour stops giving its edge.${n.pc >= DIPLOMACY.pcCap ? ' Your political capital is already as high as it can go, so only the production points would count.' : ''}`)}
      ${opt('school', 'Send its veterans to the schools', `The men who did it teach the next drafts how. ${away}${lose} The honour stops giving its edge.`)}`;
  }
  const more = unseen(g).filter((x) => x.id !== a.id).length;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">${mine ? 'Honour' : `${n.adj} honour`} · ${date(a.turn)}${a.first ? ` · first of the ${n.adj} ${serviceOf(a.kind)}` : ''}</div><h2>${esc(awardTitle(g, a))}</h2></header>
    <div class="body">
      <div class="cite">${ribbonOf(a, true)}<div><b>${esc(a.name)}</b>${u?.nick ? ` <i class="nick">${esc(u.nick)}</i>` : ''}<small>${esc(d?.name ?? '')}${placeOf(a.kind, a.at) ? ` · ${esc(placeOf(a.kind, a.at))}` : ''}</small></div></div>
      <p class="lead citation">${esc(citation(g, a))}</p>
      ${d?.real ? `<div class="note real"><b>In the real war.</b> ${esc(d.real)}</div>` : ''}
      ${choice}
    </div><footer>${u ? `<button class="btn" data-act="record" data-type="${a.kind}" data-id="${a.unit}">Service record</button>` : ''}${where >= 0 ? `<button class="btn" data-act="showmap" data-id="${where}">Show on map</button>` : ''}<button class="btn go" data-act="nextaward">${more ? `Next honour (${more})` : 'Close'}</button></footer>`;
}

/** The Honours tab: the player's honoured units, their old enemies, the memorial wall, and the roll of honour of the war. */
function honoursPane(): string {
  const me = g.human;
  const S = g.stories;
  const unitRow = (kind: UnitKind, u: AnyUnit, whose = false): string => {
    const at = kind === 'corps' ? esc(g.regions[(u as Corps).reg].name) : kind === 'air' ? esc(g.regions[(u as AirFleet).base].name) : esc(g.seas[(u as Fleet).zone]?.name ?? (u as Fleet).port);
    return `<button class="gen" data-act="record" data-type="${kind}" data-id="${u.id}"><span class="grow"><b>${esc(u.name)}${u.nick ? ` <i class="nick">${esc(u.nick)}</i>` : ''}</b><small>${whose ? `${g.nations[u.nat].adj} · ` : ''}${esc(statLine(kind, u))} · ${at}</small></span>${ribbons(u, 6)}</button>`;
  };
  const wallRow = (f: Fallen, whose = false): string => `<button class="gen gone" data-act="fallen" data-id="${f.id}"><span class="grow"><b>${esc(f.name)}${f.nick ? ` <i class="nick">${esc(f.nick)}</i>` : ''}</b><small>${whose ? `${g.nations[f.nat].adj} · ` : ''}${fallenLine(f)}${f.heir !== undefined ? ' · raised again' : ''}</small></span>${ribbons(f, 6)}</button>`;
  const rollRow = (a: Award): string => `<button class="gen" data-act="award" data-id="${a.id}">${ribbonOf(a, true)}<span class="grow"><b>${esc(a.name)}</b><small>${g.nations[a.nat].adj} · ${esc(awardTitle(g, a))} · ${date(a.turn)}</small></span></button>`;
  const units = (nat: string | null): [UnitKind, AnyUnit][] => [
    ...g.corps.filter((c) => !c.dead && (!nat || c.nat === nat) && c.hon?.length).map((c): [UnitKind, AnyUnit] => ['corps', c]),
    ...g.airFleets.filter((f) => (!nat || f.nat === nat) && f.hon?.length).map((f): [UnitKind, AnyUnit] => ['air', f]),
    ...g.fleets.filter((f) => (!nat || f.nat === nat) && f.hon?.length).map((f): [UnitKind, AnyUnit] => ['fleet', f]),
  ].sort((x, y) => (y[1].hon?.length ?? 0) - (x[1].hon?.length ?? 0));

  if (!me) {
    const roll = S.awards.filter((a) => !a.old).slice(-40).reverse();
    const famous = units(null).slice(0, 12);
    const wall = S.fallen.slice(-25).reverse();
    return `<h2>Honours of the war</h2><p class="sub">Every unit keeps a record of what it has done, and when it has done enough it is honoured: a breakthrough, a stand against the odds, a capital entered, a full tour of missions. Open any of them to read its story.</p>
      <div class="facts"><div class="fact"><span>Honours given</span><b>${S.awards.filter((a) => !a.old).length}</b></div><div class="fact"><span>Units honoured</span><b>${units(null).length}</b></div><div class="fact"><span>On the wall</span><b>${S.fallen.length}</b></div></div>
      <h3>The most honoured units</h3>${famous.length ? famous.map(([k, u]) => unitRow(k, u, true)).join('') : '<div class="empty">No unit has made a name yet.</div>'}
      <h3>Roll of honour</h3>${roll.length ? roll.map(rollRow).join('') : '<div class="empty">No honours yet.</div>'}
      <h3>Memorial wall</h3>${wall.length ? wall.map((f) => wallRow(f, true)).join('') : '<div class="empty">No formation has been destroyed yet.</div>'}`;
  }

  const mine = units(me);
  const held = S.awards.filter((a) => a.nat === me && !a.old);
  const waiting = unseen(g);
  const wall = S.fallen.filter((f) => f.nat === me).reverse();
  const feuds = S.rivals.filter((r) => !r.over);
  const settled = S.rivals.filter((r) => r.over === 'won');
  const schools = S.schools[me] ?? 0;
  const t = Math.round(training(g, me) * 100);
  const others = S.awards.filter((a) => a.nat !== me && !a.old && (a.first || fameOf(a) >= 3)).slice(-12).reverse();
  let out = `<h2>Honours</h2><p class="sub">Every unit keeps a record of what it has done. When it has done enough it is honoured, and what you do with the fame is yours to decide: keep it at the front, where the honour makes the unit fight harder, or send its heroes home to sell war bonds or train the next drafts.</p>
    <div class="facts"><div class="fact"><span>Honours held</span><b>${held.length}</b></div><div class="fact"><span>Cadres at the schools</span><b>${schools} / ${STORIES.veterans.maxSchools}</b></div><div class="fact"><span>On the wall</span><b>${wall.length}</b></div></div>
    <p class="sub">Drafts reach the front at experience ${t} (${esc(grade(t / 100).toLowerCase())}), and a new corps starts at ${Math.round(raisedXp(g, me) * 100)}. Every cadre of veterans sent to the schools raises both.</p>`;
  if (waiting.length) out += `<div class="note"><b>${waiting.length} ${waiting.length === 1 ? 'citation is' : 'citations are'} waiting to be read.</b> <button class="btn small" data-act="nextaward" style="margin-left:6px">Read ${waiting.length === 1 ? 'it' : 'them'}</button></div>`;
  out += `<h3>Your honoured units</h3>${mine.length ? mine.map(([k, u]) => unitRow(k, u)).join('') : '<div class="empty">None yet. Honours are earned from the record: breakthroughs, stands against the odds, capitals entered, sieges outlasted, long service, a full tour of bombing missions. Every unit has a <b>Record</b> strip under its name; open it to see how it stands.</div>'}`;
  if (feuds.length || settled.length) {
    out += '<h3>Old enemies</h3>';
    for (const r of [...feuds, ...settled.slice(-3)]) {
      const c = g.corps.find((x) => x.id === r.mine && !x.dead);
      const adj = g.nations[r.theirsNat]?.adj ?? '';
      const text = r.over ? `Settled ${date(r.ended ?? r.since)} after ${r.met} meetings` : `Met ${r.met} times, first at ${esc(g.regions[r.first]?.name ?? 'the front')}${r.won + r.lost ? ` · ${r.won} won, ${r.lost} lost since` : ''}`;
      out += `<${c ? 'button' : 'div'} class="gen${r.over ? ' gone' : ''}" ${c ? `data-act="record" data-type="corps" data-id="${c.id}"` : ''}><span class="grow"><b>${esc(r.mineName)} <span style="font-weight:400;color:var(--ink-3)">against</span> ${esc(adj)} ${esc(r.theirsName)}</b><small>${text}</small></span></${c ? 'button' : 'div'}>`;
    }
  }
  out += `<h3>Memorial wall</h3>${wall.length ? `${wall.slice(0, 30).map((f) => wallRow(f)).join('')}<p class="sub">A formation on the wall can be raised again under its old name from <b>Build</b>. It carries the old colours and part of what the old corps knew.</p>` : '<div class="empty">No formation of yours has been destroyed.</div>'}`;
  out += `<h3>Honours of the other armies</h3>${others.length ? others.map(rollRow).join('') : '<div class="empty">None of note yet.</div>'}`;
  return out;
}

function reelPane(): string {
  if (!g.headlines.length) return '<div class="empty">No news yet.</div>';
  let out = '<h2>Newsreel</h2><div class="reel">';
  let t = -1;
  for (let i = g.headlines.length - 1; i >= 0; i--) {
    const h = g.headlines[i];
    if (h.turn !== t) { t = h.turn; out += `<div class="mo">${date(t)}</div>`; }
    out += h.kind === 'letter' ? letterBlock(h) : `<p class="${h.kind}"><span class="wk">Week ${h.week}</span>${esc(h.text)}</p>`;
  }
  return out + '</div>';
}

function verdictFor(rep: BattleReport): { text: string; cls: string } {
  const me = g.human;
  const defSide = me !== null && rep.defenders.some((u) => u.nat === me) && !rep.attackers.some((u) => u.nat === me);
  // Ground given up under orders is neither a victory nor a defeat for the troops who left it.
  if (rep.yielded) return defSide ? { text: 'Fell back', cls: '' } : { text: 'Taken unopposed', cls: 'win' };
  if (!defSide) return { text: rep.level, cls: rep.taken ? 'win' : 'loss' };
  // Seen from the defender's side of the hill.
  const flip: Record<string, string> = {
    'Heroic victory': 'Crushing defeat', 'Decisive victory': 'Decisive defeat', 'Close victory': 'Close defeat', 'Pyrrhic victory': 'Close defeat',
    'Close defeat': 'Close victory', 'Decisive defeat': 'Decisive victory', 'Crushing defeat': 'Heroic victory',
  };
  return { text: flip[rep.level] ?? rep.level, cls: rep.taken ? 'loss' : 'win' };
}

function seaVerdict(r: SeaReport): { text: string; cls: string } {
  const me = g.human;
  const defSide = me !== null && r.def.some((u) => u.nat === me) && !r.att.some((u) => u.nat === me);
  if (!defSide) return { text: r.level, cls: r.won ? 'win' : 'loss' };
  const flip: Record<string, string> = { 'Decisive victory': 'Decisive defeat', 'Close victory': 'Close defeat', 'Close defeat': 'Close victory', 'Decisive defeat': 'Decisive victory' };
  return { text: flip[r.level] ?? r.level, cls: r.won ? 'loss' : 'win' };
}

function seaRow(r: SeaReport): string {
  const v = seaVerdict(r);
  return `<button class="brow" data-act="seareport" data-id="${r.id}"><b>${esc(r.zoneName)}</b><span class="res ${v.cls}">${v.text}</span><span>${date(r.turn)} · fleet action, ${g.nations[r.attNat].adj} against ${g.nations[r.defNat].adj} ships</span></button>`;
}

function airVerdict(r: AirReport): { text: string; cls: string } {
  const me = g.human;
  const defSide = me !== null && r.def.some((u) => u.nat === me) && !r.att.some((u) => u.nat === me);
  if (!defSide) return { text: r.level, cls: r.won ? 'win' : 'loss' };
  const flip: Record<string, string> = { 'Decisive victory': 'Decisive defeat', 'Close victory': 'Close defeat', 'Close defeat': 'Close victory', 'Decisive defeat': 'Decisive victory' };
  return { text: flip[r.level] ?? r.level, cls: r.won ? 'loss' : 'win' };
}

function airTitle(r: AirReport): string {
  return r.mission === 'superiority' ? `The sky over ${r.place}` : r.mission === 'bomb' ? `Bombing of ${r.place}` : r.mission === 'night' ? `Night raids on ${r.place}` : `Air strikes, ${r.place.replace(/^the /, '')}`;
}

function airBrow(r: AirReport): string {
  const v = airVerdict(r);
  return `<button class="brow" data-act="airreport" data-id="${r.id}"><b>${esc(airTitle(r))}</b><span class="res ${v.cls}">${v.text}</span><span>${date(r.turn)} · air campaign, ${g.nations[r.attNat].adj} against ${r.defNat ? g.nations[r.defNat].adj : 'enemy'} ${r.mission === 'strike' ? 'shipping' : r.mission === 'superiority' ? 'fighters' : 'industry'}</span></button>`;
}

function landRow(r: BattleReport): string {
  const v = verdictFor(r);
  return `<button class="brow" data-act="report" data-id="${r.id}"><b>${esc(r.regionName)}</b><span class="res ${v.cls}">${v.text}</span><span>${date(r.turn)}, week ${r.week} · ${g.nations[r.attNat].adj} attack on ${g.nations[r.defNat].adj} forces</span></button>`;
}

function battlesPane(): string {
  if (!g.reports.length && !g.seaReports.length && !g.airReports.length) return '<h2>Battles</h2><div class="empty">No battles yet. Every battle is settled from its Battle Ticket and filed here.</div>';
  // Newest first. Within a month the land battles are fought first, then the air campaigns, then the fleet actions.
  const rows: { turn: number; rank: number; i: number; html: () => string }[] = [];
  g.reports.forEach((r, i) => rows.push({ turn: r.turn, rank: 0, i, html: () => landRow(r) }));
  g.airReports.forEach((r, i) => rows.push({ turn: r.turn, rank: 1, i, html: () => airBrow(r) }));
  g.seaReports.forEach((r, i) => rows.push({ turn: r.turn, rank: 2, i, html: () => seaRow(r) }));
  rows.sort((a, b) => b.turn - a.turn || b.rank - a.rank || b.i - a.i);
  return '<h2>Battles</h2><p class="sub">Open any battle, on land, at sea or in the air, to see its Battle Ticket: what the campaign map fed into the fight.</p><div style="height:8px"></div>'
    + rows.slice(0, 90).map((x) => x.html()).join('');
}

function powersPane(): string {
  const at = Object.values(g.nations).filter((n) => !n.dormant && (n.atWar.length || n.fellOn));
  const neutral = Object.values(g.nations).filter((n) => n.alive && !n.atWar.length && !n.dormant);
  let out = '<h2>Powers at war</h2><p class="sub">The bar shows how much fight a nation has left. It drains as the capital, territory and army are lost.</p><div style="height:10px"></div>';
  for (const n of at.sort((a, b) => (b.alive ? 1 : 0) - (a.alive ? 1 : 0) || b.armyPeak - a.armyPeak)) {
    if (n.alive) {
      const will = Math.max(0, 1 - collapseScore(g, n.id) / n.resilience);
      out += `<div class="nat">${chip(n.id)}<b>${esc(n.name)}</b><small>${num(fieldArmy(g, n.id))} divisions</small><div class="will"><div class="meter"><i class="${will < 0.35 ? 'low' : ''}" style="width:${Math.round(will * 100)}%"></i></div><small>At war with ${n.atWar.map(natName).join(', ')}</small></div></div>`;
    } else {
      out += `<div class="nat" style="opacity:.7">${chip(n.id)}<b>${esc(n.name)}</b><small>fell ${date(n.fellOn!.turn)}</small><div class="will"><small>Asked ${natName(n.fellOn!.to)} for terms in week ${n.fellOn!.week}</small></div></div>`;
    }
  }
  out += trackPane(neutral);
  return out;
}

/** The alignment track: where every country at peace stands between the two blocs, and which way it is drifting. */
function trackPane(list: Nation[]): string {
  const me = g.human && g.nations[g.human].alive ? g.nations[g.human] : null;
  const mine = me ? bloc(me) : null;
  const m = momentum(g);
  const wind = Math.abs(m) < 0.15 ? 'Neither side has the upper hand, so neutrals are sitting tight.' : `${m > 0 ? 'The Axis is' : 'The Allies are'} winning, and neutrals ${Math.abs(m) > 0.6 ? 'are bending' : 'are starting to bend'} that way.`;
  let out = `<h3>The alignment track</h3><p class="sub">Every country stands somewhere between the two blocs. The dot is its public opinion. ${wind}</p>`;
  if (me && mine) {
    const partners = tradePartners(g, me.id).length;
    out += `<div class="facts"><div class="fact"><span>Political capital</span><b>${Math.floor(me.pc)}</b></div><div class="fact"><span>Each month</span><b>+${pcIncome(me.id)}</b></div><div class="fact"><span>Trade partners</span><b>${partners}</b></div></div>
      <p class="sub" style="margin-top:6px">Courting a neutral costs ${DIPLOMACY.court.cost} and pulls it toward ${mine === 'axis' ? 'the Axis' : 'the Allies'}. A country that leans your way trades with you; one that is fully won over joins your bloc.</p>`;
  }
  out += `<div class="trackhead"><span>◂ Allies</span><span>Independent</span><span>Axis ▸</span></div>`;
  const rows = list.filter((n) => Math.abs(step(n)) <= 1 || !n.atWar.length).sort((a, b) => a.opinion - b.opinion);
  for (const n of rows) {
    const pos = 50 + Math.max(-100, Math.min(100, n.opinion)) / 2;
    let btn = '';
    if (me && mine && courtable(n) && humanUp(g)) {
      const c = canCourt(g, me.id, n.id);
      const k = receptive(n, mine);
      const word = k >= 1.3 ? 'Receptive' : k >= 0.8 ? 'Open to offers' : k >= 0.5 ? 'Wary' : 'Very hard to move';
      btn = `<button class="btn small" data-act="court" data-nat="${n.id}" ${c.ok ? '' : 'disabled'} title="${esc(c.ok ? word : c.why)}">Court</button>`;
    }
    out += `<div class="trk">${chip(n.id)}<b>${esc(n.name)}</b><small>${TRACK_LABEL[n.align]}</small>${btn}
      <div class="track"><i class="t" style="left:15%"></i><i class="t" style="left:35%"></i><i class="t mid" style="left:50%"></i><i class="t" style="left:65%"></i><i class="t" style="left:85%"></i><i class="dot" style="left:${pos.toFixed(1)}%"></i></div></div>`;
  }
  out += `<p class="sub" style="margin-top:8px">The inner marks are where a country starts to lean; the outer marks are where it asks to join.</p>`;
  return out;
}

// ---- The war at sea -----------------------------------------------------------------------

function whereFleet(f: Fleet): string {
  if (!atSea(f)) return `In harbour at ${esc(f.port)}`;
  return `${f.stance === 'raid' ? 'Raiding shipping in' : 'Patrolling'} the ${esc(g.seas[f.zone].name)} from ${esc(f.port)}`;
}

function fleetRow(f: Fleet, mine: boolean, whose = false): string {
  const adm = f.admiral ? ` <span style="font-weight:400;color:var(--ink-2)">· ${esc(f.admiral.name)} <span class="lvl" title="Level ${f.admiral.level} admiral">${f.admiral.level}</span></span>` : '';
  const lost = shipCount(f.lost);
  const el = mine ? 'button' : 'div';
  return `<${el} class="corps fleet${mine && ui.fleet === f.id ? ' pick' : ''}" ${mine ? `data-act="fleet" data-id="${f.id}"` : ''}>
    ${hullIcon(f)}<span class="nm">${esc(f.name)}${f.nick ? ` <i class="nick">${esc(f.nick)}</i>` : ''}${adm}</span><span></span>
    <span class="meta">${whose ? `${g.nations[f.nat].adj} · ` : ''}${whereFleet(f)}</span>
    <span class="meta">${esc(capital(shipList(f.ships)))}${lost ? ` · lost so far: ${esc(shipList(f.lost))}` : ''}</span>
    <span class="bars"><span>Readiness</span><span class="meter"><i class="${f.ready < 0.5 ? 'low' : ''}" style="width:${Math.round(f.ready * 100)}%"></i></span></span></${el}>${recStrip('fleet', f)}`;
}

/** The orders the player can give the fleet that is picked. */
function fleetOrders(f: Fleet): string {
  const rows = fleetReach(g, f).map((z) => {
    const them = seaSide(g, z, f.nat, true), us = seaSide(g, z, f.nat, false);
    const here = atSea(f) && f.zone === z;
    const foe = them.total >= 1 ? `Enemy strength ${num(them.total)}${them.air > them.ships ? ', mostly aircraft' : ''}` : 'No enemy seen';
    return `<button class="pickrow${here ? ' pick' : ''}" data-act="sail" data-zone="${z}"><span class="grow"><b>${esc(g.seas[z].name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${foe} · your side ${num(us.total)}</span></span>${here ? '<span class="tag good">Here now</span>' : ''}</button>`;
  }).join('');
  return `<div class="orders">
    <div class="acts"><button class="btn small${ui.stance === 'patrol' ? ' on' : ''}" data-act="stance" data-stance="patrol" title="Hold the sea: bring enemy fleets to battle, guard convoys and troopships.">Patrol</button><button class="btn small${ui.stance === 'raid' ? ' on' : ''}" data-act="stance" data-stance="raid" title="Hunt merchant ships and keep clear of the enemy's fleet.">Raid shipping</button><button class="btn small" data-act="toport" ${atSea(f) ? '' : 'disabled'} title="A fleet in harbour refits quickly.">Return to port</button><button class="btn small${ui.rebase ? ' on' : ''}" data-act="rebase">Change base</button></div>
    <p class="sub" style="margin:8px 0 6px">Fleet strength ${num(fleetPower(g, f))}. It can reach the seas within ${NAVY.range} of ${esc(f.port)}: click a ringed sea on the map, or pick one here. It will ${ui.stance === 'raid' ? 'raid shipping' : 'patrol'} there.</p>${rows}</div>`;
}

/** What a route that feeds an army overseas means for that army this month. */
function desertNote(route: (typeof NAVY.convoys)[number]): string {
  if (route.id !== DESERT.route.id) return '';
  const extra = convoyDrag(g, route.to);
  const line = extra > 0.05 ? `Cargoes sunk make the Axis supply line in Libya ${extra.toFixed(1)} regions longer this month.` : 'The Axis army in Libya is getting what it is sent.';
  const b = route.base ? g.byName[route.base.region] : null;
  if (!b) return ` ${line}`;
  if (!atWar(g, b.ctrl, route.to)) return ` ${line} ${esc(b.name)} no longer threatens the route.`;
  const starved = !!g.supply[b.ctrl] && !g.supply[b.ctrl][b.id];
  return ` ${line} ${esc(b.name)} is hunting the convoys${starved ? ', but is itself cut off and short of everything' : ''}. In Axis hands the route would be safe.`;
}

/** The convoy war and the ore traffic, for whoever has a stake in them. */
function seaNotes(): string {
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  let out = '';
  for (const route of NAVY.convoys) {
    const m = g.convoys[route.id];
    if (!m || (me && me !== route.to && !atWar(g, me, route.to))) continue;
    const share = m.raid + m.guard > 0 ? m.raid / (m.raid + m.guard * NAVY.convoy.escortWorth) : 0;
    out += `<div class="note"><b>${esc(capital(route.name))}.</b> ${num(m.tons)} tons of shipping sunk ${m.turn === g.turn ? 'this month' : 'last month'}, cutting ${g.nations[route.to].adj} production by ${(m.share * 100).toFixed(m.share < 0.095 ? 1 : 0)}%. ${m.subsLost} submarine${m.subsLost === 1 ? '' : 's'} and ${m.escortsLost} escort${m.escortsLost === 1 ? '' : 's'} lost. ${num(m.total)} tons since the war began.
      <div class="oddsbar" title="Raiders against escorts"><i style="width:${Math.round(share * 100)}%;background:var(--red)"></i><i style="flex:1;background:#6f9fd0"></i></div><span style="font-size:11.5px">Raiders ${num(m.raid)} against escorts ${num(m.guard)}. More escorts and patrolling fleets on the route cut the losses.${desertNote(route)}</span></div>`;
  }
  const ore = oreRoute(g);
  const to = NAVY.ore.to;
  if (ore !== 'none' && (!me || me === to || atWar(g, me, to))) {
    const text = ore === 'baltic' ? `Swedish iron ore is reaching ${natName(to)} across the Baltic.` : ore === 'narvik' ? `The Baltic ports are frozen. Swedish iron ore is reaching ${natName(to)} through ${NAVY.ore.winterPort}.` : `Swedish iron ore is not getting through. ${natName(to)} loses ${Math.round(NAVY.ore.share * 100)}% of its production until the route is open again.`;
    out += `<div class="note"${ore === 'cut' ? ' style="border-left-color:var(--red)"' : ''}><b>The ore traffic.</b> ${esc(text)}</div>`;
  }
  return out;
}

function navyPane(): string {
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  const afloat = (f: Fleet): boolean => shipCount(f.ships) > 0;
  const size = (a: Fleet, b: Fleet): number => fleetPower(g, b) - fleetPower(g, a);
  let out = '';
  if (me) {
    const mine = fleetsOf(g, me).filter(afloat);
    const n = g.nations[me];
    const order = humanUp(g);
    out += `<h2>The ${n.adj} navy</h2><p class="sub">A fleet holds a sea by patrolling it. Where one side is much the stronger, that sea is closed to the other's troopships and supply ships. Ships at sea wear down, so bring a fleet home to refit after a fight.</p>
      <div class="facts"><div class="fact"><span>Ships</span><b>${num(mine.reduce((t, f) => t + shipCount(f.ships), 0))}</b></div><div class="fact"><span>Fleets at sea</span><b>${mine.filter(atSea).length} of ${mine.length}</b></div><div class="fact"><span>Sea lift left</span><b>${n.sealift > 0 ? `${liftLeft(g, me)} corps` : 'None'}</b></div></div>${budgeted(g) ? '<div class="acts"><button class="btn small" data-act="armgo" data-arm="navy" title="Lay down warships and troopships in your shipyards.">Lay down more ships</button></div>' : ''}`;
    if (!n.atWar.length) out += `<div class="note"><b>At peace.</b> The fleet stays in harbour until the nation is at war.</div>`;
    out += seaNotes();
    out += `<h3>Your fleets</h3>${mine.length ? mine.map((f) => fleetRow(f, order && n.atWar.length > 0) + (order && ui.fleet === f.id && n.atWar.length ? fleetOrders(f) : '')).join('') : '<div class="empty">No ships under your flag.</div>'}`;
    const friends = g.fleets.filter((f) => f.nat !== me && afloat(f) && allied(g, f.nat, me)).sort(size);
    const foes = g.fleets.filter((f) => afloat(f) && atWar(g, f.nat, me)).sort(size);
    if (friends.length) out += `<h3>Allied fleets</h3>${friends.map((f) => fleetRow(f, false, true)).join('')}`;
    if (foes.length) out += `<h3>Enemy fleets</h3>${foes.map((f) => fleetRow(f, false, true)).join('')}`;
    return out;
  }
  out += `<h2>The war at sea</h2><p class="sub">A fleet holds a sea by patrolling it. Where one side is much the stronger, that sea is closed to the other's troopships and supply ships. Click any sea on the map to see who holds it.</p>${seaNotes()}`;
  const war = g.fleets.filter((f) => afloat(f) && g.nations[f.nat].atWar.length > 0);
  const nats = [...new Set(war.map((f) => f.nat))].sort((a, b) => war.filter((f) => f.nat === b).reduce((t, f) => t + fleetPower(g, f), 0) - war.filter((f) => f.nat === a).reduce((t, f) => t + fleetPower(g, f), 0));
  for (const nat of nats) out += `<h3>${esc(natName(nat))}</h3>${war.filter((f) => f.nat === nat).map((f) => fleetRow(f, false)).join('')}`;
  const peace = g.fleets.filter((f) => afloat(f) && !g.nations[f.nat].atWar.length).sort(size);
  if (peace.length) out += `<h3>Navies at peace</h3>${peace.map((f) => fleetRow(f, false, true)).join('')}`;
  return out;
}

/** One sea: who holds it, which fleets are in it, and what it would cost to cross. */
function seaPane(): string {
  const z = g.seas[ui.sea];
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  const afloat = (f: Fleet): boolean => shipCount(f.ships) > 0;
  const here = g.fleets.filter((f) => f.zone === z.id && atSea(f) && afloat(f));
  const docked = g.fleets.filter((f) => f.home === z.id && !atSea(f) && afloat(f));
  let head: string;
  if (me && g.nations[me].atWar.length) {
    const us = seaSide(g, z.id, me, false), them = seaSide(g, z.id, me, true);
    const loss = Math.round(seaRisk(g, z.id, me) * NAVY.intercept.loss * 100);
    head = `<div class="sub">${closedTo(g, z.id, me) ? 'Closed to your troopships and supply ships: the enemy is much the stronger here.' : them.total >= 1 ? 'Contested, but still open to your shipping.' : 'Open to your shipping.'}</div>
      <div class="facts"><div class="fact"><span>Your side</span><b>${num(us.total)}</b></div><div class="fact"><span>The enemy</span><b>${num(them.total)}</b></div><div class="fact"><span>Lost crossing</span><b>${loss ? `about ${loss}%` : 'None'}</b></div></div>
      <p class="sub" style="margin-top:6px">Strength counts the fleets at sea here and the aircraft that can reach this sea from the coasts each side holds${us.air + them.air >= 1 ? `: yours ${num(us.air)}, the enemy's ${num(them.air)}` : ''}. The last figure is the share of a corps likely to be lost if it crossed this sea now.</p>`;
  } else head = `<div class="sub">${z.coast.length} coastal region${z.coast.length === 1 ? '' : 's'} on this sea.</div>`;
  let straits = '';
  for (const gt of g.gates) {
    if (gt.a !== z.id && gt.b !== z.id) continue;
    const other = g.seas[gt.a === z.id ? gt.b : gt.a];
    const holder = gt.region >= 0 ? g.regions[gt.region].ctrl : gt.nation!;
    straits += `<div class="note"><b>${esc(capital(gt.name))}.</b> The way to the ${esc(other.name)}, held by ${esc(natName(holder))}.${me ? ` ${canPass(g, gt.a, gt.b, me) ? 'Open to your ships.' : 'Closed to your ships.'}` : ''}</div>`;
  }
  return `<h2>${esc(z.name)}</h2>${head}${straits}
    <h3>Fleets at sea here</h3>${here.length ? here.map((f) => fleetRow(f, false, true)).join('') : '<div class="empty">No fleet at sea here.</div>'}
    ${docked.length ? `<h3>In harbour on this sea</h3>${docked.map((f) => fleetRow(f, false, true)).join('')}` : ''}
    ${me && humanUp(g) ? '<p class="sub" style="margin-top:10px">To send a fleet here, open <b style="color:var(--ink)">Navy</b>, pick the fleet, then click this sea.</p>' : ''}`;
}

function seaReportCard(r: SeaReport): string {
  const v = seaVerdict(r);
  const count = (ss: SeaSide[], k: 'ships' | 'sunk'): number => ss.reduce((n, x) => n + shipCount(x[k]), 0);
  const side = (ss: SeaSide[]): string => `<div class="big">${count(ss, 'ships')}</div><small>ships in ${ss.length} fleet${ss.length > 1 ? 's' : ''} · lost ${count(ss, 'sunk')}</small>`;
  const list = (ss: SeaSide[]): string => `<ul class="ulist">${ss.map((x) => `<li><span>${esc(x.fleet)} <span style="color:var(--ink-3)">(${g.nations[x.nat].adj}${x.admiral ? `, ${esc(x.admiral)}` : ''})</span><br><span style="color:var(--ink-2)">${esc(capital(shipList(x.ships)))}</span></span><span>${shipCount(x.sunk) ? `lost ${esc(shipList(x.sunk))}` : 'no ships lost'}</span></li>`).join('')}</ul>`;
  const beaten = g.nations[r.won ? r.defNat : r.attNat].adj;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Battle Ticket · fleet action · ${date(r.turn)}</div><h2>Battle of the ${esc(r.zoneName)}</h2></header>
    <div class="body">
      <div class="verdict ${v.cls}">${v.text}</div>
      <div class="sub">The ${beaten} ships broke off and made for port. Final odds ${r.odds.toFixed(2)} to 1.</div>
      <div class="vs"><div><h4>${chip(r.attNat)} ${natName(r.attNat)} offering battle</h4>${side(r.att)}</div><div class="mid">vs</div><div class="r"><h4>${natName(r.defNat)} ${chip(r.defNat)}</h4>${side(r.def)}</div></div>
      <h3>What the campaign map fed into the battle</h3>
      <table class="ticket">${r.lines.map((l) => `<tr><td>${esc(l.label)}</td><td>${esc(l.value)}</td><td class="fx">${esc(l.effect ?? '')}</td></tr>`).join('')}</table>
      <h3>Offering battle</h3>${list(r.att)}<h3>Brought to battle</h3>${list(r.def)}
      <div class="note" style="margin-top:14px"><b>Auto-resolved.</b> A fleet action is settled from its ticket in the same way as a land battle, so the full game can hand it to a 3D sea battle and take the same kind of result back.</div>
    </div><footer><button class="btn" data-act="showsea" data-id="${r.zone}">Show on map</button><button class="btn go" data-act="close">Close</button></footer>`;
}

/** The plan for a landing from the sea: the crossing, the odds on the beach, and the order to go. */
function landingCard(): string {
  const m = ui.modal!;
  const target = g.regions[m.target];
  const cs = g.corps.filter((c) => m.corps.has(c.id));
  const plan = previewLanding(g, cs, target.id);
  let body: string;
  if (plan.why) body = `<div class="empty">${esc(plan.why)}</div>`;
  else {
    const a = plan.assess;
    const air = !!plan.air;
    const route = plan.path.map((z) => g.seas[z].name).join(', then the ');
    const loss = Math.round(plan.risk * NAVY.intercept.loss * 100);
    const crossing = plan.surprise
      ? `War has only just been declared, so the enemy is caught unready: no fleet can stop the crossing this month, and the troops go ashore at full strength.`
      : loss > 0 ? `${plan.sortie >= 1 ? 'The enemy fleet will put to sea against the invasion' : 'Enemy ships and aircraft are at work on the way'}${plan.sky ? ', but your air force holds the sky over the beach and will keep most of it off the troopships' : ''}. Expect to lose about ${loss}% of the force at sea.` : 'No enemy ship or aircraft is in a position to stop the crossing.';
    let odds = `<div class="verdict win" style="font-size:24px">The coast is undefended</div><div class="sub">The force walks ashore and takes ${esc(target.name)}.</div>`;
    if (a) {
      const p = Math.max(0, Math.min(1, (1.2 - 1.2 / a.odds) / 0.4));
      const share = a.A / (a.A + a.D);
      const word = p >= 0.95 ? 'Victory is all but certain' : p >= 0.6 ? 'The odds favour you' : p >= 0.3 ? 'It could go either way' : p > 0 ? 'A long shot' : air ? 'This assault will fail' : 'This landing will fail';
      odds = `<div class="verdict ${p >= 0.6 ? 'win' : p >= 0.3 ? '' : 'loss'}" style="font-size:24px">${word}</div>
        <div class="oddsbar"><i style="width:${Math.round(share * 100)}%;background:${g.nations[g.human!].color}"></i><i style="flex:1;background:${g.nations[a.defenders[0].nat].color}"></i></div>
        <div class="sub">Odds ${a.odds.toFixed(2)} to 1 · about ${Math.round(p * 100)}% chance to take ${esc(target.name)}${air ? ` · airborne troops without their heavy weapons count for ${Math.round(NAVY.landing.airborne * 100)}%` : plan.surprise ? '' : ` · troops fighting their way off the beach count for ${Math.round(assaultStrength(g, cs[0].nat) * 100)}%`}</div>`;
    }
    body = `${odds}
      ${air ? `<h3>The flight</h3><p class="lead" style="font-size:13px">Transport aircraft and gliders carry the force across the water. It needs no ships and no fleet can stop it, but the transports can mount only one assault a month${plan.sky ? ', and your air force holds the sky over the target' : ''}.</p>` : `<h3>The crossing</h3><p class="lead" style="font-size:13px">By way of the ${esc(route)}. ${crossing}</p>`}
      ${a ? `<h3>Battle Ticket</h3><table class="ticket">${a.lines.map((l) => `<tr><td>${esc(l.label)}</td><td>${esc(l.value)}</td><td class="fx">${esc(l.effect ?? '')}</td></tr>`).join('')}</table>` : ''}
      <h3>${air ? 'Going in' : 'Going ashore'}</h3>${cs.map((c) => `<div class="pickrow pick">${icon(c)}<span class="grow"><b>${esc(c.name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">from ${esc(g.regions[c.reg].name)} · ${c.divs} division${c.divs > 1 ? 's' : ''} · ${num(men(c))} men</span></span></div>`).join('')}
      <div class="note"><b>Once ${air ? 'down' : 'ashore'}</b> the force is supplied through a port you hold on a sea the enemy has not closed. If your navy loses that sea, the troops are cut off.</div>`;
  }
  const fromAir = byAir(cs);
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">${fromAir ? 'Airborne assault' : 'Landing from the sea'} · ${date(g.turn)}</div><h2>${fromAir ? 'Assault on' : 'Landing at'} ${esc(target.name)}</h2></header>
    <div class="body">${body}</div>
    <footer><button class="btn" data-act="close">Call it off</button><button class="btn go" data-act="doland" ${plan.why ? 'disabled' : ''}>${fromAir ? 'Drop the troops' : 'Land the troops'}</button></footer>`;
}

// ---- The war in the air -------------------------------------------------------------------

const craft = (ftr: number, bmr: number): string => [ftr ? `${num(ftr)} fighter${ftr === 1 ? '' : 's'}` : '', bmr ? `${num(bmr)} bomber${bmr === 1 ? '' : 's'}` : ''].filter(Boolean).join(' and ') || 'no aircraft';

/** The player's air fleet being given orders, if it is still theirs to order. */
function myAir(): AirFleet | null {
  if (ui.air < 0 || !humanUp(g)) return null;
  const f = g.airFleets.find((x) => x.id === ui.air);
  return f && f.nat === g.human ? f : null;
}

function missionText(f: AirFleet): string {
  const where = f.mission === 'strike' ? `the ${g.seas[f.target]?.name ?? 'sea'}` : g.regions[f.target]?.name ?? '';
  switch (f.mission) {
    case 'support': return 'supporting the armies and defending its sky';
    case 'rest': return 'resting and refitting';
    case 'superiority': return `fighting for the sky over ${where}`;
    case 'bomb': return `bombing ${where} by day`;
    case 'night': return `bombing ${where} by night`;
    case 'strike': return `striking at shipping in ${where}`;
  }
}

function airRow(f: AirFleet, mine: boolean, whose = false): string {
  const ldr = f.leader ? ` <span style="font-weight:400;color:var(--ink-2)">· ${esc(f.leader.name)} <span class="lvl" title="Level ${f.leader.level} air commander">${f.leader.level}</span></span>` : '';
  const short = Math.max(0, f.est.ftr - f.ftr) + Math.max(0, f.est.bmr - f.bmr);
  const crowd = crowding(g, f);
  const tags: string[] = [];
  if (crowd < 1) tags.push(`<span class="tag bad" title="More aircraft are based in ${esc(g.regions[f.base].name)} than its airfields can serve. Move a fleet to a neighbouring region.">Crowded airfields: flies ${Math.round(crowd * 100)}%</span>`);
  if (short > (f.est.ftr + f.est.bmr) * 0.2) tags.push(`<span class="tag bad">${num(short)} aircraft short</span>`);
  if (f.moved === g.turn) tags.push('<span class="tag">Moved this month</span>');
  const lost = f.lost.ftr + f.lost.bmr;
  const el = mine ? 'button' : 'div';
  return `<${el} class="corps fleet${mine && ui.air === f.id ? ' pick' : ''}" ${mine ? `data-act="airfleet" data-id="${f.id}"` : ''}>
    ${planeIcon(f)}<span class="nm">${esc(f.name)}${f.nick ? ` <i class="nick">${esc(f.nick)}</i>` : ''}${ldr}</span><span></span>
    <span class="meta">${whose ? `${g.nations[f.nat].adj} · ` : ''}${esc(g.regions[f.base].name)}, ${esc(missionText(f))}</span>
    <span class="meta">${esc(capital(craft(f.ftr, f.bmr)))}${lost ? ` · lost so far: ${esc(craft(f.lost.ftr, f.lost.bmr))}` : ''}</span>
    <span class="bars"><span>Readiness</span><span class="meter"><i class="${f.ready < 0.5 ? 'low' : ''}" style="width:${Math.round(f.ready * 100)}%"></i></span></span>
    ${tags.length ? `<span class="tags">${tags.join('')}</span>` : ''}</${el}>${recStrip('air', f)}`;
}

/** The orders the player can give the air fleet that is picked. */
function airOrders(f: AirFleet): string {
  const R = airRange(f);
  const war = g.nations[f.nat].atWar.length > 0;
  const btn = (m: AirMission): string => {
    const off = isOffensive(m);
    const ok = !off || airTargets(g, f, m).length > 0;
    const why = !war ? 'The nation is at peace.' : m !== 'superiority' && !f.bmr ? 'It has no bombers.' : 'Nothing in reach to fly this against.';
    const on = off ? ui.airMode === m || (!ui.airMode && f.mission === m) : !ui.airMode && f.mission === m;
    return `<button class="btn small${on ? ' on' : ''}" data-act="mission" data-mission="${m}" ${ok ? '' : 'disabled'} title="${esc(ok ? MISSION[m].what : why)}">${MISSION[m].label}</button>`;
  };
  const moved = f.moved === g.turn;
  const btns = (['support', 'superiority', 'bomb', 'night', 'strike', 'rest'] as AirMission[]).map(btn).join('')
    + `<button class="btn small${ui.airMode === 'base' ? ' on' : ''}" data-act="airbase" ${moved ? 'disabled' : ''} title="${moved ? 'It has already changed base this month.' : 'Move to airfields in another friendly region. Settling in costs readiness.'}">Change base</button>`;
  let body: string;
  if (ui.airMode === 'base') {
    body = `<p class="sub" style="margin:8px 0 6px">Click any blue region on the map to move ${esc(f.name)} there. Settling in costs it ${Math.round(AIR.ready.move * 100)}% readiness, which a month of rest more than makes up. The airfields of one region serve about ${num(AIR.airfields.holds)} aircraft: pack in more and they cannot all fly.</p>`;
  } else if (ui.airMode) {
    const m = ui.airMode;
    const sea = m === 'strike';
    const ts = airTargets(g, f, m).map((t) => ({ t, d: sea ? seaKm(g, f.base, t) : airKm(g, f.base, t) })).sort((a, b) => a.d - b.d);
    const rows = ts.slice(0, 8).map(({ t, d }) => {
      const name = sea ? g.seas[t].name : g.regions[t].name;
      const c = cover(d, R.ftr);
      const esc2 = m === 'night' || !f.ftr ? '' : c >= 1 ? 'well inside fighter range' : c > 0 ? `fighters at ${Math.round(c * 100)}%` : 'beyond the fighters';
      const ind = !sea && m !== 'superiority' ? `industry ${g.regions[t].fac}` : '';
      const on = f.mission === m && f.target === t;
      return `<button class="pickrow${on ? ' pick' : ''}" data-act="airtarget" data-target="${t}"><span class="grow"><b>${esc(name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${[`${num(d)} km`, esc2, ind].filter(Boolean).join(' · ')}</span></span>${on ? '<span class="tag good">Ordered</span>' : ''}</button>`;
    }).join('');
    body = `<p class="sub" style="margin:8px 0 6px">${esc(MISSION[m].what)} Click a ${sea ? 'ringed sea' : 'red region'} on the map, or pick one of the nearest here, to see the odds before you commit.</p>${rows}${ts.length > 8 ? `<p class="sub">${ts.length - 8} more in reach: see the map.</p>` : ''}`;
  } else {
    body = `<p class="sub" style="margin:8px 0 6px">${esc(capital(missionText(f)))}. ${esc(MISSION[f.mission].what)} ${f.ftr ? `Its fighters reach ${num(R.ftr)} km and are at full effect to ${num(R.ftr * AIR.range.fullAt)} km. ` : ''}${f.bmr ? `Its bombers reach ${num(R.bmr)} km. ` : ''}The rings on the map show both.</p>`;
  }
  return `<div class="orders"><div class="acts">${btns}</div>${body}</div>`;
}

function airOrderText(o: AirOrder): string {
  const from = o.base?.length ? ` from ${o.base[0]}` : '';
  const sea = o.sea ? g.seas.find((z) => z.key === o.sea)?.name ?? 'sea' : '';
  const what = o.mission === 'superiority' ? `fights for the sky over ${o.region}` : o.mission === 'bomb' ? `bombs ${o.region} by day` : o.mission === 'night' ? `bombs ${o.region} by night` : o.mission === 'strike' ? `strikes at shipping in the ${sea}` : o.mission === 'rest' ? 'rests' : 'supports the armies';
  return `${o.fleet}${from} ${what}.`;
}

function airPane(): string {
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  const size = (a: AirFleet, b: AirFleet): number => b.ftr + b.bmr - (a.ftr + a.bmr);
  const held = g.regions.filter((r) => skyOver(g, r.id));
  const skyNote = held.length ? `<div class="note"><b>The sky.</b> ${held.map((r) => `The ${g.nations[skyOver(g, r.id)!].adj} air force holds the sky over ${esc(r.name)}.`).join(' ')} An army attacking there has the air on its side, and on a coast the holder's aircraft command the sea. It has to be won again every month.</div>` : '';
  const plan = (nat: string, mine: boolean): string => {
    // A player sees the month he is about to play; a spectator the month that has just been flown.
    const ph = airPlanFor(g, nat, mine ? g.turn : Math.max(1, g.turn - 1));
    return ph ? `<div class="note"><b>${mine ? "History's plan this month" : `${esc(natName(nat))}, on history's plan`}: ${esc(ph.name)}.</b> ${esc(ph.orders.map(airOrderText).join(' '))}${mine ? ' You are free to do something else.' : ''}</div>` : '';
  };
  let out = '';
  if (me) {
    const n = g.nations[me];
    const mine = airFleetsOf(g, me);
    const p = airPool(g, me), d = airDeliveries(g, me);
    const order = humanUp(g);
    out += `<h2>The ${n.adj} air force</h2><p class="sub">An air fleet left alone supports the armies and defends the sky within reach of its fighters. Give it a mission and it flies a month-long air campaign, settled from a Battle Ticket when you end the month.</p>
      <div class="facts"><div class="fact"><span>Fighters</span><b>${num(mine.reduce((t, f) => t + f.ftr, 0))}</b></div><div class="fact"><span>Bombers</span><b>${num(mine.reduce((t, f) => t + f.bmr, 0))}</b></div><div class="fact"><span>Spare pilots</span><b>${num(p.pilots)}</b></div></div>
      <p class="sub" style="margin-top:6px">In reserve: ${craft(p.ftr, p.bmr)}. Each month the factories deliver ${craft(d.ftr, d.bmr)} and the flying schools ${num(d.pilots)} pilots. A new fighter needs a pilot, and resting fleets are made up first.${p.lostFtr + p.lostBmr ? ` Lost since the war began: ${craft(p.lostFtr, p.lostBmr)}, and ${num(p.lostPilots)} fighter pilots.` : ''}</p>${budgeted(g) ? '<div class="acts"><button class="btn small" data-act="armgo" data-arm="air" title="Buy fighter and bomber wings from the arsenal.">Order more aircraft</button></div>' : ''}`;
    if (!n.atWar.length) out += `<div class="note"><b>At peace.</b> The air force flies no missions until the nation is at war.</div>`;
    out += plan(me, true);
    const chain = AIR.radar.chains.find((c) => c.nat === me && c.from <= g.turn);
    if (chain) out += `<div class="note"><b>${esc(capital(chain.name))}.</b> Radar warns of raids on your home ground: every fighter in reach gets up in time, and few aircraft are caught on the ground. An enemy without it meets raids with only ${Math.round(AIR.radar.without * 100)}% of his fighters.</div>`;
    const cut = airIncomeLoss(g, me);
    if (cut > 0) out += `<div class="note" style="border-left-color:var(--red)"><b>Under the bombs.</b> Bombing and attacks on coastal shipping stopped ${(cut * 100).toFixed(1)}% of your production this month.</div>`;
    out += skyNote;
    out += `<h3>Your air fleets</h3>${mine.length ? mine.map((f) => airRow(f, order) + (order && ui.air === f.id ? airOrders(f) : '')).join('') : '<div class="empty">No air fleets under your flag.</div>'}`;
    const friends = g.airFleets.filter((f) => f.nat !== me && allied(g, f.nat, me)).sort(size);
    const foes = g.airFleets.filter((f) => atWar(g, f.nat, me)).sort(size);
    if (friends.length) out += `<h3>Allied air forces</h3>${friends.map((f) => airRow(f, false, true)).join('')}`;
    if (foes.length) out += `<h3>Enemy air forces</h3>${foes.map((f) => airRow(f, false, true)).join('')}`;
    return out;
  }
  out += `<h2>The war in the air</h2><p class="sub">Air fleets fly from the airfields of a region. Left alone they support the armies and defend their sky. Given a mission they fight a month-long campaign for the sky, against industry or against shipping, as far as their range allows. The lines on the map show who is flying against what.</p>`;
  const war = g.airFleets.filter((f) => g.nations[f.nat].atWar.length > 0);
  const nats = [...new Set(war.map((f) => f.nat))].sort((a, b) => war.filter((f) => f.nat === b).reduce((t, f) => t + f.ftr + f.bmr, 0) - war.filter((f) => f.nat === a).reduce((t, f) => t + f.ftr + f.bmr, 0));
  for (const nat of nats) out += plan(nat, false);
  out += skyNote;
  for (const nat of nats) {
    const p = airPool(g, nat);
    out += `<h3>${esc(natName(nat))}</h3>${p.lostFtr + p.lostBmr ? `<p class="sub" style="margin:-2px 0 6px">Lost so far: ${craft(p.lostFtr, p.lostBmr)}, and ${num(p.lostPilots)} fighter pilots.</p>` : ''}${war.filter((f) => f.nat === nat).map((f) => airRow(f, false)).join('')}`;
  }
  const peace = g.airFleets.filter((f) => !g.nations[f.nat].atWar.length).sort(size);
  if (peace.length) out += `<h3>Air forces at peace</h3>${peace.map((f) => airRow(f, false, true)).join('')}`;
  return out;
}

const ticketTable = (lines: { label: string; value: string; effect?: string }[]): string => `<table class="ticket">${lines.map((l) => `<tr><td>${esc(l.label)}</td><td>${esc(l.value)}</td><td class="fx">${esc(l.effect ?? '')}</td></tr>`).join('')}</table>`;

/** The look at an air mission before the order is given: the odds, the ticket, who flies and who will meet them. */
function airPlanCard(): string {
  const m = ui.modal!;
  const f = g.airFleets.find((x) => x.id === m.fleet)!;
  const mission = m.mission as AirMission;
  const sea = mission === 'strike';
  const place = sea ? `the ${g.seas[m.target].name}` : g.regions[m.target].name;
  const title = mission === 'superiority' ? `The sky over ${place}` : mission === 'bomb' ? `Bombing ${place}` : mission === 'night' ? `Night raids on ${place}` : `Strikes on shipping in ${place}`;
  const a = previewAirOrder(g, f.id, mission, m.target);
  const why = whyNotMission(g, f, mission, m.target) ?? a?.why ?? (a ? null : 'Nothing can reach the target.');
  let body: string;
  if (why || !a) body = `<div class="empty">${esc(why ?? '')}</div>`;
  else {
    const fc = forecastAir(g, a, mission, m.target);
    const lossPct = Math.round(fc.bomberLoss * 100);
    const defNat = a.def[0]?.f.nat ?? (sea ? '' : g.regions[m.target].ctrl);
    let head: string;
    if (mission === 'superiority') {
      const word = a.D <= 0 ? 'No fighters will come up' : fc.odds >= AIR.sky.win * 1.25 ? 'The sky is within reach' : fc.odds >= AIR.sky.win ? 'The sky can be won, at a price' : fc.odds >= 1.2 ? 'A hard fight that wears them down' : fc.odds >= 0.8 ? 'An even fight' : 'The defenders have the edge';
      const share = a.Epow + a.Dpow > 0 ? a.Epow / (a.Epow + a.Dpow) : 1;
      head = `<div class="verdict ${fc.odds >= AIR.sky.win ? 'win' : fc.odds >= 1.2 ? '' : 'loss'}" style="font-size:24px">${word}</div>
        <div class="oddsbar"><i style="width:${Math.round(share * 100)}%;background:${g.nations[f.nat].color}"></i><i style="flex:1;background:${defNat ? g.nations[defNat].color : 'var(--panel-3)'}"></i></div>
        <div class="sub">Fighter odds ${fc.odds.toFixed(2)} to 1 going in. The sky is won when your fighters end the month with ${AIR.sky.win} times what the defender can still put up once his losses are made good.${a.B > 0 ? ` About ${lossPct}% of the bombers are likely to be lost.` : ''}${fc.cut > 0 ? ` Those that get through should cut the readiness of the fighters based there by about ${Math.round(fc.cut * 100)}%, and that carries over to next month.` : ''}</div>`;
    } else if (sea) {
      head = `<div class="verdict ${lossPct <= 10 ? 'win' : lossPct <= 25 ? '' : 'loss'}" style="font-size:24px">${lossPct <= 10 ? 'Light opposition' : lossPct <= 25 ? 'The fighters will take a toll' : 'Heavy losses are likely'}</div>
        <div class="sub">About ${lossPct}% of the bombers are likely to be lost. Warships caught at sea there could lose up to ${Math.round(fc.cut * 100)}% of their number, and merchant ships in these waters will be sunk.</div>`;
    } else {
      head = `<div class="verdict ${lossPct <= 10 ? 'win' : lossPct <= 25 ? '' : 'loss'}" style="font-size:24px">${lossPct <= 10 ? 'Light losses expected' : lossPct <= 25 ? 'The fighters will take a toll' : 'Heavy losses are likely'}</div>
        <div class="sub">About ${lossPct}% of the bombers are likely to be lost. The raids should stop about ${Math.round(fc.cut * 100)}% of ${esc(place)}'s output for a month: ${(fc.nation * 100).toFixed(1)}% of ${g.nations[g.regions[m.target].ctrl].adj} production.</div>`;
    }
    const flying = a.att.map((x) => `<div class="pickrow pick">${planeIcon(x.f)}<span class="grow"><b>${esc(x.f.name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">from ${esc(g.regions[x.f.base].name)}, ${num(x.d)} km · ${esc(craft(mission === 'night' ? 0 : x.f.ftr, x.bmr > 0 ? x.f.bmr : 0))}${x.crowd < 1 ? ` · crowded airfields, ${Math.round(x.crowd * 100)}% can fly` : ''}</span></span></div>`).join('');
    const meeting = a.def.length ? a.def.map((x) => `<div class="pickrow">${planeIcon(x.f)}<span class="grow"><b>${esc(x.f.name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${g.nations[x.f.nat].adj}, from ${esc(g.regions[x.f.base].name)} · ${num(x.f.ftr)} fighters, about ${num(x.f.ftr * x.w)} able to meet the raids</span></span></div>`).join('') : '<div class="empty">No enemy fighters are in reach.</div>';
    body = `${head}<h3>Battle Ticket</h3>${ticketTable(a.lines)}<h3>Flying the mission</h3>${flying}<h3>Rising to meet them</h3>${meeting}
      <div class="note"><b>Flown when you end the month.</b> Other fleets given the same mission against the same target fly it together. A fleet on a mission does not defend its own sky that month.</div>`;
  }
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Air mission · ${esc(f.name)} · ${date(g.turn)}</div><h2>${esc(title)}</h2></header>
    <div class="body">${body}</div>
    <footer><button class="btn" data-act="close">Call it off</button><button class="btn go" data-act="doair" ${why || !a ? 'disabled' : ''}>Give the order</button></footer>`;
}

function airReportCard(r: AirReport): string {
  const v = airVerdict(r);
  const sum = (ss: AirSide[], k: 'ftr' | 'bmr' | 'lostFtr' | 'lostBmr'): number => ss.reduce((n, x) => n + x[k], 0);
  const side = (ss: AirSide[]): string => ss.length ? `<div class="big">${num(sum(ss, 'ftr') + sum(ss, 'bmr'))}</div><small>aircraft in ${ss.length} air fleet${ss.length > 1 ? 's' : ''} · lost ${num(sum(ss, 'lostFtr') + sum(ss, 'lostBmr'))}</small>` : '<div class="big">0</div><small>no fighters came up</small>';
  const list = (ss: AirSide[]): string => ss.length ? `<ul class="ulist">${ss.map((x) => `<li><span>${esc(x.fleet)} <span style="color:var(--ink-3)">(${g.nations[x.nat].adj}${x.leader ? `, ${esc(x.leader)}` : ''})</span><br><span style="color:var(--ink-2)">${esc(capital(craft(x.ftr, x.bmr)))}</span></span><span>${x.lostFtr + x.lostBmr ? `lost ${esc(craft(x.lostFtr, x.lostBmr))}` : 'no losses'}</span></li>`).join('')}</ul>` : '<div class="empty">None.</div>';
  const sky = r.mission === 'superiority' ? (r.sky ? ` The ${g.nations[r.attNat].adj} air force won the sky over ${esc(r.place)}: its fighters ended the month with ${r.hold >= 9 ? 'no opposition left' : `${r.hold.toFixed(2)} times what the defenders could still put up`}.` : ` The attackers ended the month with ${r.hold.toFixed(2)} times the fighters the defenders could still put up. It takes ${AIR.sky.win} to win the sky.`) : '';
  const what = r.mission === 'superiority' ? 'air superiority' : r.mission === 'bomb' ? 'day bombing' : r.mission === 'night' ? 'night bombing' : 'strikes on shipping';
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Battle Ticket · air campaign, ${what} · ${date(r.turn)}</div><h2>${esc(airTitle(r))}</h2></header>
    <div class="body">
      <div class="verdict ${v.cls}">${v.text}</div>
      <div class="sub">${esc(r.result)}${sky}</div>
      <div class="vs"><div><h4>${chip(r.attNat)} ${natName(r.attNat)} attacking</h4>${side(r.att)}</div><div class="mid">vs</div><div class="r"><h4>${r.defNat ? natName(r.defNat) : 'No defenders'} ${r.defNat ? chip(r.defNat) : ''}</h4>${side(r.def)}</div></div>
      <h3>What the campaign map fed into the battle</h3>
      ${ticketTable(r.lines)}
      <h3>Flying the mission</h3>${list(r.att)}<h3>Rising to meet them</h3>${list(r.def)}
      <div class="note" style="margin-top:14px"><b>Auto-resolved.</b> A month of air fighting is settled from its ticket. In the full game its big days can be flown as 3D air battles, with the same ticket going in and the same kind of result coming back.</div>
    </div><footer><button class="btn" data-act="${r.zone >= 0 ? 'showsea' : 'showmap'}" data-id="${r.zone >= 0 ? r.zone : r.region}">Show on map</button><button class="btn go" data-act="close">Close</button></footer>`;
}

// ---- Modals -------------------------------------------------------------------------------

function openModal(m: typeof ui.modal): void {
  ui.modal = m;
  const veil = $('#veil');
  if (!m) { veil.classList.remove('open'); return; }
  veil.classList.add('open');
  const card = $('#card');
  card.classList.toggle('wide', m.kind === 'attack' || m.kind === 'report' || m.kind === 'ledger');
  if (m.kind === 'start') card.innerHTML = startCard();
  else if (m.kind === 'report') {
    const rep = g.reports.find((r) => r.id === m.id);
    if (!rep) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = reportCard(rep);
  }
  else if (m.kind === 'seareport') {
    const rep = g.seaReports.find((r) => r.id === m.id);
    if (!rep) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = seaReportCard(rep);
  }
  else if (m.kind === 'airreport') {
    const rep = g.airReports.find((r) => r.id === m.id);
    if (!rep) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = airReportCard(rep);
  }
  else if (m.kind === 'airplan') card.innerHTML = airPlanCard();
  else if (m.kind === 'landing') card.innerHTML = landingCard();
  else if (m.kind === 'attack') card.innerHTML = attackCard();
  else if (m.kind === 'news') card.innerHTML = newsCard(m.from, m.reports, m.sea ?? g.seaReports.length, m.air ?? g.airReports.length);
  else if (m.kind === 'declare') card.innerHTML = declareCard();
  else if (m.kind === 'ledger') {
    const nat = m.nat || g.human;
    if (!nat || !g.nations[nat]) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = ledgerCard(nat);
  }
  else if (m.kind === 'general') {
    const x = g.generals[m.id];
    if (!x) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = generalCard(x);
  }
  else if (m.kind === 'appoint') {
    const c = g.corps.find((k) => k.id === m.corps);
    if (!c) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = appointCard(c);
  }
  else if (m.kind === 'unit') {
    const u = unitOf(g, m.type, m.id);
    if (!u) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = unitRecordCard(m.type, u);
  }
  else if (m.kind === 'fallen') {
    const f = fallenOf(g, m.id);
    if (!f) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = fallenCard(f);
  }
  else if (m.kind === 'award') {
    const a = awardOf(g, m.id);
    if (!a) { ui.modal = null; veil.classList.remove('open'); return; }
    orderSeen(g, a.id);
    card.innerHTML = awardCard(a);
  }
  card.scrollTop = 0;
}

// ---- The start screen: the grand campaign at the front door, the historical scenarios behind it ----

interface GrandYear { year: number; turn: number; date: string; text: string }
const GRAND = grandData as unknown as { years: GrandYear[]; nations: { id: string; years: Record<string, string> }[]; watch: Record<string, string> };

/** The years a country can begin a grand campaign in. Nobody at the controls can begin in any. */
const grandYears = (nat: string): number[] => (nat ? Object.keys(GRAND.nations.find((n) => n.id === nat)?.years ?? {}).map(Number) : GRAND.years.map((y) => y.year));

/** Reads back the country and year picked last time, and keeps the pick to what is on offer. */
function grandPick(): { nat: string; year: GrandYear; text: string } {
  if (ui.gcNat && !GRAND.nations.some((n) => n.id === ui.gcNat)) ui.gcNat = 'GER';
  const years = grandYears(ui.gcNat);
  if (!years.includes(ui.gcYear)) ui.gcYear = years.filter((y) => y <= ui.gcYear).pop() ?? years[0];
  const year = GRAND.years.find((y) => y.year === ui.gcYear) ?? GRAND.years[0];
  const text = ui.gcNat ? GRAND.nations.find((n) => n.id === ui.gcNat)!.years[String(year.year)] : GRAND.watch[String(year.year)] ?? GRAND.watch.later;
  return { nat: ui.gcNat, year, text };
}

function rememberPick(): void {
  try { localStorage.setItem('tc-grand', `${ui.gcNat}:${ui.gcYear}`); } catch { /* the pick is only a convenience */ }
}

function recallPick(): void {
  try {
    const [nat, year] = (localStorage.getItem('tc-grand') ?? '').split(':');
    if (year) { ui.gcNat = nat; ui.gcYear = Number(year) || 1939; }
  } catch { /* nothing kept */ }
}

const startHead = (title: string): string => `<header><div class="kick">Grand strategy · 1939 to 1945</div><h2>${title}</h2></header>`;
const STEPS = `<ol class="steps"><li>Click one of your armies, then a green region to march or a red one to attack.</li><li>Press <b>End month</b> and the other nations take their turns.</li><li>Spend your factories' output under <b>Build</b>, steer your designers under <b>Research</b>, and send your fleets out under <b>Navy</b> and your air fleets under <b>Air</b>. The game saves itself as you play.</li><li>Drag the map to move it. Scroll or pinch to zoom.</li></ol>`;

function startCard(): string {
  if (ui.menu === 'scenarios') return scenarioCard();
  const saved = peekSave(readSave() ?? '');
  const resume = saved ? `<button class="sidebtn wide resume" data-act="continue"><b>Continue your campaign</b><span>${saved.human ? `${esc(natName(saved.human))}, ${date(saved.turn)}` : `${saved.open ? 'Watching a war nobody has seen' : 'Watching history'}, ${date(saved.turn)}`}${saved.human && saved.open ? ', grand campaign' : ''}. Saved in this browser.</span></button>` : '';
  const pick = grandPick();
  const years = grandYears(pick.nat);
  const tile = (id: string, name: string): string => `<button class="gcnat${id === pick.nat ? ' on' : ''}" data-act="gcnat" data-nat="${id}" aria-pressed="${id === pick.nat}">${id ? flagIcon(id, g.nations[id].color, 54) : '<span class="gcwatch">◉</span>'}<b>${esc(name)}</b></button>`;
  const goals = pick.nat && pick.year.turn === 1 ? GOAL_KEYS.map((key) => { const goal = GOALS[pick.nat]?.[key]; return goal ? `<span class="sgoal"><i>${key === 'short' ? 'Short campaign' : 'Long campaign'}</i>${esc(goal.text)}</span>` : ''; }).join('') : '';
  return `${startHead('Theater Command')}
    <div class="body"><p class="lead">Pick a country and a year, then <b>make your own history</b>. From the day you take over nobody follows the history books: you decide where your armies, fleets and air fleets go, and so does the staff of every other nation.</p>
    ${resume ? `<div class="sides">${resume}</div>` : staleSave(readSave() ?? '') ? '<div class="note">The campaign saved in this browser was played on an earlier, smaller map and cannot be continued in this version. Starting a new game replaces it.</div>' : ''}
    <h3>The grand campaign · choose your country</h3>
    <div class="gcnats">${GRAND.nations.map((n) => tile(n.id, g.nations[n.id].name)).join('')}${tile('', 'Watch')}</div>
    <h3>Choose the year you take over</h3>
    <div class="gcyears">${GRAND.years.map((y) => `<button class="gcyear${y.year === pick.year.year ? ' on' : ''}" data-act="gcyear" data-year="${y.year}" ${years.includes(y.year) ? '' : 'disabled'} aria-pressed="${y.year === pick.year.year}"><b>${y.year}</b><span>${y.turn === 1 ? 'The war begins' : esc(date(y.turn))}</span></button>`).join('')}</div>
    <div class="gcbrief">${pick.nat ? flagIcon(pick.nat, g.nations[pick.nat].color, 88) : ''}<div class="grow"><div class="kick">${esc(pick.year.date)}</div><h4>${pick.nat ? esc(g.nations[pick.nat].name) : 'A war nobody has seen'}</h4>
      <p>${esc(pick.year.text)}</p><p class="you">${esc(pick.text)}</p>${goals ? `<div class="gcgoals">${goals}</div>` : ''}
      <button class="btn go gcgo" data-act="start" data-nat="${pick.nat}" data-from="${pick.year.turn}" data-view="grand">${pick.nat ? 'Begin the campaign' : 'Watch it unfold'} ▸</button>${pick.year.turn > 1 ? `<small class="gcwait">The computer plays the first ${pick.year.turn - 1} months of the war first. That takes a few seconds.</small>` : ''}</div></div>
    <h3>Historical scenarios</h3>
    <div class="sides"><button class="sidebtn wide" data-act="menu" data-menu="scenarios"><b>Play a turning point of the war</b><span>Fourteen shorter campaigns, from the Battle of Britain and Barbarossa to D-Day and Operation Bagration. Each starts at a turning point with the other nations following history's plan, or you can sit back and watch history unfold.</span></button></div>
    ${STEPS}</div>`;
}

function scenarioCard(): string {
  const aims = (id: string | null, from: number): string => (id && from === 1 ? GOAL_KEYS.map((key) => { const goal = GOALS[id]?.[key]; return goal ? `<span class="sgoal"><i>${key === 'short' ? 'Short campaign' : 'Long campaign'}</i>${esc(goal.text)}</span>` : ''; }).join('') : '');
  const side = (id: string | null, name: string, blurb: string, wide = false, from = 1, view = '') => `<button class="sidebtn${wide ? ' wide' : ''}" data-act="start" data-nat="${id ?? ''}" data-from="${from}" data-view="${view}">${id ? `<b>${flagIcon(id, g.nations[id].color, 26)} ${name}</b>` : `<b>${name}</b>`}<span>${blurb}</span>${aims(id, from)}</button>`;
  return `${startHead('Historical scenarios')}
    <div class="body"><p class="lead"><button class="btn small" data-act="menu" data-menu="grand">◂ Back to the grand campaign</button></p>
    <p class="lead">Each scenario starts at a turning point of the war. The computer plays history up to that day, and the nations you do not play go on following <b>history's plan</b>, so the war around you unfolds as it did unless you change it.</p>
    <h3>September 1939 · The war as it was fought</h3>
    <div class="sides">
      ${side(null, 'Watch history unfold', 'Every nation follows its plan. Press Play and watch Poland, then the West, then the East.', true)}
      ${side('GER', 'Germany', 'On the attack from the first day. The hardest plan to pull off on schedule.')}
      ${side('FRA', 'France', 'The Maginot Line, a large army, and a slow command. Can you keep Paris?')}
      ${side('UK', 'United Kingdom', 'A small army, the largest navy in the world, and convoys to keep alive.')}
      ${side('POL', 'Poland', 'Outnumbered and nearly surrounded. You place your army on the eve of war, then hold as long as you can.')}
    </div>
    <h3>July 1940 · The Battle of Britain</h3>
    <div class="sides">
      ${side(null, 'Watch the Battle of Britain', 'France has fallen. The computer flies history\'s plan: the Channel, the airfields, London, then the night Blitz.', true, 11)}
      ${side('GER', 'Germany', 'Five air fleets and a summer of flying weather. History split its effort and lost. Can you win the sky over Kent?', false, 11)}
      ${side('UK', 'United Kingdom', 'Radar, four fighter groups and the Channel. Keep Fighter Command in being until the autumn.', false, 11)}
    </div>
    <h3>September 1940 · The desert war</h3>
    <div class="sides">
      ${side(null, 'Watch the desert war', 'One road, no water, and a front that swings a thousand kilometres each way: Egypt, Operation Compass, Rommel, Tobruk.', true, 13)}
      ${side('UK', 'United Kingdom', 'Two divisions against an army. Throw the Italians out of Egypt, then decide how far from Alexandria you dare to chase them.', false, 13)}
      ${side('ITA', 'Italy', 'A large army on foot, a long way from Tripoli. Take Egypt before the British tanks find your open flank.', false, 13)}
    </div>
    <h3>October 1940 · Greece and the Balkans</h3>
    <div class="sides">
      ${side(null, 'Watch the Balkans', 'Italy marches into the Greek mountains in the autumn rain. In April the panzers come through Bulgaria, and in May the parachutes open over Crete.', true, 14)}
      ${side('GRE', 'Greece', 'A small army that knows its mountains. Hold the Italians in Epirus all winter, then decide what can be saved when Germany comes.', false, 14)}
      ${side('ITA', 'Italy', 'Mussolini expects Athens in two weeks. You have a mountain wall in front of you, and an army in Libya that needs the same replacements.', false, 14)}
    </div>
    <h3>June 1941 · Barbarossa</h3>
    <div class="sides">
      ${side(null, 'Watch Barbarossa unfold', 'The computer plays the first 21 months, then the invasion of the Soviet Union and the first winter.', true, 22)}
      ${side('GER', 'Germany', 'Three army groups on the frontier. Reach Moscow before the snow, or outrun your supplies trying.', false, 22)}
      ${side('SOV', 'Soviet Union', 'The frontier armies are about to be hit. Trade space for time until the reserves and the winter arrive.', false, 22)}
    </div>
    <h3>May 1942 · Gazala and El Alamein</h3>
    <div class="sides">
      ${side(null, 'Watch the desert in 1942', 'The computer plays the first 32 months, then Gazala, the fall of Tobruk, the stand at El Alamein and the long pursuit back across Libya.', true, 33)}
      ${side('UK', 'United Kingdom', 'Rommel strikes at the Gazala line as you take over, and Tobruk is not the fortress it was. Save the army, hold El Alamein, and strike back when the armour arrives.', false, 33)}
      ${side('ITA', 'Italy', 'Most of the Axis army in Africa is yours, and so is the convoy route that feeds it. Tobruk is in reach. Decide how far past it you dare to go.', false, 33)}
    </div>
    <h3>June 1942 · Case Blue and Stalingrad</h3>
    <div class="sides">
      ${side(null, 'Watch the road to Stalingrad', 'The computer plays the first 33 months, then the drive for the Don, the Volga and the Caucasus oil, and the winter that turned the war.', true, 34)}
      ${side('GER', 'Germany', 'Your tanks are gathered in the south and your allies will hold the Don behind you. The oil and the Volga are both in reach. Decide how much of the army goes into the city.', false, 34)}
      ${side('SOV', 'Soviet Union', 'Give ground all summer and save the army. Hold Stalingrad, watch who is guarding the Don, and strike when the snow comes.', false, 34)}
    </div>
    <h3>November 1942 · Torch and Tunisia</h3>
    <div class="sides">
      ${side(null, 'Watch Torch and Tunisia', 'The computer plays the first 38 months, then the American and British landings in Morocco and Algeria, the race for Tunis, Kasserine, the Mareth Line and the end in Africa.', true, 39)}
      ${side('UK', 'United Kingdom', 'Montgomery is chasing Rommel across Libya, and a second army waits at Gibraltar beside the Americans. Get it ashore in Algeria, try for Tunis before the Germans do, and close the trap from both ends.', false, 39)}
      ${side('ITA', 'Italy', 'Your army is falling back from El Alamein and a new enemy is about to land behind it. Hold Tunisia, bring the army home to it, and keep the sea road to Sicily open for as long as you can.', false, 39)}
    </div>
    <h3>July 1943 · Kursk and the Dnieper</h3>
    <div class="sides">
      ${side(null, 'Watch Kursk and the road to the Dnieper', 'The computer plays the first 46 months, then the last German summer offensive in the east, the Soviet strokes at Orel and Kharkov, the race for the Dnieper and the freeing of Kiev.', true, 47)}
      ${side('GER', 'Germany', 'Your tanks are gathered on both shoulders of the Kursk salient, and the Red Army has had three months to dig in there. Strike or hold them back: either way the weight is about to fall on Orel and Kharkov, and the Dnieper is a long way behind you.', false, 47)}
      ${side('SOV', 'Soviet Union', 'You know where the blow will fall and you have dug in to meet it. Break the panzers on the lines round Kursk, then send the reserve fronts at Orel and Kharkov and do not stop until you are over the Dnieper.', false, 47)}
    </div>
    <h3>July 1943 · Sicily and Italy</h3>
    <div class="sides">
      ${side(null, 'Watch Sicily and Italy', 'The computer plays the first 46 months, then the landing in Sicily, the fall of Mussolini, the Italian armistice, Salerno and Naples, and the German winter line across the mountains south of Rome.', true, 47, 'italy')}
      ${side('UK', 'United Kingdom', 'Two armies wait at Malta and your air force has spent a month over Sicily. Keep the sky, put the Eighth Army ashore, and follow it across the strait before the Germans have dug in across the mountains.', false, 47, 'italy')}
      ${side('ITA', 'Italy', 'Africa is gone, the army has lost heart and the invasion fleet is gathering at Malta. If Sicily falls the King will dismiss Mussolini, and an armistice follows. Hold the island and Italy stays in the war.', false, 47, 'italy')}
    </div>
    <h3>January 1944 · The eastern front, from Leningrad to the Danube</h3>
    <div class="sides">
      ${side(null, 'Watch the Red Army\'s year', 'The computer plays the first 52 months, then the relief of Leningrad, the spring offensive through the mud of the Ukraine, the Crimea, the destruction of Army Group Centre in the summer, Romania and Finland leaving the war, and the road to Budapest.', true, 53, 'east')}
      ${side('SOV', 'Soviet Union', 'You have the men, the guns and the initiative on every part of the front. History freed Leningrad in January, the Ukraine by April, Minsk in July and Bucharest in August. Choose where each blow falls, and do not let the armies you cut off get away.', false, 53, 'east')}
      ${side('GER', 'Germany', 'A front of two thousand kilometres, too few divisions to hold it, and orders to give up nothing. Italy has to be held as well, and the landing in France is coming in the summer. Decide what to give up while it is still yours to give.', false, 53, 'east')}
    </div>
    <h3>January 1944 · Anzio and the road to Rome</h3>
    <div class="sides">
      ${side(null, 'Watch Anzio, Cassino and Rome', 'The computer plays the first 52 months, then the landing at Anzio, the four battles for Monte Cassino, the fall of Rome in June, the pursuit to Florence and the autumn battles on the Gothic Line.', true, 53, 'italy')}
      ${side('UK', 'United Kingdom', 'The Eighth Army holds the Adriatic end of a front that has not moved since October, and the Americans are about to land behind it at Anzio. Monte Cassino bars the one good road to Rome. Decide when to break yourself against it, and when to wait for the spring.', false, 53, 'italy')}
    </div>
    <h3>May 1944 · Overlord and the liberation of France</h3>
    <div class="sides">
      ${side(null, 'Watch D-Day and the drive to the Rhine', 'The computer plays the first 56 months, then the air battle over the beaches, D-Day in June, the breakout and the liberation of Paris in August, Brussels and Antwerp, Market Garden, the West Wall, and the Ardennes offensive in December.', true, 57, 'west')}
      ${side('UK', 'United Kingdom', 'Two corps wait on the quays in Hampshire and the Americans beside you have a beach of their own. Win the sky over Normandy in May, land in June, and hold the German armour on your front while the Americans break out. Then see how far a single road to Arnhem will take you.', false, 57, 'west')}
      ${side('GER', 'Germany', 'Some fifty divisions stand behind the Atlantic Wall and nobody can tell you which beach. The panzers can wait close to the coast or well back from it, not both. And every division kept in France is one the eastern front will not have when the Soviet summer offensive opens.', false, 57, 'west')}
    </div>
    <h3>June 1944 · Operation Bagration</h3>
    <div class="sides">
      ${side(null, 'Watch Bagration', 'The computer plays the first 57 months, then the blow that destroyed Army Group Centre: Vitebsk and Minsk in June, the Bug and the Vistula in July, the Warsaw rising, and Romania changing sides as the summer ends.', true, 58, 'east')}
      ${side('SOV', 'Soviet Union', 'The landing in France is days away and the German tanks have gone south, where they expect you. Four fronts are ready in Byelorussia. Break the fortified places at Vitebsk and Mogilev, close the ring at Minsk, and see how far the summer will carry you.', false, 58, 'east')}
      ${side('GER', 'Germany', 'Army Group Centre holds a great bulge round Minsk, its tanks have been sent south, and the fortified places are to be held to the last man. The Allies are about to land in France. You have a few weeks to decide what can be saved.', false, 58, 'east')}
    </div>
    ${STEPS}</div>`;
}


// ---- The face-off: two armies drawn up against each other, before the battle and after it ----

interface Side {
  nats: string[];
  /** 'attacking' or 'defending'. */
  role: string;
  /** The senior general, null when the army has none, left out when nobody recorded it. */
  who?: General | null;
  men: number;
  corps: number;
  divs: number;
  cards: string;
  /** A line under the numbers, such as the losses. */
  under?: string;
}

const adjs = (nats: string[]): string => nats.map((n) => g.nations[n].adj).join(' and ');
const initialsOf = (name: string): string => name.replace(/\s*\(.*\)\s*$/, '').split(/\s+/).map((w) => w[0]).filter((ch) => /[A-Z]/.test(ch)).slice(-2).join('');

/** One army in a face-off: its colours, who leads it, what it brings, and its corps as cards. */
function sidePanel(o: Side, cls: string): string {
  const n = g.nations[o.nats[0]];
  const head = o.who
    ? `<span class="seal" style="background:${n.color}">${esc(initialsOf(o.who.name))}</span><span class="grow"><b>${esc(o.who.name)}</b><small>${esc(adjs(o.nats))} army ${o.role} · level ${o.who.level} general</small>${o.who.traits.length ? `<span class="tags">${traitTags(o.who)}</span>` : ''}</span>`
    : `<span class="seal none" style="background:${n.color}"></span><span class="grow"><b>${esc(adjs(o.nats))} army</b><small>${o.role[0].toUpperCase() + o.role.slice(1)}${o.who === null && o.corps ? ' · no general in command' : ''}</small></span>`;
  return `<div class="fside ${cls}" style="--side:${n.color}"><div class="fhead">${head}</div>
    <div class="fnum"><span><b>${num(o.men)}</b><small>men</small></span><span><b>${num(o.divs)}</b><small>division${o.divs === 1 ? '' : 's'}</small></span><span><b>${o.corps}</b><small>corps</small></span></div>${o.under ?? ''}
    <div class="fcards">${o.cards}</div></div>`;
}

/** A corps in a face-off. One the player may send or hold back is a button. */
function battleCard(c: Corps, opts: { act?: boolean; on?: boolean; note?: string; from?: boolean } = {}): string {
  const el = opts.act ? 'button' : 'div';
  const gen = generalOf(g, c);
  const where = opts.from ? ` · ${esc(g.regions[c.reg].name)}` : gen ? ` · ${esc(surname(gen.name))}` : '';
  return `<${el} class="ucard${opts.act ? (opts.on ? ' pick' : ' out') : ''}" ${opts.act ? `data-act="atkpick" data-id="${c.id}"` : ''} title="${esc(c.name)}${gen ? ` · ${esc(gen.name)}` : ''} · ${num(men(c))} men${opts.from ? ` · from ${esc(g.regions[c.reg].name)}, ${c.ap} move${c.ap === 1 ? '' : 's'} left` : ''}">
    <span class="utop">${icon(c)}${gen ? `<span class="lvl" title="${esc(gen.name)}, level ${gen.level} general">${gen.level}</span>` : ''}</span><b>${esc(c.name)}</b>
    <span class="um">${c.divs} ${KIND[c.kind].label.toLowerCase()} div${c.divs > 1 ? 's' : ''}${where}</span>
    <span class="meter" title="Strength ${Math.round(c.str * 100)}%"><i class="${c.str < 0.5 ? 'low' : ''}" style="width:${Math.round(c.str * 100)}%"></i></span>
    <span class="meter" title="Cohesion ${Math.round(c.org * 100)}%"><i class="org${c.org < 0.4 ? ' low' : ''}" style="width:${Math.round(c.org * 100)}%"></i></span>${opts.note ? `<span class="tags"><span class="tag bad">${esc(opts.note)}</span></span>` : ''}</${el}>`;
}

const FATE: Record<string, string> = { '': 'held its ground', retreated: 'retreated', surrendered: 'surrendered', destroyed: 'destroyed', advanced: 'advanced', evacuated: 'evacuated by sea', yielded: 'gave ground under orders' };

/** A corps after the battle: what it lost and what became of it. */
function fateCard(u: BattleReport['attackers'][number], won: boolean): string {
  const left = u.men > 0 ? Math.max(0, 1 - u.lost / u.men) : 0;
  const gone = u.fate === 'surrendered' || u.fate === 'destroyed';
  const fate = u.fate === '' && !won ? 'fell back' : FATE[u.fate];
  return `<div class="ucard${gone ? ' gone' : ''}" title="${esc(u.name)} · ${num(u.men)} men, lost ${num(u.lost)}">
    <span class="utop"><svg width="34" height="24" viewBox="-1 -1 26 19" class="ctr"><rect class="box" x="0" y="0" width="24" height="15" rx="1.5" fill="${g.nations[u.nat].color}"/>${symbol(u.kind, 24, 15)}</svg></span><b>${esc(u.name)}</b>
    <span class="um">${u.divs} ${KIND[u.kind].label.toLowerCase()} div${u.divs > 1 ? 's' : ''} · lost ${num(u.lost)}</span>
    <span class="meter" title="${Math.round(left * 100)}% of its men left"><i class="${left < 0.5 ? 'low' : ''}" style="width:${Math.round(left * 100)}%"></i></span>
    <span class="tags"><span class="tag ${gone || u.fate === 'retreated' ? 'bad' : u.fate === 'advanced' ? 'good' : ''}">${fate}</span></span></div>`;
}

const ticketBlock = (title: string, lines: BattleReport['lines']): string => `<h3>${title}</h3><table class="ticket">${lines.map((l) => `<tr><td>${esc(l.label)}</td><td>${esc(l.value)}</td><td class="fx">${esc(l.effect ?? '')}</td></tr>`).join('')}</table>`;

function reportCard(r: BattleReport): string {
  const v = verdictFor(r);
  const side = (us: BattleReport['attackers'], lost: number, role: string, won: boolean): Side => {
    const total = us.reduce((s, u) => s + u.men, 0);
    const share = total > 0 ? Math.min(1, lost / total) : 0;
    return {
      nats: [...new Set(us.map((u) => u.nat))], role, men: total, corps: us.length, divs: us.reduce((s, u) => s + u.divs, 0),
      under: `<div class="floss"><span class="meter"><i class="low" style="width:${Math.round(share * 100)}%"></i></span><small>Lost ${num(lost)} men, ${Math.round(share * 100)}% of those engaged</small></div>`,
      cards: us.map((u) => fateCard(u, won)).join(''),
    };
  };
  const share = r.aPower / Math.max(1e-9, r.aPower + r.dPower);
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">${r.yielded ? 'Ground given up' : 'Battle result'} · ${date(r.turn)}, week ${r.week}</div><h2>${r.yielded ? `${esc(r.regionName)} given up` : `Battle of ${esc(r.regionName)}`}</h2></header>
    <div class="body">
      <div class="verdict ${v.cls}">${v.text}</div>
      <div class="sub">${r.yielded ? `The defenders were under orders to give ground and slipped away before the blow fell. ${esc(r.regionName)} was given up without a fight. The odds against them were ${r.odds.toFixed(2)} to 1.` : `${r.taken ? `${esc(r.regionName)} was taken.` : `The attack on ${esc(r.regionName)} was thrown back.`} Final odds ${r.odds.toFixed(2)} to 1${r.prisoners ? ` · ${num(r.prisoners)} prisoners` : ''}.`}</div>
      <div class="faceoff">${sidePanel(side(r.attackers, r.attLost, 'attacking', r.taken), 'att')}<div class="fmid"><span class="vsmark">VS</span></div>${sidePanel(side(r.defenders, r.defLost, 'defending', !r.taken), 'def')}</div>
      <div class="balance"><div class="btrack"><div class="bal"><i style="width:${Math.round(share * 100)}%;background:${g.nations[r.attNat].color}"></i><i style="flex:1;background:${g.nations[r.defNat].color}"></i></div><span class="need" style="left:${(100 * 1.2 / 2.2).toFixed(1)}%" title="What it takes to carry the ground: roughly 1.2 to 1"></span></div>
        <div class="ends"><span>${esc(g.nations[r.attNat].adj)} strength on the day</span><span>${esc(g.nations[r.defNat].adj)} strength on the day</span></div></div>
      ${ticketBlock('What the campaign map fed into the battle', r.lines)}
      ${r.yielded ? '<div class="note" style="margin-top:14px"><b>No battle was fought.</b> Troops ordered to fall back keep their strength and lose some cohesion and the ground. They stand if they are caught again before their own month comes round.</div>' : '<div class="note" style="margin-top:14px"><b>Auto-resolved.</b> In the full game you can choose to fight this battle yourself in 3D. The same ticket goes in and the same kind of result comes back, so the campaign never needs to know which one happened.</div>'}
    </div><footer><button class="btn" data-act="showmap" data-id="${r.region}">Show on map</button><button class="btn go" data-act="close">Continue</button></footer>`;
}

function attackCard(): string {
  const m = ui.modal!;
  const target = g.regions[m.target];
  const me = g.human!;
  const cands = candidates(g, target);
  const chosen = limitAttackers(g, cands.filter((c) => m.chosen.has(c.id)), target);
  const inSet = new Set(chosen.map((c) => c.id));
  const a = previewAttack(g, chosen, target);
  const foes = a ? a.defenders : hostileUnits(g, target, me);
  const why = (c: Corps): string => (m.chosen.has(c.id) && !inSet.has(c.id) ? (g.nations[c.nat].maxAttack && chosen.length >= g.nations[c.nat].maxAttack ? 'Staff at its limit' : 'No room on the roads') : '');
  const ours: Side = {
    nats: [me], role: 'attacking', who: leaderOf(chosen), men: chosen.reduce((s, c) => s + men(c), 0), corps: chosen.length, divs: sumDivs(chosen),
    cards: cands.map((c) => battleCard(c, { act: true, on: inSet.has(c.id), note: why(c), from: true })).join(''),
    under: `<small class="fhelp">${cands.length > 1 ? 'Click a corps to send it in or hold it back.' : 'The one corps in reach.'}</small>`,
  };
  const theirs: Side = {
    nats: [...new Set(foes.map((c) => c.nat))], role: 'defending', who: leaderOf(foes), men: foes.reduce((s, c) => s + men(c), 0), corps: foes.length, divs: sumDivs(foes),
    cards: foes.map((c) => battleCard(c)).join(''),
    under: target.fort ? `<small class="fhelp">Dug in behind level ${target.fort.lv} fortifications${target.fort.vs.includes(me) ? '' : ', which do not face you'}.</small>` : '',
  };
  let body = '';
  if (a) {
    const p = Math.max(0, Math.min(1, (1.2 - 1.2 / a.odds) / 0.4));
    const share = a.A / (a.A + a.D);
    const word = p >= 0.95 ? 'Victory is all but certain' : p >= 0.6 ? 'The odds favour you' : p >= 0.3 ? 'It could go either way' : p > 0 ? 'A long shot' : 'This attack will fail';
    body = `<div class="balance"><div class="bhead"><span class="verdict ${p >= 0.6 ? 'win' : p >= 0.3 ? '' : 'loss'}">${word}</span><span class="sub">Odds ${a.odds.toFixed(2)} to 1 · about ${Math.round(p * 100)}% chance to take ${esc(target.name)}</span></div>
      <div class="btrack"><div class="bal"><i style="width:${Math.round(share * 100)}%;background:${g.nations[me].color}"></i><i style="flex:1;background:${g.nations[foes[0].nat].color}"></i></div><span class="need" style="left:${(100 * 1.2 / 2.2).toFixed(1)}%" title="What it takes to carry the ground: roughly 1.2 to 1 on the day"></span></div>
      <div class="ends"><span>Your strength on the day</span><span>The white mark is what it takes: about 1.2 to 1</span><span>Theirs</span></div></div>
      ${ticketBlock('What the campaign map feeds into this battle', a.lines)}`;
  } else body = '<div class="empty">Choose at least one corps to attack with.</div>';
  const ground = `${target.terrain[0].toUpperCase() + target.terrain.slice(1)}`;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">The armies meet · ${date(g.turn)}</div><h2>Battle for ${esc(target.name)}</h2></header>
    <div class="body"><div class="faceoff">${sidePanel(ours, 'att')}<div class="fmid"><span class="vsmark">VS</span><small>${esc(ground)}</small></div>${sidePanel(theirs, 'def')}</div>${body}</div>
    <footer><button class="btn" data-act="close" title="Call the attack off. Nothing is spent.">Withdraw</button><button class="btn" disabled title="Fighting a battle yourself in real time comes with the 3D build of the game. Until then every battle is settled from the Battle Ticket.">Fight it yourself · 3D build</button><button class="btn go" data-act="attack" ${a ? '' : 'disabled'} title="Settle the battle at once from the Battle Ticket">Auto-resolve</button></footer>`;
}

/** The news that changes the war, a declaration or a fall, set out as a card of its own. */
function eventCard(h: Game['headlines'][number]): string {
  const n = h.nat ? g.nations[h.nat] : undefined;
  if (h.kind === 'honour') {
    const a = h.award ? awardOf(g, h.award) : undefined;
    const mine = !!a && a.nat === g.human;
    return `<div class="evcard honour" style="--side:${n?.color ?? 'var(--brass)'}"><span class="ek">${mine ? 'Honour' : `${esc(n?.adj ?? '')} honour`} · ${date(h.turn).split(' ')[0]}${a ? ` ${ribbonOf(a)}` : ''}</span><p>${esc(h.text)}</p>${a && mine ? `<button class="btn small" data-act="award" data-id="${a.id}">${a.cash || a.kind === 'corps' && !unitOf(g, a.kind, a.unit) ? 'Read the citation' : 'Read the citation and decide'}</button>` : h.region !== undefined ? `<button class="btn small" data-act="showmap" data-id="${h.region}">Show on map</button>` : ''}</div>`;
  }
  return `<div class="evcard ${h.kind}" style="--side:${n?.color ?? 'var(--brass)'}"><span class="ek">${h.kind === 'war' ? 'War' : h.kind === 'goal' ? 'Campaign goal' : 'Fallen'} · ${date(h.turn).split(' ')[0]}, week ${h.week}</span><p>${esc(h.text)}</p>${h.region !== undefined ? `<button class="btn small" data-act="showmap" data-id="${h.region}">Show on map</button>` : ''}</div>`;
}

/** A letter from the front, set apart from the news: somebody wrote it by hand. */
function letterBlock(h: Game['headlines'][number]): string {
  return `<blockquote class="letter"><p>${esc(h.text)}</p><cite>${esc(h.by ?? 'A soldier')}${h.unit !== undefined && h.unit >= 0 && g.corps.some((c) => c.id === h.unit) ? ` <button class="link" data-act="record" data-type="corps" data-id="${h.unit}">Record ›</button>` : ''}</cite></blockquote>`;
}

function newsCard(from: number, repFrom: number, seaFrom: number, airFrom: number): string {
  const hs = g.headlines.slice(from);
  // The player's own honours are cards; another army's are a line of news.
  const cited = (h: Game['headlines'][number]): boolean => h.kind === 'honour' && h.nat === g.human;
  const big = hs.filter((h) => h.kind === 'war' || h.kind === 'fall' || h.kind === 'goal' || cited(h));
  const letters = hs.filter((h) => h.kind === 'letter');
  const rest = hs.filter((h) => !big.includes(h) && h.kind !== 'letter');
  const me = g.human;
  const reps = g.reports.slice(repFrom).filter((r) => me && (r.attackers.some((u) => u.nat === me) || r.defenders.some((u) => u.nat === me)));
  const seas = g.seaReports.slice(seaFrom).filter((r) => me && (r.att.some((u) => u.nat === me) || r.def.some((u) => u.nat === me)));
  const airs = g.airReports.slice(airFrom).filter((r) => me && (r.att.some((u) => u.nat === me) || r.def.some((u) => u.nat === me)));
  const all = g.reports.length - repFrom + (g.seaReports.length - seaFrom) + (g.airReports.length - airFrom);
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Dispatches</div><h2>${date(g.turn)}</h2></header>
    <div class="body">${big.length ? `<div class="evcards">${big.map(eventCard).join('')}</div>` : ''}${rest.length ? `<div class="reel">${rest.map((h) => `<p class="${h.kind}"><span class="wk">${date(h.turn).split(' ')[0]} wk ${h.week}</span>${esc(h.text)}</p>`).join('')}</div>` : hs.length ? '' : '<p class="lead">A quiet month. No news of note.</p>'}
    ${letters.length ? `<h3>Letters from the front</h3>${letters.map(letterBlock).join('')}` : ''}
    ${reps.length || seas.length || airs.length ? `<h3>Battles your forces fought</h3>${airs.map(airBrow).join('')}${seas.map(seaRow).join('')}${reps.map((r) => { const v = verdictFor(r); return `<button class="brow" data-act="report" data-id="${r.id}"><b>${esc(r.regionName)}</b><span class="res ${v.cls}">${v.text}</span><span>${date(r.turn)}, week ${r.week}</span></button>`; }).join('')}` : ''}
    ${all ? `<p class="sub" style="margin-top:10px">${all} battle${all > 1 ? 's were' : ' was'} fought across the map. They are filed under Battles.</p>` : ''}</div>
    <footer><button class="btn go" data-act="close">${me && humanUp(g) ? 'Give orders' : 'Continue'}</button></footer>`;
}

// ---- The ledger: where the money comes from, what the forces cost, and what every province pays ----

type Arm = 'army' | 'navy' | 'air' | 'works';
const ARMS: Arm[] = ['army', 'navy', 'air', 'works'];
const ARM_OF = (key: string): Arm => (key.startsWith('corps:') ? 'army' : key.startsWith('ships:') ? 'navy' : key.startsWith('works:') ? 'works' : 'air');
const ARM_NAME = { army: 'Army', navy: 'Navy', air: 'Air force', works: 'Guns and airfields' } as const;

function ledgerRows(lines: LedgerLine[], sign: 1 | -1, unit: (l: LedgerLine) => string): string {
  return lines.map((l) => `<tr${sign < 0 ? ' class="sub"' : ''}><td>${esc(l.label)}${unit(l) ? ` <small>${unit(l)}</small>` : ''}</td><td class="${l.amount * sign < 0 ? 'neg' : ''}">${cash(l.amount * sign, sign < 0 || l.amount < 0)}</td></tr>`).join('');
}

function ledgerCard(nat: string): string {
  const n = g.nations[nat];
  const L: Ledger = ledger(g, nat);
  const mine = nat === g.human;
  const by: Record<Arm, number> = { army: 0, navy: 0, air: 0, works: 0 };
  // The three services always show; the guns and airfields only once some have been built.
  const arm3 = ARMS.filter((k) => k !== 'works' || by.works > 0 || L.upkeep.some((l) => ARM_OF(l.key) === 'works'));
  for (const l of L.upkeep) by[ARM_OF(l.key)] += l.amount;
  // The picture: one bar for the month's income, cut into what each arm takes and what is left to spend.
  const whole = Math.max(L.gross, L.upkeepTotal, 1e-9);
  const pct = (x: number): string => `${Math.max(0, (x / whole) * 100).toFixed(1)}%`;
  const share = (x: number): string => `${Math.round((x / Math.max(L.gross, 1e-9)) * 100)}%`;
  const left = Math.max(0, L.net);
  const bar = L.budget
    ? `<div class="lbar" role="img" aria-label="Of each month's income the army takes ${share(by.army)}, the navy ${share(by.navy)}, the air force ${share(by.air)}${by.works > 0 ? `, the guns and airfields ${share(by.works)}` : ''}, and ${share(left)} is left to spend.">${arm3.map((k) => (by[k] > 0 ? `<i class="${k}" style="width:${pct(by[k])}" title="${ARM_NAME[k]}: ${cash(by[k])} a month"></i>` : '')).join('')}${left > 0 ? `<i class="free" style="width:${pct(left)}" title="Left to spend: ${cash(left)} a month"></i>` : ''}</div>
      <div class="lkey">${arm3.map((k) => `<span><i class="${k}"></i>${ARM_NAME[k]} ${cash(by[k])} <small>${share(by[k])}</small></span>`).join('')}<span><i class="free"></i>${L.net >= 0 ? `Left to spend ${cash(left)} <small>${share(left)}</small>` : `Short by ${cash(-L.net)} a month`}</span></div>`
    : '';
  const unitIn = (l: LedgerLine): string => (l.count === undefined ? '' : l.key === 'land' ? `${l.count} province${l.count === 1 ? '' : 's'}` : l.key === 'trade' ? `${l.count} partner${l.count === 1 ? '' : 's'}` : l.key === 'capital' ? '' : `${num(l.count)}`);
  const unitOut = (l: LedgerLine): string => (l.count === undefined ? '' : l.key.startsWith('corps:') ? `${num(l.count)} division${l.count === 1 ? '' : 's'}` : l.key.startsWith('air:') ? `${num(l.count)} aircraft` : `${num(l.count)}`);
  const moneyIn = `<table class="ltab">${ledgerRows(L.income, 1, unitIn)}${ledgerRows(L.changes, 1, () => '')}<tr class="sum"><td>Income each month</td><td>${cash(L.gross, true)}</td></tr></table>`;
  const arms = ARMS.filter((k) => by[k] > 0).map((k) => `<tr class="arm"><td><i class="dot ${k}"></i>${ARM_NAME[k]}</td><td>${cash(-by[k], true)}</td></tr>${ledgerRows(L.upkeep.filter((l) => ARM_OF(l.key) === k), -1, unitOut)}`).join('');
  const moneyOut = L.budget
    ? `<table class="ltab">${arms || '<tr><td>No forces under arms</td><td>0</td></tr>'}<tr class="sum"><td>Upkeep each month</td><td>${cash(-L.upkeepTotal, true)}</td></tr></table>${n.atWar.length ? '' : `<p class="sub" style="margin-top:6px">At peace the forces are on a peacetime footing and cost ${Math.round(BUDGET.upkeep.peace * 100)}% of this in war.</p>`}`
    : `<p class="sub">In a historical scenario the forces are kept by the state as history kept them: no upkeep is charged, so that history's timetable holds. In the grand campaign every corps, ship and aircraft draws its keep from the treasury.</p>`;
  const all = !!ui.modal?.all;
  const shown = all ? L.provinces : L.provinces.slice(0, 10);
  const prov = shown.map((p) => {
    const r = g.regions[p.id];
    const bits: string[] = [];
    if (p.capital) bits.push(`capital ${cash(p.capital)}`);
    if (p.fac) bits.push(`${r.fac} factor${r.fac === 1 ? 'y' : 'ies'} ${cash(p.fac)}`);
    if (p.oil) bits.push(`oil ${cash(p.oil)}`);
    if (p.port) bits.push(`harbour ${cash(p.port)}`);
    if (p.land) bits.push(`${r.terrain} ${cash(p.land)}`);
    return `<button class="lprov" data-act="ledgergo" data-id="${p.id}" title="Show ${esc(p.name)} on the map"><span class="grow"><b>${esc(p.name)}</b>${r.owner !== nat ? ' <span class="tag">Occupied</span>' : r.overseas ? ' <span class="tag">Overseas</span>' : ''}<small>${bits.join(' · ')}</small></span><span class="lbarx"><i style="width:${Math.max(2, (p.total / Math.max(L.provinces[0]?.total ?? 1, 1e-9)) * 100).toFixed(0)}%"></i></span><b class="amt">${cash(p.total)}</b></button>`;
  }).join('');
  const debt = L.treasury < 0
    ? `<div class="note" style="border-left-color:var(--red)"><b>In debt.</b> While the treasury is empty no replacements go out and nothing new can be ordered. Unpaid troops recover up to ${Math.round(BUDGET.debt.rest * 100)}% less cohesion each month and up to ${Math.round(BUDGET.debt.desert * 100)}% of every corps drifts away, worse the deeper the debt. Creditors stop lending at ${BUDGET.debt.floor} months of income.</div>`
    : '';
  const story = L.budget
    ? `Think of it as a household budget. Every province is a pay cheque, large or small. The armed forces are the standing bills, due every month whether they fight or not. What is left after the bills replaces losses first and then buys new forces, and a new corps is not only its price: it is another bill every month after.`
    : `Every month the factories pay in, losses in the field are made good, and what is left buys new corps and works.`;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">The ledger · ${date(g.turn)}</div><h2>${flagIcon(nat, n.color, 34, 'inh')} ${esc(mine ? 'Your treasury' : `The treasury of ${natName(nat)}`)}</h2></header>
    <div class="body ledger">
      <div class="lfacts"><div class="lfact"><span>In the treasury</span><b class="${L.treasury < 0 ? 'neg' : ''}">${inHand(L.treasury)}</b></div><div class="lfact"><span>Income a month</span><b>${cash(L.gross, true)}</b></div><div class="lfact"><span>Upkeep a month</span><b>${L.budget ? cash(-L.upkeepTotal, true) : '0'}</b></div><div class="lfact net"><span>${L.net >= 0 ? 'Left to spend a month' : 'Short each month'}</span><b class="${L.net < 0 ? 'neg' : 'pos'}">${cash(L.net, true)}</b></div></div>
      ${bar}${debt}
      <p class="sub">${story}</p>
      <div class="lcols"><div><h3>Money in</h3>${moneyIn}</div><div><h3>Money out</h3>${moneyOut}</div></div>
      <h3>What the provinces pay <span class="inhand">${L.provinces.length} held · richest first</span></h3>
      <div class="lprovs">${prov || '<div class="empty">No provinces held.</div>'}</div>
      ${L.provinces.length > 10 ? `<div class="acts"><button class="btn small" data-act="ledgerall">${all ? 'Show the ten richest' : `Show all ${L.provinces.length} provinces`}</button></div>` : ''}
    </div>
    <footer>${mine ? '<button class="btn" data-act="ledgerbuild">Raise forces</button>' : ''}<button class="btn go" data-act="close">Close</button></footer>`;
}

function declareCard(): string {
  const list = declarable(g);
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Diplomacy</div><h2>Declare war</h2></header>
    <div class="body"><p class="lead">A neutral that is attacked joins the other side, and its friends may march to help it at once.</p>
    ${list.map((id) => `<button class="pickrow" data-act="dodeclare" data-nat="${id}">${chip(id)}<span class="grow"><b>${esc(natName(id))}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${num(fieldArmy(g, id))} divisions</span></span></button>`).join('')}</div>
    <footer><button class="btn" data-act="close">Not now</button></footer>`;
}

// ---- Running the game ---------------------------------------------------------------------

/** Starts a campaign. With a later starting month, the computer plays history up to it and then hands over. */
function newGame(human: string | null, seed?: number, from = 1, view = ''): void {
  stopAuto();
  dropPassing();
  g = createGame(seed ?? (Date.now() % 900000) + 1, from > 1 ? null : human);
  // A grand campaign: history is played to the day the player takes over, and from then on nobody follows it.
  const grand = view === 'grand' || view === 'open';
  if (grand && from === 1) startOpen(g);
  while (g.turn < from) runTurnAI(g);
  if (grand && from > 1) startOpen(g);
  g.human = human;
  // Honours earned before the player took over are his to carry; their citations are old news.
  if (from > 1) takeOver(g);
  if (human) openGoals(g, human, from);
  ui.live = true;
  ui.sel = -1; ui.picked.clear(); ui.rail = ui.ship = false; ui.tab = 'region'; ui.unread = 0; ui.seenReports = from > 1 ? g.reports.length : 0; ui.flash = [];
  ui.sea = ui.fleet = -1; ui.land = ui.rebase = false; ui.msg = ''; ui.seenSea = from > 1 ? g.seaReports.length : 0;
  ui.air = -1; ui.airMode = ''; ui.seenAir = from > 1 ? g.airReports.length : 0;
  drawStatic();
  const italy = view === 'italy', west = view === 'west', east = view === 'east';
  const britain = from === 11, desert = from === 13 || from === 33, balkans = from === 14, blue = from === 34, torch = from === 39, kursk = from === 47 && !italy;
  if (britain) ui.tab = 'air';
  const focus = britain ? g.byName['Calais'] : desert ? g.byName['Tobruk'] : torch ? g.byName['Constantine'] : balkans ? g.byName['Thessaly'] : blue ? g.byName[human === 'SOV' ? 'Stalingrad' : 'Kharkov'] : kursk ? g.byName['Kursk'] : east ? g.byName[from >= 58 ? 'Minsk' : 'Kiev'] : west ? g.byName['Rouen'] : italy ? g.byName[from >= 53 ? 'Rome' : 'Calabria'] : from > 1 ? g.byName[human === 'SOV' ? 'Smolensk' : 'Brest'] : human ? g.regions[g.nations[human].capital] : g.byName['Poznan'];
  ui.view = { x: focus.x - (torch ? 420 : 300), y: focus.y - (torch ? 250 : 230), w: torch ? 760 : desert || balkans ? 580 : human === 'GER' || from > 1 ? 640 : 600 };
  // Italy in 1944 is fought over a hundred miles of mountain: start close in.
  if (italy && from >= 53) ui.view = { x: focus.x - 215, y: focus.y - 185, w: 440 };
  // The West in 1944: the south of England, the invasion coast, and the roads to Paris and the Rhine.
  if (west) ui.view = { x: focus.x - 215, y: focus.y - 200, w: 520 };
  // The eastern front of 1944 runs from the Gulf of Finland to the Black Sea: start with all of it in view.
  if (east) ui.view = { x: focus.x - 400, y: focus.y - (from >= 58 ? 300 : 330), w: 800 };
  applyView();
  // A country that is attacked before its first month comes round places its army on the eve of war.
  if (human && from === 1 && beginEve(g)) refresh();
  else if (human) advanceToHuman(from > 1 && human !== 'GER'); else { ui.unread = from > 1 ? 0 : g.headlines.length; refresh(); }
}

/** Picks up the campaign saved in this browser. Returns false if there is none that can be read. */
function continueGame(): boolean {
  const loaded = loadGame(readSave() ?? '');
  if (!loaded) return false;
  stopAuto();
  dropPassing();
  g = loaded;
  ui.live = true;
  ui.sel = -1; ui.picked.clear(); ui.rail = ui.ship = false; ui.tab = 'region'; ui.unread = 0; ui.seenReports = g.reports.length; ui.flash = [];
  ui.sea = ui.fleet = -1; ui.land = ui.rebase = false; ui.msg = ''; ui.seenSea = g.seaReports.length;
  ui.air = -1; ui.airMode = ''; ui.seenAir = g.airReports.length;
  drawStatic();
  const me = g.human;
  const cap = me ? g.nations[me].capital : -1;
  const focus = cap >= 0 ? g.regions[cap] : g.byName['Paris'];
  ui.view = { x: focus.x - 300, y: focus.y - 230, w: 620 };
  applyView();
  if (me && !humanUp(g)) advanceToHuman(false); else refresh();
  return true;
}

/** Lets the computer play until it is the player's month again. */
type Since = { h: number; r: number; s: number; a: number };

/** The other nations' months being played out in front of the player, one nation at a time. */
let passing: { since: Since; before: string[]; show: boolean; view: { x: number; y: number; w: number }; timer: number; nat: string; note: string; done: number; total: number } | null = null;
/** How long the map rests on a nation that has done something, in milliseconds. */
const WATCH_PAUSE = 700;

function watchPref(): boolean {
  try { return localStorage.getItem('tc-watch') !== '0'; } catch { return true; }
}

function advanceToHuman(show: boolean, since?: Since): void {
  const from: Since = since ?? { h: g.headlines.length, r: g.reports.length, s: g.seaReports.length, a: g.airReports.length };
  const before = g.regions.map((r) => r.ctrl);
  if (show && g.human && watchPref() && !humanUp(g)) {
    passing = { since: from, before, show, view: { ...ui.view }, timer: 0, nat: '', note: '', done: 0, total: g.order.length - 1 };
    ui.sel = -1; ui.picked.clear(); ui.rail = ui.ship = ui.land = ui.rebase = false; ui.msg = ''; ui.airMode = ''; ui.flash = [];
    refresh();
    passStep();
    return;
  }
  runOthers();
  arrive(show, from, before);
}

/** Every computer nation's month until the player is up again, or has fallen. */
function runOthers(): void {
  let guard = 0;
  while (!humanUp(g) && guard++ < 80) {
    if (g.human && !g.nations[g.human].alive) break;
    stepOther();
  }
}

/** One computer nation's month. A goal that asks what stands when a month opens is judged the moment it opens. */
function stepOther(): string {
  const month = g.turn;
  const nat = stepAI(g);
  if (g.human && g.turn !== month) judgeGoals(g, g.human);
  return nat;
}

const some = (names: string[]): string => (names.length <= 3 ? names.join(', ') : `${names.slice(0, 3).join(', ')} and ${names.length - 3} more`);

/** Plays computer nations until one of them has done something worth seeing, shows it, and waits a moment. */
function passStep(): void {
  const p = passing;
  if (!p) return;
  p.timer = 0;
  for (let guard = 0; guard < 80; guard++) {
    if (humanUp(g) || !g.human || !g.nations[g.human].alive) break;
    const ctrl = g.regions.map((r) => r.ctrl);
    const r0 = g.reports.length;
    const nat = stepOther();
    p.done++;
    const taken = g.regions.filter((r, i) => r.ctrl !== ctrl[i]).map((r) => r.id);
    const reps = g.reports.slice(r0);
    if (!taken.length && !reps.length) continue;
    const won = reps.filter((r) => r.taken).map((r) => r.regionName);
    const held = reps.filter((r) => !r.taken).map((r) => r.regionName);
    const walked = taken.filter((id) => !reps.some((r) => r.taken && r.region === id)).map((id) => g.regions[id].name);
    const said = [won.length ? `took ${some(won)}` : '', walked.length ? `moved into ${some(walked)}` : '', held.length ? `${won.length || walked.length ? 'and were' : 'were'} thrown back at ${some(held)}` : ''].filter(Boolean);
    p.nat = nat;
    p.note = `${g.nations[nat].adj} forces ${said.join(', ')}.`;
    ui.flash = taken;
    const mine = reps.some((r) => r.defNat === g.human || r.defenders.some((u) => u.nat === g.human)) || taken.some((id) => p.before[id] === g.human);
    refresh();
    glide(viewFor([...new Set([...taken, ...reps.map((r) => r.region)])]));
    p.timer = window.setTimeout(passStep, WATCH_PAUSE + (mine ? 500 : 0) + Math.min(400, 100 * (reps.length - 1)));
    return;
  }
  endPassing();
}

/** The watching is over, or cut short: whatever is left is played at once and the player's month opens. */
function endPassing(): void {
  const p = passing;
  if (!p) return;
  clearTimeout(p.timer);
  passing = null;
  runOthers();
  glide(p.view, 260);
  arrive(p.show, p.since, p.before);
}

/** Drops a watched month without finishing it: a new game is about to replace this one. */
function dropPassing(): void {
  if (passing) clearTimeout(passing.timer);
  passing = null;
  glideToken++;
}

function arrive(show: boolean, from: Since, before: string[]): void {
  const hFrom = from.h, rFrom = from.r, sFrom = from.s, aFrom = from.a;
  if (g.human) judgeGoals(g, g.human);
  const lost = g.human && !g.nations[g.human].alive ? g.human : null;
  if (lost) {
    // The month the country fell in is played out to its end, so the watcher's calendar starts on a month's last day.
    g.human = null;
    while (g.phase > 0) stepAI(g);
  }
  if (humanUp(g)) beginHuman(g);
  ui.flash = g.regions.filter((r, i) => r.ctrl !== before[i]).map((r) => r.id);
  ui.unread += g.headlines.length - hFrom;
  ui.sel = -1; ui.picked.clear(); ui.rail = ui.ship = ui.land = ui.rebase = false; ui.msg = ''; ui.airMode = '';
  refresh();
  if (lost) {
    openModal({ kind: 'news', from: hFrom, reports: rFrom, sea: sFrom, air: aFrom });
    // A nation whose goals are about holding out is told how long it held.
    const weeks = Object.values(GOALS[lost] ?? {}).some((x) => x?.outlast !== undefined) ? weeksLasted(g, lost) : null;
    const nth = ['', 'first', 'second', 'third', 'fourth', 'fifth', 'sixth', 'seventh', 'eighth', 'ninth', 'tenth', 'eleventh', 'twelfth'];
    $('#card').querySelector('.kick')!.textContent = weeks !== null && nth[weeks]
      ? `${natName(lost)} has fallen in the ${nth[weeks]} week of the war. The war goes on without you.`
      : `${natName(lost)} has fallen. The war goes on without you.`;
    return;
  }
  if (show && (g.headlines.length > hFrom || g.reports.length > rFrom)) openModal({ kind: 'news', from: hFrom, reports: rFrom, sea: sFrom, air: aFrom });
}

function nextMonth(): void {
  const before = g.regions.map((r) => r.ctrl);
  const hFrom = g.headlines.length, rFrom = g.reports.length;
  runTurnAI(g);
  ui.flash = g.regions.filter((r, i) => r.ctrl !== before[i]).map((r) => r.id);
  ui.unread += g.headlines.length - hFrom;
  // Follow the action.
  const hot = new Set<number>(ui.flash);
  for (const r of g.reports.slice(rFrom)) hot.add(r.region);
  if (!hot.size) for (const r of g.airReports) if (r.turn === g.turn - 1 && r.region >= 0) hot.add(r.region);
  if (hot.size) focusOn([...hot]);
  if (ui.tab === 'reel') ui.unread = 0;
  refresh();
  if (g.turn > (isOpen(g) ? FREE_ENDS : STORY_ENDS)) stopAuto();
}

/** The view that takes in these regions with some room round them. */
function viewFor(ids: number[]): { x: number; y: number; w: number } {
  let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
  for (const id of ids) { const r = g.regions[id]; x0 = Math.min(x0, r.x); y0 = Math.min(y0, r.y); x1 = Math.max(x1, r.x); y1 = Math.max(y1, r.y); }
  const svg = $('#map');
  const a = svg.clientHeight / Math.max(1, svg.clientWidth);
  const w = Math.max(420, x1 - x0 + 260, (y1 - y0 + 240) / a);
  return { x: (x0 + x1) / 2 - w / 2, y: (y0 + y1) / 2 - (w * a) / 2, w };
}

function focusOn(ids: number[]): void {
  glideToken++;
  ui.view = viewFor(ids);
  applyView();
}

let glideToken = 0;

/** Moves the map to a new view smoothly, as a camera would travel, instead of jumping there. */
function glide(to: { x: number; y: number; w: number }, ms = 340): void {
  const from = { ...ui.view };
  const token = ++glideToken;
  const t0 = performance.now();
  const step = (now: number): void => {
    if (token !== glideToken) return;
    const k = Math.min(1, (now - t0) / ms);
    const e = k < 0.5 ? 2 * k * k : 1 - (-2 * k + 2) ** 2 / 2;
    ui.view = { x: from.x + (to.x - from.x) * e, y: from.y + (to.y - from.y) * e, w: from.w + (to.w - from.w) * e };
    applyView();
    if (k < 1) requestAnimationFrame(step);
  };
  requestAnimationFrame(step);
}

function focusOnPoint(x: number, y: number): void {
  const svg = $('#map');
  const a = svg.clientHeight / Math.max(1, svg.clientWidth);
  const w = Math.max(ui.view.w, 520);
  ui.view = { x: x - w / 2, y: y - (w * a) / 2, w };
  applyView();
}

/** Where on the map a point of the screen falls. */
function mapPoint(cx: number, cy: number): { x: number; y: number } {
  const rect = $('#map').getBoundingClientRect();
  const k = ui.view.w / Math.max(1, rect.width);
  return { x: ui.view.x + (cx - rect.left) * k, y: ui.view.y + (cy - rect.top) * k };
}

/** The player's fleet being given orders, if it is still theirs to order. */
function myFleet(): Fleet | null {
  if (ui.fleet < 0 || !humanUp(g)) return null;
  const f = g.fleets.find((x) => x.id === ui.fleet);
  return f && f.nat === g.human && shipCount(f.ships) > 0 ? f : null;
}

function onSea(z: number): void {
  ui.flash = [];
  ui.msg = '';
  const af = ui.tab === 'air' ? myAir() : null;
  if (af && ui.airMode) {
    if (ui.airMode === 'strike') {
      const why = whyNotMission(g, af, 'strike', z);
      if (!why) { openModal({ kind: 'airplan', fleet: af.id, mission: 'strike', target: z }); return; }
      ui.msg = `The ${g.seas[z].name}: ${why}`;
    } else ui.msg = ui.airMode === 'base' ? 'Airfields are on land: click one of the blue regions.' : 'This mission needs a region: click one of the red ones.';
    refresh();
    return;
  }
  const fl = ui.tab === 'navy' ? myFleet() : null;
  if (fl) {
    if (ui.rebase) ui.msg = 'A base has to be a port: click one of the blue regions.';
    else if (!orderFleet(g, fl.id, z, ui.stance)) ui.msg = `The ${g.seas[z].name} is out of reach of the ${fl.name} from ${fl.port}.`;
    refresh();
    return;
  }
  ui.sea = z; ui.sel = -1; ui.picked.clear(); ui.rail = ui.ship = ui.land = false;
  ui.tab = 'region';
  refresh();
}

function stopAuto(): void {
  if (ui.auto) { clearInterval(ui.auto); ui.auto = 0; }
}

// ---- Clicks -------------------------------------------------------------------------------

function onClick(e: MouseEvent): void {
  const t = (e.target as HTMLElement).closest('[data-act]') as HTMLElement | null;
  if (!t) { if (e.target === $('#veil') && ui.modal && ui.modal.kind !== 'start') openModal(null); return; }
  const act = t.dataset.act!;
  const id = Number(t.dataset.id);
  ui.msg = '';
  switch (act) {
    case 'start': if (t.dataset.view === 'grand') rememberPick(); openModal(null); newGame(t.dataset.nat || null, undefined, Number(t.dataset.from || 1), t.dataset.view || ''); break;
    case 'menu': ui.menu = t.dataset.menu === 'scenarios' ? 'scenarios' : 'grand'; openModal({ kind: 'start' }); break;
    case 'gcnat': case 'gcyear': {
      if (act === 'gcnat') ui.gcNat = t.dataset.nat ?? ''; else ui.gcYear = Number(t.dataset.year) || 1939;
      // Repainting the card must not throw the reader back to its top.
      const keep = $('#card').scrollTop;
      openModal({ kind: 'start' });
      $('#card').scrollTop = keep;
      break;
    }
    case 'continue': if (continueGame()) openModal(null); else openModal({ kind: 'start' }); break;
    case 'new': stopAuto(); endPassing(); ui.menu = 'grand'; openModal({ kind: 'start' }); break;
    case 'skip': endPassing(); break;
    case 'alert': followAlert(t.dataset.kind!); break;
    case 'ledger': openModal({ kind: 'ledger', nat: t.dataset.nat || g.human || '' }); break;
    case 'ledgerall': { const top = $('#card').scrollTop; openModal({ ...ui.modal!, all: !ui.modal?.all }); $('#card').scrollTop = top; break; }
    case 'ledgergo': { openModal(null); ui.tab = 'region'; selectRegion(id); refresh(); focusOn([id]); break; }
    case 'ledgerbuild': openModal(null); ui.tab = 'build'; refresh(); break;
    case 'record': openModal({ kind: 'unit', type: t.dataset.type as UnitKind, id }); break;
    case 'fallen': openModal({ kind: 'fallen', id }); break;
    case 'award': openModal({ kind: 'award', id }); refresh(); break;
    case 'nextaward': { const next = unseen(g)[0]; openModal(next ? { kind: 'award', id: next.id } : null); refresh(); break; }
    case 'cash': {
      const a = awardOf(g, id);
      if (a && orderCash(g, id, t.dataset.how as Cash)) { ui.picked.clear(); queueSave(); }
      refresh();
      openModal({ kind: 'award', id });
      break;
    }
    case 'drafts': {
      const c = g.corps.find((x) => x.id === id);
      if (c && orderDrafts(g, c, t.dataset.v === 'take')) queueSave();
      refresh();
      openModal({ kind: 'unit', type: 'corps', id });
      break;
    }
    case 'revive': {
      const f = fallenOf(g, id), at = f ? raiseSite(g, g.human!, f.kind, ui.sel) : -1;
      if (f && at >= 0 && orderBuildCorps(g, f.kind, at, f.id)) { ui.msg = `${f.name} will be raised again at ${g.regions[at].name}.`; queueSave(); if (ui.modal?.kind === 'fallen') openModal(null); }
      refresh();
      break;
    }
    case 'watchpref': { try { localStorage.setItem('tc-watch', watchPref() ? '0' : '1'); } catch { /* storage is off: the choice lasts as long as the default */ } refresh(); break; }
    case 'close': openModal(null); refresh(); break;
    case 'tab':
      ui.tab = t.dataset.tab as typeof ui.tab;
      if (ui.tab === 'reel') ui.unread = 0;
      if (ui.tab === 'battles') { ui.seenReports = g.reports.length; ui.seenSea = g.seaReports.length; ui.seenAir = g.airReports.length; }
      ui.rail = ui.ship = ui.land = ui.rebase = false; ui.airMode = '';
      refresh();
      break;
    case 'zin': zoomBy(0.7); break;
    case 'zout': zoomBy(1.4); break;
    case 'next': nextMonth(); break;
    case 'auto':
      if (ui.auto) stopAuto(); else { nextMonth(); ui.auto = window.setInterval(nextMonth, 1500); }
      drawBar();
      break;
    case 'end': {
      // The player's own fleet actions are fought as the month closes, so the dispatches start from here.
      const since = { h: g.headlines.length, r: g.reports.length, s: g.seaReports.length, a: g.airReports.length };
      if (onEve(g)) endEve(g); else { judgeGoals(g, g.human!); endHuman(g); }
      advanceToHuman(true, since);
      break;
    }
    case 'pick': {
      if (ui.picked.has(id)) ui.picked.delete(id); else ui.picked.add(id);
      ui.rail = ui.ship = ui.land = false; refresh(); break;
    }
    case 'pickall': selectRegion(ui.sel); refresh(); break;
    case 'eveplan': {
      const moved = orderEvePlan(g);
      ui.sel = -1; ui.picked.clear(); ui.rail = ui.ship = ui.land = false;
      ui.msg = moved
        ? `${moved} corps sent by rail as the staff advised. Now give each army its orders: click an army, then Stand, Dig in or Fall back.`
        : 'The army is already where the staff wants it, or has made its move for the eve.';
      refresh(); break;
    }
    case 'ifattacked': {
      const a = trayArmy;
      if (!a || a.nat !== g.human) break;
      const posture = t.dataset.v as Posture;
      const at = g.regions[a.reg].name;
      const told = g.regions[a.reg].units.filter((c) => c.nat === a.nat && orderStance(g, c, posture)).length;
      if (told) ui.msg = posture === 'back'
        ? `The army at ${at} will give ground if the enemy attacks with the odds to carry the position.`
        : posture === 'dig'
          ? `The army at ${at} is digging in. Leave it where it is this month and it fights from trenches from the next.`
          : `The army at ${at} will stand and fight where it is.`;
      refresh(); break;
    }
    case 'unpick': ui.picked.clear(); ui.rail = ui.ship = ui.land = false; refresh(); break;
    case 'rail': ui.rail = !ui.rail; ui.ship = ui.land = false; refresh(); break;
    case 'ship': ui.ship = !ui.ship; ui.rail = ui.land = false; refresh(); break;
    case 'land': ui.land = !ui.land; ui.rail = ui.ship = false; refresh(); break;
    case 'fleet': {
      const f = g.fleets.find((x) => x.id === id);
      ui.fleet = ui.fleet === id ? -1 : id;
      ui.rebase = false;
      if (f && ui.fleet === id) ui.stance = atSea(f) ? f.stance : f.duty === 'raid' ? 'raid' : 'patrol';
      refresh(); break;
    }
    case 'stance': {
      ui.stance = t.dataset.stance as Stance;
      const f = myFleet();
      if (f && atSea(f)) orderFleet(g, f.id, f.zone, ui.stance);
      refresh(); break;
    }
    case 'sail': {
      const f = myFleet();
      if (f && !orderFleet(g, f.id, Number(t.dataset.zone), ui.stance)) ui.msg = `The ${f.name} cannot reach that sea.`;
      ui.rebase = false;
      refresh(); break;
    }
    case 'toport': { const f = myFleet(); if (f) orderFleet(g, f.id, f.home, 'port'); ui.rebase = false; refresh(); break; }
    case 'rebase': ui.rebase = !ui.rebase; refresh(); break;
    case 'showsea': openModal(null); ui.sea = id; ui.sel = -1; ui.picked.clear(); ui.tab = 'region'; focusOnPoint(g.seas[id].x, g.seas[id].y); refresh(); break;
    case 'seareport': openModal({ kind: 'seareport', id }); break;
    case 'airfleet': {
      ui.air = ui.air === id ? -1 : id;
      ui.airMode = '';
      const f = g.airFleets.find((x) => x.id === ui.air);
      if (f) { ui.sel = -1; ui.sea = -1; ui.picked.clear(); }
      refresh(); break;
    }
    case 'mission': {
      const f = myAir();
      const m = t.dataset.mission as AirMission;
      if (!f) break;
      if (isOffensive(m)) ui.airMode = ui.airMode === m ? '' : m;
      else { orderAir(g, f.id, m); ui.airMode = ''; }
      refresh(); break;
    }
    case 'airbase': ui.airMode = ui.airMode === 'base' ? '' : 'base'; refresh(); break;
    case 'airtarget': {
      const f = myAir();
      if (f && ui.airMode && ui.airMode !== 'base') openModal({ kind: 'airplan', fleet: f.id, mission: ui.airMode, target: Number(t.dataset.target) });
      break;
    }
    case 'doair': {
      const m = ui.modal!;
      if (orderAir(g, m.fleet, m.mission, m.target)) ui.airMode = ''; else ui.msg = 'That order could not be given.';
      openModal(null); refresh(); break;
    }
    case 'airreport': openModal({ kind: 'airreport', id }); break;
    case 'doland': {
      const m = ui.modal!;
      const h = g.headlines.length;
      const before = g.regions.map((r) => r.ctrl);
      const res = orderLand(g, g.corps.filter((c) => m.corps.has(c.id)), m.target);
      ui.unread += g.headlines.length - h;
      ui.flash = g.regions.filter((r, i) => r.ctrl !== before[i]).map((r) => r.id);
      ui.land = false;
      if (!res.ok) { ui.msg = res.why ?? ''; openModal(null); refresh(); break; }
      ui.picked.clear();
      ui.sel = m.target; ui.sea = -1;
      if (res.report) { ui.seenReports = g.reports.length; refresh(); openModal({ kind: 'report', id: res.report.id }); }
      else { openModal(null); refresh(); }
      break;
    }
    case 'arm': ui.arm = t.dataset.arm as typeof ui.arm; refresh(); break;
    case 'armgo': ui.arm = t.dataset.arm as typeof ui.arm; ui.tab = 'build'; refresh(); break;
    case 'arsenal': {
      const b = orderArsenal(g, t.dataset.key!, ui.sel);
      if (b) { ui.msg = `Ordered: ${b.name}. Ready ${date(b.eta)}.`; queueSave(); }
      refresh();
      break;
    }
    case 'fortify': if (ui.sel >= 0) orderBuildFort(g, ui.sel); refresh(); break;
    case 'cancelbuild': orderCancelBuild(g, id); refresh(); break;
    case 'teams': orderTeams(g, t.dataset.field as Field, Number(t.dataset.n)); refresh(); break;
    case 'bureau': orderBureau(g); refresh(); break;
    case 'court': { const h = g.headlines.length; orderCourt(g, t.dataset.nat!); ui.unread += g.headlines.length - h; refresh(); break; }
    case 'evac': {
      for (const c of pickedCorps()) orderEvacuate(g, c);
      ui.picked.clear(); refresh(); break;
    }
    case 'report': openModal({ kind: 'report', id }); break;
    case 'general': openModal({ kind: 'general', id }); break;
    case 'appoint': openModal({ kind: 'appoint', corps: id }); break;
    case 'assign': orderAssign(g, id, Number(t.dataset.corps)); refresh(); openModal({ kind: 'general', id }); break;
    case 'relieve': orderRelieve(g, id); refresh(); openModal({ kind: 'general', id }); break;
    case 'skill': orderSkill(g, id, t.dataset.skill as Skill); refresh(); openModal({ kind: 'general', id }); break;
    case 'keep': openModal({ kind: 'general', id }); break;
    case 'sack': {
      if (!ui.modal?.confirm) { openModal({ kind: 'general', id, confirm: true }); break; }
      const h = g.headlines.length;
      orderSack(g, id);
      ui.unread += g.headlines.length - h;
      refresh();
      openModal({ kind: 'general', id });
      break;
    }
    case 'promote': {
      const x = orderPromote(g, id);
      refresh();
      if (x) openModal({ kind: 'general', id: x.id });
      break;
    }
    case 'showmap': openModal(null); ui.sel = id; ui.tab = 'region'; focusOn([id]); refresh(); break;
    case 'declare': openModal({ kind: 'declare' }); break;
    case 'dodeclare': { const h = g.headlines.length; orderDeclare(g, t.dataset.nat!); ui.unread += g.headlines.length - h; openModal(null); refresh(); break; }
    case 'atkpick': {
      const m = ui.modal!;
      if (m.chosen.has(id)) m.chosen.delete(id); else m.chosen.add(id);
      $('#card').innerHTML = attackCard(); break;
    }
    case 'attack': {
      const m = ui.modal!;
      const target = g.regions[m.target];
      const h = g.headlines.length;
      const before = g.regions.map((r) => r.ctrl);
      const rep = orderAttack(g, g.corps.filter((c) => m.chosen.has(c.id)), target);
      ui.unread += g.headlines.length - h;
      ui.flash = g.regions.filter((r, i) => r.ctrl !== before[i]).map((r) => r.id);
      if (rep) {
        ui.seenReports = g.reports.length;
        if (rep.taken) { ui.sel = target.id; ui.picked = new Set(target.units.filter((c) => canOrder(g, c) && c.ap > 0).map((c) => c.id)); }
        refresh();
        openModal({ kind: 'report', id: rep.id });
      } else openModal(null);
      break;
    }
  }
}

/** The keyboard: Escape backs out of whatever is open, Enter ends the month, Space skips the watching. */
function onKey(e: KeyboardEvent): void {
  const el = e.target as HTMLElement | null;
  if (e.ctrlKey || e.metaKey || e.altKey || (el && /^(INPUT|TEXTAREA|SELECT)$/.test(el.tagName))) return;
  if (e.key === 'Escape') {
    if (ui.modal) { if (ui.modal.kind !== 'start' || ui.live) { openModal(null); refresh(); } }
    else if (ui.picked.size || ui.sel >= 0) { ui.picked.clear(); ui.sel = -1; ui.rail = ui.ship = ui.land = false; ui.msg = ''; refresh(); }
    return;
  }
  // A key pressed on a button belongs to that button.
  if (el && el.tagName === 'BUTTON') return;
  if (e.key === ' ' && passing) { e.preventDefault(); endPassing(); }
  else if (e.key === 'Enter' && !ui.modal && !passing && g.human && humanUp(g)) { e.preventDefault(); ($('#barbtns').querySelector('[data-act="end"]') as HTMLElement | null)?.click(); }
}

function selectRegion(id: number): void {
  ui.sel = id;
  ui.sea = -1;
  ui.rail = ui.ship = ui.land = false;
  ui.picked = new Set(humanUp(g) ? g.regions[id].units.filter((c) => canOrder(g, c) && c.ap > 0).map((c) => c.id) : []);
  ui.tab = 'region';
}

function onRegion(id: number): void {
  const r = g.regions[id];
  ui.flash = [];
  ui.msg = '';
  // Under the Air tab with a fleet picked and an order half given, a click on the map finishes it.
  const af = ui.tab === 'air' ? myAir() : null;
  if (af && ui.airMode) {
    if (ui.airMode === 'base') {
      const why = whyNotBase(g, af, r);
      if (why) ui.msg = `${af.name} cannot move to ${r.name}. ${why}`;
      else { orderAirBase(g, af.id, id); ui.airMode = ''; }
    } else if (ui.airMode === 'strike') ui.msg = 'A strike on shipping needs a sea: click one of the ringed seas.';
    else {
      const why = whyNotMission(g, af, ui.airMode, id);
      if (!why) { openModal({ kind: 'airplan', fleet: af.id, mission: ui.airMode, target: id }); return; }
      ui.msg = `${r.name}: ${why}`;
    }
    refresh();
    return;
  }
  // Under the Navy tab with a fleet picked, a click on a port is a change of base.
  const fl = ui.tab === 'navy' ? myFleet() : null;
  if (fl && ui.rebase) {
    if (orderRebase(g, fl.id, id)) ui.rebase = false;
    else ui.msg = `${r.name} cannot be the fleet's base: it has to be a friendly port the ships can steam to.`;
    refresh();
    return;
  }
  // While planning production the map only chooses where to build. No army moves by accident.
  if (ui.tab === 'build') {
    ui.sel = id; ui.sea = -1; ui.picked.clear(); ui.rail = ui.ship = false;
    refresh();
    return;
  }
  const picked = pickedCorps();
  if (picked.length && ui.land) {
    const plan = previewLanding(g, picked, id);
    if (plan.why) { ui.msg = plan.why; refresh(); return; }
    openModal({ kind: 'landing', target: id, corps: new Set(picked.map((c) => c.id)) });
    return;
  }
  if (picked.length && (ui.rail || ui.ship)) {
    for (const c of picked) { if (ui.rail) orderRail(g, c, id); else orderShip(g, c, id); }
    selectRegion(id);
    ui.picked.clear();
    ui.tab = 'region';
    refresh();
    return;
  }
  // A click on another of the player's armies picks that one up: sending this one to join it takes a right-click.
  if (picked.length && id !== ui.sel && !r.units.some((c) => canOrder(g, c) && c.ap > 0) && orderAt(id)) return;
  selectRegion(id);
  refresh();
}

// ---- Moving around the map ----------------------------------------------------------------

function applyView(): void {
  const svg = $('#map');
  const a = svg.clientHeight / Math.max(1, svg.clientWidth);
  const v = ui.view;
  v.w = Math.max(150, Math.min(2100, v.w));
  const h = v.w * a;
  v.x = Math.max(-200, Math.min(g.width + 200 - v.w, v.x));
  // The map ends in the Sahara: there is nothing to scroll to below it.
  v.y = Math.max(-200, Math.min(g.height - h, v.y));
  svg.setAttribute('viewBox', `${v.x.toFixed(1)} ${v.y.toFixed(1)} ${v.w.toFixed(1)} ${h.toFixed(1)}`);
  svg.classList.toggle('near', v.w < 600);
  svg.classList.toggle('mid', v.w < 760);
}

function zoomBy(f: number, cx?: number, cy?: number): void {
  const svg = $('#map');
  const rect = svg.getBoundingClientRect();
  const a = rect.height / Math.max(1, rect.width);
  const v = ui.view;
  const px = cx === undefined ? 0.5 : (cx - rect.left) / rect.width;
  const py = cy === undefined ? 0.5 : (cy - rect.top) / rect.height;
  const mx = v.x + px * v.w, my = v.y + py * v.w * a;
  v.w = Math.max(150, Math.min(2100, v.w * f));
  v.x = mx - px * v.w; v.y = my - py * v.w * a;
  applyView();
}

function wireMap(): void {
  const svg = $('#map');
  const pts = new Map<number, { x: number; y: number }>();
  let start: { x: number; y: number; vx: number; vy: number } | null = null;
  let moved = false, pinch = 0;
  // On a touch screen there is no right button: pressing and holding a region gives the order instead.
  let hold = 0, held = false;
  const regionAt = (x: number, y: number): number => {
    const el = document.elementFromPoint(x, y)?.closest('.reg') as HTMLElement | null;
    return el ? Number(el.dataset.id) : -1;
  };
  svg.addEventListener('contextmenu', (e) => {
    e.preventDefault();
    const id = regionAt(e.clientX, e.clientY);
    if (id >= 0 && humanUp(g)) { ui.msg = ''; if (!orderAt(id) && pickedCorps().length && id !== ui.sel) { ui.msg = `${g.regions[id].name} is out of reach for the army you have picked up this month.`; refresh(); } }
  });
  svg.addEventListener('pointerdown', (e) => {
    if (e.pointerType === 'mouse' && e.button !== 0) return;
    held = false;
    clearTimeout(hold);
    if (e.pointerType !== 'mouse') {
      const x = e.clientX, y = e.clientY;
      hold = window.setTimeout(() => {
        if (moved || pts.size !== 1 || !humanUp(g)) return;
        const id = regionAt(x, y);
        if (id >= 0 && orderAt(id)) held = true;
      }, 520);
    }
    pts.set(e.pointerId, { x: e.clientX, y: e.clientY });
    try { svg.setPointerCapture(e.pointerId); } catch { /* not all browsers */ }
    if (pts.size === 1) { start = { x: e.clientX, y: e.clientY, vx: ui.view.x, vy: ui.view.y }; moved = false; }
    else if (pts.size === 2) { const [p, q] = [...pts.values()]; pinch = Math.hypot(p.x - q.x, p.y - q.y); moved = true; }
  });
  svg.addEventListener('pointermove', (e) => {
    const rect = svg.getBoundingClientRect();
    if (pts.has(e.pointerId)) pts.set(e.pointerId, { x: e.clientX, y: e.clientY });
    if (pts.size === 2) {
      const [p, q] = [...pts.values()];
      const d = Math.hypot(p.x - q.x, p.y - q.y);
      if (pinch > 0 && d > 0) zoomBy(pinch / d, (p.x + q.x) / 2, (p.y + q.y) / 2);
      pinch = d;
      return;
    }
    if (pts.size === 1 && start) {
      const dx = e.clientX - start.x, dy = e.clientY - start.y;
      if (Math.abs(dx) + Math.abs(dy) > 5) { moved = true; clearTimeout(hold); svg.classList.add('drag'); }
      if (moved) { const k = ui.view.w / rect.width; ui.view.x = start.vx - dx * k; ui.view.y = start.vy - dy * k; applyView(); }
      return;
    }
    // Hover: say what is under the cursor.
    const el = (e.target as Element).closest?.('[data-id]') as HTMLElement | null;
    const tip = $('#tip');
    if (e.pointerType !== 'mouse') { tip.style.display = 'none'; return; }
    drawRoute(el && el.classList.contains('reg') ? Number(el.dataset.id) : -1);
    if (!el) {
      const p = mapPoint(e.clientX, e.clientY);
      const z = seaAt(p.x, p.y);
      if (z < 0) { tip.style.display = 'none'; return; }
      const by = new Map<string, number>();
      for (const f of g.fleets) if (f.zone === z && atSea(f)) by.set(f.nat, (by.get(f.nat) ?? 0) + shipCount(f.ships));
      tip.innerHTML = `<b>${esc(g.seas[z].name)}</b><br>${by.size ? [...by].map(([nat, n]) => `${g.nations[nat].adj}: ${n} ships`).join(' · ') : 'No fleet at sea here'}`;
    } else {
    const r = g.regions[Number(el.dataset.id)];
    const armies = new Map<string, Corps[]>();
    for (const c of r.units) (armies.get(c.nat) ?? armies.set(c.nat, []).get(c.nat)!).push(c);
    const here = [...armies].map(([nat, cs]) => { const who = leaderOf(cs); return `<br>${esc(g.nations[nat].adj)} army${who ? ` under ${esc(surname(who.name))}` : ''}: ${cs.length} corps, ${num(sumDivs(cs))} divisions`; }).join('');
    const way = routeShown === r.id ? routeTo(r.id) : null;
    const order = way ? `<br><span class="tipgo">${way.attack ? 'Right-click to plan an attack' : `Right-click to march: ${way.cost} of ${way.ap} moves`}</span>` : '';
    const rel = relationWord(r.ctrl);
    tip.innerHTML = `<b>${esc(r.name)}</b><br>${natName(r.ctrl)} <span class="tiprel ${rel.kind}">${rel.word}</span>${r.ctrl !== r.owner ? ` (occupied ${g.nations[r.owner].adj} ground)` : ''} · ${r.terrain}${here}${order}`;
    }
    tip.style.display = 'block';
    const wrap = $('#mapwrap').getBoundingClientRect();
    tip.style.left = Math.min(wrap.width - 250, e.clientX - wrap.left + 14) + 'px';
    tip.style.top = Math.min(wrap.height - 70, e.clientY - wrap.top + 14) + 'px';
  });
  const up = (e: PointerEvent) => {
    const was = pts.size;
    pts.delete(e.pointerId);
    clearTimeout(hold);
    svg.classList.remove('drag');
    if (was === 1 && !moved && !held && e.type === 'pointerup') {
      $('#tip').style.display = 'none';
      const el = document.elementFromPoint(e.clientX, e.clientY)?.closest('[data-id]') as HTMLElement | null;
      if (el && el.classList.contains('reg')) onRegion(Number(el.dataset.id));
      else if (!el) {
        const p = mapPoint(e.clientX, e.clientY);
        const z = seaAt(p.x, p.y);
        if (z >= 0) onSea(z);
      }
    }
    if (pts.size === 0) start = null;
    else if (pts.size === 1) { const p = [...pts.values()][0]; start = { x: p.x, y: p.y, vx: ui.view.x, vy: ui.view.y }; moved = true; }
  };
  svg.addEventListener('pointerup', up);
  svg.addEventListener('pointercancel', up);
  svg.addEventListener('pointerleave', () => { $('#tip').style.display = 'none'; drawRoute(-1); });
  svg.addEventListener('wheel', (e) => { e.preventDefault(); zoomBy(Math.exp(e.deltaY * 0.0016), e.clientX, e.clientY); }, { passive: false });
}

// ---- Start --------------------------------------------------------------------------------

g = createGame(1939, null);
mount();
drawStatic();
applyView();
refresh();
recallPick();
openModal({ kind: 'start' });

// A handle for automated checks and for curious players in the browser console.
(window as any).theater = { game: () => g, ui, newGame, nextMonth, onRegion, onSea, refresh, applyView, continueGame, save: () => saveGame(g) };

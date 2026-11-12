// The campaign screen: a staff map of Europe, the armies on it, and the paperwork a
// commander would see. All of the rules live in ../game, ../ai and ../turn; this file
// only draws the state of the game and turns clicks into orders.

import {
  DESERT, allied, atWar, collapseScore, convoyDrag, createGame, ensureSupply, fieldArmy, frozen, hostileUnits, inDesert, limitAttackers, lineStrength, men, railReach, refuge, weather,
} from '../game';
import {
  beginEve, beginHuman, canOrder, candidates, declarable, endEve, endHuman, eveAttacker, humanUp, landTargets, onEve, orderAttack, orderBuildCorps, orderBuildFort,
  orderAir, orderAirBase, orderAssign, orderBureau, orderCancelBuild, orderCourt, orderDeclare, orderEvacuate, orderFleet, orderLand, orderMove, orderPromote, orderRail, orderRebase, orderRelieve, orderSack, orderShip, orderSkill, orderTeams,
  previewAirOrder, previewAttack, previewLanding, reach, runTurnAI, shipTargets, stepAI,
} from '../turn';
import {
  AIR, MISSION, airBases, airDeliveries, airFleetsOf, airIncomeLoss, airPlanFor, airPool, airRange, airTargets, cover, crowding, forecastAir, isOffensive,
  km as airKm, seaKm, skyOver, whyNotBase, whyNotMission, type AirOrder,
} from '../air';
import {
  NAVY, SEA_BORDERS, airborne as byAir, atSea, baseTargets, canPass, closedTo, fleetPower, fleetReach, fleetsOf, liftLeft, oreRoute, seaAt, seaRisk, seaSide, shipCount, shipList, surfaceShips,
} from '../navy';
import { phaseFor, theatrePhases } from '../ai';
import { PRODUCTION, canRaise, fortCost, income, isHome } from '../economy';
import { FIELDS, MAX_TEAMS, RESEARCH, arrival, bureauCost, doctrineOf, inService, lead, monthsAhead, nextStep, researches, stepsOf, teamsFree, teamsTotal } from '../research';
import { loadGame, peekSave, saveGame, staleSave } from '../save';
import { COMMAND, SKILLS, corpsOf, fit, generalOf, popular, serving, vacant, waiting, whyNotAssign, whyNotPromote, xpToNext } from '../commanders';
import { DIPLOMACY, TRACK_LABEL, canCourt, courtable, momentum, pcIncome, receptive, step, tradePartners } from '../diplomacy';
import { bloc } from '../game';
import { KIND, type AirFleet, type AirMission, type AirReport, type AirSide, type BattleReport, type Corps, type Field, type Fleet, type Game, type General, type Kind, type Nation, type Region, type SeaReport, type SeaSide, type Skill, type Stance } from '../types';
import { turnToDate } from '../../core/ticket';

let g: Game;
/** The month after the last one the historical plans cover: April 1942. */
const STORY_ENDS = 32;
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
  tab: 'region' as 'region' | 'build' | 'research' | 'generals' | 'navy' | 'air' | 'reel' | 'battles' | 'powers',
  view: { x: 380, y: 300, w: 620 },
  unread: 0,
  seenReports: 0,
  flash: [] as number[],
  auto: 0 as number,
  modal: null as null | { kind: string; [k: string]: any },
};

const $ = (s: string): HTMLElement => document.querySelector(s) as HTMLElement;
const esc = (s: string): string => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]!));
const num = (n: number): string => Math.round(n).toLocaleString('en-US');
const natName = (id: string): string => g.nations[id]?.name ?? id;
const date = (t: number): string => turnToDate(t).label;
const capital = (t: string): string => (t ? t[0].toUpperCase() + t.slice(1) : t);
/** Enemy coasts the picked corps could land on, worked out once each time the screen is drawn. */
let lands: Region[] = [];

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
  }
}

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
    out += `<g class="ctr flt${fs[0].stance === 'raid' ? ' raid' : ''}${small ? ' port' : ''}" transform="translate(${x.toFixed(1)} ${y.toFixed(1)})${small ? ' scale(.68)' : ''}">${picked ? '<circle class="fsel" r="17"/>' : ''}<path class="hull" d="${shape}" fill="${g.nations[fs[0].nat].color}"/><text x="0" y="11.5">${n}</text></g>`;
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
    out += `<g class="ctr flt air" transform="translate(${(r.x - 17 - i * 13).toFixed(1)} ${(r.y + 11).toFixed(1)}) scale(.62)">${picked ? '<circle class="fsel" r="17"/>' : ''}<path class="hull" d="${PLANE}" fill="${g.nations[fs[0].nat].color}"/><text x="0" y="17.5">${n}</text></g>`;
  }
  return out;
}

/** The reach of the picked air fleet, and where its next order could send it. */
function airMarks(f: AirFleet): string {
  const b = g.regions[f.base];
  const R = airRange(f.nat);
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
      <span class="spacer"></span>
      <span id="barbtns" style="display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end"></span>
    </div>
    <div class="main">
      <div class="mapwrap" id="mapwrap">
        <svg id="map" preserveAspectRatio="xMidYMid slice">
          <defs><pattern id="hatch" width="5" height="5" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><rect width="1.6" height="5" fill="rgba(20,22,24,.3)"/></pattern></defs>
          <g id="seas"></g><g id="land"></g><g id="regs"></g><g id="occ"></g><g id="fx"></g><g id="marks"></g><g id="labels"></g><g id="fleets"></g><g id="units"></g>
        </svg>
        <div class="hint" id="hint"></div>
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
    return `${star}${fort}<text class="lbl${r.cap ? ' cap' : ''}" x="${r.x}" y="${r.y + (busy ? 23 : 3)}">${esc(r.name)}</text>`;
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
    for (const c of picked) for (const [id, rc] of reach(g, c)) if (id !== c.reg) goes.set(id, (goes.get(id) ?? false) || rc.attack);
    for (const [id, attack] of goes) marks += `<path class="mk ${attack ? 'hit' : 'move'}" d="${pathOf(g.regions[id].polys)}"/>`;
  }
  if (ui.sel >= 0) marks += `<path class="mk sel" d="${pathOf(g.regions[ui.sel].polys)}"/>`;
  else if (ui.sea >= 0 && ui.tab === 'region') marks += `<circle class="mk seasel" cx="${g.seas[ui.sea].x}" cy="${g.seas[ui.sea].y + 9}" r="33"/>`;
  $('#marks').innerHTML = marks;
  $('#fleets').innerHTML = airBaseMarks() + fleetMarks();

  // Counters.
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
      const x = r.x - 11 + k * 4.5, y = r.y - 7 - k * 4;
      units += `<g class="ctr${ready ? ' ready' : ''}${spent ? ' spent' : ''}" transform="translate(${x} ${y})">
        <rect class="box" width="22" height="14" rx="1.4" fill="${g.nations[nat].color}"/>${symbol(lead.kind, 22, 14)}
        ${k === 0 ? `<text x="11" y="21.5">${num(sumDivs(cs))}</text><text class="size" x="11" y="-1.6">${cs.length > 1 ? 'XXXX' : 'XXX'}</text>` : ''}
        ${k === 0 && r.units.length > 1 ? `<circle class="more" cx="22" cy="0" r="4"/><text class="moret" x="22" y="2">${r.units.length}</text>` : ''}</g>`;
    });
  }
  $('#units').innerHTML = units;
  drawLabels();

  drawBar();
  drawTabs();
  drawPane();
  $('#hint').textContent = hintText();
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
  if (onEve(g)) return `The eve of war. ${natName(eveAttacker(g) ?? 'GER')} strikes before your first month comes round: place your army now, by road or by rail, then finish. No month passes.`;
  if (ui.tab === 'build') return 'Planning production: click one of your own home regions to raise troops or build works there.';
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
  if (pickedCorps().length) return 'Click a green region to march there, or a red one to plan an attack.';
  const ready = g.corps.some((c) => c.nat === g.human && c.ap > 0 && c.kind !== 'fort');
  return ready ? 'Click one of your armies (gold outline) to give it orders. End the month when you are done.' : 'All of your armies have used their month. End the month to continue.';
}

function drawBar(): void {
  const me = g.human;
  const w = weather(g);
  const eve = onEve(g);
  const who = me ? (g.nations[me].alive ? (eve ? `The eve of war: place the army of ${natName(me)}` : `Your orders: ${natName(me)}`) : `${natName(me)} has fallen`) : 'Watching history unfold: every nation on its historical plan';
  const when = eve ? '31 August 1939' : me || g.turn === 1 ? (g.turn === 1 && !me ? '1 September 1939' : date(g.turn)) : `End of ${date(g.turn - 1)}`;
  const pts = me && g.nations[me].alive ? ` · ${num(Math.floor(g.nations[me].pp))} production pts` : '';
  $('#when').innerHTML = `${when}<small>${esc(who)}${pts}${me ? ` · ${esc(w.text)}` : ''}</small>`;
  let b = '';
  if (me && humanUp(g)) {
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
  $('#tabs').innerHTML = t('region', 'Region') + build + t('research', 'Research') + t('generals', 'Generals', idle) + t('navy', 'Navy') + t('air', 'Air') + t('reel', 'News', ui.unread) + t('battles', 'Battles', newB) + t('powers', 'Powers');
}

function advice(): string {
  const me = g.human;
  if (!me || !g.nations[me].alive) return '';
  const list = (a?: string[]) => (a ?? []).slice(0, 7).join(', ');
  // Only what is still to do: wars not yet declared, objectives still in enemy hands.
  const todo = (a?: string[]) => (a ?? []).filter((nm) => { const r = g.byName[nm]; return r && atWar(g, me, r.ctrl); });
  // The plan's standing orders beyond its objectives: forces to gather or keep in place, and sectors left to allies.
  const orders = (ph: NonNullable<ReturnType<typeof phaseFor>>): string => {
    const out: string[] = [];
    const held = (nm: string): boolean => { const r = g.byName[nm]; return !!r && allied(g, me, r.ctrl); };
    const fast = Object.entries(ph.concentrate ?? {}).filter(([nm]) => held(nm));
    if (fast.length) out.push(`Take the tanks out of the quiet parts of the line and gather them by rail for the summer: ${esc(fast.map(([nm, k]) => `${k} corps at ${nm}`).join(', '))}.`);
    const keep = ph.posture === 'redeploy' ? [] : Object.entries(ph.staging ?? {}).filter(([nm]) => held(nm));
    if (keep.length) out.push(`Keep ${esc(keep.map(([nm, k]) => `${k} corps at ${nm}`).join(' and '))}.`);
    const lent = Object.entries(ph.entrust ?? {}).flatMap(([ally, places]) => places.filter((nm) => held(nm) && g.byName[nm].units.filter((u) => u.nat === ally).length >= 2));
    if (lent.length) out.push(`Your allies now hold ${esc(list(lent))}: history left one corps to stiffen each and sent the rest on.`);
    const out2 = (ph.abandon ?? []).filter((nm) => held(nm) && g.byName[nm].units.some((u) => u.nat === me && u.kind !== 'fort'));
    if (out2.length) out.push(`Give up ${esc(list(out2))}: march the corps there back toward home before the road behind them is cut.`);
    const dear = Object.entries(ph.oddsFor ?? {}).filter(([nm, o]) => o < (ph.minOdds ?? 1.3) && !held(nm)).map(([nm]) => nm);
    if (dear.length) out.push(`History threw its army at ${esc(list(dear))} against the odds. You need not.`);
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
  const away = theatrePhases(g, me).map(({ theatre, phase: ph }) => {
    const head = `<b>History's plan in ${esc(theatre)}${ph?.name ? `: ${esc(ph.name)}` : ''}.</b>`;
    if (ph?.posture === 'attack') {
      const declare = (ph.declare ?? []).filter((e) => g.nations[e].alive && !atWar(g, me, e));
      // Until the wars the plan calls for are declared, every objective is still to be won.
      const left = declare.length ? (ph.objectives ?? []).filter((nm) => { const r = g.byName[nm]; return r && !allied(g, me, r.ctrl); }) : todo(ph.objectives);
      const drops = (ph.landings ?? []).filter((l) => l.to.some((nm) => { const r = g.byName[nm]; return r && !allied(g, me, r.ctrl); }));
      return `<div class="note">${head} ${declare.length ? `Declare war on ${declare.map(natName).join(', ')}. ` : ''}${left.length ? `Objectives, in order: ${esc(list(left))}.` : 'Its objectives are taken.'}${ph.siege?.length ? ` Do not storm ${esc(list(ph.siege))}: starve it out.` : ''}${drops.length ? ` Across the water: ${esc(drops.map((l) => `${l.corps} to ${l.to[0]}`).join(', '))}.` : ''} You are free to do something else.</div>`;
    }
    if (ph?.withdraw) {
      const lost = ph.withdraw.when.filter((nm) => { const r = g.byName[nm]; return r && atWar(g, me, r.ctrl); });
      return `<div class="note">${head} ${lost.length ? `${esc(list(lost))} has fallen. Get the expedition to a port and lift it off before it is lost.` : `Stand beside your ally, and be ready to leave: if ${esc(list(ph.withdraw.when))} falls, get the expedition to a port and lift it off.`}</div>`;
    }
    return `<div class="note">${head} ${ph?.quiet ? 'Stand on the defensive and risk nothing: the army is being saved for a later stroke.' : 'Stand on the defensive and build up.'}</div>`;
  }).join('');
  return main() + away;
}

function scopeNote(): string {
  if (g.turn < STORY_ENDS) return '';
  return `<div class="note"><b>This is as far as the prototype's story goes.</b> History's plans cover the land war from September 1939 to the spring of 1942. You can play on, but the computer's nations will only hold what they have. Shipbuilding and the later campaigns come in later phases.</div>`;
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
  if (c.airborne) tags.push(`<span class="tag good" title="Parachute and glider troops. Once a month they can be flown from a friendly coast to an enemy one across a single sea, with no ships and whoever holds the water. They fight there at ${Math.round(NAVY.landing.airborne * 100)}%, without heavy weapons, and not under a sky the enemy's fighters hold.">Airborne</span>`);
  if (c.hold && !mine) tags.push('<span class="tag">Garrison</span>');
  const pips = c.kind === 'fort' ? '' : `<span class="pips" title="Moves left this month">${Array.from({ length: Math.max(c.apMax, c.ap) }, (_, i) => `<i class="${i < c.ap ? '' : 'off'}"></i>`).join('')}</span>`;
  const el = mine && c.kind !== 'fort' ? 'button' : 'div';
  return `<${el} class="corps${pick ? ' pick' : ''}" ${el === 'button' ? `data-act="pick" data-id="${c.id}"` : ''}>
    ${icon(c)}<span class="nm">${esc(c.name)}${gen ? ` <span style="font-weight:400;color:var(--ink-2)">· ${esc(gen.name)} <span class="lvl" title="Level ${gen.level} general">${gen.level}</span></span>` : ''}</span>${pips}
    <span class="meta">${g.nations[c.nat].adj} ${k.label.toLowerCase()} · ${c.divs} division${c.divs > 1 ? 's' : ''} · ${num(men(c))} men</span>
    <span class="bars"><span>Strength</span><span class="meter"><i class="${c.str < 0.5 ? 'low' : ''}" style="width:${Math.round(c.str * 100)}%"></i></span><span>Cohesion</span><span class="meter"><i class="org${c.org < 0.4 ? ' low' : ''}" style="width:${Math.round(c.org * 100)}%"></i></span></span>
    ${tags.length ? `<span class="tags">${tags.join('')}</span>` : ''}</${el}>`;
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
  else if (ui.tab === 'navy') pane.innerHTML = navyPane();
  else if (ui.tab === 'air') pane.innerHTML = airPane();
  else if (ui.tab === 'reel') pane.innerHTML = reelPane();
  else if (ui.tab === 'battles') pane.innerHTML = battlesPane();
  else pane.innerHTML = powersPane();
}

function regionPane(): string {
  if (ui.sel < 0 && ui.sea >= 0) return seaPane();
  if (ui.sel < 0) {
    return `<h2>${g.human ? 'Your orders' : 'The map'}</h2><p class="sub">${g.human ? 'Click a region to see who holds it and which armies are there.' : 'Click any region to inspect it. Use Next month or Play to watch the war unfold.'}</p>${scopeNote()}${advice()}
      <h3>How a month works</h3><ol class="steps"><li>Each army gets four moves a month, shown as gold dots. Tanks and motorized troops in a fresh offensive get one or two more.</li><li>Marching through friendly ground costs one move. Entering enemy ground costs foot infantry two.</li><li>An attack is settled at once from the Battle Ticket: forces, terrain, forts, supply, air, commanders.</li><li>A nation asks for terms when it has lost too much of its capital, its territory and its army.</li><li>Fleets hold the seas. A sea the enemy commands is closed to troopships and supply ships. Click any sea to see who holds it.</li><li>Air fleets fly from airfields. Left alone they support the armies and defend their own sky. Given a mission they fight for the sky, bomb industry or strike at shipping, as far as their range allows.</li>${g.turn >= 9 ? '<li>In the desert an army is only as strong as the port behind it. Every region of desert between a corps and its nearest working port weakens its attacks and slows its advance, and cargoes sunk on the way to Libya lengthen the Axis line. Infantry on foot in open desert is easy meat for tanks unless it stands in a fortress.</li>' : ''}${g.corps.some((c) => c.airborne) ? '<li>Airborne troops need no ships. Once a month a corps marked Airborne can be dropped on an enemy coast across one sea, whoever holds the water, as long as the enemy\'s fighters do not hold the sky there. It fights without heavy weapons, so pick a weakly held target.</li>' : ''}${g.turn >= 20 ? '<li>Deep inside the Soviet Union an invader outruns his supplies: attacks weaken and marches slow until the railways behind are relaid. Mud and winter make it worse.</li>' : ''}${g.human ? '<li>Your factories earn production points every month. Open <b>Build</b> to raise new corps or fortify a region.</li><li>Neutral countries drift toward whoever is winning. Open <b>Powers</b> to court them with political capital.</li><li>Generals gain experience in battle and earn traits from how they fight. Open <b>Generals</b> to give them commands and spend their skill points.</li><li>Open <b>Navy</b> to send your fleets out. A corps standing in a port can be shipped to a friendly port or landed on an enemy coast.</li><li>Open <b>Air</b> to move your air fleets and give them missions. The month\'s air campaigns are flown when you end the month.</li><li>Weapons improve on history\'s clock. Open <b>Research</b> to put extra design teams on the branches you want early; a branch left without a team falls behind.</li>' : ''}</ol>`;
  }
  const r = g.regions[ui.sel];
  const me = g.human;
  const held = r.ctrl === r.owner ? `${g.nations[r.owner].adj} territory` : `${g.nations[r.owner].adj} territory, held by ${natName(r.ctrl)}`;
  const tags: string[] = [];
  if (r.cap) tags.push('<span class="tag good">Capital</span>');
  if (r.port) tags.push('<span class="tag">Port</span>');
  if (r.seas.length) tags.push(`<span class="tag">Coast: ${r.seas.map((z) => esc(g.seas[z].name)).join(', ')}</span>`);
  if (r.fort) tags.push(`<span class="tag">Fortified, level ${r.fort.lv}, facing ${r.fort.vs.map((v) => g.nations[v]?.adj ?? v).join(' and ')} ground</span>`);
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
  return `<h2>${esc(r.name)}</h2><div class="sub">${held}</div>
    <div class="facts"><div class="fact"><span>Terrain</span><b>${r.terrain[0].toUpperCase() + r.terrain.slice(1)}</b></div><div class="fact"><span>Victory pts</span><b>${r.vp}</b></div><div class="fact"><span>Industry</span><b>${r.fac}</b></div></div>
    ${tags.length ? `<div class="tags" style="display:flex;gap:5px;flex-wrap:wrap;margin-top:8px">${tags.join('')}</div>` : ''}
    <h3>Forces here</h3>${acts}${units}
    ${based.length ? `<h3>On the airfields</h3>${based.map((f) => airRow(f, false, true)).join('')}` : ''}${ui.sel >= 0 && me ? advice() : ''}`;
}

/** Where a new corps would form up: the selected region if it is home ground, otherwise the capital. */
function raiseAt(): Region | null {
  const me = g.human!;
  if (ui.sel >= 0 && canRaise(g, me, g.regions[ui.sel])) return g.regions[ui.sel];
  const cap = g.nations[me].capital;
  if (cap >= 0 && canRaise(g, me, g.regions[cap])) return g.regions[cap];
  return g.regions.find((r) => canRaise(g, me, r) && r.fac > 0) ?? g.regions.find((r) => canRaise(g, me, r)) ?? null;
}

function buildPane(): string {
  const me = g.human!;
  const n = g.nations[me];
  const mine = humanUp(g);
  const have = Math.floor(n.pp);
  const inc = income(g, me);
  const at = raiseAt();
  const picked = ui.sel >= 0 && at !== null && at.id === ui.sel;
  let out = `<h2>War production</h2><p class="sub">Your factories earn production points every month. Losses in the field are made good first, at ${PRODUCTION.replacementCost} points a division, as fast as the depots can train men: about ${Math.round(PRODUCTION.manpower * 100)}% of the army a month. What is left is yours to spend.</p>
    <div class="facts"><div class="fact"><span>In hand</span><b>${num(have)}</b></div><div class="fact"><span>Each month</span><b>+${num(inc)}</b></div><div class="fact"><span>On order</span><b>${n.queue.length}</b></div></div>`;
  if (!n.atWar.length) out += `<div class="note"><b>At peace.</b> Industry runs at half speed until the nation is at war.</div>`;

  out += `<h3>Raise a new corps</h3>`;
  if (!at) out += `<div class="empty">There is no home ground left to raise troops on.</div>`;
  else {
    out += `<p class="sub" style="margin:0 0 7px">Forms up at <b style="color:var(--ink)">${esc(at.name)}</b>. ${picked ? '' : 'Click one of your own home regions on the map to raise it there instead.'}</p>`;
    for (const [kind, b] of Object.entries(PRODUCTION.builds)) {
      const ok = mine && have >= b.cost;
      const label = kind === 'arm' ? `${PRODUCTION.armourName[me] ?? PRODUCTION.armourName._default} corps` : b.label;
      out += `<button class="buy" data-act="build" data-kind="${kind}" ${ok ? '' : 'disabled'}>${icon({ nat: me, kind } as Corps)}<span class="grow"><b>${esc(label)}</b><small>${b.divs} divisions · ready in ${b.months} months</small><small>${esc(b.blurb)}</small></span><span class="cost${have >= b.cost ? '' : ' short'}">${b.cost}<small>pts</small></span></button>`;
    }
  }

  out += `<h3>Fortify</h3>`;
  const sel = ui.sel >= 0 ? g.regions[ui.sel] : null;
  if (!sel || !isHome(g, me, sel)) out += `<div class="empty">Click one of your own home regions on the map to fortify it.</div>`;
  else {
    const cost = fortCost(g, me, sel);
    if (cost === null) out += `<div class="empty">${esc(sel.name)} is already fortified as heavily as it can be.</div>`;
    else {
      const lv = (sel.fort?.lv ?? 0) + n.queue.filter((b) => b.what === 'fort' && b.region === sel.id).length + 1;
      out += `<button class="buy" data-act="fortify" ${mine && have >= cost ? '' : 'disabled'}><span class="grow"><b>Fortify ${esc(sel.name)} to level ${lv}</b><small>Ready in ${PRODUCTION.fort.months} months · faces every direction</small><small>Defenders behind works fight far harder. A line only helps if the enemy has to come through it.</small></span><span class="cost${have >= cost ? '' : ' short'}">${cost}<small>pts</small></span></button>`;
    }
  }

  out += `<h3>On order</h3>`;
  if (!n.queue.length) out += `<div class="empty">Nothing on order.</div>`;
  for (const b of [...n.queue].sort((a, c) => a.eta - c.eta)) {
    const left = b.eta - g.turn;
    const where = g.regions[b.region]?.name ?? '';
    out += `<div class="order"><span class="grow"><b>${esc(b.name)}</b><small>${b.what === 'corps' ? `Forming at ${esc(where)} · ` : ''}ready ${date(b.eta)}${left > 0 ? ` (${left} month${left > 1 ? 's' : ''})` : ''}</small></span>${b.ordered === g.turn && mine ? `<button class="btn small" data-act="cancelbuild" data-id="${b.id}">Cancel</button>` : `<span class="cost">${b.cost}<small>paid</small></span>`}</div>`;
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
    : `<button class="buy" data-act="bureau" ${mine && n.pp >= cost ? '' : 'disabled'}><span class="grow"><b>Found a new design team</b><small>One more team to put on any branch, for the rest of the war.</small><small>Paid for from production: ${num(Math.floor(n.pp))} points in hand.</small></span><span class="cost${n.pp >= cost ? '' : ' short'}">${cost}<small>pts</small></span></button>`;
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
  return `<button class="gen${serving(x) ? '' : ' gone'}" data-act="${act}" data-id="${x.id}" ${extra} ${off ? `disabled title="${esc(off)}"` : ''}><span class="lv">${x.level}</span><span class="grow"><b>${esc(x.name)}</b><small>${whose}${genWhere(x)}</small>${x.traits.length || pts ? `<span class="tags">${pts}${traitTags(x)}</span>` : ''}</span></button>`;
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
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">${n.adj} army · General · ${x.historical ? 'Historical' : 'Rose from the ranks'}</div><h2>${esc(x.name)}</h2></header>
    <div class="body">
      <div class="record"><div class="lvbig"><b>${x.level}</b><span>LEVEL</span></div><div class="who"><b>${state}</b>
        <div class="xp"><i style="width:${need === Infinity ? 100 : Math.round((x.xp / need) * 100)}%"></i></div>
        <small>${need === Infinity ? 'At the highest level' : `XP ${num(x.xp)} / ${num(need)} · Next level brings a skill point`}${x.points > 0 && serving(x) ? ` · <b style="color:var(--brass)">${x.points} to spend</b>` : ''}</small></div></div>
      <h3>Traits</h3>${traits ? `<ul class="traits">${traits}</ul>` : '<div class="empty">None yet. Traits are earned from how a general fights: closing pockets, holding against the odds, costly assaults.</div>'}
      <h3>Skills</h3>${skills}
      <div class="tally">Battles ${x.rec.battles} · Won ${x.rec.won}${x.rec.wounds ? ` · Wounded ${times(x.rec.wounds)}` : ''}</div>
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

function reelPane(): string {
  if (!g.headlines.length) return '<div class="empty">No news yet.</div>';
  let out = '<h2>Newsreel</h2><div class="reel">';
  let t = -1;
  for (let i = g.headlines.length - 1; i >= 0; i--) {
    const h = g.headlines[i];
    if (h.turn !== t) { t = h.turn; out += `<div class="mo">${date(t)}</div>`; }
    out += `<p class="${h.kind}"><span class="wk">Week ${h.week}</span>${esc(h.text)}</p>`;
  }
  return out + '</div>';
}

function verdictFor(rep: BattleReport): { text: string; cls: string } {
  const me = g.human;
  const defSide = me !== null && rep.defenders.some((u) => u.nat === me) && !rep.attackers.some((u) => u.nat === me);
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
      out += `<div class="nat"><span class="chip" style="background:${n.color}"></span><b>${esc(n.name)}</b><small>${num(fieldArmy(g, n.id))} divisions</small><div class="will"><div class="meter"><i class="${will < 0.35 ? 'low' : ''}" style="width:${Math.round(will * 100)}%"></i></div><small>At war with ${n.atWar.map(natName).join(', ')}</small></div></div>`;
    } else {
      out += `<div class="nat" style="opacity:.7"><span class="chip" style="background:${n.color}"></span><b>${esc(n.name)}</b><small>fell ${date(n.fellOn!.turn)}</small><div class="will"><small>Asked ${natName(n.fellOn!.to)} for terms in week ${n.fellOn!.week}</small></div></div>`;
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
    out += `<div class="trk"><span class="chip" style="background:${n.color}"></span><b>${esc(n.name)}</b><small>${TRACK_LABEL[n.align]}</small>${btn}
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
    ${hullIcon(f)}<span class="nm">${esc(f.name)}${adm}</span><span></span>
    <span class="meta">${whose ? `${g.nations[f.nat].adj} · ` : ''}${whereFleet(f)}</span>
    <span class="meta">${esc(capital(shipList(f.ships)))}${lost ? ` · lost so far: ${esc(shipList(f.lost))}` : ''}</span>
    <span class="bars"><span>Readiness</span><span class="meter"><i class="${f.ready < 0.5 ? 'low' : ''}" style="width:${Math.round(f.ready * 100)}%"></i></span></span></${el}>`;
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
      <div class="facts"><div class="fact"><span>Ships</span><b>${num(mine.reduce((t, f) => t + shipCount(f.ships), 0))}</b></div><div class="fact"><span>Fleets at sea</span><b>${mine.filter(atSea).length} of ${mine.length}</b></div><div class="fact"><span>Sea lift left</span><b>${n.sealift > 0 ? `${liftLeft(g, me)} corps` : 'None'}</b></div></div>`;
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
      <div class="vs"><div><h4><span class="chip" style="background:${g.nations[r.attNat].color}"></span> ${natName(r.attNat)} offering battle</h4>${side(r.att)}</div><div class="mid">vs</div><div class="r"><h4>${natName(r.defNat)} <span class="chip" style="background:${g.nations[r.defNat].color}"></span></h4>${side(r.def)}</div></div>
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
        <div class="sub">Odds ${a.odds.toFixed(2)} to 1 · about ${Math.round(p * 100)}% chance to take ${esc(target.name)}${air ? ` · airborne troops without their heavy weapons count for ${Math.round(NAVY.landing.airborne * 100)}%` : plan.surprise ? '' : ` · troops fighting their way off the beach count for ${Math.round(NAVY.landing.assault * 100)}%`}</div>`;
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
    ${planeIcon(f)}<span class="nm">${esc(f.name)}${ldr}</span><span></span>
    <span class="meta">${whose ? `${g.nations[f.nat].adj} · ` : ''}${esc(g.regions[f.base].name)}, ${esc(missionText(f))}</span>
    <span class="meta">${esc(capital(craft(f.ftr, f.bmr)))}${lost ? ` · lost so far: ${esc(craft(f.lost.ftr, f.lost.bmr))}` : ''}</span>
    <span class="bars"><span>Readiness</span><span class="meter"><i class="${f.ready < 0.5 ? 'low' : ''}" style="width:${Math.round(f.ready * 100)}%"></i></span></span>
    ${tags.length ? `<span class="tags">${tags.join('')}</span>` : ''}</${el}>`;
}

/** The orders the player can give the air fleet that is picked. */
function airOrders(f: AirFleet): string {
  const R = airRange(f.nat);
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
      <p class="sub" style="margin-top:6px">In reserve: ${craft(p.ftr, p.bmr)}. Each month the factories deliver ${craft(d.ftr, d.bmr)} and the flying schools ${num(d.pilots)} pilots. A new fighter needs a pilot, and resting fleets are made up first.${p.lostFtr + p.lostBmr ? ` Lost since the war began: ${craft(p.lostFtr, p.lostBmr)}, and ${num(p.lostPilots)} fighter pilots.` : ''}</p>`;
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
      <div class="vs"><div><h4><span class="chip" style="background:${g.nations[r.attNat].color}"></span> ${natName(r.attNat)} attacking</h4>${side(r.att)}</div><div class="mid">vs</div><div class="r"><h4>${r.defNat ? natName(r.defNat) : 'No defenders'} <span class="chip" style="background:${r.defNat ? g.nations[r.defNat].color : 'var(--panel-3)'}"></span></h4>${side(r.def)}</div></div>
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
  card.scrollTop = 0;
}

function startCard(): string {
  const side = (id: string | null, name: string, blurb: string, wide = false, from = 1) => `<button class="sidebtn${wide ? ' wide' : ''}" data-act="start" data-nat="${id ?? ''}" data-from="${from}"><b>${name}</b><span>${blurb}</span></button>`;
  const saved = peekSave(readSave() ?? '');
  const resume = saved ? `<button class="sidebtn wide resume" data-act="continue"><b>Continue your campaign</b><span>${saved.human ? `${esc(natName(saved.human))}, ${date(saved.turn)}` : `Watching history, ${date(saved.turn)}`}. Saved in this browser.</span></button>` : '';
  return `<header><div class="kick">Phase 1 prototype · campaign layer</div><h2>Theater Command 1939 to 1945</h2></header>
    <div class="body"><p class="lead">This is the strategy layer of the game: a map of Europe, North Africa and the Middle East in ${g.regions.length} regions and ${g.seas.length} seas, monthly turns, armies, fleets and air fleets, and battles settled from a <b>Battle Ticket</b>. Pick where to start and which side to play, or watch the computer play out history's plan.</p>
    ${resume ? `<div class="sides">${resume}</div>` : staleSave(readSave() ?? '') ? '<div class="note">The campaign saved in this browser was played on the earlier, smaller map, before North Africa was added, and cannot be continued in this version. Starting a new game replaces it.</div>' : ''}
    <h3>September 1939 · The war begins</h3>
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
    <h3>June 1942 · Case Blue and Stalingrad</h3>
    <div class="sides">
      ${side(null, 'Watch the road to Stalingrad', 'The computer plays the first 33 months, then the drive for the Don, the Volga and the Caucasus oil, and the winter that turned the war.', true, 34)}
      ${side('GER', 'Germany', 'Your tanks are gathered in the south and your allies will hold the Don behind you. The oil and the Volga are both in reach. Decide how much of the army goes into the city.', false, 34)}
      ${side('SOV', 'Soviet Union', 'Give ground all summer and save the army. Hold Stalingrad, watch who is guarding the Don, and strike when the snow comes.', false, 34)}
    </div>
    <ol class="steps"><li>Click one of your armies, then a green region to march or a red one to attack.</li><li>Press <b>End month</b> and the other nations take their turns.</li><li>Spend your factories' output under <b>Build</b>, steer your designers under <b>Research</b>, and send your fleets out under <b>Navy</b> and your air fleets under <b>Air</b>. The game saves itself as you play.</li><li>Drag the map to move it. Scroll or pinch to zoom.</li></ol></div>`;
}

const FATE: Record<string, string> = { '': 'held its ground', retreated: 'retreated', surrendered: 'surrendered', destroyed: 'destroyed', advanced: 'advanced', evacuated: 'evacuated by sea' };

function reportCard(r: BattleReport): string {
  const v = verdictFor(r);
  const side = (us: BattleReport['attackers'], lost: number) => `<div class="big">${num(us.reduce((s, u) => s + u.men, 0))}</div><small>men in ${us.length} corps · lost ${num(lost)}</small>`;
  const list = (us: BattleReport['attackers']) => `<ul class="ulist">${us.map((u) => `<li><span>${esc(u.name)} <span style="color:var(--ink-3)">(${g.nations[u.nat].adj})</span></span><span>${FATE[u.fate]}</span></li>`).join('')}</ul>`;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Battle Ticket · ${date(r.turn)}, week ${r.week}</div><h2>Battle of ${esc(r.regionName)}</h2></header>
    <div class="body">
      <div class="verdict ${v.cls}">${v.text}</div>
      <div class="sub">${r.taken ? `${esc(r.regionName)} was taken.` : `The attack on ${esc(r.regionName)} was thrown back.`} Final odds ${r.odds.toFixed(2)} to 1${r.prisoners ? ` · ${num(r.prisoners)} prisoners` : ''}.</div>
      <div class="vs"><div><h4><span class="chip" style="background:${g.nations[r.attNat].color}"></span> ${natName(r.attNat)} attacking</h4>${side(r.attackers, r.attLost)}</div><div class="mid">vs</div><div class="r"><h4>${natName(r.defNat)} defending <span class="chip" style="background:${g.nations[r.defNat].color}"></span></h4>${side(r.defenders, r.defLost)}</div></div>
      <h3>What the campaign map fed into the battle</h3>
      <table class="ticket">${r.lines.map((l) => `<tr><td>${esc(l.label)}</td><td>${esc(l.value)}</td><td class="fx">${esc(l.effect ?? '')}</td></tr>`).join('')}</table>
      <h3>Attackers</h3>${list(r.attackers)}<h3>Defenders</h3>${list(r.defenders)}
      <div class="note" style="margin-top:14px"><b>Auto-resolved.</b> In the full game you can choose to fight this battle yourself in 3D. The same ticket goes in and the same kind of result comes back, so the campaign never needs to know which one happened.</div>
    </div><footer><button class="btn" data-act="showmap" data-id="${r.region}">Show on map</button><button class="btn go" data-act="close">Close</button></footer>`;
}

function attackCard(): string {
  const m = ui.modal!;
  const target = g.regions[m.target];
  const cands = candidates(g, target);
  const chosen = limitAttackers(g, cands.filter((c) => m.chosen.has(c.id)), target);
  const inSet = new Set(chosen.map((c) => c.id));
  const a = previewAttack(g, chosen, target);
  let body = '';
  if (a) {
    const p = Math.max(0, Math.min(1, (1.2 - 1.2 / a.odds) / 0.4));
    const share = a.A / (a.A + a.D);
    const word = p >= 0.95 ? 'Victory is all but certain' : p >= 0.6 ? 'The odds favour you' : p >= 0.3 ? 'It could go either way' : p > 0 ? 'A long shot' : 'This attack will fail';
    body = `<div class="verdict ${p >= 0.6 ? 'win' : p >= 0.3 ? '' : 'loss'}" style="font-size:24px">${word}</div>
      <div class="oddsbar"><i style="width:${Math.round(share * 100)}%;background:${g.nations[g.human!].color}"></i><i style="flex:1;background:${g.nations[a.defenders[0].nat].color}"></i></div>
      <div class="sub">Odds ${a.odds.toFixed(2)} to 1 · about ${Math.round(p * 100)}% chance to take ${esc(target.name)} · it needs roughly 1.2 to 1 on the day</div>
      <h3>Battle Ticket</h3><table class="ticket">${a.lines.map((l) => `<tr><td>${esc(l.label)}</td><td>${esc(l.value)}</td><td class="fx">${esc(l.effect ?? '')}</td></tr>`).join('')}</table>`;
  } else body = '<div class="empty">Choose at least one corps to attack with.</div>';
  const rows = cands.map((c) => `<button class="pickrow${inSet.has(c.id) ? ' pick' : ''}" data-act="atkpick" data-id="${c.id}">${icon(c)}<span class="grow"><b>${esc(c.name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">from ${esc(g.regions[c.reg].name)} · ${c.divs} divisions · ${c.ap} move${c.ap > 1 ? 's' : ''} left${m.chosen.has(c.id) && !inSet.has(c.id) ? (g.nations[c.nat].maxAttack && chosen.length >= g.nations[c.nat].maxAttack ? ' · more than the staff can handle in one battle' : ' · no room on the roads') : ''}</span></span></button>`).join('');
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Plan of attack · ${date(g.turn)}</div><h2>Attack on ${esc(target.name)}</h2></header>
    <div class="body">${body}<h3>Your corps in reach</h3>${rows}</div>
    <footer><button class="btn" data-act="close">Call it off</button><button class="btn go" data-act="attack" ${a ? '' : 'disabled'}>Attack</button></footer>`;
}

function newsCard(from: number, repFrom: number, seaFrom: number, airFrom: number): string {
  const hs = g.headlines.slice(from);
  const me = g.human;
  const reps = g.reports.slice(repFrom).filter((r) => me && (r.attackers.some((u) => u.nat === me) || r.defenders.some((u) => u.nat === me)));
  const seas = g.seaReports.slice(seaFrom).filter((r) => me && (r.att.some((u) => u.nat === me) || r.def.some((u) => u.nat === me)));
  const airs = g.airReports.slice(airFrom).filter((r) => me && (r.att.some((u) => u.nat === me) || r.def.some((u) => u.nat === me)));
  const all = g.reports.length - repFrom + (g.seaReports.length - seaFrom) + (g.airReports.length - airFrom);
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Dispatches</div><h2>${date(g.turn)}</h2></header>
    <div class="body">${hs.length ? `<div class="reel">${hs.map((h) => `<p class="${h.kind}"><span class="wk">${date(h.turn).split(' ')[0]} wk ${h.week}</span>${esc(h.text)}</p>`).join('')}</div>` : '<p class="lead">A quiet month. No news of note.</p>'}
    ${reps.length || seas.length || airs.length ? `<h3>Battles your forces fought</h3>${airs.map(airBrow).join('')}${seas.map(seaRow).join('')}${reps.map((r) => { const v = verdictFor(r); return `<button class="brow" data-act="report" data-id="${r.id}"><b>${esc(r.regionName)}</b><span class="res ${v.cls}">${v.text}</span><span>${date(r.turn)}, week ${r.week}</span></button>`; }).join('')}` : ''}
    ${all ? `<p class="sub" style="margin-top:10px">${all} battle${all > 1 ? 's were' : ' was'} fought across the map. They are filed under Battles.</p>` : ''}</div>
    <footer><button class="btn go" data-act="close">${me && humanUp(g) ? 'Give orders' : 'Continue'}</button></footer>`;
}

function declareCard(): string {
  const list = declarable(g);
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Diplomacy</div><h2>Declare war</h2></header>
    <div class="body"><p class="lead">A neutral that is attacked joins the other side, and its friends may march to help it at once.</p>
    ${list.map((id) => `<button class="pickrow" data-act="dodeclare" data-nat="${id}"><span class="chip" style="background:${g.nations[id].color}"></span><span class="grow"><b>${esc(natName(id))}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${num(fieldArmy(g, id))} divisions</span></span></button>`).join('')}</div>
    <footer><button class="btn" data-act="close">Not now</button></footer>`;
}

// ---- Running the game ---------------------------------------------------------------------

/** Starts a campaign. With a later starting month, the computer plays history up to it and then hands over. */
function newGame(human: string | null, seed?: number, from = 1): void {
  stopAuto();
  g = createGame(seed ?? (Date.now() % 900000) + 1, from > 1 ? null : human);
  while (g.turn < from) runTurnAI(g);
  g.human = human;
  ui.live = true;
  ui.sel = -1; ui.picked.clear(); ui.rail = ui.ship = false; ui.tab = 'region'; ui.unread = 0; ui.seenReports = from > 1 ? g.reports.length : 0; ui.flash = [];
  ui.sea = ui.fleet = -1; ui.land = ui.rebase = false; ui.msg = ''; ui.seenSea = from > 1 ? g.seaReports.length : 0;
  ui.air = -1; ui.airMode = ''; ui.seenAir = from > 1 ? g.airReports.length : 0;
  drawStatic();
  const britain = from === 11, desert = from === 13, balkans = from === 14, blue = from === 34;
  if (britain) ui.tab = 'air';
  const focus = britain ? g.byName['Calais'] : desert ? g.byName['Tobruk'] : balkans ? g.byName['Thessaly'] : blue ? g.byName[human === 'SOV' ? 'Stalingrad' : 'Kharkov'] : from > 1 ? g.byName[human === 'SOV' ? 'Smolensk' : 'Brest'] : human ? g.regions[g.nations[human].capital] : g.byName['Poznan'];
  ui.view = { x: focus.x - 300, y: focus.y - 230, w: desert || balkans ? 580 : human === 'GER' || from > 1 ? 640 : 600 };
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
function advanceToHuman(show: boolean, since?: { h: number; r: number; s: number; a: number }): void {
  const hFrom = since?.h ?? g.headlines.length, rFrom = since?.r ?? g.reports.length, sFrom = since?.s ?? g.seaReports.length, aFrom = since?.a ?? g.airReports.length;
  const before = g.regions.map((r) => r.ctrl);
  let guard = 0;
  while (!humanUp(g) && guard++ < 80) {
    if (g.human && !g.nations[g.human].alive) break;
    stepAI(g);
  }
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
    $('#card').querySelector('.kick')!.textContent = `${natName(lost)} has fallen. The war goes on without you.`;
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
  if (g.turn > STORY_ENDS) stopAuto();
}

function focusOn(ids: number[]): void {
  let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
  for (const id of ids) { const r = g.regions[id]; x0 = Math.min(x0, r.x); y0 = Math.min(y0, r.y); x1 = Math.max(x1, r.x); y1 = Math.max(y1, r.y); }
  const svg = $('#map');
  const a = svg.clientHeight / Math.max(1, svg.clientWidth);
  const w = Math.max(420, x1 - x0 + 260, (y1 - y0 + 240) / a);
  ui.view = { x: (x0 + x1) / 2 - w / 2, y: (y0 + y1) / 2 - (w * a) / 2, w };
  applyView();
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
    case 'start': openModal(null); newGame(t.dataset.nat || null, undefined, Number(t.dataset.from || 1)); break;
    case 'continue': if (continueGame()) openModal(null); else openModal({ kind: 'start' }); break;
    case 'new': stopAuto(); openModal({ kind: 'start' }); break;
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
      if (onEve(g)) endEve(g); else endHuman(g);
      advanceToHuman(true, since);
      break;
    }
    case 'pick': {
      if (ui.picked.has(id)) ui.picked.delete(id); else ui.picked.add(id);
      ui.rail = ui.ship = ui.land = false; refresh(); break;
    }
    case 'pickall': selectRegion(ui.sel); refresh(); break;
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
    case 'build': { const at = raiseAt(); if (at) orderBuildCorps(g, t.dataset.kind as Kind, at.id); refresh(); break; }
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
  if (picked.length && id !== ui.sel) {
    if (hostileUnits(g, r, g.human!).length) {
      const near = picked.filter((c) => g.regions[c.reg].nb.includes(id));
      if (near.length) { openModal({ kind: 'attack', target: id, chosen: new Set(near.map((c) => c.id)) }); return; }
    } else if (atWar(g, g.human!, r.ctrl) || allied(g, g.human!, r.ctrl)) {
      const h = g.headlines.length;
      const before = g.regions.map((x) => x.ctrl);
      let moved = 0;
      for (const c of picked) if (orderMove(g, c, id) > 0) moved++;
      if (moved) {
        ui.unread += g.headlines.length - h;
        ui.flash = g.regions.filter((x, i) => x.ctrl !== before[i]).map((x) => x.id);
        ui.sel = id;
        ui.picked = new Set(picked.filter((c) => c.reg === id && c.ap > 0).map((c) => c.id));
        ui.tab = 'region';
        refresh();
        return;
      }
    }
  }
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
  svg.addEventListener('pointerdown', (e) => {
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
      if (Math.abs(dx) + Math.abs(dy) > 5) { moved = true; svg.classList.add('drag'); }
      if (moved) { const k = ui.view.w / rect.width; ui.view.x = start.vx - dx * k; ui.view.y = start.vy - dy * k; applyView(); }
      return;
    }
    // Hover: say what is under the cursor.
    const el = (e.target as Element).closest?.('[data-id]') as HTMLElement | null;
    const tip = $('#tip');
    if (e.pointerType !== 'mouse') { tip.style.display = 'none'; return; }
    if (!el) {
      const p = mapPoint(e.clientX, e.clientY);
      const z = seaAt(p.x, p.y);
      if (z < 0) { tip.style.display = 'none'; return; }
      const by = new Map<string, number>();
      for (const f of g.fleets) if (f.zone === z && atSea(f)) by.set(f.nat, (by.get(f.nat) ?? 0) + shipCount(f.ships));
      tip.innerHTML = `<b>${esc(g.seas[z].name)}</b><br>${by.size ? [...by].map(([nat, n]) => `${g.nations[nat].adj}: ${n} ships`).join(' · ') : 'No fleet at sea here'}`;
    } else {
    const r = g.regions[Number(el.dataset.id)];
    const divs = sumDivs(r.units);
    tip.innerHTML = `<b>${esc(r.name)}</b><br>${natName(r.ctrl)}${r.ctrl !== r.owner ? ` (occupied ${g.nations[r.owner].adj} ground)` : ''} · ${r.terrain}${r.units.length ? `<br>${r.units.length} corps, ${num(divs)} divisions` : ''}`;
    }
    tip.style.display = 'block';
    const wrap = $('#mapwrap').getBoundingClientRect();
    tip.style.left = Math.min(wrap.width - 250, e.clientX - wrap.left + 14) + 'px';
    tip.style.top = Math.min(wrap.height - 70, e.clientY - wrap.top + 14) + 'px';
  });
  const up = (e: PointerEvent) => {
    const was = pts.size;
    pts.delete(e.pointerId);
    svg.classList.remove('drag');
    if (was === 1 && !moved && e.type === 'pointerup') {
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
  svg.addEventListener('pointerleave', () => { $('#tip').style.display = 'none'; });
  svg.addEventListener('wheel', (e) => { e.preventDefault(); zoomBy(Math.exp(e.deltaY * 0.0016), e.clientX, e.clientY); }, { passive: false });
}

// ---- Start --------------------------------------------------------------------------------

g = createGame(1939, null);
mount();
drawStatic();
applyView();
refresh();
openModal({ kind: 'start' });

// A handle for automated checks and for curious players in the browser console.
(window as any).theater = { game: () => g, ui, newGame, nextMonth, onRegion, onSea, refresh, continueGame, save: () => saveGame(g) };

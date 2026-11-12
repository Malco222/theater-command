// The campaign screen: a staff map of Europe, the armies on it, and the paperwork a
// commander would see. All of the rules live in ../game, ../ai and ../turn; this file
// only draws the state of the game and turns clicks into orders.

import {
  allied, armySize, atWar, collapseScore, createGame, ensureSupply, frozen, hostileUnits, limitAttackers, lineStrength, men, weather,
} from '../game';
import {
  beginHuman, canOrder, candidates, declarable, endHuman, humanUp, orderAttack, orderBuildCorps, orderBuildFort,
  orderAssign, orderCancelBuild, orderCourt, orderDeclare, orderEvacuate, orderMove, orderPromote, orderRail, orderRelieve, orderSack, orderShip, orderSkill, previewAttack, reach, runTurnAI,
  shipTargets, stepAI,
} from '../turn';
import { phaseFor } from '../ai';
import { PRODUCTION, fortCost, income, isHome } from '../economy';
import { loadGame, peekSave, saveGame } from '../save';
import { COMMAND, SKILLS, corpsOf, fit, generalOf, popular, serving, vacant, waiting, whyNotAssign, whyNotPromote, xpToNext } from '../commanders';
import { DIPLOMACY, TRACK_LABEL, canCourt, courtable, momentum, pcIncome, receptive, step, tradePartners } from '../diplomacy';
import { bloc } from '../game';
import { KIND, type BattleReport, type Corps, type Game, type General, type Kind, type Nation, type Region, type Skill } from '../types';
import { turnToDate } from '../../core/ticket';

let g: Game;
/** The month after the last one the historical plans cover: April 1942. */
const STORY_ENDS = 32;
const ui = {
  sel: -1,
  picked: new Set<number>(),
  rail: false,
  ship: false,
  /** False until a game has been started or resumed, so the title screen never overwrites a saved game. */
  live: false,
  tab: 'region' as 'region' | 'build' | 'generals' | 'reel' | 'battles' | 'powers',
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
          <g id="land"></g><g id="regs"></g><g id="occ"></g><g id="fx"></g><g id="marks"></g><g id="labels"></g><g id="units"></g>
        </svg>
        <div class="hint" id="hint"></div>
        <div class="legend"><span><i style="background:rgba(92,170,96,.5);border-color:#2f6b39"></i>Can march here</span><span><i style="background:rgba(207,90,65,.55);border-color:#a22d18"></i>Can attack</span><span><i style="background:repeating-linear-gradient(45deg,#8a8a80 0 2px,#cfc9b8 2px 4px);border-color:#555"></i>Occupied</span></div>
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
  $('#fx').innerHTML = ui.flash.map((id) => `<path class="flash" d="${pathOf(g.regions[id].polys)}"/>`).join('');

  // Where the picked corps can go.
  let marks = '';
  const picked = pickedCorps();
  if (ui.rail && picked.length) {
    for (const r of railTargets(picked[0])) marks += `<path class="mk rail" d="${pathOf(r.polys)}"/>`;
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
  $('#marks').innerHTML = marks;

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
  if (c.ap < 4) return [];
  const seen = new Set<number>([c.reg]);
  const q = [c.reg];
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (!seen.has(j) && allied(g, c.nat, g.regions[j].ctrl)) { seen.add(j); q.push(j); }
  seen.delete(c.reg);
  return [...seen].map((id) => g.regions[id]);
}

function hintText(): string {
  if (!g.human) return '';
  if (!humanUp(g)) return '';
  if (ui.tab === 'build') return 'Planning production: click one of your own home regions to raise troops or build works there.';
  if (ui.rail) return 'Rail move: click any blue region. It takes the whole month.';
  if (ui.ship) return 'Sea transport: click any blue port. It takes the whole month.';
  if (pickedCorps().length) return 'Click a green region to march there, or a red one to plan an attack.';
  const ready = g.corps.some((c) => c.nat === g.human && c.ap > 0 && c.kind !== 'fort');
  return ready ? 'Click one of your armies (gold outline) to give it orders. End the month when you are done.' : 'All of your armies have used their month. End the month to continue.';
}

function drawBar(): void {
  const me = g.human;
  const w = weather(g);
  const who = me ? (g.nations[me].alive ? `Your orders: ${natName(me)}` : `${natName(me)} has fallen`) : 'Watching history unfold: every nation on its historical plan';
  const when = me || g.turn === 1 ? (g.turn === 1 && !me ? '1 September 1939' : date(g.turn)) : `End of ${date(g.turn - 1)}`;
  const pts = me && g.nations[me].alive ? ` · ${num(Math.floor(g.nations[me].pp))} production pts` : '';
  $('#when').innerHTML = `${when}<small>${esc(who)}${pts}${me ? ` · ${esc(w.text)}` : ''}</small>`;
  let b = '';
  if (me && humanUp(g)) {
    if (declarable(g).length) b += `<button class="btn warn" data-act="declare">Declare war</button>`;
    b += `<button class="btn go" data-act="end">End month ▸</button>`;
  } else {
    b += `<button class="btn" data-act="auto">${ui.auto ? 'Pause' : 'Play ▸▸'}</button><button class="btn go" data-act="next">Next month ▸</button>`;
  }
  b += `<button class="btn" data-act="new">New game</button>`;
  $('#barbtns').innerHTML = b;
}

function drawTabs(): void {
  const newB = g.reports.length - ui.seenReports;
  const t = (id: string, label: string, n = 0) => `<button class="tab${ui.tab === id ? ' on' : ''}" data-act="tab" data-tab="${id}">${label}${n ? `<span class="n">${n}</span>` : ''}</button>`;
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  const build = me ? t('build', 'Build') : '';
  // Generals with no corps, when there is a corps for them: a decision waiting for the player.
  const idle = me && vacant(g, me).length ? waiting(g, me).length : 0;
  $('#tabs').innerHTML = t('region', 'Region') + build + t('generals', 'Generals', idle) + t('reel', 'News', ui.unread) + t('battles', 'Battles', newB) + t('powers', 'Powers');
}

function advice(): string {
  const me = g.human;
  if (!me || !g.nations[me].alive) return '';
  const ph = phaseFor(g, me);
  if (!ph) return '';
  const list = (a?: string[]) => (a ?? []).slice(0, 7).join(', ');
  if (ph.posture === 'attack') return `<div class="note"><b>History's plan this month${ph.name ? `: ${esc(ph.name)}` : ''}.</b> ${ph.declare?.length ? `Declare war on ${ph.declare.map(natName).join(', ')}. ` : ''}Objectives, in order: ${esc(list(ph.objectives))}. You are free to do something else.</div>`;
  if (ph.posture === 'redeploy') return `<div class="note"><b>History's plan this month.</b> Move the army west by rail and wait for spring. Assembly areas: ${esc(list(ph.assembly))}.</div>`;
  return `<div class="note"><b>History's plan this month.</b> Hold the line and keep a reserve near the capital.</div>`;
}

function scopeNote(): string {
  if (g.turn < STORY_ENDS) return '';
  return `<div class="note"><b>This is as far as the prototype's story goes.</b> History's plans cover the land war from September 1939 to the spring of 1942. You can play on, but the computer's nations will only hold what they have. The war at sea and in the air, research and the later campaigns come in later phases.</div>`;
}

function corpsRow(c: Corps, mine: boolean): string {
  const k = KIND[c.kind];
  const pick = ui.picked.has(c.id);
  const tags: string[] = [];
  if (!c.supplied) tags.push('<span class="tag bad">Cut off</span>');
  if (c.fought) tags.push('<span class="tag">Fought this month</span>');
  const line = lineStrength(g, c);
  if (line.mul < 1) tags.push(`<span class="tag bad">Long supply line: attacks at ${Math.round(line.mul * 100)}%</span>`);
  if (frozen(g, c.nat, g.regions[c.reg])) tags.push('<span class="tag bad">No winter kit</span>');
  const gen = generalOf(g, c);
  for (const t of gen?.traits ?? []) {
    const d = COMMAND.traits[t];
    if (d) tags.push(`<span class="tag ${d.good ? 'good' : 'bad'}" title="${esc(d.effect)}">${esc(d.label)}</span>`);
  }
  if (c.hold && !mine) tags.push('<span class="tag">Garrison</span>');
  const pips = c.kind === 'fort' ? '' : `<span class="pips" title="Moves left this month">${Array.from({ length: Math.max(c.apMax, c.ap) }, (_, i) => `<i class="${i < c.ap ? '' : 'off'}"></i>`).join('')}</span>`;
  const el = mine && c.kind !== 'fort' ? 'button' : 'div';
  return `<${el} class="corps${pick ? ' pick' : ''}" ${el === 'button' ? `data-act="pick" data-id="${c.id}"` : ''}>
    ${icon(c)}<span class="nm">${esc(c.name)}${gen ? ` <span style="font-weight:400;color:var(--ink-2)">· ${esc(gen.name)} <span class="lvl" title="Level ${gen.level} general">${gen.level}</span></span>` : ''}</span>${pips}
    <span class="meta">${g.nations[c.nat].adj} ${k.label.toLowerCase()} · ${c.divs} division${c.divs > 1 ? 's' : ''} · ${num(men(c))} men</span>
    <span class="bars"><span>Strength</span><span class="meter"><i class="${c.str < 0.5 ? 'low' : ''}" style="width:${Math.round(c.str * 100)}%"></i></span><span>Cohesion</span><span class="meter"><i class="org${c.org < 0.4 ? ' low' : ''}" style="width:${Math.round(c.org * 100)}%"></i></span></span>
    ${tags.length ? `<span class="tags">${tags.join('')}</span>` : ''}</${el}>`;
}

function drawPane(): void {
  const pane = $('#pane');
  if (ui.tab === 'build' && !(g.human && g.nations[g.human].alive)) ui.tab = 'region';
  if (ui.tab === 'region') pane.innerHTML = regionPane();
  else if (ui.tab === 'build') pane.innerHTML = buildPane();
  else if (ui.tab === 'generals') pane.innerHTML = generalsPane();
  else if (ui.tab === 'reel') pane.innerHTML = reelPane();
  else if (ui.tab === 'battles') pane.innerHTML = battlesPane();
  else pane.innerHTML = powersPane();
}

function regionPane(): string {
  if (ui.sel < 0) {
    return `<h2>${g.human ? 'Your orders' : 'The map'}</h2><p class="sub">${g.human ? 'Click a region to see who holds it and which armies are there.' : 'Click any region to inspect it. Use Next month or Play to watch the war unfold.'}</p>${scopeNote()}${advice()}
      <h3>How a month works</h3><ol class="steps"><li>Each army gets four moves a month, shown as gold dots. Tanks and motorized troops in a fresh offensive get one or two more.</li><li>Marching through friendly ground costs one move. Entering enemy ground costs foot infantry two.</li><li>An attack is settled at once from the Battle Ticket: forces, terrain, forts, supply, air, commanders.</li><li>A nation asks for terms when it has lost too much of its capital, its territory and its army.</li>${g.turn >= 20 ? '<li>Deep inside the Soviet Union an invader outruns his supplies: attacks weaken and marches slow until the railways behind are relaid. Mud and winter make it worse.</li>' : ''}${g.human ? '<li>Your factories earn production points every month. Open <b>Build</b> to raise new corps or fortify a region.</li><li>Neutral countries drift toward whoever is winning. Open <b>Powers</b> to court them with political capital.</li><li>Generals gain experience in battle and earn traits from how they fight. Open <b>Generals</b> to give them commands and spend their skill points.</li>' : ''}</ol>`;
  }
  const r = g.regions[ui.sel];
  const me = g.human;
  const held = r.ctrl === r.owner ? `${g.nations[r.owner].adj} territory` : `${g.nations[r.owner].adj} territory, held by ${natName(r.ctrl)}`;
  const tags: string[] = [];
  if (r.cap) tags.push('<span class="tag good">Capital</span>');
  if (r.port) tags.push('<span class="tag">Port</span>');
  if (r.fort) tags.push(`<span class="tag">Fortified, level ${r.fort.lv}, facing ${r.fort.vs.map((v) => g.nations[v]?.adj ?? v).join(' and ')} ground</span>`);
  if (me && g.nations[me].alive && allied(g, me, r.ctrl)) tags.push(g.supply[me]?.[r.id] ? '<span class="tag good">In your supply net</span>' : '<span class="tag bad">No supply line to here</span>');
  const mineHere = r.units.filter((c) => canOrder(g, c));
  let acts = '';
  const picked = pickedCorps();
  if (picked.length) {
    const canRail = picked.some((c) => c.ap >= 4);
    const canShip = picked.some((c) => shipTargets(g, c).length > 0);
    const canEvac = picked.some((c) => g.nations[c.nat].seaSupply && r.port && r.owner !== c.nat);
    acts = `<div class="acts">${canRail ? `<button class="btn small${ui.rail ? ' on' : ''}" data-act="rail">Rail move</button>` : ''}${canShip ? `<button class="btn small${ui.ship ? ' on' : ''}" data-act="ship">Ship by sea</button>` : ''}${canEvac ? '<button class="btn small" data-act="evac" title="An emergency lift home. The corps leaves a quarter of its strength behind.">Evacuate by sea</button>' : ''}${picked.length === 1 ? (picked[0].general !== null ? `<button class="btn small" data-act="general" data-id="${picked[0].general}">General's record</button>` : `<button class="btn small" data-act="appoint" data-id="${picked[0].id}">Appoint a general</button>`) : ''}<button class="btn small" data-act="unpick">Stand down</button></div>`;
  } else if (mineHere.some((c) => c.ap > 0)) {
    acts = `<div class="acts"><button class="btn small" data-act="pickall">Select all here</button></div>`;
  }
  const units = r.units.length ? [...r.units].sort((a, b) => (a.nat === me ? 0 : 1) - (b.nat === me ? 0 : 1)).map((c) => corpsRow(c, canOrder(g, c))).join('') : '<div class="empty">No armies here.</div>';
  return `<h2>${esc(r.name)}</h2><div class="sub">${held}</div>
    <div class="facts"><div class="fact"><span>Terrain</span><b>${r.terrain[0].toUpperCase() + r.terrain.slice(1)}</b></div><div class="fact"><span>Victory pts</span><b>${r.vp}</b></div><div class="fact"><span>Industry</span><b>${r.fac}</b></div></div>
    ${tags.length ? `<div class="tags" style="display:flex;gap:5px;flex-wrap:wrap;margin-top:8px">${tags.join('')}</div>` : ''}
    <h3>Forces here</h3>${acts}${units}${ui.sel >= 0 && me ? advice() : ''}`;
}

/** Where a new corps would form up: the selected region if it is home ground, otherwise the capital. */
function raiseAt(): Region | null {
  const me = g.human!;
  if (ui.sel >= 0 && isHome(g, me, g.regions[ui.sel])) return g.regions[ui.sel];
  const cap = g.nations[me].capital;
  if (cap >= 0 && isHome(g, me, g.regions[cap])) return g.regions[cap];
  return g.regions.find((r) => isHome(g, me, r) && r.fac > 0) ?? g.regions.find((r) => isHome(g, me, r)) ?? null;
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

function battlesPane(): string {
  if (!g.reports.length) return '<h2>Battles</h2><div class="empty">No battles yet. Every battle is settled from its Battle Ticket and filed here.</div>';
  let out = '<h2>Battles</h2><p class="sub">Open any battle to see its Battle Ticket: what the campaign map fed into the fight.</p><div style="height:8px"></div>';
  for (let i = g.reports.length - 1; i >= Math.max(0, g.reports.length - 80); i--) {
    const r = g.reports[i];
    const v = verdictFor(r);
    out += `<button class="brow" data-act="report" data-id="${r.id}"><b>${esc(r.regionName)}</b><span class="res ${v.cls}">${v.text}</span><span>${date(r.turn)}, week ${r.week} · ${g.nations[r.attNat].adj} attack on ${g.nations[r.defNat].adj} forces</span></button>`;
  }
  return out;
}

function powersPane(): string {
  const at = Object.values(g.nations).filter((n) => !n.dormant && (n.atWar.length || n.fellOn));
  const neutral = Object.values(g.nations).filter((n) => n.alive && !n.atWar.length && !n.dormant);
  let out = '<h2>Powers at war</h2><p class="sub">The bar shows how much fight a nation has left. It drains as the capital, territory and army are lost.</p><div style="height:10px"></div>';
  for (const n of at.sort((a, b) => (b.alive ? 1 : 0) - (a.alive ? 1 : 0) || b.armyPeak - a.armyPeak)) {
    if (n.alive) {
      const will = Math.max(0, 1 - collapseScore(g, n.id) / n.resilience);
      out += `<div class="nat"><span class="chip" style="background:${n.color}"></span><b>${esc(n.name)}</b><small>${num(armySize(g, n.id))} divisions</small><div class="will"><div class="meter"><i class="${will < 0.35 ? 'low' : ''}" style="width:${Math.round(will * 100)}%"></i></div><small>At war with ${n.atWar.map(natName).join(', ')}</small></div></div>`;
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
  else if (m.kind === 'attack') card.innerHTML = attackCard();
  else if (m.kind === 'news') card.innerHTML = newsCard(m.from, m.reports);
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
    <div class="body"><p class="lead">This is the strategy layer of the game: a map of Europe in ${g.regions.length} regions, monthly turns, and battles settled from a <b>Battle Ticket</b>. Pick where to start and which side to play, or watch the computer play out history's plan.</p>
    ${resume ? `<div class="sides">${resume}</div>` : ''}
    <h3>September 1939 · The war begins</h3>
    <div class="sides">
      ${side(null, 'Watch history unfold', 'Every nation follows its plan. Press Play and watch Poland, then the West, then the East.', true)}
      ${side('GER', 'Germany', 'On the attack from the first day. The hardest plan to pull off on schedule.')}
      ${side('FRA', 'France', 'The Maginot Line, a large army, and a slow command. Can you keep Paris?')}
      ${side('UK', 'United Kingdom', 'A small expeditionary force and the sea at your back.')}
      ${side('POL', 'Poland', 'Outnumbered and nearly surrounded. Hold as long as you can.')}
    </div>
    <h3>June 1941 · Barbarossa</h3>
    <div class="sides">
      ${side(null, 'Watch Barbarossa unfold', 'The computer plays the first 21 months, then the invasion of the Soviet Union and the first winter.', true, 22)}
      ${side('GER', 'Germany', 'Three army groups on the frontier. Reach Moscow before the snow, or outrun your supplies trying.', false, 22)}
      ${side('SOV', 'Soviet Union', 'The frontier armies are about to be hit. Trade space for time until the reserves and the winter arrive.', false, 22)}
    </div>
    <ol class="steps"><li>Click one of your armies, then a green region to march or a red one to attack.</li><li>Press <b>End month</b> and the other nations take their turns.</li><li>Spend your factories' output under <b>Build</b>. The game saves itself as you play.</li><li>Drag the map to move it. Scroll or pinch to zoom.</li></ol></div>`;
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

function newsCard(from: number, repFrom: number): string {
  const hs = g.headlines.slice(from);
  const me = g.human;
  const reps = g.reports.slice(repFrom).filter((r) => me && (r.attackers.some((u) => u.nat === me) || r.defenders.some((u) => u.nat === me)));
  const all = g.reports.length - repFrom;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Dispatches</div><h2>${date(g.turn)}</h2></header>
    <div class="body">${hs.length ? `<div class="reel">${hs.map((h) => `<p class="${h.kind}"><span class="wk">${date(h.turn).split(' ')[0]} wk ${h.week}</span>${esc(h.text)}</p>`).join('')}</div>` : '<p class="lead">A quiet month. No news of note.</p>'}
    ${reps.length ? `<h3>Battles your forces fought</h3>${reps.map((r) => { const v = verdictFor(r); return `<button class="brow" data-act="report" data-id="${r.id}"><b>${esc(r.regionName)}</b><span class="res ${v.cls}">${v.text}</span><span>${date(r.turn)}, week ${r.week}</span></button>`; }).join('')}` : ''}
    ${all ? `<p class="sub" style="margin-top:10px">${all} battle${all > 1 ? 's were' : ' was'} fought across the map. They are filed under Battles.</p>` : ''}</div>
    <footer><button class="btn go" data-act="close">${me && humanUp(g) ? 'Give orders' : 'Continue'}</button></footer>`;
}

function declareCard(): string {
  const list = declarable(g);
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Diplomacy</div><h2>Declare war</h2></header>
    <div class="body"><p class="lead">A neutral that is attacked joins the other side, and its friends may march to help it at once.</p>
    ${list.map((id) => `<button class="pickrow" data-act="dodeclare" data-nat="${id}"><span class="chip" style="background:${g.nations[id].color}"></span><span class="grow"><b>${esc(natName(id))}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${num(armySize(g, id))} divisions</span></span></button>`).join('')}</div>
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
  drawStatic();
  const focus = from > 1 ? g.byName[human === 'SOV' ? 'Smolensk' : 'Brest'] : human ? g.regions[g.nations[human].capital] : g.byName['Poznan'];
  ui.view = { x: focus.x - 300, y: focus.y - 230, w: human === 'GER' || from > 1 ? 640 : 600 };
  applyView();
  if (human) advanceToHuman(from > 1 && human !== 'GER'); else { ui.unread = from > 1 ? 0 : g.headlines.length; refresh(); }
}

/** Picks up the campaign saved in this browser. Returns false if there is none that can be read. */
function continueGame(): boolean {
  const loaded = loadGame(readSave() ?? '');
  if (!loaded) return false;
  stopAuto();
  g = loaded;
  ui.live = true;
  ui.sel = -1; ui.picked.clear(); ui.rail = ui.ship = false; ui.tab = 'region'; ui.unread = 0; ui.seenReports = g.reports.length; ui.flash = [];
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
function advanceToHuman(show: boolean): void {
  const hFrom = g.headlines.length, rFrom = g.reports.length;
  const before = g.regions.map((r) => r.ctrl);
  let guard = 0;
  while (!humanUp(g) && guard++ < 80) {
    if (g.human && !g.nations[g.human].alive) break;
    stepAI(g);
  }
  if (humanUp(g)) beginHuman(g);
  ui.flash = g.regions.filter((r, i) => r.ctrl !== before[i]).map((r) => r.id);
  ui.unread += g.headlines.length - hFrom;
  ui.sel = -1; ui.picked.clear(); ui.rail = ui.ship = false;
  refresh();
  if (g.human && !g.nations[g.human].alive) {
    const lost = g.human;
    g.human = null;
    refresh();
    openModal({ kind: 'news', from: hFrom, reports: rFrom });
    $('#card').querySelector('.kick')!.textContent = `${natName(lost)} has fallen. The war goes on without you.`;
    return;
  }
  if (show && (g.headlines.length > hFrom || g.reports.length > rFrom)) openModal({ kind: 'news', from: hFrom, reports: rFrom });
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

function stopAuto(): void {
  if (ui.auto) { clearInterval(ui.auto); ui.auto = 0; }
}

// ---- Clicks -------------------------------------------------------------------------------

function onClick(e: MouseEvent): void {
  const t = (e.target as HTMLElement).closest('[data-act]') as HTMLElement | null;
  if (!t) { if (e.target === $('#veil') && ui.modal && ui.modal.kind !== 'start') openModal(null); return; }
  const act = t.dataset.act!;
  const id = Number(t.dataset.id);
  switch (act) {
    case 'start': openModal(null); newGame(t.dataset.nat || null, undefined, Number(t.dataset.from || 1)); break;
    case 'continue': if (continueGame()) openModal(null); else openModal({ kind: 'start' }); break;
    case 'new': stopAuto(); openModal({ kind: 'start' }); break;
    case 'close': openModal(null); refresh(); break;
    case 'tab':
      ui.tab = t.dataset.tab as typeof ui.tab;
      if (ui.tab === 'reel') ui.unread = 0;
      if (ui.tab === 'battles') ui.seenReports = g.reports.length;
      refresh();
      break;
    case 'zin': zoomBy(0.7); break;
    case 'zout': zoomBy(1.4); break;
    case 'next': nextMonth(); break;
    case 'auto':
      if (ui.auto) stopAuto(); else { nextMonth(); ui.auto = window.setInterval(nextMonth, 1500); }
      drawBar();
      break;
    case 'end': endHuman(g); advanceToHuman(true); break;
    case 'pick': {
      if (ui.picked.has(id)) ui.picked.delete(id); else ui.picked.add(id);
      ui.rail = ui.ship = false; refresh(); break;
    }
    case 'pickall': selectRegion(ui.sel); refresh(); break;
    case 'unpick': ui.picked.clear(); ui.rail = ui.ship = false; refresh(); break;
    case 'rail': ui.rail = !ui.rail; ui.ship = false; refresh(); break;
    case 'ship': ui.ship = !ui.ship; ui.rail = false; refresh(); break;
    case 'build': { const at = raiseAt(); if (at) orderBuildCorps(g, t.dataset.kind as Kind, at.id); refresh(); break; }
    case 'fortify': if (ui.sel >= 0) orderBuildFort(g, ui.sel); refresh(); break;
    case 'cancelbuild': orderCancelBuild(g, id); refresh(); break;
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
  ui.rail = ui.ship = false;
  ui.picked = new Set(humanUp(g) ? g.regions[id].units.filter((c) => canOrder(g, c) && c.ap > 0).map((c) => c.id) : []);
  ui.tab = 'region';
}

function onRegion(id: number): void {
  const r = g.regions[id];
  ui.flash = [];
  // While planning production the map only chooses where to build. No army moves by accident.
  if (ui.tab === 'build') {
    ui.sel = id; ui.picked.clear(); ui.rail = ui.ship = false;
    refresh();
    return;
  }
  const picked = pickedCorps();
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
  v.y = Math.max(-200, Math.min(g.height + 200 - h, v.y));
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
    if (!el || e.pointerType !== 'mouse') { tip.style.display = 'none'; return; }
    const r = g.regions[Number(el.dataset.id)];
    const divs = sumDivs(r.units);
    tip.innerHTML = `<b>${esc(r.name)}</b><br>${natName(r.ctrl)}${r.ctrl !== r.owner ? ` (occupied ${g.nations[r.owner].adj} ground)` : ''} · ${r.terrain}${r.units.length ? `<br>${r.units.length} corps, ${num(divs)} divisions` : ''}`;
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
      const el = document.elementFromPoint(e.clientX, e.clientY)?.closest('[data-id]') as HTMLElement | null;
      if (el && el.classList.contains('reg')) onRegion(Number(el.dataset.id));
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
(window as any).theater = { game: () => g, ui, newGame, nextMonth, onRegion, continueGame, save: () => saveGame(g) };

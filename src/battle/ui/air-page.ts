// The air battle page: choose a raid the campaign is about to fly, fight it over its target, and see what the campaign
// makes of it. The air's first slice of the played battles (ruling 217): a browser page of its own, beside the land and
// sea pages, with no press on the campaign screen until the play version ships and the Playtester finds it easy.
//
// Mouse: left picks (drag for a box, Shift adds), right orders your fighters (on an enemy group to attack it, on the
// sky to patrol there). Keys: Space pause, 1 2 3 speed, H hold, R break off, Tab next group, Esc drops the choice. A
// plain left click never gives an order, and Enter does nothing. The words are the UI director's (3d-handoff.md
// section 4); TODO(polish): the page's own look waits on the UI and art directors.

import { afterCard, type AfterCard } from '../../campaign/airfought';
import { launchRaid, planRaid, raidTargets, type RaidKind } from '../../campaign/airwar';
import { createGame, setWar } from '../../campaign/game';
import { startOpen } from '../../campaign/staff';
import type { Game } from '../../campaign/types';
import { Clock, type Speed } from '../core/clock';
import { unitAt, unitsIn } from '../core/select';
import type { Command, FieldEvent, FieldTicket, Side } from '../core/types';
import { SKY } from '../air/book';
import { AirSim } from '../air/sim';
import { airTicket, foughtAir } from '../air/ticket';
import type { AirPlace, AirUnit } from '../air/types';
import { BAND_WORD, drawAir, live, planeSvg, takeEvents, type AirScene } from './air-draw';
import { toField, toScreen } from './draw';

const KEY_GUARD = 0.5;
const esc = (s: string): string => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]!);
const mmss = (t: number): string => `${Math.floor(t / 60)}:${String(Math.floor(t % 60)).padStart(2, '0')}`;
/** A row of facts with the kit's dots between them, wrapping between facts and never inside one. */
const facts = (bits: string[]): string => `<span class="facts"><span>${bits.map((b) => `<span class="fact">${b}</span>`).join('')}</span></span>`;
/** A number never parts from its noun (the UI director, air review 11). */
const NB = '\u00a0';

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
  asking: boolean;
  last: number;
  trayAt: number;
  /** After the campaign books the raid: each of the player's groups' aircraft as booked, which the tray then shows. */
  booked: Map<number, number> | null;
}

const app = document.getElementById('app')!;
const S: State = {
  screen: 'choose', sim: null, raid: null, clock: new Clock(),
  scene: { cam: { x: SKY.w / 2, y: SKY.h / 2, z: 0.008 }, W: 800, H: 600, me: 'att', sel: new Set(), hover: null, box: null, line: null, now: 0, ping: null, marks: [], at: null },
  guardUntil: 0, dispatch: null, asking: false, last: 0, trayAt: 0, booked: null,
};
const now = (): number => performance.now() / 1000;
const guard = (): void => { S.guardUntil = now() + KEY_GUARD; };

// ---- The raids on offer ----------------------------------------------------------------------------------------------

/** The raids the first slice can fight, from the grand campaign's opening in September 1939, as its raid card picks them. */
const OFFERS: { nat: string; wars: string[]; kind: RaidKind; name: string; about: string }[] = [
  { nat: 'GER', wars: ['UK', 'FRA'], kind: 'bomb', name: 'Alsace', about: 'An escorted day raid into the French fighters' },
  { nat: 'GER', wars: ['UK', 'FRA'], kind: 'superiority', name: 'Alsace', about: 'Fighters only: win the sky over the front' },
  { nat: 'UK', wars: ['GER'], kind: 'bomb', name: 'Bremen', about: 'A small escort against many fighters' },
  { nat: 'GER', wars: ['UK', 'FRA'], kind: 'night', name: 'Kent', about: 'By night into radar: the fighters see only what comes close' },
];

function raidOf(i: number): Raid | null {
  const o = OFFERS[i];
  const g = startOpen(createGame(7, o.nat));
  for (const w of o.wars) setWar(g, o.nat, w);
  const t = raidTargets(g, o.kind, o.nat).find((x) => planRaid(g, o.kind, x.id, undefined, o.nat).place === o.name);
  if (!t) return null;
  const plan = planRaid(g, o.kind, t.id, undefined, o.nat);
  // Every fighter group that can go with it by day, as the raid card offers them.
  const ids = o.kind === 'night' ? plan.picked : [...plan.picked, ...plan.seats.filter((x) => !x.why && !x.picked && x.kind === 'ftr').map((x) => x.id)];
  const { ticket } = airTicket(g, o.kind, t.id, ids, o.nat);
  return ticket ? { g, nat: o.nat, kind: o.kind, target: t.id, ids, ticket } : null;
}

/** "300 bombers and 96 fighters": what a side sends up. */
function forceWords(t: FieldTicket<AirPlace>, side: Side): string {
  const n = (k: string): number => t[side].forces.filter((f) => (k === 'bmr' ? f.kind === 'bmr' : f.kind !== 'bmr')).reduce((s, f) => s + f.men, 0);
  const f = n('ftr'), b = n('bmr');
  return [b ? `${b}${NB}bombers` : '', f ? `${f}${NB}fighters` : ''].filter(Boolean).join(' and ') || 'nothing';
}

/** What kind of battle it is, for the top bar's subtitle: "an escorted day raid", "a fight for the sky", "a night raid". */
function kindWords(t: FieldTicket<AirPlace>): string {
  if (t.place.mission === 'superiority') return 'a fight for the sky';
  if (t.place.night) return 'a night raid';
  return t.att.forces.some((f) => f.kind !== 'bmr') ? 'an escorted day raid' : 'a day raid';
}

function showChooser(): void {
  S.screen = 'choose'; S.sim = null; S.raid = null;
  guard();
  const raids = OFFERS.map((_, i) => raidOf(i));
  app.innerHTML = `<div class="choose tc air">
    <header class="chead"><div><div class="tc-label">Air battles · first slice</div><h1 class="tc-title-1">Choose a raid to fight</h1></div></header>
    <div class="cbody">
      <section class="tc-paper intro"><p><b>The campaign settles every raid from its raid card until you choose to fly one here.</b> Each air group is a formation that fights as one; the raid comes on as one stream and the defenders rise to meet it. Height is yours to choose: a group above its enemy dives on it and strikes first. What happens here goes back to the campaign as the aircraft each side lost and the bombers that bombed, and you can hand the rest to the staff at any moment: what you have won stays won.</p></section>
      <h2 class="tc-label shead">From the campaign · September 1939</h2>
      <div class="offers">${raids.map((r, i) => (r ? `<button class="tc-card is-press offer" data-raid="${i}">
        <span class="otop"><b class="tc-title-3">${esc(r.ticket.title)}</b></span>
        <span class="tc-data omid">${esc(OFFERS[i].about)}</span>
        <span class="tc-data olow">${esc(forceWords(r.ticket, 'att'))} against ${esc(forceWords(r.ticket, 'def'))}</span></button>` : '')).join('')}</div>
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
  const sim = new AirSim(t);
  S.sim = sim; S.raid = r; S.screen = 'battle';
  const sc = S.scene;
  sc.me = t.att.human ? 'att' : 'def';
  sc.sel = new Set(); sc.hover = null; sc.box = null; sc.line = null; sc.ping = null; sc.marks = []; sc.at = null;
  S.clock = new Clock(); S.clock.set(1); S.clock.toggle();
  S.asking = false;
  S.booked = null;
  // One sentence (the UI director, air review 6).
  S.dispatch = { t: 0, what: 'contact', text: `${t.place.mission === 'superiority' ? 'Your fighters are formed up' : 'Your raid is formed up'} and heading for ${t.place.name}: give your orders, then press Space to begin.` };
  guard();
  const mine = t[sc.me], theirs = t[sc.me === 'att' ? 'def' : 'att'];
  app.innerHTML = `<div class="battle tc air">
    <header class="bbar">
      <div class="btitle"><b class="tc-title-3">${esc(t.title)}</b><span class="tc-data">${facts([esc(t.date), esc(kindWords(t))])}</span></div>
      <div class="bclock"><span class="tc-label">Battle time</span><span class="tc-data" id="time">0:00</span></div>
      <div class="bspeed" role="group" aria-label="Clock">
        <button class="tc-btn tc-btn--sm" data-clock="0" title="Pause (Space)">Pause <span class="tc-key">Space</span></button>
        <button class="tc-btn tc-btn--sm" data-clock="1" title="Normal speed (1)">×1 <span class="tc-key">1</span></button>
        <button class="tc-btn tc-btn--sm" data-clock="2" title="Twice as fast (2)">×2 <span class="tc-key">2</span></button>
        <button class="tc-btn tc-btn--sm" data-clock="4" title="Four times as fast (3)">×4 <span class="tc-key">3</span></button>
      </div>
      <div class="bbal" title="Each side's aircraft still in the fight, against both at the start.">
        <div class="bends"><span>${esc(mine.adj)}</span><span>${esc(theirs.adj)}</span></div>
        <div class="btrack"><i id="balA"></i><i id="balD"></i></div>
        <span id="ratio" class="tc-data bratio"></span>
      </div>
      <div class="bacts">
        <button class="tc-btn" data-act="dice" title="The staff settles the rest at once, from where the battle stands. What you have won stays won.">Hand it to the staff</button>
        <button class="tc-btn tc-btn--grave" data-act="withdraw">Break off all</button>
        <div class="wask tc-paper" id="wask" hidden role="alertdialog" aria-label="Break off every air group?"><p>Break off every air group? The battle ends here.</p><div class="wbtns"><button class="tc-btn" data-act="wgo">Break off</button><button class="tc-btn tc-btn--primary" data-act="wstay">Stay</button></div></div>
      </div>
    </header>
    <div class="bfield"><canvas id="field"></canvas>
      <div class="dispatch tc-paper" id="dispatch"></div>
      <div class="zoom"><button class="tc-btn tc-btn--sm" data-zoom="1" aria-label="Closer">+</button><button class="tc-btn tc-btn--sm" data-zoom="-1" aria-label="Further">−</button><button class="tc-btn tc-btn--sm" data-zoom="0" title="The whole sky">Whole sky</button></div>
      <div class="airorders tc-paper" id="airorders" hidden>
        <span class="tc-label">Height</span><span class="tc-seg" id="band">${BAND_WORD.map((w, i) => `<button data-band="${i}">${w}</button>`).join('')}</span>
        <span class="aftr" id="aftr"><span class="tc-seg"><button data-escort="1">Stay close</button><button data-escort="0">Range ahead</button></span><button class="tc-btn tc-btn--sm" data-act="hunt">Hunt</button></span>
        <button class="tc-btn tc-btn--sm tc-btn--grave" id="jett" data-act="jettison">Jettison the bombs</button>
      </div>
    </div>
    <div class="keys tc-data">${facts(['Left click: pick up', 'Right click an enemy group: attack it', 'Right click the sky: patrol there', '<span class="tc-key">H</span> hold', '<span class="tc-key">R</span> break off', '<span class="tc-key">Tab</span> next group', '<span class="tc-key">Esc</span> put down'])}</div>
    <footer class="tray" id="tray">${sim.units.filter((u) => u.side === sc.me).map((u) => card(u, mine.color)).join('')}</footer>
  </div>`;
  const el = (id: string): HTMLElement => document.getElementById(id)!;
  el('balA').style.background = t.att.color; el('balD').style.background = t.def.color;
  canvas = el('field') as HTMLCanvasElement;
  ctx = canvas.getContext('2d')!;
  fit(true);
  wireBattle();
  updateTop(true);
  updateTray(true);
  S.last = now();
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

function updateTop(force = false): void {
  const sim = S.sim!;
  const el = (id: string): HTMLElement => document.getElementById(id)!;
  el('time').textContent = `${mmss(sim.t)} of ${mmss(sim.ticket.limit)}`;
  const count = (side: Side): number => sim.units.filter((u) => u.side === side && live(u)).reduce((s, u) => s + u.n, 0);
  const a = count('att'), d = count('def');
  el('balA').style.width = `${((100 * a) / Math.max(1, a + d)).toFixed(1)}%`;
  el('balD').style.width = `${((100 * d) / Math.max(1, a + d)).toFixed(1)}%`;
  el('ratio').textContent = `${S.scene.me === 'att' ? a : d} against ${S.scene.me === 'att' ? d : a} in the air`;
  document.querySelectorAll<HTMLElement>('[data-clock]').forEach((x) => x.classList.toggle('is-lit', Number(x.dataset.clock) === S.clock.speed));
  (el('wask') as HTMLElement).hidden = !S.asking;
  // The orders strip shows for the groups in hand: height for all, the escort's and the hunt's for fighters, the bombs for bombers.
  const ids = chosen();
  const us = ids.map((id) => unit(id)!);
  const strip = el('airorders');
  strip.hidden = us.length === 0;
  const bands = new Set(us.map((u) => u.to));
  strip.querySelectorAll<HTMLElement>('[data-band]').forEach((b) => b.classList.toggle('is-on', bands.size === 1 && bands.has(Number(b.dataset.band) as 0 | 1 | 2)));
  const ftr = us.filter((u) => u.air === 'ftr'), bmr = us.filter((u) => u.air === 'bmr' && !u.dropped && !u.jettisoned);
  el('aftr').hidden = ftr.length === 0 || !sim.units.some((u) => u.side === S.scene.me && u.air === 'bmr' && live(u));
  if (ftr.length === 0) el('aftr').hidden = true;
  const ways = new Set<string>(ftr.map((u) => (u.job.do === 'escort' ? (u.job.close ? '1' : '0') : u.job.do === 'hunt' ? 'h' : '')));
  strip.querySelectorAll<HTMLElement>('[data-escort]').forEach((b) => b.classList.toggle('is-on', ways.size === 1 && ways.has(b.dataset.escort!)));
  (strip.querySelector('[data-act="hunt"]') as HTMLElement).classList.toggle('is-lit', ways.size === 1 && ways.has('h'));
  el('jett').hidden = bmr.length === 0;
  if (force || S.dispatch) {
    const d = S.dispatch;
    el('dispatch').innerHTML = d ? `<span class="tc-label">${mmss(d.t)}</span><span>${esc(d.text)}</span>` : '';
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
// The whole sky, with room under its southern edge for the raid coming on and the orders strip.
function whole(): void { const z = Math.min(S.scene.W / SKY.w, (S.scene.H - 70) / SKY.h) * 0.97; S.scene.cam = { x: SKY.w / 2, y: SKY.h / 2 - 30 / z, z }; }
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
  c.x = hw * 2 >= SKY.w ? SKY.w / 2 : Math.max(hw, Math.min(SKY.w - hw, c.x));
  c.y = hh * 2 >= SKY.h ? SKY.h / 2 : Math.max(hh, Math.min(SKY.h - hh, c.y));
}

// ---- Orders -----------------------------------------------------------------------------------------------------------

function chosen(): number[] { return [...S.scene.sel].filter((id) => { const u = unit(id); return !!u && orderable(u); }); }
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
  const foe = unitAt(visible(), fx, fy, reachPx());
  return foe ? `Attack ${foe.name}` : 'Patrol here';
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
      const u = unitAt([...sim.units.filter((x) => x.side === sc.me && live(x)), ...visible()], fx, fy, reachPx(), sc.me);
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
        const got = unitsIn(sim.units, sc.me, x0, y0, x1, y1).filter(orderable);
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
  app.querySelectorAll<HTMLElement>('[data-clock]').forEach((el) => el.addEventListener('click', () => { S.clock.set(Number(el.dataset.clock) as Speed); updateTop(); }));
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
  app.querySelector('[data-act="dice"]')!.addEventListener('click', () => { give({ do: 'dice' }); });
  app.querySelector('[data-act="withdraw"]')!.addEventListener('click', () => { S.asking = true; guard(); updateTop(); });
  app.querySelector('[data-act="wstay"]')!.addEventListener('click', () => { S.asking = false; updateTop(); });
  app.querySelector('[data-act="wgo"]')!.addEventListener('click', () => { if (now() < S.guardUntil) return; S.asking = false; give({ do: 'withdrawAll', side: sc.me }); });
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
  const u = unitAt(sim.units.filter((x) => x.side === sc.me && orderable(x)), fx, fy, reachPx(), sc.me);
  if (!u) { if (!add) sc.sel.clear(); return; }
  if (add) { if (sc.sel.has(u.id)) sc.sel.delete(u.id); else sc.sel.add(u.id); } else { sc.sel.clear(); sc.sel.add(u.id); }
}

function orderAt(px: number, py: number): void {
  const sc = S.scene;
  const ids = chosen();
  if (!ids.length) return;
  const [fx, fy] = toField(sc, px, py);
  const foe = unitAt(visible(), fx, fy, reachPx());
  if (foe) { if (give({ do: 'attack', units: ids, target: foe.id })) sc.ping = { x: foe.x, y: foe.y, at: now(), kind: 'attack' }; }
  else if (give({ do: 'move', units: ids, x: fx, y: fy })) sc.ping = { x: fx, y: fy, at: now(), kind: 'move' };
}

window.addEventListener('keydown', (e) => {
  if (e.key === 'Enter') { e.preventDefault(); return; }
  if (now() < S.guardUntil) { if (e.key === 'Tab' || e.key === ' ') e.preventDefault(); return; }
  if (S.screen !== 'battle' || !S.sim) return;
  const sc = S.scene, sim = S.sim;
  const key = e.key.length === 1 ? e.key.toLowerCase() : e.key;
  const pan = 120 / sc.cam.z;
  switch (key) {
    case ' ': e.preventDefault(); S.clock.toggle(); break;
    case '1': S.clock.set(1); break;
    case '2': S.clock.set(2); break;
    case '3': S.clock.set(4); break;
    case 'h': give({ do: 'hold', units: chosen() }); break;
    case 'r': give({ do: 'withdraw', units: chosen() }); break;
    case 'Escape': if (S.asking) S.asking = false; else { sc.sel.clear(); sc.box = null; } break;
    case 'Tab': {
      e.preventDefault();
      const mine = sim.units.filter((u) => u.side === sc.me && orderable(u));
      if (!mine.length) break;
      const cur = [...sc.sel][0];
      const i = mine.findIndex((u) => u.id === cur);
      const next = mine[(i + (e.shiftKey ? mine.length - 1 : 1)) % mine.length];
      sc.sel.clear(); sc.sel.add(next.id);
      sc.cam.x = next.x; sc.cam.y = next.y; clampCam();
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

// ---- The loop -----------------------------------------------------------------------------------------------------------

function frame(): void {
  requestAnimationFrame(frame);
  if (S.screen === 'choose' || !S.sim) return;
  const t = now();
  const dt = Math.min(0.25, t - S.last);
  S.last = t;
  S.scene.now = t;
  const sim = S.sim;
  S.clock.advance(sim, dt);
  const v = sim.view(S.scene.me);
  takeEvents(S.scene, sim, v.events, t);
  if (sim.t > 0 && S.dispatch?.t === 0 && !S.dispatch.unit) S.dispatch = { t: sim.t, what: 'contact', text: 'The raid is on its way in.', unit: -1 };
  for (const e of v.events) if (e.text && e.what !== 'air:lost') S.dispatch = e;
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
      ctx.fillStyle = words.startsWith('Attack') ? '#f3a184' : '#e0b872'; ctx.fillText(words, a + 14, b + 12);
    }
  }
  updateTop();
  updateTray();
  if (v.over && S.screen === 'battle') showAfter();
}

// ---- After the battle: what the campaign makes of it -------------------------------------------------------------------

function showAfter(): void {
  const sim = S.sim!, r = sim.over!, raid = S.raid!;
  S.screen = 'after';
  guard();
  const t = sim.ticket;
  // The campaign flies the raid with the battle's result in place of its dice (or, handed to the staff, its dice on the rest).
  const fought = foughtAir(sim.outcome());
  const rep = launchRaid(raid.g, raid.kind, raid.target, raid.ids, raid.nat, fought);
  const card: AfterCard | null = rep ? afterCard(raid.g, rep, fought) : null;
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
  }
  // The roster: what each group lost of what it sent up, the same form on both sides (the design director, 19:29Z).
  const sides = card ? `<div class="aside2">${(['att', 'def'] as const).map((sd) => `<div><h3 class="tc-label">${esc(t[sd].adj)} ${sd === 'att' ? (sky ? 'fighters' : 'raid') : 'fighters'}</h3><ul>${card.groups.filter((x) => x.side === sd).map((x) =>
    `<li><b>${esc(x.name)}</b> <span>${esc(x.words)}</span></li>`).join('')}</ul></div>`).join('')}</div>` : '';
  const veil = document.createElement('div');
  veil.className = 'veil';
  veil.innerHTML = `<div class="after tc-paper" role="dialog" aria-label="Raid result">
    <div class="tc-label kick">${sky ? 'Battle result' : 'Raid result'} · ${esc(t.date)}</div>
    <h2 class="tc-title-2">${esc(t.title)}</h2>
    <div class="verdict"><span class="tc-stamp ${won ? 'tc-stamp--won' : ''}">${esc(level)}</span></div>
    ${rep ? `<p class="told"><b>${esc(rep.result)}</b></p>` : ''}
    <h3 class="tc-label">In the air</h3><ul class="flines">${lines.map((l) => `<li>${esc(l)}</li>`).join('')}</ul>
    ${card?.reckoning ? `<h3 class="tc-label">Against the staff's reckoning</h3><p class="dice">${esc(card.reckoning)}</p>` : ''}
    ${sides}
    <div class="afoot"><button class="tc-btn tc-btn--primary tc-btn--lg" data-act="continue">Continue</button></div>
  </div>`;
  document.body.appendChild(veil);
  veil.querySelector('[data-act="continue"]')!.addEventListener('click', () => { if (now() < S.guardUntil) return; veil.remove(); showChooser(); });
}

// A handle for the page's own checks (screenshots, the playtester's harness). Not used by the page itself.
(window as unknown as { __air: unknown }).__air = { state: S, give, showChooser, startBattle, raidOf, OFFERS, toScreen };

showChooser();
requestAnimationFrame(frame);

// The convoy night's practice page (release 2; ruling 241 D2, Q9; 217(5): no press on the campaign screen until the
// Playtester finds the five practice nights easy). A page of its own, beside the fleet action's: choose a night, take the
// escorts or the pack, fight it top down on the night sea, and see how the night reads against the staff's own nights
// for the same card. Nothing a practice night does is booked.
//
// Mouse: left picks your own pieces (drag a box, Shift adds); right orders. An escort: right on a boat you hold to hunt
// her, on a wreck to pick up survivors, on the water to steam there and search. A boat: right on a ship to attack her.
// The wheel zooms. Keys: Space pause, 1 2 3 4 speed, F follow the convoy, Tab next piece, Esc put down, S star shell
// (escorts), G deck gun (boats).

import { ranksOf, restFrom, staffNights, verdictOf, type StaffNights } from '../convoy/calibrate';
import { CONVOY_PRACTICE, convoyPracticeTicket, type ConvoyPractice } from '../convoy/practice';
import { ConvoySim } from '../convoy/sim';
import type { ConvoyCommand, ConvoyTicket, ConvoyUnit } from '../convoy/types';
import type { FieldEvent, Side, Zone } from '../core/types';

const esc = (s: string): string => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]!);
const cap = (k: string): string => `<span class="tc-key">${k}</span>`;
const hhmm = (sec: number): string => `${Math.floor(sec / 3600)}:${String(Math.floor((sec % 3600) / 60)).padStart(2, '0')}`;
const MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
const PEOPLE: Record<string, string> = { UK: 'British', GER: 'German', ITA: 'Italian', USA: 'American', JAP: 'Japanese', FRA: 'French', SOV: 'Soviet' };
/** Battle seconds a real second is worth at each speed (the night runs ten to eighteen hours). */
const SPEEDS = [0, 1, 10, 60, 300] as const;
/** The most battle seconds one frame may work, so a hidden tab never runs the night away. */
const MOST_A_FRAME = 600;
/** Staff nights the after card ranks the night among (the campaign's own count: handoff.ts convoyNight). */
const STAFF_NIGHTS = 30;

interface Track { from: [number, number]; to: [number, number]; at: number; run: number; faint: boolean }
interface Charge { x: number; y: number; r: number; at: number; burst: number; ahead: boolean }
interface Said { t: number; text: string; side?: Side }

interface State {
  screen: 'choose' | 'night' | 'after';
  card: number;
  side: Side;
  seed: number;
  t: ConvoyTicket | null;
  sim: ConvoySim | null;
  units: ConvoyUnit[];
  zones: Zone[];
  picked: Set<number>;
  speed: number;
  before: number;
  owed: number;
  cam: { x: number; y: number; scale: number; follow: boolean };
  tracks: Track[];
  charges: Charge[];
  said: Said[];
  mode: 'starshell' | 'gun' | null;
  tag: { text: string; until: number } | null;
  staff: StaffNights | null;
  handing: boolean;
  /** The clock runs fast on its own until the first contact of the night; after that its speed is the player's (×300 alone gives way to ×60 at a contact). */
  rush: boolean;
  drag: { x0: number; y0: number; x1: number; y1: number } | null;
}

const S: State = {
  screen: 'choose', card: 0, side: 'def', seed: 1, t: null, sim: null, units: [], zones: [], picked: new Set(), speed: 1, before: 10, owed: 0,
  cam: { x: 0, y: 0, scale: 0.06, follow: true }, tracks: [], charges: [], said: [], mode: null, tag: null, staff: null, handing: false, rush: true, drag: null,
};

let app: HTMLElement;
let canvas: HTMLCanvasElement | null = null;
let last = 0;

// ---- The chooser ------------------------------------------------------------------------------------------------------

function kitWords(p: ConvoyPractice): string {
  const k = p.kit[p.escorts[0]?.nat ?? 'UK'];
  if (!k) return 'sonar';
  const bits = [k.asdic ? 'sonar' : '', k.radar ? 'radar' : '', k.hedgehog ? 'ahead-throwers' : '', k.groups ? 'group training' : ''].filter(Boolean);
  return bits.length > 1 ? `${bits.slice(0, -1).join(', ')} and ${bits[bits.length - 1]}` : bits[0] ?? 'nothing';
}

function choose(): void {
  S.screen = 'choose';
  const cards = CONVOY_PRACTICE.map((p, i) => {
    const boats = p.boats.reduce((s, b) => s + b.n, 0), escorts = p.escorts.reduce((s, e) => s + e.n, 0);
    return `<button class="tc-panel offer${i === S.card ? ' is-on' : ''}" data-card="${i}">
      <div class="tc-label">${esc(MONTHS[p.month - 1])} ${p.year}</div>
      <div class="otitle">${esc(p.title)}</div>
      <p class="oabout">${esc(p.about)}.</p>
      <div class="ofacts"><span class="nw">${boats} boats</span> <span class="nw">against ${escorts} escorts</span></div>
      <div class="ofacts tc-muted">The escorts have ${esc(kitWords(p))}.</div>
    </button>`;
  }).join('');
  app.innerHTML = `<div class="tc convoy choose">
    <header class="chead"><div class="tc-label">Practice nights</div><h1 class="tc-title-1">A convoy at night</h1></header>
    <section class="tc-paper intro"><p><b>One night on one convoy, from dusk to dawn.</b> The boats come up through the dark and fire into the columns. The escorts find them by radar, by sight and by sonar, run them down and drive them under with depth charges. A boat hurt badly enough is forced up, where her crew fight her deck gun, dive again if they can, or abandon her. What each side brings decides the night: the boats and their navy's habits, the escorts and the kit their navy has. Nothing a practice night does is counted in a campaign.</p></section>
    <div class="offers">${cards}</div>
    <div class="cfoot">
      <div class="tc-seg" role="group" aria-label="Your side">
        <button class="tc-btn ${S.side === 'def' ? 'tc-btn--primary' : ''}" data-side="def">Command the escorts</button>
        <button class="tc-btn ${S.side === 'att' ? 'tc-btn--primary' : ''}" data-side="att">Command the pack</button>
      </div>
      <button class="tc-btn tc-btn--primary tc-btn--lg" data-go>Fight the night</button>
    </div>
  </div>`;
  app.querySelectorAll<HTMLElement>('[data-card]').forEach((b) => b.addEventListener('click', () => { S.card = +b.dataset.card!; choose(); }));
  app.querySelectorAll<HTMLElement>('[data-side]').forEach((b) => b.addEventListener('click', () => { S.side = b.dataset.side as Side; choose(); }));
  app.querySelector('[data-go]')!.addEventListener('click', () => begin(Math.floor(Math.random() * 1e9)));
}

// ---- The night --------------------------------------------------------------------------------------------------------

function begin(seed: number): void {
  const p = CONVOY_PRACTICE[S.card];
  S.seed = seed;
  S.t = convoyPracticeTicket(p, seed, S.side);
  S.sim = new ConvoySim(S.t);
  S.units = []; S.zones = []; S.picked = new Set(); S.tracks = []; S.charges = []; S.said = []; S.mode = null; S.staff = null; S.handing = false;
  S.speed = 60; S.before = 60; S.owed = 0; S.rush = true; S.cam.follow = true; S.cam.scale = 0.05;
  S.tag = { text: 'DUSK · RUNNING ×60 UNTIL THE FIRST CONTACT', until: performance.now() + 4000 };
  S.screen = 'night';
  app.innerHTML = `<div class="tc convoy night">
    <header class="nbar">
      <div class="ntitle"><div class="tc-label">${esc(p.title)}</div><div class="nfacts" data-facts></div></div>
      <div class="nscore" data-score></div>
      <div class="nclock">
        <button class="tc-btn tc-btn--sm" data-speed="0" title="Space">Pause</button>
        <button class="tc-btn tc-btn--sm" data-speed="1" title="1">×1</button>
        <button class="tc-btn tc-btn--sm" data-speed="2" title="2">×10</button>
        <button class="tc-btn tc-btn--sm" data-speed="3" title="3">×60</button>
        <button class="tc-btn tc-btn--sm" data-speed="4" title="4">×300</button>
        <button class="tc-btn tc-btn--sm" data-staffrun title="The staff command your side, and you watch">Staff command</button>
        <button class="tc-btn tc-btn--sm tc-btn--grave" data-hand>Hand it to the staff</button>
      </div>
    </header>
    <div class="nmain">
      <canvas></canvas>
      <div class="ntag" data-tag></div>
      <aside class="tc-panel norders" data-orders></aside>
      <div class="nlog" data-log></div>
    </div>
    <footer class="nkeys tc-muted">Click or drag a box: pick up · Right-click: order · Wheel: zoom · ${cap('F')} follow the convoy · ${cap('Tab')} next · ${cap('Esc')} put down · ${cap('Space')} pause · ${cap('1')}–${cap('4')} speed</footer>
  </div>`;
  canvas = app.querySelector('canvas');
  app.querySelectorAll<HTMLElement>('[data-speed]').forEach((b) => b.addEventListener('click', () => setSpeed(SPEEDS[+b.dataset.speed!])));
  app.querySelector('[data-hand]')!.addEventListener('click', handToStaff);
  app.querySelector('[data-staffrun]')!.addEventListener('click', () => { const s = S.sim!; s.command({ do: 'auto', side: S.side, on: !s.ai[S.side] }, S.side); paintOrders(); });
  wireCanvas();
  take();
  paintOrders();
  last = performance.now();
  requestAnimationFrame(frame);
}

function setSpeed(v: number): void { if (v === 0 && S.speed !== 0) S.before = S.speed; S.speed = v; S.rush = false; paintClock(); }

/** Takes the sim's view: the pieces the player's side sees, and what happened since. */
function take(): void {
  const s = S.sim!;
  const v = s.view(S.side);
  S.units = v.units as ConvoyUnit[];
  S.zones = v.zones;
  const at = new Map(S.units.map((u) => [u.id, u]));
  for (const e of v.events) note(e, at);
  if (v.over && S.screen === 'night') after();
}

/** One event: its words in the log, its marks on the sea, and the clock slowed for the player's own contacts. */
function note(e: FieldEvent, at: Map<number, ConvoyUnit>): void {
  const x = (e.extra ?? {}) as Record<string, number | string | boolean | undefined>;
  if (e.what === 'sea:torpedo') {
    const u = e.unit !== undefined ? at.get(e.unit) : undefined;
    if (u && typeof x.x === 'number' && typeof x.y === 'number') S.tracks.push({ from: [u.x, u.y], to: [x.x, x.y as number], at: e.t, run: Number(x.running ?? 60), faint: !!x.wakeless });
  }
  if (e.what === 'sea:depthCharge' && typeof x.x === 'number') {
    S.charges.push({ x: x.x, y: x.y as number, r: Math.max(20, Number(x.spread ?? 60) / 2), at: e.t, burst: e.t + Number(x.sinking ?? 20), ahead: x.kind === 'ahead' });
  }
  if (e.text) S.said.push({ t: e.t, text: e.text, side: e.side });
  if (S.said.length > 40) S.said.splice(0, S.said.length - 40);
  // (a new contact of the player's own, or his own boat found: back to a speed a man can follow)
  const mine = e.side === S.side || (e.unit !== undefined && at.get(e.unit)?.side === S.side);
  const alarm = (e.what === 'sea:sonar' && e.text && S.side === 'def') || (e.what === 'sea:sonar' && S.side === 'att' && !x.lost)
    || e.what === 'contact' || (e.what === 'sea:dive' && x.why === 'forced' && mine) || e.what === 'sea:torpedoHit';
  if (alarm) {
    const to = S.rush ? 10 : S.speed > 60 ? 60 : S.speed;
    S.rush = false;
    if (to < S.speed) { S.speed = to; S.tag = { text: `${e.what === 'contact' ? 'THE CONVOY IS UNDER ATTACK' : 'CONTACT'} · RUNNING ×${to}`, until: performance.now() + 3500 }; paintClock(); }
  }
}

function frame(nowMs: number): void {
  if (S.screen !== 'night' || !S.sim) return;
  const real = Math.min(0.25, (nowMs - last) / 1000);
  last = nowMs;
  const s = S.sim;
  if (!s.over && S.speed > 0) {
    S.owed = Math.min(MOST_A_FRAME, S.owed + real * S.speed);
    const until = s.t + S.owed;
    let guard = 0;
    while (!s.over && s.t + 1e-9 < until && guard++ < 20000) s.step();
    S.owed = Math.max(0, until - s.t);
    take();
  }
  if (S.screen !== 'night') return;
  draw();
  paintBar();
  requestAnimationFrame(frame);
}

// ---- Orders ----------------------------------------------------------------------------------------------------------

function own(): ConvoyUnit[] { return S.units.filter((u) => u.side === S.side && u.state !== 'gone' && u.state !== 'withdrawn' && u.kind !== 'mv'); }
function picked(): ConvoyUnit[] { return own().filter((u) => S.picked.has(u.id)); }

function order(c: ConvoyCommand | { do: 'move'; units: number[]; x: number; y: number }): void {
  const s = S.sim!;
  if (s.ai[S.side]) s.command({ do: 'auto', side: S.side, on: false }, S.side);
  s.command(c as ConvoyCommand, S.side);
  paintOrders();
}

function paintOrders(): void {
  const el = app.querySelector<HTMLElement>('[data-orders]');
  if (!el || !S.sim) return;
  const ps = picked();
  const staff = S.sim.ai[S.side];
  app.querySelector('[data-staffrun]')?.classList.toggle('is-lit', staff);
  if (!ps.length) {
    el.innerHTML = `<div class="tc-label">${S.side === 'def' ? 'The escorts' : 'The pack'}</div><p class="tc-muted">${staff ? 'The staff command your side. Give any order to take it back.' : S.side === 'def'
      ? 'Your escorts keep their stations round the convoy and report what they find. Pick one up and right-click a boat it holds to send it hunting.'
      : 'Your boats close the convoy, fire and turn away to reload as their captains see fit. Pick one up to give her orders.'}</p>`;
    return;
  }
  const head = ps.length === 1 ? esc(ps[0].name) : `${ps.length} ${S.side === 'def' ? 'escorts' : 'boats'}`;
  const ids = ps.map((u) => u.id);
  const btn = (key: string, label: string, title = ''): string => `<button class="tc-btn tc-btn--sm" data-o="${key}" title="${esc(title)}">${label}</button>`;
  const detail = ps.length === 1 ? pieceWords(ps[0]) : '';
  if (S.side === 'def') {
    el.innerHTML = `<div class="tc-label">${head}</div><p class="tc-muted">${detail}</p><div class="obtns">
      ${btn('screen', 'Back to the screen')}${btn('starshell', `Star shell ${cap('S')}`, 'Then click where to light the sea')}</div>
      <p class="tc-faint ohint">Right-click a boat you hold to hunt her, a wreck to pick up survivors, the sea to steam there and search.</p>`;
  } else {
    el.innerHTML = `<div class="tc-label">${head}</div><p class="tc-muted">${detail}</p><div class="obtns">
      ${btn('attack', 'Attack')}${btn('under', 'Attack submerged')}${btn('dive', 'Dive')}${btn('deep', 'Go deep')}${btn('surface', 'Surface')}${btn('evade', 'Evade')}${btn('gun', `Deck gun ${cap('G')}`, 'Then click a ship')}${btn('leave', 'Leave the night')}</div>
      <p class="tc-faint ohint">Right-click a ship to attack her.</p>`;
  }
  el.querySelectorAll<HTMLElement>('[data-o]').forEach((b) => b.addEventListener('click', () => {
    const o = b.dataset.o!;
    if (o === 'starshell' || o === 'gun') { S.mode = o; return; }
    if (o === 'screen') order({ do: 'convoy:screen', units: ids });
    else if (o === 'attack') order({ do: 'convoy:attack', units: ids });
    else if (o === 'under') order({ do: 'convoy:attack', units: ids, under: true });
    else if (o === 'dive') order({ do: 'convoy:dive', units: ids });
    else if (o === 'deep') order({ do: 'convoy:dive', units: ids, deep: true });
    else if (o === 'surface') order({ do: 'convoy:surface', units: ids });
    else if (o === 'evade') order({ do: 'convoy:evade', units: ids });
    else if (o === 'leave') order({ do: 'convoy:leave', units: ids });
  }));
}

function pieceWords(u: ConvoyUnit): string {
  const x = u.extra;
  if (u.kind === 'ss') {
    const dive = x.dive === 'surfaced' ? 'On the surface' : x.dive === 'periscope' ? 'At periscope depth' : `Deep, at ${Math.round(x.keel ?? 0)} m`;
    const heard = (x.heard ?? []).length ? `, held by ${(x.heard ?? []).length} escort${(x.heard ?? []).length === 1 ? '' : 's'}` : '';
    return `${dive}${heard}. ${x.tubes ?? 0} torpedoes in the tubes.`;
  }
  const phase = x.huntPhase === 'search' ? 'Searching' : x.huntPhase === 'run' ? 'Running in' : x.huntPhase === 'blind' ? 'Over her, sonar deaf' : x.huntPhase === 'open' ? 'Opening the range' : 'On station';
  const echo = x.echo ? `, holding a boat by ${x.echo.kind === 'asdic' ? 'sonar' : x.echo.kind === 'radar' ? 'radar' : 'sight'} at ${x.echo.range} m` : '';
  return `${phase}${echo}.${x.ships > 1 ? ` ${x.ships} ships.` : ''}`;
}

function handToStaff(): void {
  if (!S.sim || S.sim.over || S.handing) return;
  S.handing = true;
  setSpeed(0);
  S.tag = { text: 'THE STAFF ARE WORKING OUT THE REST OF THE NIGHT', until: performance.now() + 60000 };
  paintBar();
  setTimeout(() => {
    S.staff = staffNights(S.t!, STAFF_NIGHTS);
    S.sim!.rest = restFrom(S.staff, S.sim!.limit);
    S.sim!.command({ do: 'dice' }, S.side);
    take();
  }, 30);
}

// ---- The after card ---------------------------------------------------------------------------------------------------

function after(): void {
  S.screen = 'after';
  const s = S.sim!, t = S.t!, over = s.over!;
  const x = over.extra as { counts: { ss: Record<number, number>; dd: Record<number, number>; mv: number }; tally: Record<string, number>; handed: boolean };
  const lines = over.lines.map((l) => `<p>${esc(l)}</p>`).join('');
  const tl = x.tally;
  const facts = [`${tl.salvoes} salvoes fired, ${tl.hits} torpedoes hit`, `${tl.hunts} hunts, ${tl.attacks} attacks on boats`, `${tl.forcedUp} boats forced up, ${tl.scuttled} scuttled, ${tl.crushed} crushed, ${tl.escaped} got away`];
  app.innerHTML = `<div class="tc convoy afterc"><section class="tc-panel acard">
    <div class="tc-label">${esc(t.title)} · ${hhmm(over.seconds ?? s.worked)} of dark</div>
    <h1 class="tc-title-1">${x.counts.mv === 0 ? 'The convoy comes through' : `${x.counts.mv} ship${x.counts.mv === 1 ? '' : 's'} lost`}</h1>
    <div class="alines">${lines}</div>
    <ul class="afacts tc-muted">${facts.map((f) => `<li>${esc(f)}</li>`).join('')}</ul>
    <p class="averdict" data-verdict>The staff are fighting the same night ${STAFF_NIGHTS} times to see how yours compares…</p>
    <div class="abtns">
      <button class="tc-btn" data-again>Same night again</button>
      <button class="tc-btn" data-new>A new night</button>
      <button class="tc-btn tc-btn--primary" data-choose>Choose another</button>
    </div>
  </section></div>`;
  app.querySelector('[data-again]')!.addEventListener('click', () => begin(S.seed));
  app.querySelector('[data-new]')!.addEventListener('click', () => begin(Math.floor(Math.random() * 1e9)));
  app.querySelector('[data-choose]')!.addEventListener('click', choose);
  setTimeout(() => {
    const staff = S.staff ?? staffNights(t, STAFF_NIGHTS);
    const ranks = ranksOf(t, x.counts, staff);
    const def = verdictOf(ranks).def;
    const share = Math.round(100 * (S.side === 'def' ? def : 1 - def));
    const el = app.querySelector('[data-verdict]');
    if (el) el.innerHTML = `<b>${S.side === 'def' ? 'Your escorts' : 'Your pack'} did better than ${share} in 100 of the staff's own nights on this card.</b> In a campaign the night would be counted at that place among them: boats and escorts lost, and the merchants' tonnage.`;
  }, 50);
}

// ---- The sea ----------------------------------------------------------------------------------------------------------

function wireCanvas(): void {
  const c = canvas!;
  c.addEventListener('contextmenu', (e) => e.preventDefault());
  c.addEventListener('mousedown', (e) => {
    const [x, y] = local(e);
    if (e.button === 0) {
      if (S.mode) { modeAt(x, y); return; }
      S.drag = { x0: x, y0: y, x1: x, y1: y };
    }
  });
  c.addEventListener('mousemove', (e) => { if (S.drag) { const [x, y] = local(e); S.drag.x1 = x; S.drag.y1 = y; } });
  c.addEventListener('mouseup', (e) => {
    const [x, y] = local(e);
    if (e.button === 2) { rightAt(x, y); return; }
    if (e.button !== 0 || !S.drag) return;
    const d = S.drag; S.drag = null;
    if (!e.shiftKey) S.picked.clear();
    if (Math.abs(d.x1 - d.x0) + Math.abs(d.y1 - d.y0) < 6) { const u = nearest(x, y, own(), 16); if (u) S.picked.add(u.id); }
    else {
      const [ax, bx] = [Math.min(d.x0, d.x1), Math.max(d.x0, d.x1)], [ay, by] = [Math.min(d.y0, d.y1), Math.max(d.y0, d.y1)];
      for (const u of own()) { const [sx, sy] = toScreen(u.x, u.y); if (sx >= ax && sx <= bx && sy >= ay && sy <= by) S.picked.add(u.id); }
    }
    paintOrders();
  });
  c.addEventListener('wheel', (e) => { e.preventDefault(); S.cam.scale = Math.max(0.008, Math.min(1.2, S.cam.scale * (e.deltaY > 0 ? 0.85 : 1.18))); }, { passive: false });
}

function local(e: MouseEvent): [number, number] { const r = canvas!.getBoundingClientRect(); return [e.clientX - r.left, e.clientY - r.top]; }

function toScreen(x: number, y: number): [number, number] {
  const c = canvas!;
  return [c.clientWidth / 2 + (x - S.cam.x) * S.cam.scale, c.clientHeight / 2 - (y - S.cam.y) * S.cam.scale];
}
function toSea(sx: number, sy: number): [number, number] {
  const c = canvas!;
  return [S.cam.x + (sx - c.clientWidth / 2) / S.cam.scale, S.cam.y - (sy - c.clientHeight / 2) / S.cam.scale];
}

function nearest(sx: number, sy: number, us: ConvoyUnit[], within: number): ConvoyUnit | null {
  let best: ConvoyUnit | null = null, bd = within;
  for (const u of us) { const [x, y] = toScreen(u.x, u.y); const d = Math.hypot(x - sx, y - sy); if (d < bd) { bd = d; best = u; } }
  return best;
}

function rightAt(sx: number, sy: number): void {
  const ps = picked();
  if (S.mode) { modeAt(sx, sy); return; }
  if (!ps.length) return;
  const ids = ps.map((u) => u.id);
  if (S.side === 'def') {
    const boat = nearest(sx, sy, S.units.filter((u) => u.kind === 'ss' && u.state !== 'gone'), 18);
    if (boat) { order({ do: 'convoy:hunt', units: ids, target: boat.id }); return; }
    const wreck = nearest(sx, sy, S.units.filter((u) => u.state === 'gone' && u.kind !== 'ss'), 18);
    if (wreck) { order({ do: 'convoy:rescue', units: ids, target: wreck.id }); return; }
    const [x, y] = toSea(sx, sy);
    order({ do: 'move', units: ids, x, y });
  } else {
    const ship = nearest(sx, sy, S.units.filter((u) => u.side === 'def' && u.state !== 'gone'), 18);
    if (ship) order({ do: 'convoy:attack', units: ids, target: ship.id });
  }
}

function modeAt(sx: number, sy: number): void {
  const ids = picked().map((u) => u.id);
  const m = S.mode; S.mode = null;
  if (!ids.length) return;
  if (m === 'starshell') { const [x, y] = toSea(sx, sy); order({ do: 'convoy:starshell', units: ids, x, y }); }
  if (m === 'gun') { const ship = nearest(sx, sy, S.units.filter((u) => u.side === 'def' && u.state !== 'gone'), 18); if (ship) order({ do: 'convoy:gun', units: ids, target: ship.id }); }
}

function draw(): void {
  const c = canvas!, s = S.sim!, t = S.t!;
  const dpr = window.devicePixelRatio || 1;
  if (c.width !== Math.round(c.clientWidth * dpr) || c.height !== Math.round(c.clientHeight * dpr)) { c.width = Math.round(c.clientWidth * dpr); c.height = Math.round(c.clientHeight * dpr); }
  const g = c.getContext('2d')!;
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  const W = c.clientWidth, H = c.clientHeight, now = s.t;
  const mv = S.units.filter((u) => u.kind === 'mv' && u.state !== 'gone');
  if (S.cam.follow && mv.length) { S.cam.x = mv.reduce((a, u) => a + u.x, 0) / mv.length; S.cam.y = mv.reduce((a, u) => a + u.y, 0) / mv.length; }
  // The night sea: darker under no moon.
  const lit = t.place.light.moon.lit * (t.place.light.moon.up > 0 ? 1 : 0);
  g.fillStyle = `rgb(${8 + lit * 10}, ${14 + lit * 16}, ${24 + lit * 22})`;
  g.fillRect(0, 0, W, H);
  // A kilometre grid, faint.
  g.strokeStyle = 'rgba(143,176,200,0.06)'; g.lineWidth = 1;
  const step = S.cam.scale > 0.08 ? 1000 : 5000;
  const [x0, y1] = toSea(0, 0), [x1, y0] = toSea(W, H);
  for (let x = Math.floor(x0 / step) * step; x <= x1; x += step) { const [sx] = toScreen(x, 0); g.beginPath(); g.moveTo(sx, 0); g.lineTo(sx, H); g.stroke(); }
  for (let y = Math.floor(y0 / step) * step; y <= y1; y += step) { const [, sy] = toScreen(0, y); g.beginPath(); g.moveTo(0, sy); g.lineTo(W, sy); g.stroke(); }
  // Star shell and patterns' zones.
  for (const z of S.zones) {
    const [sx, sy] = toScreen(z.x, z.y), r = Math.max(4, z.r * S.cam.scale);
    if (z.kind === 'starshell') {
      const grd = g.createRadialGradient(sx, sy, 0, sx, sy, r * 1.6);
      grd.addColorStop(0, 'rgba(244,222,168,0.35)'); grd.addColorStop(1, 'rgba(244,222,168,0)');
      g.fillStyle = grd; g.beginPath(); g.arc(sx, sy, r * 1.6, 0, Math.PI * 2); g.fill();
    }
  }
  // Depth charges: a ring as they sink, a flash as they burst.
  S.charges = S.charges.filter((d) => now - d.burst < 40);
  for (const d of S.charges) {
    const [sx, sy] = toScreen(d.x, d.y), r = Math.max(3, d.r * S.cam.scale);
    if (now < d.burst) { g.strokeStyle = d.ahead ? 'rgba(240,166,58,0.8)' : 'rgba(224,184,114,0.7)'; g.setLineDash([3, 3]); g.beginPath(); g.arc(sx, sy, r, 0, Math.PI * 2); g.stroke(); g.setLineDash([]); }
    else { const k = 1 - (now - d.burst) / 40; g.fillStyle = `rgba(237,230,214,${0.5 * k})`; g.beginPath(); g.arc(sx, sy, r * (1 + (1 - k)), 0, Math.PI * 2); g.fill(); }
  }
  // Torpedo tracks.
  S.tracks = S.tracks.filter((k) => now - k.at < k.run + 20);
  for (const k of S.tracks) {
    const f = Math.min(1, (now - k.at) / Math.max(1, k.run));
    const [ax, ay] = toScreen(k.from[0], k.from[1]), [bx, by] = toScreen(k.to[0], k.to[1]);
    g.strokeStyle = k.faint ? 'rgba(237,230,214,0.25)' : 'rgba(237,230,214,0.6)'; g.lineWidth = 1;
    g.beginPath(); g.moveTo(ax, ay); g.lineTo(ax + (bx - ax) * f, ay + (by - ay) * f); g.stroke();
  }
  // Contacts: an escort's line to the boat it holds.
  for (const u of S.units) {
    const e = u.extra.echo;
    if (!e || u.state === 'gone') continue;
    const b = S.units.find((v) => v.id === e.on);
    if (!b) continue;
    const [ax, ay] = toScreen(u.x, u.y), [bx, by] = toScreen(b.x, b.y);
    g.strokeStyle = e.kind === 'asdic' ? 'rgba(156,192,138,0.7)' : e.kind === 'radar' ? 'rgba(143,176,200,0.7)' : 'rgba(237,230,214,0.6)';
    g.setLineDash(e.kind === 'asdic' ? [2, 3] : []); g.beginPath(); g.moveTo(ax, ay); g.lineTo(bx, by); g.stroke(); g.setLineDash([]);
  }
  // The pieces.
  for (const u of S.units) drawPiece(g, u);
  // The box being dragged.
  if (S.drag) { const d = S.drag; g.strokeStyle = 'rgba(224,184,114,0.8)'; g.strokeRect(Math.min(d.x0, d.x1), Math.min(d.y0, d.y1), Math.abs(d.x1 - d.x0), Math.abs(d.y1 - d.y0)); }
  // The tag.
  const tag = app.querySelector<HTMLElement>('[data-tag]');
  if (tag) {
    const show = S.mode ? (S.mode === 'starshell' ? 'CLICK WHERE TO FIRE STAR SHELL · ESC TO CANCEL' : 'CLICK A SHIP FOR THE DECK GUN · ESC TO CANCEL') : S.tag && S.tag.until > performance.now() ? S.tag.text : '';
    tag.textContent = show; tag.style.display = show ? 'block' : 'none';
  }
}

function drawPiece(g: CanvasRenderingContext2D, u: ConvoyUnit): void {
  const [sx, sy] = toScreen(u.x, u.y);
  const gone = u.state === 'gone';
  const on = S.picked.has(u.id);
  const len = Math.max(u.kind === 'mv' ? 7 : 9, u.d * S.cam.scale), beam = Math.max(3, u.w * S.cam.scale * 1.6);
  g.save();
  g.translate(sx, sy);
  g.rotate(u.face);
  if (u.kind === 'mv') {
    g.fillStyle = gone ? 'rgba(140,47,34,0.6)' : '#b7ad98';
    g.fillRect(-beam / 2, -len / 2, beam, len);
  } else if (u.kind === 'dd') {
    g.fillStyle = gone ? 'rgba(140,47,34,0.6)' : '#8fb0c8';
    g.beginPath(); g.moveTo(0, -len / 2 - 3); g.lineTo(beam / 2 + 1.5, len / 2); g.lineTo(-beam / 2 - 1.5, len / 2); g.closePath(); g.fill();
  } else {
    const dive = u.extra.dive;
    const col = u.side === S.side ? '#e0b872' : '#e08a6a';
    g.strokeStyle = gone ? 'rgba(140,47,34,0.6)' : col; g.fillStyle = gone ? 'rgba(140,47,34,0.6)' : col; g.lineWidth = 1.5;
    if (dive === 'deep' && !gone) { g.setLineDash([2, 2]); g.beginPath(); g.arc(0, 0, 5, 0, Math.PI * 2); g.stroke(); g.setLineDash([]); }
    else { g.beginPath(); g.moveTo(0, -6); g.lineTo(3.5, 5); g.lineTo(-3.5, 5); g.closePath(); if (dive === 'surfaced' || gone) g.fill(); else g.stroke(); }
  }
  g.restore();
  if (on) { g.strokeStyle = '#f4dea8'; g.lineWidth = 1; g.beginPath(); g.arc(sx, sy, Math.max(9, len / 2 + 4), 0, Math.PI * 2); g.stroke(); }
  if (u.extra.gun !== undefined && !gone) {
    const t = S.units.find((v) => v.id === u.extra.gun);
    if (t) { const [tx, ty] = toScreen(t.x, t.y); g.strokeStyle = 'rgba(240,166,58,0.5)'; g.beginPath(); g.moveTo(sx, sy); g.lineTo(tx, ty); g.stroke(); }
  }
}

// ---- The bar ----------------------------------------------------------------------------------------------------------

function paintClock(): void {
  app.querySelectorAll<HTMLElement>('[data-speed]').forEach((b) => b.classList.toggle('tc-btn--primary', SPEEDS[+b.dataset.speed!] === S.speed));
}

let barWas = '';
function paintBar(): void {
  const s = S.sim!, t = S.t!, L = t.place.light;
  const sunk = s.counts.mv, boats = S.units.filter((u) => u.kind === 'ss').length;
  const lostB = Object.values(s.counts.ss).reduce((a, b) => a + b, 0), lostE = Object.values(s.counts.dd).reduce((a, b) => a + b, 0);
  const facts = `${esc(t.place.seaName)} · ${esc(MONTHS[t.place.when.month - 1])} ${t.place.when.year} · dusk +${hhmm(s.t)} · dawn in ${hhmm(Math.max(0, s.limit - s.t))} · moon ${Math.round(L.moon.lit * 100)}%${L.moon.up > 0 ? '' : ' (down)'} · a boat on the surface seen at ${L.vis.toLocaleString('en-GB')} m · you are the ${S.side === 'def' ? `${PEOPLE[t.def.nat] ?? t.def.nat} escorts` : `${PEOPLE[t.att.nat] ?? t.att.nat} pack`}`;
  const score = `<span class="nw">Merchants sunk <b>${sunk}</b> of ${t.place.ships}</span> <span class="nw">Boats lost <b>${lostB}</b>${S.side === 'att' ? ` of ${boats}` : ''}</span> <span class="nw">Escorts lost <b>${lostE}</b></span>`;
  const log = S.said.slice(-6).map((m) => `<div class="${m.side === S.side ? 'is-mine' : 'is-theirs'}"><span class="tc-faint">${hhmm(m.t)}</span> ${esc(m.text)}</div>`).join('');
  const now = facts + score + log;
  if (now === barWas) return;
  barWas = now;
  // (A campaign's night, with Units decide and the route's count laid: the month's convoys on the route, the card's sighted line, and the pack.)
  const k = t.place.card?.count;
  app.querySelector('[data-facts]')!.innerHTML = k ? `${facts} · <span class="nw">of about ${Math.round(k.sailed)} convoys this month, ${Math.round(k.sighted)} sighted, ${Math.round(k.attacked)} attacked${k.pack > 0 ? `, by about ${k.pack < 10 ? k.pack.toFixed(1) : Math.round(k.pack)} boats each` : ''}${k.sunk !== undefined ? `, losing about ${k.sunk < 10 ? k.sunk.toFixed(1) : Math.round(k.sunk)} ships each` : ''}</span>` : facts;
  app.querySelector('[data-score]')!.innerHTML = score;
  app.querySelector('[data-log]')!.innerHTML = log;
  paintClock();
}

// ---- Keys -------------------------------------------------------------------------------------------------------------

function keys(e: KeyboardEvent): void {
  if (S.screen !== 'night') return;
  if (e.key === ' ') { e.preventDefault(); setSpeed(S.speed === 0 ? S.before || 10 : 0); }
  else if (e.key >= '1' && e.key <= '4') setSpeed(SPEEDS[+e.key]);
  else if (e.key === 'f' || e.key === 'F') S.cam.follow = true;
  else if (e.key === 'Escape') { S.mode = null; S.picked.clear(); paintOrders(); }
  else if (e.key === 'Tab') {
    e.preventDefault();
    const us = own();
    if (!us.length) return;
    const i = us.findIndex((u) => S.picked.has(u.id));
    const u = us[(i + 1) % us.length];
    S.picked = new Set([u.id]); S.cam.follow = false; S.cam.x = u.x; S.cam.y = u.y;
    paintOrders();
  } else if ((e.key === 's' || e.key === 'S') && S.side === 'def' && picked().length) S.mode = 'starshell';
  else if ((e.key === 'g' || e.key === 'G') && S.side === 'att' && picked().length) S.mode = 'gun';
  else if (e.key.startsWith('Arrow')) {
    S.cam.follow = false;
    const d = 120 / S.cam.scale;
    if (e.key === 'ArrowLeft') S.cam.x -= d; if (e.key === 'ArrowRight') S.cam.x += d; if (e.key === 'ArrowUp') S.cam.y += d; if (e.key === 'ArrowDown') S.cam.y -= d;
  }
}

export function bootConvoyPage(): void {
  app = document.getElementById('app')!;
  document.addEventListener('keydown', keys);
  const open = (window as unknown as { CONVOY_OPEN?: { practice?: number; side?: Side; seed?: number } }).CONVOY_OPEN;
  if (open?.practice !== undefined) { S.card = Math.max(0, Math.min(CONVOY_PRACTICE.length - 1, open.practice)); S.side = open.side ?? 'def'; begin(open.seed ?? 1); }
  else choose();
}

(window as unknown as { convoyState?: State }).convoyState = S;
bootConvoyPage();

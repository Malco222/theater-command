// The field battle page: choose one of the campaign's attacks (or a practice ground), fight it on the field, and see
// the campaign's own report of what it made of the result. The first slice of the played battles (ruling 217): a
// browser page of its own, with no press on the campaign screen until the play version ships.
//
// Mouse: left picks up (drag for a box, Shift adds), right orders (on the ground to march, on an enemy to attack, drag
// to draw a line). Keys: Space pause, 1 2 3 speed, H hold, R fall back (the divisions in hand; WITHDRAW is the whole
// army), Tab next, Esc puts down. A plain left click never gives an order, and Enter does nothing. The clock stops
// while a question stands on the field (the design director, b358).

import type { Game } from '../../campaign/types';
import { Clock, type Speed } from '../core/clock';
import { diceChance } from '../core/parity';
import { RATIO } from '../core/result';
import { lineUp, unitAt, unitsIn } from '../core/select';
import type { Command, FieldEvent, FieldTicket, Side } from '../core/types';
import type { LandPlace, Terrain } from '../land/field';
import { sampleTicket } from '../land/sample';
import { LandBattle } from '../land/sim';
import type { LandUnit } from '../land/units';
import { newCampaign, offers, settle, type Offer } from './campaign';
import { drawFrame, signSvg, toField, toScreen, type Scene } from './draw';
import { paintField, type Painted } from './paint';

const KEY_GUARD = 0.5;
/** Real seconds a dispatch stands, while the clock runs, before it fades. */
const DISPATCH_FOR = 8;
const esc = (s: string): string => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]!);
const num = (n: number): string => Math.round(n).toLocaleString('en-GB');
const mmss = (t: number): string => `${Math.floor(t / 60)}:${String(Math.floor(t % 60)).padStart(2, '0')}`;
const odds = (o: number): string => (o >= 10 ? o.toFixed(0) : o.toFixed(2));
/** The ground in the province card's own word (data/campaign/ground.json, 'says'). */
const TERRAIN_WORD: Record<Terrain, string> = { plains: 'Open country', forest: 'Forest', hills: 'Hill country', mountain: 'Mountains', marsh: 'Marsh', urban: 'City streets', desert: 'Open desert' };
/** Facts in a row that fold whole: the dot between two goes with the fold, never ending or starting a line (brief check 19). */
const facts = (bits: string[]): string => `<span class="facts"><span>${bits.map((b) => `<span class="fact">${b}</span>`).join('')}</span></span>`;
/** "Warsaw, the capital"; "the town of Gdynia". */
const townWords = (p: LandPlace): string => (p.townSize === 'capital' ? `${esc(p.town!)}, the capital` : `the ${p.townSize ?? 'town'} of ${esc(p.town!)}`);

interface State {
  g: Game;
  screen: 'choose' | 'battle' | 'after';
  offer: Offer | null;
  b: LandBattle | null;
  field: Painted | null;
  clock: Clock;
  scene: Scene;
  guardUntil: number;
  dispatch: FieldEvent | null;
  /** Real seconds the dispatch line has stood while the clock ran: it fades after a while so it never reads as stale. */
  dispatchAge: number;
  /** The question WITHDRAW asks, open in place. */
  asking: boolean;
  /** The speed the clock had when a question stopped it: STAY or Esc starts it again there. */
  resume: Speed | null;
  last: number;
  trayAt: number;
}

const app = document.getElementById('app')!;
const S: State = {
  g: newCampaign(), screen: 'choose', offer: null, b: null, field: null, clock: new Clock(),
  scene: { cam: { x: 1500, y: 1000, z: 0.3 }, W: 800, H: 600, me: 'att', sel: new Set(), hover: null, box: null, line: null, now: 0, ping: null },
  guardUntil: 0, dispatch: null, dispatchAge: 0, asking: false, resume: null, last: 0, trayAt: 0,
};
const now = (): number => performance.now() / 1000;
const guard = (): void => { S.guardUntil = now() + KEY_GUARD; };

// ---- Choosing a battle --------------------------------------------------------------------------------------------

function showChooser(): void {
  S.screen = 'choose'; S.b = null; S.offer = null;
  guard();
  const g = S.g;
  const list = offers(g);
  const me = g.nations[g.human!];
  const row = (o: Offer, i: number): string => {
    const t = o.ticket, p = t.place;
    const att = t.att.forces.length, def = t.def.forces.length;
    const bits = [TERRAIN_WORD[p.terrain], p.river ? `across the ${esc(p.river)}${p.bridgeDown ? ', bridges down' : ''}` : '', p.fort ? `fortress works, level ${p.fort}` : '', p.town ? townWords(p) : ''].filter(Boolean);
    const chance = diceChance(o.odds, t.reckoning.dice);
    const staff = chance >= 0.995 ? 'the staff is sure of it' : chance <= 0.005 ? 'the staff gives it no chance' : `the staff gives it about ${Math.round(chance * 100)}%`;
    return `<button class="tc-card is-press offer" data-offer="${i}">
      <span class="otop"><b class="tc-title-3">${esc(o.target.name)}</b><span class="tc-data"><b>${odds(o.odds)}</b> to 1</span></span>
      <span class="tc-data omid">${facts(bits)}</span>
      <span class="tc-data olow">${facts([`${att} corps against ${def}`, staff])}</span></button>`;
  };
  const grounds: Terrain[] = ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert'];
  app.innerHTML = `<div class="choose tc">
    <header class="chead"><div><div class="tc-label">Field battles · first slice</div><h1 class="tc-title-1">Choose a battle to fight</h1></div></header>
    <div class="cbody">
      <section class="tc-paper intro"><p><b>The campaign settles every battle from its Battle Ticket until you choose to fight one here.</b> The field is drawn from the province: its ground, its river and bridges, its town and its fortress works. Each division is a block, and the two armies share out exactly the strength the attack card counted, so the card's odds are the field's. What happens here goes back to the campaign as the same kind of result, and you can hand the rest to the staff at any moment: what you have won stays won.</p></section>
      <h2 class="tc-label shead">${facts(['Your attacks this month', `${esc(me.name)}, ${esc(list[0]?.ticket.date ?? '')}`])}</h2>
      <div class="offers">${list.length ? list.map(row).join('') : '<p class="tc-data none">No attack can be ordered now. Start the campaign again to fight another.</p>'}</div>
      <div class="again"><button class="tc-btn tc-btn--quiet" data-act="restart">Start the campaign again</button></div>
      <h2 class="tc-label shead">${facts(['Practice grounds', 'Odds of about 1.3 to 1', 'Nothing goes back to the campaign'])}</h2>
      <div class="grounds">${grounds.map((k) => `<button class="tc-btn" data-ground="${k}">${TERRAIN_WORD[k]}</button>`).join('')}<button class="tc-btn" data-ground="plains" data-river="1">A river crossing</button><button class="tc-btn" data-ground="hills" data-fort="2">Fortress works</button></div>
    </div></div>`;
  app.querySelectorAll<HTMLElement>('[data-offer]').forEach((el) => el.addEventListener('click', () => {
    const o = list[Number(el.dataset.offer)];
    startBattle(o.ticket, o);
  }));
  app.querySelectorAll<HTMLElement>('[data-ground]').forEach((el) => el.addEventListener('click', () => {
    const t = sampleTicket(el.dataset.ground as Terrain, 1.3, { humanAtt: true, river: !!el.dataset.river, fort: Number(el.dataset.fort ?? 0), seed: 1 + Math.floor(Math.random() * 1e6) });
    startBattle(t, null);
  }));
  app.querySelector('[data-act="restart"]')?.addEventListener('click', () => { S.g = newCampaign(S.g.seed + 1); showChooser(); });
}

// ---- The battle screen --------------------------------------------------------------------------------------------

let canvas: HTMLCanvasElement;
let ctx: CanvasRenderingContext2D;

function startBattle(t: FieldTicket<LandPlace>, o: Offer | null): void {
  const b = new LandBattle(t);
  S.b = b; S.offer = o; S.screen = 'battle';
  S.field = paintField(b.field, t.place.terrain, t.seed);
  S.scene.me = t.att.human ? 'att' : 'def';
  S.scene.sel = new Set(); S.scene.hover = null; S.scene.box = null; S.scene.line = null; S.scene.ping = null;
  // The field opens paused: orders first, then the clock.
  S.clock = new Clock(); S.clock.set(1); S.clock.toggle();
  S.asking = false;
  const place = t.place.town ? `before ${t.place.town}` : `in ${t.place.name}`;
  S.dispatch = { t: 0, what: 'contact', text: `Your army stands ${place}: give your orders, then press Space to begin.` };
  S.dispatchAge = 0;
  S.resume = null;
  guard();
  const mine = t[S.scene.me], theirs = t[S.scene.me === 'att' ? 'def' : 'att'];
  app.innerHTML = `<div class="battle tc">
    <header class="bbar">
      <div class="btitle"><b class="tc-title-3">${esc(t.title)}</b><span class="tc-data">${facts([esc(t.date), esc(TERRAIN_WORD[t.place.terrain]), ...(t.place.river ? [`the ${esc(t.place.river)}`] : [])])}</span></div>
      <div class="bclock"><span class="tc-label">Battle time</span><span class="tc-data" id="time">0:00</span></div>
      <div class="bspeed" role="group" aria-label="Speed">
        <button class="tc-btn tc-btn--sm" data-speed="0" title="Pause (Space)">Pause <span class="tc-key">Space</span></button>
        <button class="tc-btn tc-btn--sm" data-speed="1" title="Normal speed (1)">×1 <span class="tc-key">1</span></button>
        <button class="tc-btn tc-btn--sm" data-speed="2" title="Twice as fast (2)">×2 <span class="tc-key">2</span></button>
        <button class="tc-btn tc-btn--sm" data-speed="4" title="Four times as fast (3)">×4 <span class="tc-key">3</span></button>
      </div>
      <div class="bbal" title="Each side's strength still standing. The pale mark is where the attack card put it.">
        <div class="bends"><span>${esc(mine.adj)}</span><span id="ratio" class="tc-data"></span><span>${esc(theirs.adj)}</span></div>
        <div class="btrack"><i id="balA" style="background:${t.att.color}"></i><i id="balD" style="background:${t.def.color}"></i><b id="balMark"></b></div>
      </div>
      <div class="bacts">
        <button class="tc-btn" data-act="dice" title="The staff settles the rest at once, at the strength each side has left, held to what the dice could have done.">Hand it to the staff</button>
        <button class="tc-btn tc-btn--grave" data-act="withdraw">Withdraw</button>
        <div class="wask tc-paper" id="wask" hidden role="alertdialog" aria-label="Withdraw the army?"><p>Withdraw the army? The battle ends and the field is theirs.</p><div class="wbtns"><button class="tc-btn" data-act="wgo">Withdraw</button><button class="tc-btn tc-btn--primary" data-act="wstay">Stay</button></div></div>
      </div>
    </header>
    <div class="bfield"><canvas id="field"></canvas>
      <div class="dispatch tc-paper" id="dispatch"></div>
      <div class="zoom"><button class="tc-btn tc-btn--sm" data-zoom="1" aria-label="Closer">+</button><button class="tc-btn tc-btn--sm" data-zoom="-1" aria-label="Further">−</button><button class="tc-btn tc-btn--sm" data-zoom="0" title="The whole field">Whole field</button></div>
    </div>
    <div class="keys tc-data">${facts(['Left click: pick up', 'Right click: march, or attack an enemy', 'Right drag: draw a line', '<span class="tc-key">H</span> hold', '<span class="tc-key">R</span> fall back', '<span class="tc-key">Tab</span> next', '<span class="tc-key">Esc</span> put down'])}</div>
    <footer class="tray" id="tray">${b.units.filter((u) => u.side === S.scene.me).map((u) => card(u, mine.color)).join('')}</footer>
  </div>`;
  canvas = document.getElementById('field') as HTMLCanvasElement;
  ctx = canvas.getContext('2d')!;
  fit();
  wireBattle();
  updateTop(true);
  updateTray(true);
  S.last = now();
}

function card(u: LandUnit, color: string): string {
  return `<button class="tc-card is-press ucard" data-unit="${u.id}" title="${esc(u.name)}">
    <span class="uhead">${signSvg(u.kind, color)}<span class="uname"><b>${esc(u.short)}</b><span>${esc(u.parent)}</span></span></span>
    <span class="tc-meter" data-m="str"><span class="tc-label">Strength</span><i><b></b></i><span></span></span>
    <span class="tc-meter is-rest" data-m="mor"><span class="tc-label">Morale</span><i><b></b></i><span></span></span>
    <span class="ustate tc-data"></span></button>`;
}

const STATE_WORD: Record<string, string> = { steady: 'Steady', wavering: 'Wavering', routing: 'Routing', gone: 'Fled the field', withdrawn: 'Withdrawn' };
const ACT_WORD: Record<string, string> = { idle: 'standing', march: 'marching', run: 'at the double', turn: 'wheeling', fight: 'in action', rout: '', down: '' };

function updateTray(force = false): void {
  const b = S.b!;
  const t = now();
  if (!force && t - S.trayAt < 0.15) return;
  S.trayAt = t;
  for (const el of document.querySelectorAll<HTMLElement>('#tray [data-unit]')) {
    const u = b.unit(Number(el.dataset.unit));
    if (!u) continue;
    const gone = u.state === 'gone' || u.state === 'withdrawn';
    el.classList.toggle('is-on', S.scene.sel.has(u.id));
    el.classList.toggle('is-off', gone);
    const set = (m: string, v: number, cls: string): void => {
      const box = el.querySelector<HTMLElement>(`[data-m="${m}"]`)!;
      box.className = `tc-meter ${cls}`;
      (box.querySelector('i b') as HTMLElement).style.width = `${Math.round(Math.max(0, Math.min(1, v)) * 100)}%`;
      (box.querySelector('span:last-child') as HTMLElement).textContent = `${Math.round(v * 100)}`;
    };
    set('str', u.str, u.str / u.str0 < 0.35 ? 'is-low' : u.str / u.str0 < 0.65 ? 'is-mid' : '');
    set('mor', u.mor, 'is-rest');
    // Steady, Wavering, Routing in words; only Routing in red (the UI director's must-haves, 3).
    const st = el.querySelector<HTMLElement>('.ustate')!;
    // Once the battle is over nothing marches: the card says so (the design director, b356).
    const doing = b.over ? (live(u) ? 'battle over' : '') : live(u) ? ACT_WORD[u.act] ?? '' : '';
    st.innerHTML = `<b>${STATE_WORD[u.state]}</b>${doing ? ` · ${doing}` : ''}`;
    st.className = `ustate tc-data${u.state === 'routing' ? ' bad' : u.state === 'wavering' ? ' warn' : ''}`;
  }
}

function updateTop(force = false): void {
  const b = S.b!;
  const el = (id: string): HTMLElement => document.getElementById(id)!;
  el('time').textContent = `${mmss(b.t)} of ${mmss(b.ticket.limit)}`;
  const bal = b.standing();
  const share = bal.att / Math.max(1e-6, bal.att + bal.def);
  el('balA').style.width = `${(share * 100).toFixed(1)}%`;
  el('balD').style.width = `${((1 - share) * 100).toFixed(1)}%`;
  const k = b.ticket.reckoning;
  el('balMark').style.left = `${((100 * k.odds) / (1 + k.odds)).toFixed(1)}%`;
  // Both shares are of the same whole, so their ratio is the odds standing now (the card's at the start).
  el('ratio').textContent = `Card ${odds(k.odds)} to 1 · now ${odds(bal.att / Math.max(1e-6, bal.def))} to 1`;
  const sp = S.clock.speed;
  document.querySelectorAll<HTMLElement>('[data-speed]').forEach((x) => x.classList.toggle('is-lit', Number(x.dataset.speed) === sp));
  (document.getElementById('wask') as HTMLElement).hidden = !S.asking;
  if (force || S.dispatch) {
    const d = S.dispatch;
    const box = el('dispatch');
    const html = d ? `<span class="tc-label">${mmss(d.t)}</span><span>${esc(d.text)}</span>` : '';
    if (box.innerHTML !== html) box.innerHTML = html;
    // While the clock runs a dispatch fades a few seconds after it comes; while paused it stays (the UI director, 8a).
    box.classList.toggle('is-old', !!d && S.dispatchAge > DISPATCH_FOR);
  }
}

function fit(): void {
  const r = canvas.getBoundingClientRect();
  const dpr = Math.min(2, window.devicePixelRatio || 1);
  canvas.width = Math.round(r.width * dpr); canvas.height = Math.round(r.height * dpr);
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  const first = S.scene.W !== r.width || S.scene.H !== r.height;
  S.scene.W = r.width; S.scene.H = r.height;
  if (first) whole();
}
function whole(): void {
  const b = S.b!;
  S.scene.cam = { x: b.field.w / 2, y: b.field.h / 2, z: Math.min(S.scene.W / b.field.w, (S.scene.H - 70) / b.field.h) * 0.97 };
}
function minZoom(): number { const b = S.b!; return Math.min(S.scene.W / b.field.w, S.scene.H / b.field.h) * 0.8; }
function zoomAt(px: number, py: number, k: number): void {
  const c = S.scene.cam;
  const [fx, fy] = toField(S.scene, px, py);
  c.z = Math.max(minZoom(), Math.min(3, c.z * k));
  // Keep the point under the pointer where it was.
  const [nx, ny] = toField(S.scene, px, py);
  c.x += fx - nx; c.y += fy - ny;
  clampCam();
}
/** The view stays on the field: once zoomed in past its edges, no dark margin shows; zoomed out, it stays centred. */
function clampCam(): void {
  const b = S.b!, c = S.scene.cam;
  const hw = S.scene.W / 2 / c.z, hh = S.scene.H / 2 / c.z;
  c.x = hw * 2 >= b.field.w ? b.field.w / 2 : Math.max(hw, Math.min(b.field.w - hw, c.x));
  c.y = hh * 2 >= b.field.h ? b.field.h / 2 : Math.max(hh, Math.min(b.field.h - hh, c.y));
}

// ---- Orders -------------------------------------------------------------------------------------------------------

const live = (u: LandUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';
const orderable = (u: LandUnit): boolean => live(u) && u.state !== 'routing' && u.kind !== 'fort';

function chosen(): number[] {
  const b = S.b!;
  return [...S.scene.sel].filter((id) => { const u = b.unit(id); return !!u && orderable(u); });
}
function give(c: Command): boolean {
  const b = S.b!;
  const ok = b.command(c, S.scene.me);
  return ok;
}

function wireBattle(): void {
  const sc = S.scene;
  let down: { x: number; y: number; button: number; at: number; touch: boolean; moved: boolean; px: number; py: number } | null = null;
  let hold: number | undefined;
  const pos = (e: PointerEvent | WheelEvent | MouseEvent): [number, number] => { const r = canvas.getBoundingClientRect(); return [e.clientX - r.left, e.clientY - r.top]; };

  canvas.addEventListener('contextmenu', (e) => e.preventDefault());
  canvas.addEventListener('pointerdown', (e) => {
    if (S.screen !== 'battle') return;
    const [px, py] = pos(e);
    canvas.setPointerCapture(e.pointerId);
    const touch = e.pointerType === 'touch';
    down = { x: px, y: py, button: e.button, at: now(), touch, moved: false, px, py };
    // On a touch screen, press and hold orders, as on the campaign map.
    if (touch) hold = window.setTimeout(() => { if (down && !down.moved) { orderAt(down.x, down.y); down = null; } }, 450);
  });
  canvas.addEventListener('pointermove', (e) => {
    const [px, py] = pos(e);
    const b = S.b;
    if (!b) return;
    const [fx, fy] = toField(sc, px, py);
    if (!down) {
      const u = unitAt(b.units, fx, fy, Math.max(85, 16 / sc.cam.z), sc.me);
      sc.hover = u ? u.id : null;
      canvas.style.cursor = u && u.side !== sc.me && sc.sel.size ? 'crosshair' : u && u.side === sc.me ? 'pointer' : 'default';
      return;
    }
    if (Math.hypot(px - down.x, py - down.y) > 6) down.moved = true;
    if (!down.moved) return;
    if (down.touch || down.button === 1) {
      // Drag the map.
      sc.cam.x -= (px - down.px) / sc.cam.z; sc.cam.y += (py - down.py) / sc.cam.z; clampCam();
    } else if (down.button === 0) {
      const [ax, ay] = toField(sc, down.x, down.y);
      sc.box = [ax, ay, fx, fy];
    } else if (down.button === 2) {
      const ids = chosen();
      if (ids.length) {
        const [ax, ay] = toField(sc, down.x, down.y);
        sc.line = { x0: ax, y0: ay, x1: fx, y1: fy, ghosts: lineUp(ids.map((i) => b.unit(i)!), sc.me, ax, ay, fx, fy, 150) };
      }
    }
    down.px = px; down.py = py;
  });
  const up = (e: PointerEvent): void => {
    window.clearTimeout(hold);
    const b = S.b;
    if (!down || !b) { down = null; return; }
    const [px, py] = pos(e);
    const d = down;
    down = null;
    if (d.touch) {
      if (!d.moved) pickAt(px, py, false);
      return;
    }
    if (d.button === 0) {
      if (sc.box) {
        const [x0, y0, x1, y1] = sc.box;
        const got = unitsIn(b.units, sc.me, x0, y0, x1, y1).filter(orderable);
        if (!e.shiftKey) sc.sel.clear();
        for (const u of got) sc.sel.add(u.id);
        sc.box = null;
      } else pickAt(px, py, e.shiftKey);
    } else if (d.button === 2) {
      if (sc.line) {
        const ids = chosen();
        if (ids.length) give({ do: 'line', units: ids, x0: sc.line.x0, y0: sc.line.y0, x1: sc.line.x1, y1: sc.line.y1 });
        sc.line = null;
      } else orderAt(px, py);
    }
    updateTray(true);
  };
  canvas.addEventListener('pointerup', up);
  canvas.addEventListener('pointercancel', () => { down = null; sc.box = null; sc.line = null; });
  canvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    const [px, py] = pos(e);
    // A pinch on a trackpad, or a mouse wheel's notches, zoom; two fingers sliding on a trackpad move the map.
    const notched = e.deltaMode !== 0 || (e.deltaX === 0 && Math.abs(e.deltaY) >= 40 && Number.isInteger(e.deltaY));
    if (e.ctrlKey || notched) zoomAt(px, py, Math.exp(-e.deltaY * (e.ctrlKey ? 0.01 : 0.0015)));
    else { sc.cam.x += e.deltaX / sc.cam.z; sc.cam.y -= e.deltaY / sc.cam.z; clampCam(); }
  }, { passive: false });
  window.onresize = (): void => { if (S.screen !== 'choose') fit(); };

  app.querySelectorAll<HTMLElement>('[data-speed]').forEach((el) => el.addEventListener('click', () => { if (S.asking) return; S.clock.set(Number(el.dataset.speed) as Speed); updateTop(); }));
  app.querySelectorAll<HTMLElement>('[data-zoom]').forEach((el) => el.addEventListener('click', () => {
    const k = Number(el.dataset.zoom);
    if (k === 0) whole(); else zoomAt(sc.W / 2, sc.H / 2, k > 0 ? 1.4 : 1 / 1.4);
  }));
  app.querySelector('[data-act="dice"]')!.addEventListener('click', () => { give({ do: 'dice' }); });
  // WITHDRAW asks once, in place; STAY, the answer that loses nothing, carries the brass (and Esc).
  app.querySelector('[data-act="withdraw"]')!.addEventListener('click', () => { ask(); updateTop(); });
  app.querySelector('[data-act="wstay"]')!.addEventListener('click', () => { answered(); updateTop(); });
  app.querySelector('[data-act="wgo"]')!.addEventListener('click', () => { if (now() < S.guardUntil) return; S.asking = false; S.resume = null; give({ do: 'withdrawAll', side: sc.me }); });
  const tray = document.getElementById('tray')!;
  tray.addEventListener('click', (e) => {
    const el = (e.target as HTMLElement).closest<HTMLElement>('[data-unit]');
    if (!el) return;
    const id = Number(el.dataset.unit), u = S.b!.unit(id);
    if (!u || !orderable(u)) return;
    if (e.shiftKey) { if (sc.sel.has(id)) sc.sel.delete(id); else sc.sel.add(id); } else { sc.sel.clear(); sc.sel.add(id); }
    updateTray(true);
  });
  tray.addEventListener('dblclick', (e) => {
    const el = (e.target as HTMLElement).closest<HTMLElement>('[data-unit]');
    const u = el ? S.b!.unit(Number(el.dataset.unit)) : undefined;
    if (u) { sc.cam.x = u.x; sc.cam.y = u.y; sc.cam.z = Math.max(sc.cam.z, 0.8); clampCam(); }
  });
}

/** A question on the field stops the clock while it stands (b358): no one loses men while reading. */
function ask(): void {
  if (S.asking) return;
  S.asking = true;
  S.resume = S.clock.speed;
  S.clock.set(0);
  guard();
}
/** STAY or Esc: the question goes, and the clock starts again at the speed it had. */
function answered(): void {
  if (!S.asking) return;
  S.asking = false;
  if (S.resume !== null) S.clock.set(S.resume);
  S.resume = null;
}

function pickAt(px: number, py: number, add: boolean): void {
  const b = S.b!, sc = S.scene;
  const [fx, fy] = toField(sc, px, py);
  const u = unitAt(b.units.filter((x) => x.side === sc.me && orderable(x)), fx, fy, Math.max(85, 16 / sc.cam.z), sc.me);
  if (!u) { if (!add) sc.sel.clear(); return; }
  if (add) { if (sc.sel.has(u.id)) sc.sel.delete(u.id); else sc.sel.add(u.id); } else { sc.sel.clear(); sc.sel.add(u.id); }
}

function orderAt(px: number, py: number): void {
  const b = S.b!, sc = S.scene;
  const ids = chosen();
  if (!ids.length) return;
  const [fx, fy] = toField(sc, px, py);
  const foe = unitAt(b.units.filter((x) => x.side !== sc.me && live(x)), fx, fy, Math.max(85, 16 / sc.cam.z));
  if (foe) { if (give({ do: 'attack', units: ids, target: foe.id })) sc.ping = { x: foe.x, y: foe.y, at: now(), kind: 'attack' }; }
  else if (give({ do: 'move', units: ids, x: fx, y: fy })) sc.ping = { x: fx, y: fy, at: now(), kind: 'move' };
}

window.addEventListener('keydown', (e) => {
  if (e.key === 'Enter') { e.preventDefault(); return; }
  if (now() < S.guardUntil) { if (e.key === 'Tab' || e.key === ' ') e.preventDefault(); return; }
  if (S.screen !== 'battle' || !S.b) return;
  const sc = S.scene, b = S.b;
  const key = e.key.length === 1 ? e.key.toLowerCase() : e.key;
  const pan = 120 / sc.cam.z;
  switch (key) {
    case ' ': e.preventDefault(); if (!S.asking) S.clock.toggle(); break;
    case '1': if (!S.asking) S.clock.set(1); break;
    case '2': if (!S.asking) S.clock.set(2); break;
    case '3': if (!S.asking) S.clock.set(4); break;
    case 'h': give({ do: 'hold', units: chosen() }); break;
    case 'r': give({ do: 'withdraw', units: chosen() }); break;
    // Esc changes nothing: it closes the question if one is open, else drops the blocks in hand.
    case 'Escape': if (S.asking) answered(); else { sc.sel.clear(); sc.line = null; sc.box = null; } break;
    case 'Tab': {
      e.preventDefault();
      const mine = b.units.filter((u) => u.side === sc.me && orderable(u));
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

// ---- The loop -----------------------------------------------------------------------------------------------------

function frame(): void {
  requestAnimationFrame(frame);
  if (S.screen === 'choose' || !S.b || !S.field) return;
  const t = now();
  const dt = Math.min(0.25, t - S.last);
  S.last = t;
  S.scene.now = t;
  const b = S.b;
  S.clock.advance(b, dt);
  const v = b.view(S.scene.me);
  // The opening line says the clock is stopped: once it runs, the first fact replaces it.
  if (b.t > 0 && S.dispatch?.t === 0 && S.dispatch.what === 'contact' && !S.dispatch.unit) { S.dispatch = { t: b.t, what: 'contact', text: `The ${b.ticket[S.scene.me].adj} army is moving.`, unit: -1 }; S.dispatchAge = 0; }
  for (const e of v.events) if (e.text) { S.dispatch = e; S.dispatchAge = 0; }
  if (S.clock.speed > 0) S.dispatchAge += dt;
  for (const id of [...S.scene.sel]) { const u = b.unit(id); if (!u || !orderable(u)) S.scene.sel.delete(id); }
  drawFrame(ctx, b, S.scene, S.field);
  updateTop();
  updateTray();
  if (v.over && S.screen === 'battle') showAfter();
}

// ---- After the battle: the campaign's own report ------------------------------------------------------------------

const FATE: Record<string, string> = { '': 'held its ground', retreated: 'retreated', surrendered: 'surrendered', destroyed: 'destroyed', advanced: 'advanced', evacuated: 'evacuated by sea', yielded: 'gave ground under orders', sheltered: 'fell back into the town' };

/** How the day went against the chance the staff gave it, in the UI director's plain words (no dice on the player's page). */
function reckoned(carried: boolean, chance: number): string {
  if (chance >= 0.995) return carried ? 'As the staff expected: they were sure of it.' : 'Worse than the staff expected: they were sure of it.';
  if (chance <= 0.005) return carried ? 'Better than the staff expected: they gave it no chance.' : 'As the staff expected: they gave it no chance.';
  const p = `they gave you about ${Math.round(chance * 100)}% to take it.`;
  if (carried) return chance < 0.5 ? `You did better than the staff expected: ${p}` : `As the staff expected: ${p}`;
  return chance >= 0.5 ? `You did worse than the staff expected: ${p}` : `As the staff feared: ${p}`;
}

function showAfter(): void {
  const b = S.b!, r = b.over!;
  S.screen = 'after';
  guard();
  // The after card is the one voice: the dispatch strip steps aside (the UI director, 3).
  S.dispatch = null;
  document.getElementById('dispatch')!.innerHTML = '';
  updateTray(true);
  const t = b.ticket, k = t.reckoning;
  const g = S.g;
  const settled = S.offer ? settle(g, S.offer, r) : null;
  const rep = settled?.report ?? null;
  const won = rep ? rep.taken || !!rep.siege : r.winner === 'att';
  const level = rep ? rep.level : r.level;
  const chance = diceChance(k.odds, k.dice);
  const A = t.att.adj, D = t.def.adj;
  const fieldLines = r.lines.map((l) => `<li>${esc(l)}</li>`).join('');
  let told = '', price = '', sides = '';
  if (rep) {
    const place = rep.regionName;
    told = rep.siege ? `The country round ${esc(t.place.town ?? place)} was won, but the town holds out: ${esc(place)} is under siege.`
      : rep.gates ? `The army outside was beaten, and ${esc(t.place.town ?? place)} opened its gates. ${esc(place)} has fallen.`
        : rep.taken ? `${esc(place)} was taken.` : `The attack on ${esc(place)} was thrown back.`;
    const sum = (us: typeof rep.attackers): number => us.reduce((s, u) => s + u.men, 0);
    const aLost = Math.max(rep.attLost, rep.attackers.reduce((s, u) => s + u.lost, 0)), dLost = Math.max(rep.defLost, rep.defenders.reduce((s, u) => s + u.lost, 0));
    const pct = (l: number, m: number): string => (m > 0 ? `, ${Math.max(1, Math.round((100 * l) / m))}% of those sent in,` : '');
    price = `It cost you ${num(aLost)} men${pct(aLost, sum(rep.attackers))} against ${num(dLost)} of theirs${rep.prisoners ? `, and ${num(rep.prisoners)} were taken prisoner` : ''}.`;
    // As the campaign's own report says it: a corps with no fate of its own fell back if its side lost.
    const list = (us: typeof rep.attackers, won: boolean): string => us.map((u) => `<li><b>${esc(u.name)}</b> <span>${facts([`${u.divs} division${u.divs === 1 ? '' : 's'}`, `lost ${num(u.lost)}`, u.fate === '' && !won ? 'fell back' : FATE[u.fate] ?? u.fate])}</span></li>`).join('');
    sides = `<div class="aside2"><div><h3 class="tc-label">${esc(A)} attack</h3><ul>${list(rep.attackers, rep.taken)}</ul></div><div><h3 class="tc-label">${esc(D)} defence</h3><ul>${list(rep.defenders, !rep.taken)}</ul></div></div>`;
  }
  const paid = rep?.paid ? `<p class="paid">${esc(rep.paid.att)}</p>` : '';
  const veil = document.createElement('div');
  veil.className = 'veil';
  veil.innerHTML = `<div class="after tc-paper" role="dialog" aria-label="Battle result">
    <div class="tc-label kick">${rep ? 'Battle result' : 'Practice result'} · ${esc(t.date)}</div>
    <h2 class="tc-title-2">${esc(t.title)}</h2>
    <div class="verdict"><span class="tc-stamp ${won ? 'tc-stamp--won' : ''}">${esc(level)}</span></div>
    ${rep ? `<p class="told"><b>${told}</b> ${price}</p>${paid}` : ''}
    <h3 class="tc-label">On the field</h3><ul class="flines">${fieldLines}</ul>
    <h3 class="tc-label">Against the staff's reckoning</h3>
    <p class="dice">${esc(reckoned(won, chance))}${rep ? '' : ' Nothing goes back to the campaign from a practice ground.'}</p>
    ${settled?.drift ? '<p class="warn">The campaign moved while the battle was fought: its odds are not the card\'s any more.</p>' : ''}
    ${sides}
    <div class="afoot"><button class="tc-btn tc-btn--primary tc-btn--lg" data-act="continue">Continue</button></div>
  </div>`;
  document.body.appendChild(veil);
  veil.querySelector('[data-act="continue"]')!.addEventListener('click', () => { if (now() < S.guardUntil) return; veil.remove(); showChooser(); });
}

// A handle for the page's own checks (screenshots, the playtester's harness). Not used by the page itself.
(window as unknown as { __field: unknown }).__field = { state: S, give, showChooser };

showChooser();
requestAnimationFrame(frame);

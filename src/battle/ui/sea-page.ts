// The fleet action page: choose an action (one the campaign is about to fight, or a practice one), fight it on open
// water, and see what the campaign makes of the result. The sea's first slice of the played battles (ruling 217): a
// browser page of its own, beside the land page, with no press on the campaign screen until the play version ships.
//
// Mouse: left picks (drag for a box, Shift adds), right orders (on the water to steer for it, on an enemy to engage it
// or, for a carrier, to strike it; drag to draw a line). Keys: Space pause, 1 2 3 speed, H hold, R the chosen ships break
// off, Tab next piece, Esc drops the choice. A plain left click never gives an order, and Enter does nothing.
// The words are the UI director's and the design director's (reviews of 17 November, logs b368 and b370).

import { createGame } from '../../campaign/game';
import { sail, seaBattles } from '../../campaign/navy';
import { beginHuman, humanUp, stepAI } from '../../campaign/turn';
import type { Game, SeaReport } from '../../campaign/types';
import { Clock, type Speed } from '../core/clock';
import { diceChance } from '../core/parity';
import { lineUp, unitAt, unitsIn } from '../core/select';
import type { Command, FieldEvent, Side } from '../core/types';
import { closeWith, foughtOf, seaAhead, type SeaAhead } from '../sea/handoff';
import { SEA } from '../sea/numbers';
import { SeaSim, type SeaLosses } from '../sea/sim';
import { plainTicket, type PlainFleet } from '../sea/ticket';
import type { SeaClass, SeaShips, SeaSpeed, SeaTicket, SeaUnit } from '../sea/types';
import { toField, toScreen } from './draw';
import { drawSea, live, shipSvg, sideColours, takeEvents, type SeaScene } from './sea-draw';

const KEY_GUARD = 0.5;
const esc = (s: string): string => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]!);
const mmss = (t: number): string => `${Math.floor(t / 60)}:${String(Math.floor(t % 60)).padStart(2, '0')}`;
const odds = (o: number): string => (o >= 10 ? o.toFixed(0) : o.toFixed(2));
const chanceOf = (t: SeaTicket): number => diceChance(t.reckoning.odds, t.reckoning.dice, t.reckoning.even);
/** Facts in a row, each kept whole, as on the land page (battle.css .facts). */
const facts = (bits: string[]): string => `<span class="facts"><span>${bits.map((b) => `<span class="fact">${b}</span>`).join('')}</span></span>`;
/** A list as English says it: "a, b and c". */
const andList = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const CLASS_ONE: Record<SeaClass | 'ss', [string, string]> = { bb: ['battleship', 'battleships'], cv: ['carrier', 'carriers'], ca: ['cruiser', 'cruisers'], dd: ['destroyer', 'destroyers'], ss: ['submarine', 'submarines'] };
/** Ships by kind, a kind at 0 left out: ["1 battleship", "2 cruisers"]. */
const shipBits = (s: Partial<Record<SeaClass | 'ss', number>>): string[] =>
  (['bb', 'cv', 'ca', 'dd', 'ss'] as const).filter((c) => (s[c] ?? 0) > 0).map((c) => `${s[c]} ${CLASS_ONE[c][s[c] === 1 ? 0 : 1]}`);
/** A number never parts from its noun (the UI director's fold rule). */
const shipsKept = (s: Partial<Record<SeaClass, number>>): string => shipBits(s).map((b) => `<span class="nw">${esc(b)}</span>`).join(', ');
/** A nation's people, for "you had lost 3 destroyers and the Germans 8". */
const PEOPLE: Record<string, string> = { British: 'the British', French: 'the French', 'Vichy French': 'the Vichy French', Dutch: 'the Dutch', Irish: 'the Irish', Swiss: 'the Swiss', Polish: 'the Poles', Danish: 'the Danes', Swedish: 'the Swedes', Finnish: 'the Finns', Turkish: 'the Turks', Spanish: 'the Spanish', Enemy: 'the enemy', Your: 'you' };
const people = (adj: string): string => PEOPLE[adj] ?? `the ${adj}s`;
const DISPATCH_FOR = 8;

/** An action the campaign is about to fight: the game it comes from, and the look-ahead's meeting and ticket. */
interface FromCampaign { g: Game; ahead: SeaAhead; close: (g: Game) => void }

interface State {
  screen: 'choose' | 'battle' | 'after';
  sim: SeaSim | null;
  from: FromCampaign | null;
  clock: Clock;
  scene: SeaScene;
  guardUntil: number;
  dispatch: FieldEvent | null;
  dispatchAge: number;
  asking: boolean;
  last: number;
  trayAt: number;
}

const app = document.getElementById('app')!;
const S: State = {
  screen: 'choose', sim: null, from: null, clock: new Clock(),
  scene: { cam: { x: 10000, y: 10000, z: 0.04 }, W: 800, H: 600, me: 'att', sel: new Set(), hover: null, box: null, line: null, now: 0, ping: null, marks: [], inbound: [] },
  guardUntil: 0, dispatch: null, dispatchAge: 0, asking: false, last: 0, trayAt: 0,
};
const now = (): number => performance.now() / 1000;
const guard = (): void => { S.guardUntil = now() + KEY_GUARD; };

// ---- The actions on offer --------------------------------------------------------------------------------------------

/** The campaign's own month with the British and German battle fleets at sea together: the action it is about to fight. */
function campaignAction(seed: number): FromCampaign | null {
  const g = createGame(seed, 'UK');
  for (let i = 0; i < 80 && !humanUp(g); i++) stepAI(g);
  beginHuman(g);
  const home = g.fleets.find((f) => f.name === 'Home Fleet'), battle = g.fleets.find((f) => f.name === 'Battle Fleet');
  if (!home || !battle || !sail(g, home, home.zone, 'patrol') || !sail(g, battle, home.zone, 'patrol')) return null;
  const close = (c: Game): void => { seaBattles(c, 'UK'); };
  const [ahead] = seaAhead(g, close);
  return ahead ? { g, ahead, close } : null;
}

interface Practice { name: string; about: string; sea: string; att: PlainFleet[]; def: PlainFleet[]; odds: number; air?: Partial<Record<Side, number>> }
// Nations and fleets the campaign has, in seas it names, so a practice action reads as the campaign's own would.
const PRACTICE: Practice[] = [
  { name: 'Two battle lines', about: 'Battleships, cruisers and destroyers on both sides, at even odds', sea: 'North Sea', odds: 1,
    att: [{ name: 'Home Fleet', nat: 'UK', ships: { bb: 3, ca: 4, dd: 8 }, admiral: 1 }], def: [{ name: 'Battle Fleet', nat: 'GER', ships: { bb: 2, ca: 4, dd: 12 }, admiral: 1 }] },
  { name: 'Carriers against a battle line', about: 'Your carriers strike from behind their screen; theirs is all guns', sea: 'Ionian Sea', odds: 1.1,
    att: [{ name: 'Mediterranean Fleet', nat: 'UK', ships: { cv: 2, ca: 3, dd: 8 } }], def: [{ name: '1st Fleet', nat: 'ITA', ships: { bb: 3, ca: 2, dd: 8 } }] },
  { name: 'Under the enemy\'s aircraft', about: 'Their coast is near and its aircraft raid your ships all through the action', sea: 'Tyrrhenian Sea', odds: 1.15, air: { def: 15 },
    att: [{ name: 'Mediterranean Fleet', nat: 'UK', ships: { bb: 3, cv: 1, ca: 5, dd: 12 } }], def: [{ name: '2nd Fleet', nat: 'ITA', ships: { bb: 4, ca: 6, dd: 14 } }] },
  { name: 'Outgunned', about: 'They are the stronger: break off well, or make them pay', sea: 'North Sea', odds: 0.85,
    att: [{ name: 'Home Fleet', nat: 'UK', ships: { bb: 2, ca: 4, dd: 10 } }], def: [{ name: 'Battle Fleet', nat: 'GER', ships: { bb: 3, ca: 4, dd: 12 } }] },
];

/** The campaign's own names and colours for the practice actions' nations. A nation it lacks reads as "Enemy", never by its code. */
let nations: Game['nations'] | null = null;
function practiceTicket(p: Practice): SeaTicket {
  nations ??= createGame(1, null).nations;
  const t = plainTicket({ att: p.att, def: p.def, seed: 1 + Math.floor(Math.random() * 1e6), odds: p.odds, air: p.air, human: 'att', zoneName: p.sea });
  for (const side of ['att', 'def'] as Side[]) {
    const n = nations[t[side].nat];
    if (n) { t[side].adj = n.adj; t[side].color = n.color; } else t[side].adj = side === 'att' ? 'Your' : 'Enemy';
  }
  t.date = 'Practice'; // Not a month of the war: these fleets never met like this.
  return t;
}

function showChooser(): void {
  S.screen = 'choose'; S.sim = null; S.from = null;
  guard();
  const from = campaignAction(7);
  const t = from?.ahead.ticket;
  const staff = (c: number): string => (c >= 0.995 ? 'the staff is sure of it' : c <= 0.005 ? 'the staff gives it no chance' : `the staff gives it about ${Math.round(c * 100)}%`);
  /** Each side's ships on a line of its own. */
  const sideShips = (fleets: { side: Side; ships: Partial<SeaShips> }[], side: Side): Partial<SeaShips> => {
    const n: SeaShips = { bb: 0, cv: 0, ca: 0, dd: 0 };
    for (const f of fleets) if (f.side === side) for (const c of ['bb', 'cv', 'ca', 'dd'] as SeaClass[]) n[c] += f.ships[c] ?? 0;
    return n;
  };
  const names = (fleets: { side: Side; name: string }[], side: Side): string => esc(andList(fleets.filter((f) => f.side === side).map((f) => f.name)));
  app.innerHTML = `<div class="choose tc sea">
    <header class="chead"><div><div class="tc-label">Fleet actions · first slice</div><h1 class="tc-title-1">Choose an action to fight</h1></div></header>
    <div class="cbody">
      <section class="tc-paper intro"><p><b>The campaign settles every fleet action from the strength of the fleets until you choose to fight one here.</b> Each battleship, carrier and cruiser sails alone, and destroyers sail in fours. The two fleets meet at exactly the odds the campaign counts. What happens here goes back to the campaign as the ships each side lost and who holds the sea, and you can hand the rest to the staff at any moment: what you have won stays won.</p></section>
      <h2 class="tc-label shead">${facts(['From the campaign', ...(t ? [esc(t.date)] : [])])}</h2>
      <div class="offers">${t ? `<button class="tc-card is-press offer" data-from="1">
        <span class="otop"><b class="tc-title-3">${esc(t.title)}</b><span class="tc-data"><b>${odds(t.reckoning.odds)}</b> to 1</span></span>
        <span class="tc-data omid"><span class="oside">${shipsKept(sideShips(t.place.fleets, 'att'))}</span><span class="oside">against ${shipsKept(sideShips(t.place.fleets, 'def'))}</span></span>
        <span class="tc-data olow">${facts([`${names(t.place.fleets, 'att')} against ${names(t.place.fleets, 'def')}`, staff(chanceOf(t))])}</span></button>` : '<p class="tc-data none">No fleet action is coming this month.</p>'}</div>
      <h2 class="tc-label shead">${facts(['Practice actions', 'Nothing goes back to the campaign'])}</h2>
      <div class="offers">${PRACTICE.map((p, i) => {
        const fl = [...p.att.map((f) => ({ ...f, side: 'att' as Side })), ...p.def.map((f) => ({ ...f, side: 'def' as Side }))];
        return `<button class="tc-card is-press offer" data-practice="${i}">
        <span class="otop"><b class="tc-title-3">${esc(p.name)}</b><span class="tc-data"><b>${odds(p.odds)}</b> to 1</span></span>
        <span class="tc-data omid">${esc(p.about)}</span>
        <span class="tc-data olow"><span class="oside">${shipsKept(sideShips(fl, 'att'))}</span><span class="oside">against ${shipsKept(sideShips(fl, 'def'))}</span><span class="oside">${names(fl, 'att')} against ${names(fl, 'def')}</span></span></button>`;
      }).join('')}</div>
    </div></div>`;
  app.querySelector('[data-from]')?.addEventListener('click', () => { if (from) startBattle(from.ahead.ticket, from); });
  app.querySelectorAll<HTMLElement>('[data-practice]').forEach((el) => el.addEventListener('click', () => startBattle(practiceTicket(PRACTICE[Number(el.dataset.practice)]), null)));
}

// ---- The action ----------------------------------------------------------------------------------------------------

let canvas: HTMLCanvasElement;
let ctx: CanvasRenderingContext2D;

const CLASS_WORD: Record<SeaClass, string> = { bb: 'Battleship', cv: 'Carrier', ca: 'Cruiser', dd: 'Destroyers' };
const STATE_WORD: Record<string, string> = { steady: 'Steady', wavering: 'Wavering', routing: 'Breaking off', gone: 'Lost', withdrawn: 'Out of the fight' };
const ACT_WORD: Record<string, string> = { idle: 'hove to', march: 'under way', run: 'at full speed', turn: 'turning', fight: 'in action', rout: 'making for port', down: '' };
const SPEED_WORD: Record<SeaSpeed, string> = { slow: 'Slow', cruise: 'Cruising', flank: 'Full speed' };

function startBattle(t: SeaTicket, from: FromCampaign | null): void {
  const sim = new SeaSim(t);
  S.sim = sim; S.from = from; S.screen = 'battle';
  const sc = S.scene;
  sc.me = t.att.human ? 'att' : 'def';
  sc.sel = new Set(); sc.hover = null; sc.box = null; sc.line = null; sc.ping = null; sc.marks = []; sc.inbound = [];
  sc.colours = sideColours(t.att.color, t.def.color);
  S.clock = new Clock(); S.clock.set(1); S.clock.toggle();
  S.asking = false;
  S.dispatch = { t: 0, what: 'contact', text: `Your fleet has the enemy in sight in the ${t.place.zoneName}: give your orders, then press Space to begin.` };
  S.dispatchAge = 0;
  guard();
  const mine = t[sc.me], theirs = t[sc.me === 'att' ? 'def' : 'att'];
  const fleetNames = (side: Side): string => esc(andList(t.place.fleets.filter((f) => f.side === side).map((f) => f.name)));
  app.innerHTML = `<div class="battle tc sea">
    <header class="bbar">
      <div class="btitle"><b class="tc-title-3">${esc(t.title)}</b><span class="tc-data">${facts([esc(t.date), `${fleetNames('att')} against ${fleetNames('def')}`])}</span></div>
      <div class="bclock"><span class="tc-label">Battle time</span><span class="tc-data" id="time">0:00</span></div>
      <div class="bspeed" role="group" aria-label="Clock">
        <button class="tc-btn tc-btn--sm" data-clock="0" title="Pause (Space)">Pause <span class="tc-key">Space</span></button>
        <button class="tc-btn tc-btn--sm" data-clock="1" title="Normal speed (1)">×1 <span class="tc-key">1</span></button>
        <button class="tc-btn tc-btn--sm" data-clock="2" title="Twice as fast (2)">×2 <span class="tc-key">2</span></button>
        <button class="tc-btn tc-btn--sm" data-clock="4" title="Four times as fast (3)">×4 <span class="tc-key">3</span></button>
      </div>
      <div class="bbal" title="Each side's strength still afloat and fighting. The pale mark is where the campaign put it.">
        <div class="bends"><span>${esc(mine.adj)}</span><span>${esc(theirs.adj)}</span></div>
        <div class="btrack"><i id="balA"></i><i id="balD"></i><b id="balMark"></b></div>
        <span id="ratio" class="tc-data bratio"></span>
      </div>
      <div class="bacts">
        <button class="tc-btn" data-act="dice" title="The staff settles the rest at once, from where the action stands. What you have won stays won.">Hand it to the staff</button>
        <button class="tc-btn tc-btn--grave" data-act="withdraw">Withdraw</button>
        <div class="wask tc-paper" id="wask" hidden role="alertdialog" aria-label="Withdraw the fleet?"><p>Withdraw the fleet? The action ends and the sea is theirs.</p><div class="wbtns"><button class="tc-btn" data-act="wgo">Withdraw</button><button class="tc-btn tc-btn--primary" data-act="wstay">Stay</button></div></div>
      </div>
    </header>
    <div class="bfield"><canvas id="field"></canvas>
      <div class="dispatch tc-paper" id="dispatch"></div>
      <div class="zoom"><button class="tc-btn tc-btn--sm" data-zoom="1" aria-label="Closer">+</button><button class="tc-btn tc-btn--sm" data-zoom="-1" aria-label="Further">−</button><button class="tc-btn tc-btn--sm" data-zoom="0" title="The whole sea">Whole sea</button></div>
      <div class="seaspeed tc-paper" id="seaspeed" hidden><span class="tc-label">Speed</span><span class="tc-seg">${(['slow', 'cruise', 'flank'] as SeaSpeed[]).map((k) => `<button data-way="${k}">${SPEED_WORD[k]}</button>`).join('')}</span></div>
    </div>
    <div class="keys tc-data">${facts(['Left click: pick up', 'Right click: steer there, or engage an enemy', 'Right drag: form a line', '<span class="tc-key">H</span> hold', '<span class="tc-key">R</span> break off', '<span class="tc-key">Tab</span> next', '<span class="tc-key">Esc</span> put down'])}</div>
    <footer class="traywrap"><div class="tray" id="tray">${sim.units.filter((u) => u.side === sc.me).map((u) => card(u, sc.colours![sc.me])).join('')}</div><button class="tmore is-left" id="tmoreL" hidden></button><button class="tmore" id="tmoreR" hidden></button></footer>
  </div>`;
  const el = (id: string): HTMLElement => document.getElementById(id)!;
  el('balA').style.background = sc.colours.att; el('balD').style.background = sc.colours.def;
  canvas = el('field') as HTMLCanvasElement;
  ctx = canvas.getContext('2d')!;
  fit(true);
  frameFleets();
  wireBattle();
  updateTop(true);
  updateTray(true);
  S.last = now();
}

/** What a piece is, without its fleet, with what it has lost: "4 destroyers", "3 of 4 destroyers", "Battleship". */
const whatOf = (u: SeaUnit): string => {
  const c = CLASS_ONE[u.extra.cls as SeaClass], n0 = u.extra.ships0, n = u.extra.ships;
  if (n0 === 1) return c[0][0].toUpperCase() + c[0].slice(1);
  return n < n0 ? `${n} of ${n0} ${c[1]}` : `${n0} ${c[1]}`;
};

function card(u: SeaUnit, color: string): string {
  return `<button class="tc-card is-press ucard" data-unit="${u.id}" title="${esc(u.name)}">
    <span class="uwhat"><b>${esc(whatOf(u))}</b><small>${shipSvg(u.extra.cls as SeaClass, color)}<span>${esc(u.extra.fleet)}</span></small></span>
    <span class="tc-meter" data-m="str"><span class="tc-label">Hull</span><i><b></b></i><span></span></span>
    <span class="tc-meter is-rest" data-m="mor"><span class="tc-label">Morale</span><i><b></b></i><span></span></span>
    <span class="ustate tc-data"></span></button>`;
}

const orderable = (u: SeaUnit): boolean => live(u) && u.state !== 'routing';
const unit = (id: number): SeaUnit | undefined => S.sim!.units.find((u) => u.id === id);

function updateTray(force = false): void {
  const t = now();
  if (!force && t - S.trayAt < 0.15) return;
  S.trayAt = t;
  for (const el of document.querySelectorAll<HTMLElement>('#tray [data-unit]')) {
    const u = unit(Number(el.dataset.unit));
    if (!u) continue;
    el.classList.toggle('is-on', S.scene.sel.has(u.id));
    el.classList.toggle('is-off', !live(u));
    // A bar's colour follows the number it shows (the UI director, 9).
    const set = (m: string, v: number, rest: boolean): void => {
      const box = el.querySelector<HTMLElement>(`[data-m="${m}"]`)!;
      const shown = Math.round(Math.max(0, Math.min(1, v)) * 100);
      box.className = `tc-meter ${rest ? 'is-rest' : shown < 35 ? 'is-low' : shown < 65 ? 'is-mid' : ''}`;
      (box.querySelector('i b') as HTMLElement).style.width = `${shown}%`;
      (box.querySelector('span:last-child') as HTMLElement).textContent = `${shown}`;
    };
    set('str', u.str, false);
    set('mor', u.mor, true);
    const what = el.querySelector<HTMLElement>('.uwhat b')!, w = whatOf(u);
    if (what.textContent !== w) what.textContent = w;
    const st = el.querySelector<HTMLElement>('.ustate')!;
    // Once the action is over nothing is under way: the card says so (the design director, b356, b368).
    const doing = S.sim!.over ? (live(u) ? 'action over' : '') : live(u) ? ACT_WORD[u.act] ?? '' : '';
    // A narrow chip keeps the state in words and lets what the ship is doing go (it shows on the sea, and in the chip's
    // title): the line is in the typed face, about 7.7 pixels a letter with its spacing.
    const word = STATE_WORD[u.state], room = st.clientWidth || 999;
    const html = `<b>${word}</b>${doing && (word.length + 3 + doing.length) * 7.7 <= room ? `<span class="act"> · ${doing}</span>` : ''}`;
    if (st.innerHTML !== html) { st.innerHTML = html; el.title = `${u.name}: ${STATE_WORD[u.state]}${doing ? ` · ${doing}` : ''}`; }
    st.className = `ustate tc-data${u.state === 'routing' ? ' bad' : u.state === 'wavering' ? ' warn' : ''}`;
  }
  trayMore();
}

/** Where the tray holds more ships than it shows, its edge fades and says how many: "2 more ▸" (the UI director, 8). */
function trayMore(): void {
  const tray = document.getElementById('tray'), L = document.getElementById('tmoreL'), R = document.getElementById('tmoreR');
  if (!tray || !L || !R) return;
  const box = tray.getBoundingClientRect();
  let left = 0, right = 0;
  for (const c of tray.querySelectorAll<HTMLElement>('[data-unit]')) {
    const r = c.getBoundingClientRect();
    if (r.left < box.left - 1) left++;
    else if (r.right > box.right + 1) right++;
  }
  L.hidden = left === 0; R.hidden = right === 0;
  const lt = `◂ ${left} more`, rt = `${right} more ▸`;
  if (L.textContent !== lt) L.textContent = lt;
  if (R.textContent !== rt) R.textContent = rt;
}

function updateTop(force = false): void {
  const sim = S.sim!;
  const el = (id: string): HTMLElement => document.getElementById(id)!;
  el('time').textContent = `${mmss(sim.t)} of ${mmss(sim.ticket.limit)}`;
  // Each side's weight still afloat and fighting, as a share of both at the start.
  const v = sim.balance();
  const share = v.att / Math.max(1e-6, v.att + v.def);
  el('balA').style.width = `${(share * 100).toFixed(1)}%`;
  el('balD').style.width = `${((1 - share) * 100).toFixed(1)}%`;
  const k = sim.ticket.reckoning;
  el('balMark').style.left = `${((100 * k.odds) / (1 + k.odds)).toFixed(1)}%`;
  el('ratio').textContent = `card ${odds(k.odds)} · now ${odds(v.att / Math.max(1e-6, v.def))} to 1`;
  document.querySelectorAll<HTMLElement>('[data-clock]').forEach((x) => x.classList.toggle('is-lit', Number(x.dataset.clock) === S.clock.speed));
  (el('wask') as HTMLElement).hidden = !S.asking;
  // The speed strip shows for the ships in hand, lit at the speed they all share.
  const ids = chosen();
  const strip = el('seaspeed');
  strip.hidden = ids.length === 0;
  const ways = new Set(ids.map((id) => unit(id)!.extra.speed));
  strip.querySelectorAll<HTMLElement>('[data-way]').forEach((b) => b.classList.toggle('is-on', ways.size === 1 && ways.has(b.dataset.way as SeaSpeed)));
  if (force || S.dispatch) {
    const d = S.dispatch;
    const box = el('dispatch');
    const html = d ? `<span class="tc-label">${mmss(d.t)}</span><span>${esc(d.text)}</span>` : '';
    if (box.innerHTML !== html) box.innerHTML = html;
    // While the clock runs a dispatch fades a few seconds after it comes; while paused it stays, as on land.
    box.classList.toggle('is-old', !!d && S.dispatchAge > DISPATCH_FOR);
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
const SIZE = SEA.field.size;
function whole(): void { S.scene.cam = { x: SIZE / 2, y: SIZE / 2, z: Math.min(S.scene.W / SIZE, (S.scene.H - 60) / SIZE) * 0.97 }; }
/** Each action opens framed on the two fleets; WHOLE SEA stays a button (the art director, 1). */
function frameFleets(): void {
  const us = S.sim!.units.filter(live);
  if (!us.length) { whole(); return; }
  const pad = 1500;
  const xs = us.map((u) => u.x), ys = us.map((u) => u.y);
  const [ax, bx, ay, by] = [Math.min(...xs) - pad, Math.max(...xs) + pad, Math.min(...ys) - pad, Math.max(...ys) + pad];
  const c = S.scene.cam;
  c.x = (ax + bx) / 2; c.y = (ay + by) / 2;
  c.z = Math.max(minZoom(), Math.min(0.6, Math.min(S.scene.W / (bx - ax), (S.scene.H - 50) / (by - ay))));
  clampCam();
}
function minZoom(): number { return Math.min(S.scene.W / SIZE, S.scene.H / SIZE) * 0.8; }
function zoomAt(px: number, py: number, k: number): void {
  const c = S.scene.cam;
  const [fx, fy] = toField(S.scene, px, py);
  c.z = Math.max(minZoom(), Math.min(0.6, c.z * k));
  const [nx, ny] = toField(S.scene, px, py);
  c.x += fx - nx; c.y += fy - ny;
  clampCam();
}
function clampCam(): void {
  const c = S.scene.cam;
  const hw = S.scene.W / 2 / c.z, hh = S.scene.H / 2 / c.z;
  c.x = hw * 2 >= SIZE ? SIZE / 2 : Math.max(hw, Math.min(SIZE - hw, c.x));
  c.y = hh * 2 >= SIZE ? SIZE / 2 : Math.max(hh, Math.min(SIZE - hh, c.y));
}

// ---- Orders -----------------------------------------------------------------------------------------------------------

function chosen(): number[] { return [...S.scene.sel].filter((id) => { const u = unit(id); return !!u && orderable(u); }); }
const give = (c: Command | { do: 'sea:speed'; units: number[]; speed: SeaSpeed }): boolean => S.sim!.command(c as Command, S.scene.me);
/** How near the pointer must be to a piece to take it, in metres: a few pixels at any zoom. */
const reachPx = (): number => Math.max(250, 18 / S.scene.cam.z);

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
    if (!down) {
      const u = unitAt(sim.units.filter(live), fx, fy, reachPx(), sc.me);
      sc.hover = u ? u.id : null;
      canvas.style.cursor = u && u.side !== sc.me && sc.sel.size ? 'crosshair' : u && u.side === sc.me ? 'pointer' : 'default';
      return;
    }
    if (Math.hypot(px - down.x, py - down.y) > 6) down.moved = true;
    if (!down.moved) return;
    if (down.touch || down.button === 1) { sc.cam.x -= (px - down.px) / sc.cam.z; sc.cam.y += (py - down.py) / sc.cam.z; clampCam(); }
    else if (down.button === 0) { const [ax, ay] = toField(sc, down.x, down.y); sc.box = [ax, ay, fx, fy]; }
    else if (down.button === 2) {
      const ids = chosen();
      if (ids.length) {
        const [ax, ay] = toField(sc, down.x, down.y);
        sc.line = { x0: ax, y0: ay, x1: fx, y1: fy, ghosts: lineUp(ids.map((i) => unit(i)!), sc.me, ax, ay, fx, fy, 600) };
      }
    }
    down.px = px; down.py = py;
  });
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
    } else if (d.button === 2) {
      if (sc.line) {
        const ids = chosen();
        if (ids.length) give({ do: 'line', units: ids, x0: sc.line.x0, y0: sc.line.y0, x1: sc.line.x1, y1: sc.line.y1 });
        sc.line = null;
      } else orderAt(px, py);
    }
    updateTray(true); updateTop();
  };
  canvas.addEventListener('pointerup', up);
  canvas.addEventListener('pointercancel', () => { down = null; sc.box = null; sc.line = null; });
  canvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    const [px, py] = pos(e);
    const notched = e.deltaMode !== 0 || (e.deltaX === 0 && Math.abs(e.deltaY) >= 40 && Number.isInteger(e.deltaY));
    if (e.ctrlKey || notched) zoomAt(px, py, Math.exp(-e.deltaY * (e.ctrlKey ? 0.01 : 0.0015)));
    else { sc.cam.x += e.deltaX / sc.cam.z; sc.cam.y -= e.deltaY / sc.cam.z; clampCam(); }
  }, { passive: false });
  window.onresize = (): void => { if (S.screen !== 'choose') { fit(); trayMore(); } };
  app.querySelectorAll<HTMLElement>('[data-clock]').forEach((el) => el.addEventListener('click', () => { S.clock.set(Number(el.dataset.clock) as Speed); updateTop(); }));
  app.querySelectorAll<HTMLElement>('[data-zoom]').forEach((el) => el.addEventListener('click', () => {
    const k = Number(el.dataset.zoom);
    if (k === 0) whole(); else zoomAt(sc.W / 2, sc.H / 2, k > 0 ? 1.4 : 1 / 1.4);
  }));
  app.querySelectorAll<HTMLElement>('[data-way]').forEach((el) => el.addEventListener('click', () => { give({ do: 'sea:speed', units: chosen(), speed: el.dataset.way as SeaSpeed }); updateTop(); }));
  app.querySelector('[data-act="dice"]')!.addEventListener('click', () => { give({ do: 'dice' }); });
  app.querySelector('[data-act="withdraw"]')!.addEventListener('click', () => { S.asking = true; guard(); updateTop(); });
  app.querySelector('[data-act="wstay"]')!.addEventListener('click', () => { S.asking = false; updateTop(); });
  app.querySelector('[data-act="wgo"]')!.addEventListener('click', () => { if (now() < S.guardUntil) return; S.asking = false; give({ do: 'withdrawAll', side: sc.me }); });
  const tray = document.getElementById('tray')!;
  tray.addEventListener('scroll', trayMore, { passive: true });
  document.getElementById('tmoreR')!.addEventListener('click', () => { tray.scrollLeft += tray.clientWidth * 0.8; });
  document.getElementById('tmoreL')!.addEventListener('click', () => { tray.scrollLeft -= tray.clientWidth * 0.8; });
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
    if (u) { sc.cam.x = u.x; sc.cam.y = u.y; sc.cam.z = Math.max(sc.cam.z, 0.12); clampCam(); }
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
  const sim = S.sim!, sc = S.scene;
  const ids = chosen();
  if (!ids.length) return;
  const [fx, fy] = toField(sc, px, py);
  const foe = unitAt(sim.units.filter((x) => x.side !== sc.me && live(x)), fx, fy, reachPx());
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
    case 'Escape': if (S.asking) S.asking = false; else { sc.sel.clear(); sc.line = null; sc.box = null; } break;
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
  // At the press of Space, one line so the wait has a meaning (the UI director, 6).
  if (sim.t > 0 && S.dispatch?.t === 0 && !S.dispatch.unit) {
    const m = minutesToRange(sim);
    S.dispatch = m > 0 ? { t: sim.t, what: 'contact', text: `The fleets are closing: in range in about ${m === 1 ? 'a minute' : `${m} minutes`}.`, unit: -1 } : null;
    S.dispatchAge = 0;
  }
  for (const e of v.events) if (e.text) { S.dispatch = e; S.dispatchAge = 0; }
  if (S.clock.speed > 0) S.dispatchAge += dt;
  for (const id of [...S.scene.sel]) { const u = unit(id); if (!u || !orderable(u)) S.scene.sel.delete(id); }
  drawSea(ctx, sim, S.scene);
  updateTop();
  updateTray();
  if (v.over && S.screen === 'battle') showAfter();
}

/** About how many minutes of battle time before the nearest guns of either side reach the other, at cruising speed. */
function minutesToRange(sim: SeaSim): number {
  let best = Infinity;
  const us = sim.units.filter(live);
  for (const a of us) for (const b of us) {
    if (a.side !== S.scene.me || b.side === a.side) continue;
    const A = SEA.classes[a.extra.cls as SeaClass], B = SEA.classes[b.extra.cls as SeaClass];
    const reach = Math.max(a.extra.cls === 'cv' ? 0 : A.reach, b.extra.cls === 'cv' ? 0 : B.reach);
    const close = (A.speed + B.speed) * SEA.speeds.cruise;
    best = Math.min(best, (Math.hypot(a.x - b.x, a.y - b.y) - reach) / close);
  }
  return Number.isFinite(best) ? Math.round(best / 60) : 0;
}

// ---- After the action: what the campaign makes of it ------------------------------------------------------------------

function reckoned(carried: boolean, chance: number): string {
  if (chance >= 0.995) return carried ? 'As the staff expected: they were sure of it.' : 'Worse than the staff expected: they were sure of it.';
  if (chance <= 0.005) return carried ? 'Better than the staff expected: they gave it no chance.' : 'As the staff expected: they gave it no chance.';
  const p = `they gave you about ${Math.round(chance * 100)}% to win it.`;
  if (carried) return chance < 0.5 ? `You did better than the staff expected: ${p}` : `As the staff expected: ${p}`;
  return chance >= 0.5 ? `You did worse than the staff expected: ${p}` : `As the staff feared: ${p}`;
}

function showAfter(): void {
  const sim = S.sim!, r = sim.over!;
  S.screen = 'after';
  guard();
  // The after card is the one voice: the dispatch strip steps aside (the UI director, 3).
  S.dispatch = null;
  document.getElementById('dispatch')!.innerHTML = '';
  updateTray(true);
  const t = sim.ticket;
  let rep: SeaReport | null = null, headline = '';
  if (S.from) {
    // The campaign closes the month with the field's result in place of the dice.
    const f = S.from;
    const n0 = f.g.seaReports.length, h0 = f.g.headlines.length;
    closeWith(f.g, new Map([[f.ahead.key, { meeting: f.ahead.meeting, fought: foughtOf(r) }]]), f.close);
    rep = f.g.seaReports.slice(n0).find((x) => x.zone === f.ahead.meeting.zone) ?? null;
    headline = f.g.headlines.slice(h0).find((h) => h.kind === 'battle')?.text ?? '';
  }
  const won = rep ? rep.won === (S.scene.me === 'att') : r.winner === S.scene.me;
  const level = rep ? rep.level : r.level;
  // The card's list, in the land and air cards' shape (the UI director's review, 2): the staff's line if it was handed
  // over, who holds the sea, what went down in sight, and the ships that came home for repair.
  const x = r.extra as unknown as SeaLosses;
  const me = S.scene.me, them: Side = me === 'att' ? 'def' : 'att';
  const bySide = (side: Side, what: 'sunk' | 'burnt'): SeaShips => {
    const n: SeaShips = { bb: 0, cv: 0, ca: 0, dd: 0 };
    for (const f of t.place.fleets) if (f.side === side) for (const c of ['bb', 'cv', 'ca', 'dd'] as SeaClass[]) n[c] += x[what][f.id]?.[c] ?? 0;
    return n;
  };
  const count = (n: SeaShips): number => n.bb + n.cv + n.ca + n.dd;
  const lost = (side: Side): string => andList(shipBits(bySide(side, 'sunk'))) || 'none';
  const staffLine = r.lines.find((l) => l.startsWith('The staff settled'));
  const noneSunk = count(bySide(me, 'sunk')) + count(bySide(them, 'sunk')) === 0;
  const lossLine = x.toDice
    ? noneSunk ? 'Before you handed it to the staff, no ship had gone down on either side.' : `Before you handed it to the staff, you had lost ${lost(me)} and ${people(t[them].adj)} ${lost(them)}.`
    : noneSunk ? 'No ship went down on either side.' : `You lost ${lost(me)} and ${people(t[them].adj)} ${lost(them)}.`;
  const hm = count(bySide(me, 'burnt')), ht = count(bySide(them, 'burnt'));
  const theirShips = (n: number): string => `${n} ${t[them].adj} ${n === 1 ? 'ship' : 'ships'}`;
  const repairLine = hm && ht ? `${hm} of your ships came home for repair, and ${theirShips(ht)}.`
    : hm ? `${hm} of your ships came home for repair.` : ht ? `${theirShips(ht)} came home for repair.` : '';
  const list = [staffLine, r.lines[0], lossLine, repairLine].filter((l): l is string => !!l);
  const sides = rep ? `<div class="aside2">${(['att', 'def'] as const).map((sd) => `<div><h3 class="tc-label">${esc(t[sd].adj)} fleet</h3><ul>${rep![sd].map((s) => `<li><b>${esc(s.fleet)}</b> <span>lost ${esc(andList(shipBits(s.sunk)) || 'no ships')}</span></li>`).join('')}</ul></div>`).join('')}</div>` : '';
  const veil = document.createElement('div');
  veil.className = 'veil';
  veil.innerHTML = `<div class="after tc-paper" role="dialog" aria-label="Action result">
    <div class="tc-label kick">${rep ? `Fleet action · ${esc(t.date)}` : 'Practice action'}</div>
    <h2 class="tc-title-2">${esc(t.title)}</h2>
    <div class="verdict"><span class="tc-stamp ${won ? 'tc-stamp--won' : ''}">${esc(S.scene.me === 'att' ? level : level.replace('victory', 'X').replace('defeat', 'victory').replace('X', 'defeat'))}</span></div>
    ${headline ? `<p class="told"><b>${esc(headline)}</b></p>` : ''}
    <h3 class="tc-label">On the water</h3><ul class="flines">${list.map((l) => `<li>${esc(l)}</li>`).join('')}</ul>
    <h3 class="tc-label">Against the staff's reckoning</h3>
    <p class="dice">${esc(reckoned(won, S.scene.me === 'att' ? chanceOf(t) : 1 - chanceOf(t)))}${rep ? '' : ' A practice action hands nothing back.'}</p>
    ${sides}
    <div class="afoot"><button class="tc-btn tc-btn--primary tc-btn--lg" data-act="continue">Continue</button></div>
  </div>`;
  document.body.appendChild(veil);
  veil.querySelector('[data-act="continue"]')!.addEventListener('click', () => { if (now() < S.guardUntil) return; veil.remove(); showChooser(); });
}

// A handle for the page's own checks (screenshots, the playtester's harness). Not used by the page itself.
(window as unknown as { __sea: unknown }).__sea = { state: S, give, showChooser, startBattle, practiceTicket, PRACTICE, toScreen };

showChooser();
requestAnimationFrame(frame);

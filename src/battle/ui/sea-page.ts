// The fleet action page: choose an action (one the campaign is about to fight, or a practice one), fight it on open
// water, and see what the campaign makes of the result. The sea's first slice of the played battles (ruling 217): a
// browser page of its own, beside the land page, with no press on the campaign screen until the play version ships.
//
// Mouse: left picks (drag for a box, Shift adds), right orders (on the water to steer for it, on an enemy to engage it
// or, for a carrier, to strike it; drag to draw a line). Keys: Space pause, 1 2 3 speed, H hold, R the chosen ships break
// off, Tab next piece, Esc drops the choice. A plain left click never gives an order, and Enter does nothing on the
// field; on the after card it presses CONTINUE, past the half-second key guard.
// The words are the UI director's and the design director's (reviews of 17 November, logs b368 and b370).

import { createGame } from '../../campaign/game';
import { sail, seaBattles } from '../../campaign/navy';
import { beginHuman, humanUp, stepAI } from '../../campaign/turn';
import type { Game, SeaReport } from '../../campaign/types';
import { Clock, type Speed } from '../core/clock';
import { diceChance } from '../core/parity';
import { lineUp } from '../core/select';
import type { Command, FieldEvent, Side } from '../core/types';
import { closeWith, foughtOf, seaAhead, type SeaAhead } from '../sea/handoff';
import { SEA } from '../sea/numbers';
import { SeaSim, type SeaLosses } from '../sea/sim';
import { PRACTICE, practiceTicket as practicePlain, type Practice } from '../sea/practice';
import { plainTicket } from '../sea/ticket';
import type { SeaClass, SeaShips, SeaSpeed, SeaTicket, SeaUnit } from '../sea/types';
import { andList, repairLines } from '../sea/words';
import { toField, toScreen, type Camera } from './draw';
import { aircraftOf, aircraftSaid, drawSea, layoutSea, live, PICK, pieceAt, piecesIn, shipSvg, shownName, sideColours, takeEvents, type SeaScene } from './sea-draw';
import { seaWhere } from '../../campaign/sea-words';

const KEY_GUARD = 0.5;
const esc = (s: string): string => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]!);
const mmss = (t: number): string => `${Math.floor(t / 60)}:${String(Math.floor(t % 60)).padStart(2, '0')}`;
const odds = (o: number): string => (o >= 10 ? o.toFixed(0) : o.toFixed(2));
const chanceOf = (t: SeaTicket): number => diceChance(t.reckoning.odds, t.reckoning.dice, t.reckoning.even);
/** Facts in a row, each kept whole, as on the land page (battle.css .facts). */
const facts = (bits: string[]): string => `<span class="facts"><span>${bits.map((b) => `<span class="fact">${b}</span>`).join('')}</span></span>`;
const cap = (k: string): string => `<span class="tc-key">${k}</span>`;
/** The hint line, in the UI director's words (ruling 226, 9): one line at 1440 and up, folding once before "Arrows"
 *  under that, where "Space pause" and "1 2 3 speed" join it because the clock's buttons drop their key caps there (b467). */
const KEYS = facts(['Click or drag a box: pick up', 'Right-click: steer there, or engage an enemy', 'Right-drag: form a line', `${cap('H')} hold`, `${cap('R')} break off`, `${cap('Tab')} next`, `${cap('Esc')} put down`, 'Arrows or two-finger drag: look round', `<span class="knarrow">${cap('Space')} pause</span>`, `<span class="knarrow">${cap('1')} ${cap('2')} ${cap('3')} speed</span>`])
  .replace('<span class="fact">Arrows', '<span class="kbreak"></span><span class="fact">Arrows');
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
  /** HAND IT TO THE STAFF's question is standing (ruling 226, 5). */
  handing: boolean;
  /** The clock's speed before a question stopped it, to go back to on KEEP FIGHTING. */
  resume: Speed;
  /** Nothing is happening yet, so time runs fast (ruling 226, 1). */
  closing: boolean;
  /** A fleet has broken off and time runs fast through the chase, until the player takes it in hand (b402, 3). */
  chasing: boolean;
  /** The player has set the clock's speed since a fleet broke off: the chase runs at their speed (b458, 1: orders do not). */
  chaseByHand: boolean;
  /** The player has taken the approach's clock in hand with a speed key once it was running (the UI director, b467). */
  approachByHand: boolean;
  /** The tag at the top of the sea for a moment: "RUNNING ×2", "THE FLEETS MEET · RUNNING ×1" (real seconds). */
  tag: { text: string; until: number } | null;
  /** The warnings at three quarters of the way to breaking off: ours stands while it holds, theirs is said once. */
  last: number;
  trayAt: number;
  /** The tray's page: the first chip it shows, how many it shows, and the choice it last turned to. */
  trayFrom: number;
  trayShown: number;
  traySel: string;
  /** The view keeps both fleets framed as they close, until the player moves it themselves (the art director, keep-together 2). */
  follow: boolean;
}

const app = document.getElementById('app')!;
const S: State = {
  screen: 'choose', sim: null, from: null, clock: new Clock(),
  scene: { cam: { x: 10000, y: 10000, z: 0.04 }, W: 800, H: 600, me: 'att', sel: new Set(), hover: null, box: null, line: null, now: 0, ping: null, marks: [], inbound: [] },
  guardUntil: 0, dispatch: null, dispatchAge: 0, asking: false, handing: false, resume: 1, closing: true, chasing: false, chaseByHand: false, approachByHand: false, tag: null, last: 0, trayAt: 0, trayFrom: 0, trayShown: 0, traySel: '', follow: true,
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

/** The campaign's own names and colours for the practice actions' nations. A nation it lacks reads as "Enemy", never by its code. */
let nations: Game['nations'] | null = null;
function practiceTicket(p: Practice): SeaTicket {
  nations ??= createGame(1, null).nations;
  // Headed by the card's name, "Practice" and the fleets in the facts under it (b458, 5: practice.ts).
  const t = practicePlain(p, 1 + Math.floor(Math.random() * 1e6));
  for (const side of ['att', 'def'] as Side[]) {
    const n = nations[t[side].nat];
    if (n) { t[side].adj = n.adj; t[side].color = n.color; } else t[side].adj = side === 'att' ? 'Your' : 'Enemy';
  }
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
    <header class="chead"><div><div class="tc-label">Fleet actions</div><h1 class="tc-title-1">Choose an action to fight</h1></div></header>
    <div class="cbody">
      <section class="tc-paper intro"><p><b>The campaign settles every fleet action from the strength of the fleets until you choose to fight one here.</b> Each battleship, carrier and cruiser sails alone, and destroyers sail in fours. The two fleets meet at exactly the odds the campaign counts. What happens here goes back to the campaign as the ships each side lost and who holds the sea. You can hand the rest to the staff at any moment, and every ship already sunk stays sunk.</p></section>
      <h2 class="tc-label shead">${facts(['From the campaign', ...(t ? [esc(t.date)] : [])])}</h2>
      <div class="offers">${t ? `<button class="tc-card is-press offer" data-from="1">
        <span class="otop"><b class="tc-title-3">${esc(t.title)}</b><span class="tc-data"><b>${odds(t.reckoning.odds)}</b> to 1</span></span>
        <span class="tc-data omid"><span class="oside">${shipsKept(sideShips(t.place.fleets, 'att'))}</span><span class="oside">against ${shipsKept(sideShips(t.place.fleets, 'def'))}</span></span>
        <span class="tc-data olow">${facts([`${names(t.place.fleets, 'att')} against ${names(t.place.fleets, 'def')}`, staff(chanceOf(t))])}</span></button>` : '<p class="tc-data none">No fleet action is coming this month.</p>'}</div>
      <h2 class="tc-label shead">${facts(['Practice actions', 'Nothing goes back to the campaign'])}</h2>
      <div class="offers">${PRACTICE.map((p, i) => {
        const fl = [...p.att.map((f) => ({ ...f, side: 'att' as Side })), ...p.def.map((f) => ({ ...f, side: 'def' as Side }))];
        const pt = plainTicket({ att: p.att, def: p.def, seed: 1, air: p.air });
        return `<button class="tc-card is-press offer" data-practice="${i}">
        <span class="otop"><b class="tc-title-3">${esc(p.name)}</b><span class="tc-data"><b>${odds(pt.reckoning.odds)}</b> to 1</span></span>
        <span class="tc-data omid">${esc(p.about)}</span>
        <span class="tc-data olow"><span class="oside">${shipsKept(sideShips(fl, 'att'))}</span><span class="oside">against ${shipsKept(sideShips(fl, 'def'))}</span><span class="oside">${facts([`${names(fl, 'att')} against ${names(fl, 'def')}`, staff(chanceOf(pt))])}</span></span></button>`;
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
/** The card says the order, the moment it is given, paused or not; nothing when there is none (sea-226 mend 7). */
const ORDER_WORD: Record<string, string> = { attack: 'engaging', move: 'steering', hold: 'holding', leave: 'breaking off' };
const SPEED_WORD: Record<SeaSpeed, string> = { slow: 'Slow', cruise: 'Cruising', flank: 'Full speed' };

function startBattle(t: SeaTicket, from: FromCampaign | null): void {
  const sim = new SeaSim(t);
  S.sim = sim; S.from = from; S.screen = 'battle';
  const sc = S.scene;
  sc.me = t.att.human ? 'att' : 'def';
  sc.sel = new Set(); sc.hover = null; sc.box = null; sc.line = null; sc.ping = null; sc.marks = []; sc.inbound = []; resting = null;
  sc.layout = undefined; sc.keep = undefined; S.follow = true;
  sc.colours = sideColours(t.att.color, t.def.color);
  S.clock = new Clock(); S.clock.set(1); S.clock.toggle();
  S.asking = false; S.handing = false; S.closing = true; S.chasing = false; S.chaseByHand = false; S.approachByHand = false; S.tag = null;
  S.dispatch = { t: 0, what: 'contact', text: `Your fleet has the enemy in sight ${seaWhere(t.place.zoneName)}: give your orders, then press Space to begin.` };
  S.dispatchAge = 0;
  guard();
  const mine = t[sc.me], theirs = t[sc.me === 'att' ? 'def' : 'att'];
  const fleetNames = (side: Side): string => esc(andList(t.place.fleets.filter((f) => f.side === side).map((f) => f.name)));
  app.innerHTML = `<div class="battle tc sea">
    <header class="bbar">
      <div class="btitle"><b class="tc-title-3">${esc(t.title)}</b><span class="tc-data">${facts([esc(t.date), `${fleetNames('att')} against ${fleetNames('def')}`])}</span></div>
      <div class="bclock" title="At ${mmss(t.limit)} the action is done, and the fleet with the better of it holds the sea."><span class="tc-label" id="clockState">Paused</span><span class="tc-label room" aria-hidden="true">Action over</span><span class="tc-label room room-live" aria-hidden="true">Running</span><span class="tc-data" id="time">0:00</span></div>
      <div class="bspeed" role="group" aria-label="Clock">
        <button class="tc-btn tc-btn--sm" data-clock="0" title="Pause (Space)"><span id="pauseWord">Paused</span> <span class="tc-key">Space</span></button>
        <button class="tc-btn tc-btn--sm" data-clock="1" title="Normal speed (1)">×1 <span class="tc-key">1</span></button>
        <button class="tc-btn tc-btn--sm" data-clock="2" title="Twice as fast (2)">×2 <span class="tc-key">2</span></button>
        <button class="tc-btn tc-btn--sm" data-clock="4" title="Four times as fast (3)">×4 <span class="tc-key">3</span></button>
      </div>
      <div class="bbal" title="Each fleet's strength still afloat. The pale mark is where the campaign's card put it.">
        <div class="bends"><span>${esc(mine.adj)}</span><span id="ratio" class="tc-data"></span><span>${esc(theirs.adj)}</span></div>
        <div class="btrack"><i id="balMe"></i><i id="balThem"></i><b id="balMark"></b></div>
        <div class="bbreak" title="When a bar fills, that fleet breaks off and leaves the action."><span class="bbar2"><i id="brkMe"></i></span><span class="tc-label">How near to breaking off</span><span class="bbar2 is-theirs"><i id="brkThem"></i></span></div>
      </div>
      <div class="bacts">
        <button class="tc-btn" data-act="dice" title="The staff settles the rest at once, from where the action stands.">Hand it to the staff</button>
        <button class="tc-btn tc-btn--grave wdbtn" data-act="withdraw"><span class="wd-word">${WITHDRAW_WORD}</span><span class="wd-word is-off" aria-hidden="true">${END_CHASE_WORD}</span></button>
        <div class="wask tc-paper" id="wask" hidden role="alertdialog" aria-label="${WITHDRAW_ASK.label}"><p id="waskText">${WITHDRAW_ASK.text}</p><div class="wbtns"><button class="tc-btn" data-act="wgo">${WITHDRAW_ASK.yes}</button><button class="tc-btn tc-btn--primary" data-act="wstay">Keep fighting</button></div></div>
        <div class="wask tc-paper" id="sask" hidden role="alertdialog" aria-label="Hand the action to the staff?"><p id="saskText"></p><div class="wbtns"><button class="tc-btn" data-act="sgo">Hand it over</button><button class="tc-btn tc-btn--primary" data-act="sstay">Keep fighting</button></div></div>
      </div>
    </header>
    <div class="bfield"><canvas id="field"></canvas>
      <div class="ftag" id="ftag" hidden></div><div class="ftop"><div class="fwarn" id="fwarn" hidden></div><div class="dispatch tc-paper" id="dispatch"></div></div>
      <div class="zoom"><button class="tc-btn tc-btn--sm" data-zoom="1" aria-label="Closer">+</button><button class="tc-btn tc-btn--sm" data-zoom="-1" aria-label="Further">−</button><button class="tc-btn tc-btn--sm" data-zoom="0" title="The whole sea">Whole sea</button></div>
      <div class="seaspeed tc-paper" id="seaspeed" hidden><span class="tc-label">Speed</span><span class="tc-seg">${(['slow', 'cruise', 'flank'] as SeaSpeed[]).map((k) => `<button data-way="${k}">${SPEED_WORD[k]}</button>`).join('')}</span></div>
    </div>
    <div class="keys tc-data">${KEYS}</div>
    <footer class="traywrap"><div class="tray" id="tray"><button class="tc-card is-press tmore" id="tmoreL" hidden></button>${sim.units.filter((u) => u.side === sc.me).map((u) => card(u, sc.colours![sc.me])).join('')}<button class="tc-card is-press tmore" id="tmoreR" hidden></button></div></footer>
  </div>`;
  const el = (id: string): HTMLElement => document.getElementById(id)!;
  const ours = sc.colours[sc.me], theirsCol = sc.colours[sc.me === 'att' ? 'def' : 'att'];
  el('brkMe').style.background = ours; el('brkThem').style.background = theirsCol;
  el('balMe').style.background = ours; el('balThem').style.background = theirsCol;
  canvas = el('field') as HTMLCanvasElement;
  ctx = canvas.getContext('2d')!;
  fitTitle();
  void document.fonts?.ready.then(() => { if (S.sim === sim) fitTitle(); });
  fit(true);
  frameFleets();
  wireBattle();
  updateTop(true);
  frameFleets();
  sizeChips();
  updateTray(true);
  S.last = now();
}

/** What a piece is, numbered in kind, with what it has lost: "Cruiser 2", "Destroyers 4 · 3 of 4" (the design director),
 *  and a carrier's aircraft once they are worn, counted the same way: "Carrier 1 · 18 aircraft" (the UI director,
 *  keep-together). */
const whatOf = (u: SeaUnit): string => shownName(u, S.sim!.ticket);

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
    if (what.textContent !== w) {
      what.textContent = w;
      // The hover on a worn carrier's name says what the count means: "18 of its 36 aircraft can still fly."
      const said = aircraftSaid(S.sim!.ticket, u);
      if (said) what.title = said; else what.removeAttribute('title');
    }
    const st = el.querySelector<HTMLElement>('.ustate')!;
    // Once the action is over nothing is under way, and the card says only that, in one form however the action ended:
    // every piece afloat reads "Action over" in plain colour, and a sunk one keeps "Lost" (the design director, b356,
    // b368; the UI director, b467 "For Brian" 2).
    const over = !!S.sim!.over, afloatOver = over && u.state !== 'gone';
    const doing = over ? '' : live(u) && u.state !== 'routing' ? ORDER_WORD[S.sim!.orderOf(u.id)?.kind ?? ''] ?? '' : '';
    // Where the line will not fit, the chip keeps the state in words and lets the order go (it shows in the chip's title).
    const word = afloatOver ? 'Action over' : STATE_WORD[u.state];
    const full = `<b>${word}</b>${doing ? `<span class="act"> · ${doing}</span>` : ''}`;
    if (st.dataset.full !== full) {
      st.dataset.full = full;
      st.innerHTML = full;
      if (doing && st.scrollWidth > st.clientWidth + 0.5) st.innerHTML = `<b>${word}</b>`;
      el.title = `${u.name}: ${word}${doing ? ` · ${doing}` : ''}`;
    }
    st.className = `ustate tc-data${afloatOver ? '' : u.state === 'routing' ? ' bad' : u.state === 'wavering' ? ' warn' : ''}`;
  }
  trayMore();
}

/** Every chip takes one width: the one that holds the longest name any of them can come to ("3 of 4 destroyers") on
 *  one line, and a steady ship's "engaging" (sea-226 mend 4). Measured in the chips' own faces. */
function sizeChips(): void {
  const tray = document.getElementById('tray'), c = tray?.querySelector<HTMLElement>('[data-unit]');
  if (!tray || !c) return;
  const probe = (sel: string, html: string): number => {
    const e = c.querySelector<HTMLElement>(sel)!, old = e.innerHTML;
    e.innerHTML = html;
    // The text's own width, to the fraction: scrollWidth rounds, and a name 0.4 px over its box loses its end.
    const r = document.createRange();
    r.selectNodeContents(e);
    const w = Math.ceil(r.getBoundingClientRect().width) + 1;
    e.innerHTML = old;
    return w;
  };
  const names = new Set<string>();
  for (const u of S.sim!.units) if (u.side === S.scene.me) {
    const n0 = u.extra.ships0;
    names.add(n0 === 1 ? u.name : `${u.name} · ${n0 - 1} of ${n0}`);
    // A carrier's name grows with its aircraft count once they are worn: "Carrier 1 · 35 aircraft", "· no aircraft".
    const full = aircraftOf(S.sim!.ticket, u);
    if (full > 0) {
      const most = String(Math.max(1, full - 1));
      // Any count up to the full one, in the widest digits of its length.
      for (const d of ['0', '4', '8']) names.add(`${u.name} · ${most.replace(/\d/g, d)} aircraft`);
      names.add(`${u.name} · no aircraft`);
    }
  }
  let w = probe('.ustate', `<b>${STATE_WORD.steady}</b><span class="act"> · ${ORDER_WORD.attack}</span>`);
  for (const n of names) w = Math.max(w, probe('.uwhat b', esc(n)));
  // The fleet's whole name beside the ship's sign, never cut (b458, 6): at the line's own size where the chip the names
  // need holds it, else at the kit's next size down (11 px, sea.css .is-smallfleet), and the chip as wide as that needs.
  // The line keeps its height, so the card does too.
  const fleets = new Set(S.sim!.units.filter((u) => u.side === S.scene.me).map((u) => u.extra.fleet));
  const small = c.querySelector<HTMLElement>('.uwhat small')!, sign = small.querySelector<SVGElement>('.bsign');
  const beside = (sign ? sign.getBoundingClientRect().width : 0) + (parseFloat(getComputedStyle(small).columnGap) || 0);
  const fleetW = (): number => Math.max(0, ...[...fleets].map((f) => probe('.uwhat small span', esc(f)))) + beside;
  tray.classList.remove('is-smallfleet');
  if (fleetW() > w) { tray.classList.add('is-smallfleet'); w = Math.max(w, fleetW()); }
  const cs = getComputedStyle(c);
  CHIP_W = Math.ceil(w + parseFloat(cs.paddingLeft) + parseFloat(cs.paddingRight) + parseFloat(cs.borderLeftWidth) + parseFloat(cs.borderRightWidth));
  tray.style.setProperty('--chip', `${CHIP_W}px`);
}

/** The tray shows whole chips only (the UI director, round 2). Where it holds more ships than fit, a small card at its
 *  end says how many more and turns the page, and one at its start turns it back. The widths are sea.css's. */
const MORE_W = 112, CHIP_GAP = 6;
let CHIP_W = 140;
function trayMore(): void {
  const tray = document.getElementById('tray'), L = document.getElementById('tmoreL'), R = document.getElementById('tmoreR');
  if (!tray || !L || !R) return;
  const chips = [...tray.querySelectorAll<HTMLElement>('[data-unit]')], n = chips.length;
  const cs = getComputedStyle(tray);
  const room = tray.clientWidth - parseFloat(cs.paddingLeft) - parseFloat(cs.paddingRight);
  const fits = (k: number, l: boolean, r: boolean): boolean => k * CHIP_W + (k - 1) * CHIP_GAP + (Number(l) + Number(r)) * (MORE_W + CHIP_GAP) <= room + 0.5;
  const shown = (from: number): number => { let k = n - from; while (k > 1 && !fits(k, from > 0, from + k < n)) k--; return k; };
  let from = 0, k = n;
  if (!fits(n, false, false)) {
    // The last page starts where it is full.
    let lastFrom = 1;
    while (lastFrom < n - 1 && !fits(n - lastFrom, true, false)) lastFrom++;
    from = Math.max(0, Math.min(S.trayFrom, lastFrom));
    // A ship newly picked up on the sea, or by Tab, brings its page into view.
    const sel = [...S.scene.sel].join(',');
    if (sel !== S.traySel) {
      S.traySel = sel;
      const at = chips.findIndex((c) => S.scene.sel.has(Number(c.dataset.unit)));
      if (at >= 0 && (at < from || at >= from + shown(from))) {
        from = Math.min(at, lastFrom);
        while (from > 0 && from + shown(from) > at + 1 && from - 1 + shown(from - 1) > at) from--;
        from = Math.min(from, lastFrom);
      }
    }
    k = shown(from);
  }
  S.trayFrom = from; S.trayShown = k;
  chips.forEach((c, i) => { const d = i < from || i >= from + k ? 'none' : ''; if (c.style.display !== d) c.style.display = d; });
  L.hidden = from === 0; R.hidden = from + k >= n;
  const lt = `◂ ${from} more`, rt = `${n - from - k} more ▸`;
  if (L.textContent !== lt) L.textContent = lt;
  if (R.textContent !== rt) R.textContent = rt;
}

/** HAND IT TO THE STAFF's question, with the staff's chance now (the UI director's words, ruling 226, 5). */
function handQuestion(sim: SeaSim): string {
  const p = S.scene.me === 'att' ? sim.handChance() : 1 - sim.handChance();
  const chance = p >= 0.995 ? 'They are sure of it.' : p <= 0.005 ? 'They give you no chance of it.' : `They give you about ${Math.round(p * 100)}% to win the action.`;
  return sim.fired() ? `Hand the action to the staff? They settle the rest at once, from where it stands now. ${chance}`
    : `Hand the action to the staff? No shot has been fired: they settle the whole action at once. ${chance}`;
}

/** WITHDRAW's question, its label and the button that agrees. Once the enemy has broken off the button ends the chase
 *  and the sea is the player's (b458, 1), so it and its question say END THE CHASE, in plain style: ending a chase that
 *  is won loses nothing, and "break off" at sea already means R and a beaten fleet's state (the UI director, b467). */
const WITHDRAW_ASK = { text: 'Withdraw the fleet? The action ends and the sea is theirs.', label: 'Withdraw the fleet?', yes: 'Withdraw' };
const LET_GO_ASK = { text: 'End the chase? The action ends here and the sea is yours.', label: 'End the chase?', yes: 'End the chase' };
/** The top bar's button, as it reads in the chase and out of it. */
const WITHDRAW_WORD = 'Withdraw', END_CHASE_WORD = 'End the chase';
/** WITHDRAW while the player's own fleet is breaking off: there is nothing left to withdraw, so the button stands down. */
const ALREADY_GOING = 'Your fleet is already breaking off.';
const ACTION_OVER = 'The action is over.';

/** Puts a question to the player and stops the clock while it stands; the answer that changes nothing starts it again. */
function ask(which: 'asking' | 'handing'): void {
  if (S.asking || S.handing) return;
  if (which === 'asking') {
    const broken = S.sim!.brokenSide;
    // The player's own fleet breaking off has nothing to withdraw: no question promises an ending that cannot come.
    if (broken === S.scene.me || S.sim!.over) return;
    const q = broken !== null ? LET_GO_ASK : WITHDRAW_ASK;
    document.getElementById('waskText')!.textContent = q.text;
    document.getElementById('wask')!.setAttribute('aria-label', q.label);
    document.querySelector<HTMLElement>('[data-act="wgo"]')!.textContent = q.yes;
  }
  S.resume = S.clock.speed === 0 ? 0 : S.clock.speed;
  S.clock.set(0);
  S[which] = true;
  guard(); updateTop();
  document.querySelector<HTMLElement>(which === 'handing' ? '[data-act="sstay"]' : '[data-act="wstay"]')?.focus();
}
function keepFighting(): void {
  if (!S.asking && !S.handing) return;
  S.asking = false; S.handing = false;
  if (S.resume !== 0) S.clock.set(S.resume);
  updateTop();
}

function updateTop(force = false): void {
  const sim = S.sim!;
  const el = (id: string): HTMLElement => document.getElementById(id)!;
  // The time only, as the air's clock: actions end long before the limit, which the hover names (sea newcomer 4).
  el('time').textContent = mmss(sim.t);
  // The clock says what it is doing (ruling 226, 8): PAUSED or RUNNING over the time, the first segment Paused when lit.
  // Once the result is in it says ACTION OVER, as the tray does, over the time the action ended at, and nothing is lit
  // (the design director, b476: both fields end the same way).
  const over = !!sim.over, paused = !over && S.clock.speed === 0, state = el('clockState');
  const sw = over ? 'Action over' : paused ? 'Paused' : 'Running';
  if (state.textContent !== sw) { state.textContent = sw; state.classList.toggle('is-paused', paused); }
  const pw = paused ? 'Paused' : 'Pause';
  if (el('pauseWord').textContent !== pw) el('pauseWord').textContent = pw;
  // How near each fleet is to breaking off (ruling 226, 3): a bar a side in its colour, no numbers, no hidden threshold.
  const me = S.scene.me, them: Side = me === 'att' ? 'def' : 'att';
  // The balance bar, as on land: the card's odds and the odds standing now, the attacker's to the defender's as the
  // campaign counts them, and under it each side's share at those odds, ours from the left, with the card's mark.
  // Now is the card's odds moved by what each side has lost, as the staff reckons it when asked to take over (the
  // whole wear, aircraft included), so before a shot the two agree and the bar and the staff's chance tell one story.
  // Once the result is in, the odds and the bar keep the last figure they showed (b476).
  if (!over || !el('ratio').dataset.was) {
    const k = sim.ticket.reckoning.odds, nowOdds = (k * (1 - sim.lostShare('att'))) / Math.max(1e-6, 1 - sim.lostShare('def'));
    const share = (o: number): number => (me === 'att' ? o : 1) / (1 + o);
    el('balMe').style.width = `${(share(nowOdds) * 100).toFixed(1)}%`;
    el('balThem').style.width = `${((1 - share(nowOdds)) * 100).toFixed(1)}%`;
    el('balMark').style.left = `${(share(k) * 100).toFixed(1)}%`;
    // Where the middle of the bar is narrow, the two odds fold at their dot, and the dot goes (the fold rule).
    const ratio = facts([`Card ${odds(k)} to 1`, `now ${odds(nowOdds)} to 1`]);
    if (el('ratio').dataset.was !== ratio) { el('ratio').dataset.was = ratio; el('ratio').innerHTML = ratio; }
  }
  el('brkMe').style.width = `${(sim.nearBreak(me) * 100).toFixed(1)}%`;
  el('brkThem').style.width = `${(sim.nearBreak(them) * 100).toFixed(1)}%`;
  // While a rush runs no speed button is lit: the chip over the sea is the one sign of the clock's speed (the UI
  // director, b467). The pause lights as ever; 1, 2 or 3 in the chase ends the rush and lights its button.
  // Once the action is over the speed buttons go off, and say why on the hover (b476).
  const rushing = S.clock.speed > 0 && S.clock.rush > 1;
  document.querySelectorAll<HTMLButtonElement>('[data-clock]').forEach((x) => {
    x.classList.toggle('is-lit', !over && !rushing && Number(x.dataset.clock) === S.clock.speed);
    if (x.disabled !== over) {
      if (over) { x.dataset.title = x.title; x.title = ACTION_OVER; } else x.title = x.dataset.title ?? x.title;
      x.disabled = over;
    }
  });
  (el('wask') as HTMLElement).hidden = !S.asking;
  // While the player's own fleet is breaking off WITHDRAW has nothing to do, so it is greyed and says why.
  const wd = document.querySelector<HTMLButtonElement>('[data-act="withdraw"]')!, going = sim.brokenSide === me;
  if (wd.disabled !== going) { wd.disabled = going; if (going) wd.title = ALREADY_GOING; else wd.removeAttribute('title'); }
  // Once the enemy has broken off the button ends the chase, in plain style (the UI director, b467).
  // The button holds both its words in one box from the first second, the one not in use hidden, so it keeps one width
  // all action and the bar is laid once (the UI director, b467: sea.css .wdbtn).
  const ending = sim.brokenSide === them && !sim.over;
  if (wd.classList.contains('tc-btn--grave') === ending) {
    wd.classList.toggle('tc-btn--grave', !ending);
    const [a, b] = wd.querySelectorAll<HTMLElement>('.wd-word');
    a.classList.toggle('is-off', ending); b.classList.toggle('is-off', !ending);
    if (ending) { a.setAttribute('aria-hidden', 'true'); b.removeAttribute('aria-hidden'); } else { b.setAttribute('aria-hidden', 'true'); a.removeAttribute('aria-hidden'); }
  }
  (el('sask') as HTMLElement).hidden = !S.handing;
  if (S.handing) { const q = handQuestion(sim); if (el('saskText').textContent !== q) el('saskText').textContent = q; }
  // The foot of what stands over the top of the sea, so the far ports' name and the opening view keep clear of it.
  const ftop = document.querySelector<HTMLElement>('.sea .ftop');
  if (ftop && canvas) S.scene.clearTop = Math.max(50, ftop.getBoundingClientRect().bottom - canvas.getBoundingClientRect().top);
  // The tag at the top of the sea (ruling 226, 8 and 1): paused, or time running fast until the fleets meet, or a
  // moment's word as the clock starts. Not while a question stands: the question is the one voice then.
  const tagEl = el('ftag');
  let tag = '';
  if (!S.asking && !S.handing && !sim.over) {
    if (paused) tag = `Paused ${cap('Space')}`;
    else if (S.closing && !S.approachByHand) tag = `×${APPROACH} until the fleets meet ${cap('1')}`;
    else if (S.chasing) tag = `×${APPROACH} until the chase is over ${cap('1')}`;
    else if (S.tag && now() < S.tag.until) tag = esc(S.tag.text);
  }
  if (tagEl.innerHTML !== tag) tagEl.innerHTML = tag;
  tagEl.hidden = !tag;
  // Ours at three quarters of the way to breaking off: a line in amber that stands until it passes, and goes the moment
  // either fleet breaks off (b458, 4): the player never sees it beside the enemy's break.
  const warn = !sim.over && sim.brokenSide === null && sim.nearBreak(me) >= 0.75 && sim.nearBreak(me) < 1;
  el('fwarn').hidden = !warn;
  if (warn && !el('fwarn').textContent) el('fwarn').textContent = 'Your fleet is near breaking off: pull back the worn, or hand it to the staff.';
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

/**
 * The title's facts fold at their dot, on purpose, only where the bar's middle would otherwise be squeezed under its
 * floor and push the buttons off the bar: folded, the title's box is as wide as its longest line and the middle takes
 * the rest (the UI director, keep-together review). Worked out again when the window changes size.
 */
function fitTitle(): void {
  const bar = document.querySelector<HTMLElement>('.sea .bbar'), title = bar?.querySelector<HTMLElement>('.btitle'), acts = bar?.querySelector<HTMLElement>('.bacts');
  const clock = bar?.querySelector<HTMLElement>('.bclock');
  if (!bar || !title || !acts || !clock) return;
  title.classList.remove('is-folded', 'is-small');
  clock.classList.remove('is-folded');
  const pad = parseFloat(getComputedStyle(bar).paddingRight) || 0;
  const over = (): boolean => acts.getBoundingClientRect().right > bar.getBoundingClientRect().right - pad + 0.5;
  // First the facts fold; then, for a sea with a long name ("the Central Mediterranean"), the title takes the type
  // scale's next size down, as it does under 1440, so the buttons are never pushed off the bar.
  if (over()) title.classList.add('is-folded');
  if (over()) title.classList.add('is-small');
  // Where a break gauge would still be thinner than 56, the facts fold at their dot too, from the first second; what is
  // short after that stands (the UI director, b467). The button already holds its longer word's width (.wdbtn).
  const gauge = bar.querySelector<HTMLElement>('.bbar2')?.getBoundingClientRect().width ?? Infinity;
  if (gauge < GAUGE_FLOOR) title.classList.add('is-folded');
  // Last, the clock's box gives up the room it holds for ACTION OVER on one line (b476) wherever that room would push the
  // buttons off the bar or, on a narrow window, lay the bar in one more row; it keeps RUNNING's width, the widest word
  // while the action runs, and the ending's two words fold onto two lines over the time.
  const short = over(), tall = bar.getBoundingClientRect().height;
  clock.classList.add('is-folded');
  if (!short && bar.getBoundingClientRect().height >= tall) clock.classList.remove('is-folded');
}
/** The thinnest a break gauge should be before the title's facts fold to give it room (the UI director, b467). */
const GAUGE_FLOOR = 56;

function fit(first = false): void {
  const r = canvas.getBoundingClientRect();
  const dpr = Math.min(2, window.devicePixelRatio || 1);
  canvas.width = Math.round(r.width * dpr); canvas.height = Math.round(r.height * dpr);
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  S.scene.W = r.width; S.scene.H = r.height;
  if (first) whole();
}
const SIZE = SEA.field.size;
/** The closest the view goes, in screen pixels a metre. */
const MAX_Z = 0.6;
function whole(): void { S.scene.cam = { x: SIZE / 2, y: SIZE / 2, z: Math.min(S.scene.W / SIZE, (S.scene.H - 60) / SIZE) * 0.97 }; }
/** How much of the view the two fleets take when framed: about two thirds of its width, or most of the sea's height
 *  under the dispatch strip, whichever binds (the art director, keep-together 2). */
const FILL_W = 0.68, FILL_H = 0.9;
/** Where the view should stand to frame both fleets: every live piece as it is drawn, clear of the dispatch strip and
 *  the far ports' name under it (the UI director, sea-226 mend 2). Null with nothing afloat. */
function frameTarget(): Camera | null {
  const sc = S.scene, lay = sc.layout;
  const us = S.sim!.units.filter(live);
  if (!us.length) return null;
  let ax = Infinity, bx = -Infinity, ay = Infinity, by = -Infinity;
  const add = (x: number, y: number): void => { ax = Math.min(ax, x); bx = Math.max(bx, x); ay = Math.min(ay, y); by = Math.max(by, y); };
  for (const u of us) {
    add(u.x, u.y);
    const p = lay?.pieces.get(u.id);
    if (p) for (const [sx, sy] of p.ships) { const r = p.L / 2 + 4; add(...toField(sc, sx - r, sy - r)); add(...toField(sc, sx + r, sy + r)); }
  }
  const top = (sc.clearTop ?? 50) + 30, foot = 24;
  const z = Math.max(minZoom(), Math.min(MAX_Z, (FILL_W * sc.W) / Math.max(1, bx - ax), (FILL_H * (sc.H - top - foot)) / Math.max(1, by - ay)));
  return { x: (ax + bx) / 2, y: (ay + by) / 2 + (top - foot) / 2 / z, z };
}
/** Each action opens framed on the two fleets; WHOLE SEA stays a button (the art director, 1). */
function frameFleets(): void {
  // The drawing spaces the hulls by their size on the screen, so the frame and the drawing are worked out together.
  for (let i = 0; i < 4; i++) {
    S.scene.layout = layoutSea(S.sim!, S.scene);
    const t = frameTarget();
    if (!t) { whole(); return; }
    Object.assign(S.scene.cam, t);
    clampCam();
  }
}
/** The view eases after the fleets as they close, a few seconds behind them, while the player leaves it to the page. */
function followFleets(dt: number): void {
  const t = frameTarget();
  if (!t) return;
  const c = S.scene.cam, k = 1 - Math.exp(-dt / 0.6);
  c.z = Math.exp(Math.log(c.z) + (Math.log(t.z) - Math.log(c.z)) * k);
  c.x += (t.x - c.x) * k; c.y += (t.y - c.y) * k;
  clampCam();
}
/** The player has moved the view themselves: it stays where they put it. */
const byHand = (): void => { S.follow = false; };
function minZoom(): number { return Math.min(S.scene.W / SIZE, S.scene.H / SIZE) * 0.8; }
function zoomAt(px: number, py: number, k: number): void {
  const c = S.scene.cam;
  const [fx, fy] = toField(S.scene, px, py);
  c.z = Math.max(minZoom(), Math.min(MAX_Z, c.z * k));
  const [nx, ny] = toField(S.scene, px, py);
  c.x += fx - nx; c.y += fy - ny;
  clampCam();
}
/** The view stays on the board, as the land page's on its field: zoomed in, no desk shows past its edges; zoomed out,
 *  it stays centred. */
function clampCam(): void {
  const c = S.scene.cam;
  const hw = S.scene.W / 2 / c.z, hh = S.scene.H / 2 / c.z;
  c.x = hw * 2 >= SIZE ? SIZE / 2 : Math.max(hw, Math.min(SIZE - hw, c.x));
  c.y = hh * 2 >= SIZE ? SIZE / 2 : Math.max(hh, Math.min(SIZE - hh, c.y));
}

// ---- Orders -----------------------------------------------------------------------------------------------------------

function chosen(): number[] { return [...S.scene.sel].filter((id) => { const u = unit(id); return !!u && orderable(u); }); }
/** The ships H and R act on: those in hand; in a chase with none in hand, the whole fleet, as the plain order would have
 *  it in hand, so the order lets the beaten enemy go (the design director, b467 snag b). */
function chosenOrFleet(): number[] {
  const c = chosen(), sim = S.sim;
  if (c.length || !sim || sim.over || sim.brokenSide === null || sim.brokenSide === S.scene.me) return c;
  return sim.units.filter((u) => u.side === S.scene.me && orderable(u)).map((u) => u.id);
}
// In the chase an order no longer drops the clock (b458, 1): one to hold, break off or steer for a point lets the beaten
// enemy go and the sim ends the action there (sim.ts letGo), so the after card comes at once; one to attack it is a
// pursuit, and the chase runs on fast. Only the speed keys take the chase's clock in hand (setClock).
const give = (c: Command | { do: 'sea:speed'; units: number[]; speed: SeaSpeed }): boolean => S.sim!.command(c as Command, S.scene.me);
/** A piece the player may take in hand: their own, afloat and not breaking off. */
const mineToTake = (id: number): boolean => { const u = unit(id); return !!u && u.side === S.scene.me && orderable(u); };

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
    resting = null;
    const touch = e.pointerType === 'touch';
    down = { x: px, y: py, button: e.button, touch, moved: false, px, py };
    if (touch) hold = window.setTimeout(() => { if (down && !down.moved) { orderAt(down.x, down.y); down = null; } }, 450);
  });
  canvas.addEventListener('pointermove', (e) => {
    const sim = S.sim;
    if (!sim) return;
    const [px, py] = pos(e);
    const [fx, fy] = toField(sc, px, py);
    if (!down) { resting = [px, py]; rehover(); return; }
    if (Math.hypot(px - down.x, py - down.y) > 6) down.moved = true;
    if (!down.moved) return;
    if (down.touch || down.button === 1) { byHand(); sc.cam.x -= (px - down.px) / sc.cam.z; sc.cam.y += (py - down.py) / sc.cam.z; clampCam(); }
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
    // The pointer rests where it was let go, if that is on the sea: the hover follows what comes under it from there.
    resting = !d.touch && px >= 0 && py >= 0 && px <= sc.W && py <= sc.H ? [px, py] : null;
    if (!resting) sc.hover = null;
    if (d.touch) { if (!d.moved) pickAt(px, py, false); return; }
    if (d.button === 0) {
      if (sc.box) {
        const [a, b] = toScreen(sc, sc.box[0], sc.box[1]), [c, d] = toScreen(sc, sc.box[2], sc.box[3]);
        const got = piecesIn(sc, a, b, c, d, mineToTake);
        if (!e.shiftKey) sc.sel.clear();
        for (const id of got) sc.sel.add(id);
        sc.box = null;
      } else pickAt(px, py, e.shiftKey);
    } else if (d.button === 2) {
      // In the chase a right-drag shorter than a hull is a right-click whose pointer slipped: it gives the click's order
      // where the button went down (b458, 1), so a press on a fleeing ship that drifts a few pixels still engages it.
      const chase = sim.brokenSide !== null && sim.brokenSide !== sc.me;
      if (sc.line && chase && Math.hypot(px - d.x, py - d.y) < hullOnScreen()) { sc.line = null; orderAt(d.x, d.y); } else if (sc.line) {
        const ids = chosen();
        if (ids.length) give({ do: 'line', units: ids, x0: sc.line.x0, y0: sc.line.y0, x1: sc.line.x1, y1: sc.line.y1 });
        sc.line = null;
      } else orderAt(px, py);
    }
    updateTray(true); updateTop();
  };
  canvas.addEventListener('pointerup', up);
  // The hover goes the moment the pointer leaves the sea, as hovers do everywhere else in the game: a stale one reads as
  // a fact about a ship the player is no longer pointing at (the UI director, 2b1f0e3).
  canvas.addEventListener('pointerleave', () => { resting = null; if (!down) { sc.hover = null; canvas.style.cursor = 'default'; } });
  canvas.addEventListener('pointercancel', () => { down = null; resting = null; sc.hover = null; sc.box = null; sc.line = null; });
  canvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    byHand();
    const [px, py] = pos(e);
    const notched = e.deltaMode !== 0 || (e.deltaX === 0 && Math.abs(e.deltaY) >= 40 && Number.isInteger(e.deltaY));
    if (e.ctrlKey || notched) zoomAt(px, py, Math.exp(-e.deltaY * (e.ctrlKey ? 0.01 : 0.0015)));
    else { sc.cam.x += e.deltaX / sc.cam.z; sc.cam.y -= e.deltaY / sc.cam.z; clampCam(); }
  }, { passive: false });
  window.onresize = (): void => { if (S.screen !== 'choose') { fitTitle(); fit(); trayMore(); } };
  app.querySelectorAll<HTMLElement>('[data-clock]').forEach((el) => el.addEventListener('click', () => setClock(Number(el.dataset.clock) as Speed)));
  app.querySelectorAll<HTMLElement>('[data-zoom]').forEach((el) => el.addEventListener('click', () => {
    byHand();
    const k = Number(el.dataset.zoom);
    if (k === 0) whole(); else zoomAt(sc.W / 2, sc.H / 2, k > 0 ? 1.4 : 1 / 1.4);
  }));
  app.querySelectorAll<HTMLElement>('[data-way]').forEach((el) => el.addEventListener('click', () => { give({ do: 'sea:speed', units: chosen(), speed: el.dataset.way as SeaSpeed }); updateTop(); }));
  // HAND IT TO THE STAFF and WITHDRAW each ask once, the clock stopped while the question stands; KEEP FIGHTING, the
  // default, and Esc answer no (ruling 226, 5).
  app.querySelector('[data-act="dice"]')!.addEventListener('click', () => ask('handing'));
  app.querySelector('[data-act="sstay"]')!.addEventListener('click', keepFighting);
  app.querySelector('[data-act="sgo"]')!.addEventListener('click', () => { if (now() < S.guardUntil) return; S.handing = false; give({ do: 'dice' }); });
  app.querySelector('[data-act="withdraw"]')!.addEventListener('click', () => ask('asking'));
  app.querySelector('[data-act="wstay"]')!.addEventListener('click', keepFighting);
  app.querySelector('[data-act="wgo"]')!.addEventListener('click', () => { if (now() < S.guardUntil) return; give({ do: 'withdrawAll', side: sc.me }); keepFighting(); });
  const tray = document.getElementById('tray')!;
  S.trayFrom = 0; S.traySel = '';
  document.getElementById('tmoreR')!.addEventListener('click', () => { S.trayFrom += S.trayShown; trayMore(); });
  document.getElementById('tmoreL')!.addEventListener('click', () => { S.trayFrom = Math.max(0, S.trayFrom - S.trayShown); trayMore(); });
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
    if (u) { byHand(); sc.cam.x = u.x; sc.cam.y = u.y; sc.cam.z = Math.max(sc.cam.z, 0.12); clampCam(); }
  });
}

/** Where the pointer rests on the sea, in screen pixels, while it is over it with no button held; null otherwise. */
let resting: [number, number] | null = null;
/**
 * The hover is the piece under the pointer now. The ships move under a pointer at rest, and the view moves with them,
 * so it is looked for again every frame, not only when the pointer moves: the line goes when its ship sails out from
 * under the pointer, and never stands over open water for a ship the pointer has left (b458, 6). The whole drawn piece
 * counts, every hull and the water between, as its outline shows (the UI director, keep-together 2).
 */
function rehover(): void {
  const sc = S.scene;
  if (!resting || !S.sim || S.screen !== 'battle') return;
  const id = pieceAt(sc, resting[0], resting[1], (i) => { const x = unit(i); return !!x && live(x); });
  const u = id !== null ? unit(id) : undefined;
  sc.hover = u ? u.id : null;
  const cur = u && u.side !== sc.me && sc.sel.size ? 'crosshair' : u && u.side === sc.me ? 'pointer' : 'default';
  if (canvas.style.cursor !== cur) canvas.style.cursor = cur;
}
/** The longest hull of the ships in hand as the sea draws them, and never under the pick's own margin each way: a
 *  right-drag shorter than this, in the chase, is a right-click. */
const hullOnScreen = (): number => Math.max(2 * PICK, ...chosen().map((id) => S.scene.layout?.pieces.get(id)?.L ?? 0));

function pickAt(px: number, py: number, add: boolean): void {
  const sc = S.scene;
  const id = pieceAt(sc, px, py, mineToTake);
  if (id === null) { if (!add) sc.sel.clear(); return; }
  if (add) { if (sc.sel.has(id)) sc.sel.delete(id); else sc.sel.add(id); } else { sc.sel.clear(); sc.sel.add(id); }
}

function orderAt(px: number, py: number): void {
  const sim = S.sim!, sc = S.scene;
  const ids = chosen();
  if (!ids.length) return;
  const [fx, fy] = toField(sc, px, py);
  const foeId = pieceAt(sc, px, py, (i) => { const x = unit(i); return !!x && x.side !== sc.me && live(x); });
  const foe = foeId !== null ? sim.units.find((x) => x.id === foeId) : undefined;
  if (foe) { if (give({ do: 'attack', units: ids, target: foe.id })) sc.ping = { x: foe.x, y: foe.y, at: now(), kind: 'attack' }; }
  else if (give({ do: 'move', units: ids, x: fx, y: fy })) sc.ping = { x: fx, y: fy, at: now(), kind: 'move' };
}

window.addEventListener('keydown', (e) => {
  if (e.key === 'Enter') {
    e.preventDefault();
    // Enter presses the lit button of the card that stands, and nothing on the field (the keys rule, 21:42): on the
    // after card it is CONTINUE, once the card has stood half a second and only for a fresh press, never one held
    // from before it came up (the key guard).
    if (S.screen === 'after' && !e.repeat && now() >= S.guardUntil) document.querySelector<HTMLElement>('.after [data-act="continue"]')?.click();
    return;
  }
  // Esc answers a standing question at once, guard or not: it is the answer that changes nothing.
  if (e.key === 'Escape' && (S.asking || S.handing)) { keepFighting(); return; }
  if (now() < S.guardUntil) { if (e.key === 'Tab' || e.key === ' ') e.preventDefault(); return; }
  if (S.screen !== 'battle' || !S.sim) return;
  const sc = S.scene, sim = S.sim;
  const key = e.key.length === 1 ? e.key.toLowerCase() : e.key;
  const pan = 120 / sc.cam.z;
  switch (key) {
    case ' ': e.preventDefault(); setClock('toggle'); break;
    case '1': setClock(1); break;
    case '2': setClock(2); break;
    case '3': setClock(4); break;
    case 'h': give({ do: 'hold', units: chosenOrFleet() }); break;
    case 'r': give({ do: 'withdraw', units: chosenOrFleet() }); break;
    case 'Escape': if (S.asking || S.handing) keepFighting(); else { sc.sel.clear(); sc.line = null; sc.box = null; } break;
    case 'Tab': {
      e.preventDefault();
      const mine = sim.units.filter((u) => u.side === sc.me && orderable(u));
      if (!mine.length) break;
      const cur = [...sc.sel][0];
      const i = mine.findIndex((u) => u.id === cur);
      const next = mine[(i + (e.shiftKey ? mine.length - 1 : 1)) % mine.length];
      sc.sel.clear(); sc.sel.add(next.id);
      byHand(); sc.cam.x = next.x; sc.cam.y = next.y; clampCam();
      break;
    }
    case 'ArrowLeft': case 'a': byHand(); sc.cam.x -= pan; clampCam(); break;
    case 'ArrowRight': case 'd': byHand(); sc.cam.x += pan; clampCam(); break;
    case 'ArrowUp': case 'w': byHand(); sc.cam.y += pan; clampCam(); break;
    case 'ArrowDown': case 's': byHand(); sc.cam.y -= pan; clampCam(); break;
    case '+': case '=': byHand(); zoomAt(sc.W / 2, sc.H / 2, 1.25); break;
    case '-': byHand(); zoomAt(sc.W / 2, sc.H / 2, 0.8); break;
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
  // Until the guns meet, the core clock runs fast on its own, then drops back to the speed the player set (ruling 226,
  // 1): a strike or a raid in the air does not stop it, since they are counted, not flown (b458, 2a). Once a fleet has
  // broken off, time runs fast through the chase until the action ends, unless the player takes the chase's clock in
  // hand with a speed key (b402, 3; b458, 1: an order lets the enemy go or pursues it, and neither slows the clock).
  // The chase runs at ×8 whatever speed the clock was set to before the break (the design director, b467): at ×32 the
  // player could not strike or choose, and the chase should read the same every time. The approach does too, as the
  // land's does ("×8 until the fleets meet"); the speed the player chose takes over when the rush ends.
  const chase = !sim.over && sim.brokenSide !== null && !S.chaseByHand;
  S.clock.rush = chase || (sim.quiet && !S.approachByHand) ? APPROACH / (S.clock.speed || 1) : 1;
  S.clock.advance(sim, dt);
  const quiet = !sim.over && sim.quiet;
  if (S.closing && !quiet && !sim.over) S.tag = { text: `The action begins · Running ×${S.clock.speed}`, until: t + 2.5 };
  S.closing = quiet;
  S.chasing = !sim.over && sim.brokenSide !== null && !S.chaseByHand;
  const v = sim.view(S.scene.me);
  takeEvents(S.scene, sim, v.events, t);
  // At the press of Space, one line so the wait has a meaning (the UI director, 6).
  if (sim.t > 0 && S.dispatch?.t === 0 && !S.dispatch.unit) {
    const m = minutesToRange(sim);
    S.dispatch = m > 0 ? { t: sim.t, what: 'contact', text: `The fleets are closing: in range in about ${m === 1 ? 'a minute' : `${m} minutes`}.`, unit: -1 } : null;
    S.dispatchAge = 0;
  }
  // Our own fleet near breaking off is said on the standing warning above the field, not as a dispatch too; and once a
  // fleet has broken off, neither side's near-breaking line is said at all (b458, 4).
  const broke = sim.brokenSide !== null;
  for (const e of v.events) if (e.text && !(e.what === 'nearBreak' && (e.side === S.scene.me || broke))) { S.dispatch = e; S.dispatchAge = 0; }
  if (broke && S.dispatch?.what === 'nearBreak') { S.dispatch = null; document.getElementById('dispatch')!.innerHTML = ''; }
  if (S.clock.speed > 0) S.dispatchAge += dt;
  for (const id of [...S.scene.sel]) { const u = unit(id); if (!u || !orderable(u)) S.scene.sel.delete(id); }
  if (S.follow) followFleets(dt);
  rehover();
  drawSea(ctx, sim, S.scene);
  updateTop();
  updateTray();
  if (v.over && S.screen === 'battle') showAfter();
}

/** How much faster time runs until the action begins: the long approach, about four battle minutes with the fleets
 *  keeping together at their slowest ship's speed, passes in about half a minute at ×1. The approach itself stays as
 *  it is, since it is part of the fleets' balance (parity). */
const APPROACH = 8;

/** Sets the clock, and as it starts from stopped says so for a moment: "RUNNING ×2" (ruling 226, 8). */
function setClock(to: Speed | 'toggle'): void {
  if (S.asking || S.handing || S.sim?.over) return;
  const was = S.clock.speed, hand = S.chaseByHand || S.approachByHand;
  // Choosing a speed takes the chase in hand; a pause does not, however it is given (button or Space).
  if (to !== 'toggle' && to !== 0 && S.sim?.brokenSide) S.chaseByHand = true;
  // One rule for both rushes (the UI director, b467): once the approach has begun, 1, 2 or 3 ends its rush at once and
  // lights that button. Before the first second a speed key starts the clock, the approach still rushed.
  if (to !== 'toggle' && to !== 0 && S.sim && S.sim.t > 0 && S.sim.quiet) S.approachByHand = true;
  if (to === 'toggle') S.clock.toggle(); else S.clock.set(to);
  // The chip goes as the rush ends, and the speed taken in its place is said for a moment, as on starting the clock.
  if (S.clock.speed > 0 && (was === 0 || (!hand && (S.chaseByHand || S.approachByHand)))) S.tag = { text: `Running ×${S.clock.speed}`, until: now() + 2 };
  updateTop();
}

/** About how many minutes on the battle clock before the nearest guns of either side reach the other: the gap closed
 *  at the speed each ship then makes, its fleet's while it keeps with it (sim.fleetSpeed), held ships closing at none
 *  (the design director: it never disagrees with the clock on screen). */
function minutesToRange(sim: SeaSim): number {
  let best = Infinity;
  const us = sim.units.filter(live);
  const way = (u: SeaUnit): number => (sim.orderOf(u.id)?.kind === 'hold' ? 0 : sim.fleetSpeed(u) ?? SEA.classes[u.extra.cls as SeaClass].speed * SEA.speeds[u.extra.speed as SeaSpeed]);
  for (const a of us) for (const b of us) {
    if (a.side !== S.scene.me || b.side === a.side) continue;
    const A = SEA.classes[a.extra.cls as SeaClass], B = SEA.classes[b.extra.cls as SeaClass];
    const reach = Math.max(a.extra.cls === 'cv' ? 0 : A.reach, b.extra.cls === 'cv' ? 0 : B.reach);
    const close = way(a) + way(b);
    if (close > 0) best = Math.min(best, (Math.hypot(a.x - b.x, a.y - b.y) - reach) / close);
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
  const bySide = (side: Side, what: 'sunk' | 'burnt' | 'hurt'): SeaShips => {
    const n: SeaShips = { bb: 0, cv: 0, ca: 0, dd: 0 };
    for (const f of t.place.fleets) if (f.side === side) for (const c of ['bb', 'cv', 'ca', 'dd'] as SeaClass[]) n[c] += x[what][f.id]?.[c] ?? 0;
    return n;
  };
  const count = (n: SeaShips): number => n.bb + n.cv + n.ca + n.dd;
  const lost = (side: Side): string => andList(shipBits(bySide(side, 'sunk'))) || 'none';
  const staffLine = r.lines.find((l) => l.startsWith('The staff settled'));
  const noShot = !!staffLine && staffLine.endsWith('no shot had been fired.');
  const noneSunk = count(bySide(me, 'sunk')) + count(bySide(them, 'sunk')) === 0;
  // Fought to the end, the campaign's headline is the one bill of losses (the UI director, round 2): the line says them
  // only where there is no headline, as in practice. Handed to the staff, it says what went down before, which differs.
  const lossLine = x.toDice
    ? noShot ? '' : noneSunk ? 'Before you handed it to the staff, no ship had gone down on either side.' : `Before you handed it to the staff, you had lost ${lost(me)} and ${people(t[them].adj)} ${lost(them)}.`
    : headline ? '' : noneSunk ? 'No ship went down on either side.' : `You lost ${lost(me)} and ${people(t[them].adj)} ${lost(them)}.`;
  // The ships each side took home badly hurt, every one named by its kind (b458, 3): out of the fight burning, or
  // afloat at half its hull or less, so a close victory reads as one. Handed to the staff, only those burning: the
  // staff's settlement may yet sink a ship that was afloat when it took over.
  const repair = repairLines({ att: bySide('att', x.toDice ? 'burnt' : 'hurt'), def: bySide('def', x.toDice ? 'burnt' : 'hurt') }, me, t[them].adj);
  // The staff's line, then the facts, then what stood before the hand-over (the UI director, sea-226 mend 6).
  const list = (x.toDice ? [staffLine, r.lines[0], ...repair, lossLine] : [staffLine, r.lines[0], lossLine, ...repair]).filter((l): l is string => !!l);
  const sides = rep ? `<div class="aside2">${(['att', 'def'] as const).map((sd) => `<div><h3 class="tc-label">${esc(t[sd].adj)} fleet</h3><ul>${rep![sd].map((s) => `<li><b>${esc(s.fleet)}</b> <span>lost ${esc(andList(shipBits(s.sunk)) || 'no ships')}</span></li>`).join('')}</ul></div>`).join('')}</div>` : '';
  const veil = document.createElement('div');
  // In the kit's type like the page it lies over: appended to the body, outside the app, it took the body's face (the UI
  // director, b467 "For Brian" 1: the handed-over card read in a sans face in the published page).
  veil.className = 'veil tc';
  veil.innerHTML = `<div class="after tc-paper" role="dialog" aria-label="Action result">
    <div class="tc-label kick">${rep ? `Fleet action · ${esc(t.date)}` : 'Practice action'}</div>
    <h2 class="tc-title-2">${esc(t.title)}</h2>
    <div class="verdict"><span class="tc-stamp ${won ? 'tc-stamp--won' : ''}">${esc(S.scene.me === 'att' ? level : level.replace('victory', 'X').replace('defeat', 'victory').replace('X', 'defeat'))}</span></div>
    ${headline ? `<p class="told"><b>${esc(headline)}</b></p>` : ''}
    <h3 class="tc-label">On the water</h3><ul class="flines">${list.map((l) => `<li>${esc(l)}</li>`).join('')}</ul>
    <h3 class="tc-label">Against the staff's reckoning</h3>
    <p class="dice">${esc(reckoned(won, S.scene.me === 'att' ? chanceOf(t) : 1 - chanceOf(t)))}${rep ? '' : ' A practice action hands nothing back.'}</p>
    ${sides}
    <div class="afoot"><button class="tc-btn tc-btn--primary tc-btn--lg" data-act="continue">Continue <span class="tc-key">Enter</span></button></div>
  </div>`;
  document.body.appendChild(veil);
  veil.querySelector('[data-act="continue"]')!.addEventListener('click', () => { if (now() < S.guardUntil) return; veil.remove(); showChooser(); });
}

// A handle for the page's own checks (screenshots, the playtester's harness). Not used by the page itself.
(window as unknown as { __sea: unknown }).__sea = { state: S, give, showChooser, startBattle, practiceTicket, PRACTICE, toScreen };

showChooser();
// A page built to open on one practice card (tools/build-sea-battle.mjs --open=N) goes straight into that action; its
// after card's CONTINUE comes back to the chooser with every card on it.
const opening = (window as unknown as { SEA_OPEN?: { practice?: number } }).SEA_OPEN?.practice;
if (opening !== undefined && PRACTICE[opening]) startBattle(practiceTicket(PRACTICE[opening]), null);
requestAnimationFrame(frame);

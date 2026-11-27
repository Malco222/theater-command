// The field battle page: choose one of the campaign's attacks (or a practice ground), fight it on the field, and see
// the campaign's own report of what it made of the result. The first slice of the played battles (ruling 217) was a
// browser page of its own (standalone.ts boots it so); the campaign screen's FIGHT IT YOURSELF opens the same field
// over the campaign for the attack on its card (fightOnField), in a layer of its own so neither page's looks touch
// the other's, and hands the result back through the campaign's own attack order (the design director, b439).
//
// Mouse: left picks up (drag for a box, Shift adds), right orders (on the ground to march, on an enemy to attack, drag
// to draw a line). Keys: Space pause, 1 2 3 speed, H hold, R fall back off the field (the divisions in hand; WITHDRAW
// is the whole army), Tab next, Esc puts down. A plain left click never gives an order, and Enter does nothing. The clock stops
// while a question stands on the field (the design director, b358).

import type { BattleReport, Game } from '../../campaign/types';
import { Clock, type Speed } from '../core/clock';
import { diceChance } from '../core/parity';
import { RATIO, levelOf } from '../core/result';
import { lineUp, unitAt, unitsIn } from '../core/select';
import type { Command, FieldEvent, FieldTicket, Side } from '../core/types';
import type { LandPlace, Terrain } from '../land/field';
import { PRACTICE_GROUNDS, practiceBill, practiceTicket, withTeams, wornWords, type PracticeGround } from '../land/sample';
import { crewLine } from '../land/crews';
import { LandBattle } from '../land/sim';
import { cardKnows, corpsOn, pursuitLine } from '../land/ticket';
import type { LandUnit } from '../land/units';
import { newCampaign, offers, settle, type CardSaid, type Offer } from './campaign';
import { drawFrame, signSvg, toField, toScreen, type Scene } from './draw';
import { paintField, type Painted } from './paint';

const KEY_GUARD = 0.5;
/** Real seconds a dispatch stands, while the clock runs, before it fades. */
const DISPATCH_FOR = 8;
const esc = (s: string): string => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]!);
const num = (n: number): string => Math.round(n).toLocaleString('en-GB');
const mmss = (t: number): string => `${Math.floor(t / 60)}:${String(Math.floor(t % 60)).padStart(2, '0')}`;
const odds = (o: number): string => (o >= 10 ? o.toFixed(0) : o.toFixed(2));
/** A practice ground's name: its ground, or the river crossing and the fortress works, as its card is headed. */
const practiceName = (p: PracticeGround): string => (p.air ? 'Air support' : p.river ? 'A river crossing' : p.fort ? 'Fortress works' : TERRAIN_WORD[p.terrain]);
/** The field's title on a practice ground: "Practice: Marsh" (b408). The after card, already headed "Practice result", gives the ground alone. */
const practiceTitle = (p: PracticeGround): string => `Practice: ${practiceName(p)}`;
/** What the staff makes of a card's chance, as the offer and practice cards say it. */
const staffSays = (c: number): string => (c >= 0.995 ? 'the staff is sure of it' : c <= 0.005 ? 'the staff gives it no chance' : `the staff gives it about ${Math.round(c * 100)}%`);
/** The ground in the province card's own word (data/campaign/ground.json, 'says'). */
const TERRAIN_WORD: Record<Terrain, string> = { plains: 'Open country', forest: 'Forest', hills: 'Hill country', mountain: 'Mountains', marsh: 'Marsh', urban: 'City streets', desert: 'Open desert' };
/** A range as the attack card says it: "between 7.7 and 13.3 to 1" (the UI director, 245e366: every range in words). */
const between = (lo: number, hi: number): string => `between ${lo.toFixed(1)} and ${hi.toFixed(1)} to 1`;
/** Facts in a row that fold whole: the dot between two goes with the fold, never ending or starting a line (brief check 19). */
const facts = (bits: string[]): string => `<span class="facts"><span>${bits.map((b) => `<span class="fact">${b}</span>`).join('')}</span></span>`;
/**
 * The title's second line: where the field is ("Open country · across the Vistula"), under the date, so the title holds
 * three lines at every width and the bar one height (the UI director, after 1969d7b). A practice ground in the open says
 * nothing more than its date.
 */
const placeLine = (t: FieldTicket<LandPlace>): string => {
  const bits = [...(S.offer || t.place.river || t.place.fort ? [esc(TERRAIN_WORD[t.place.terrain])] : []), ...(t.place.river ? [`across the ${esc(t.place.river)}`] : [])];
  return bits.length ? `<span class="tc-data">${facts(bits)}</span>` : '';
};
/** Names in a row as a sentence lists them: "XIX Corps", "XIX Corps and XVI Corps", "I Corps, XIX Corps and XVI Corps". */
const listed = (xs: string[]): string => (xs.length < 2 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
/**
 * Where some of the player's attacking corps cross the river and some come on over dry ground (b464(5), the river bends
 * away from the latter), the opening dispatch says which, in the UI director's words: "XXI Corps must cross the Vistula.
 * XIX Corps comes on over dry ground." Empty where all cross, none do, or the player defends.
 */
const riverWords = (t: FieldTicket<LandPlace>): string => {
  if (!t.place.river || !t.att.human) return '';
  const dry = t.att.forces.filter((f) => f.cross === false), wet = t.att.forces.filter((f) => f.cross !== false);
  if (!dry.length || !wet.length) return '';
  return `${listed(wet.map((f) => f.name))} must cross the ${t.place.river}. ${listed(dry.map((f) => f.name))} ${dry.length === 1 ? 'comes' : 'come'} on over dry ground. `;
};
/** "Warsaw, the capital"; "the town of Gdynia". */
const townWords = (p: LandPlace): string => (p.townSize === 'capital' ? `${esc(p.town!)}, the capital` : `the ${p.townSize ?? 'town'} of ${esc(p.town!)}`);

interface State {
  g: Game;
  screen: 'choose' | 'battle' | 'after';
  offer: Offer | null;
  /** The practice ground being fought, or null on a campaign battle. */
  ground: PracticeGround | null;
  b: LandBattle | null;
  field: Painted | null;
  clock: Clock;
  scene: Scene;
  guardUntil: number;
  dispatch: FieldEvent | null;
  /** Real seconds the dispatch line has stood while the clock ran: it fades after a while so it never reads as stale. */
  dispatchAge: number;
  /** The question standing on the field, open in place: WITHDRAW's, or HAND IT TO THE STAFF's (ruling 226(5)). */
  asking: false | 'withdraw' | 'staff';
  /** Blocks told to hold (H): their cards say so until their next order. */
  holding: Set<number>;
  /** The first card the tray shows, when the army has more than fit (ruling 226(9)). */
  trayFrom: number;
  /** A tag on the field for a moment: RUNNING ×2, THE ARMIES MEET, and when it goes (real seconds). */
  /** A passing word on the field; 'air' marks air support's own, shown even while paused (the player is aiming then). */
  tag: { text: string; until: number; air?: boolean } | null;
  /** The clock was running at the last frame, and the armies had met: for the tags when either changes. */
  wasRunning: boolean;
  met: boolean;
  /** The player chose a speed before the armies met: the rush is over and that speed runs (the UI director, b467). */
  rushByHand: boolean;
  /** Each side's colour on this field (sideColours). */
  colours: Record<Side, string>;
  /** What the chooser offered before the last battle, and how it went: for SINCE YOUR LAST BATTLE (ruling 226(11)). */
  before: { offers: Offer[]; fought: number; how: string; heavier?: boolean } | null;
  /** The speed the clock had when a question stopped it: STAY or Esc starts it again there. */
  resume: Speed | null;
  last: number;
  trayAt: number;
  /** The page is showing and takes the keys: always on its own page; over the campaign, while the field is open. */
  active: boolean;
  /** Opened over the campaign: what CONTINUE on the after card goes back to, with the campaign's report. */
  home: ((report: BattleReport | null) => void) | null;
}

/** Where the page draws: its own page's #app, or the field's layer over the campaign. */
let app: HTMLElement;
let root: Document | ShadowRoot = document;
const byId = (id: string): HTMLElement | null => root.getElementById(id);
const S: State = {
  g: null as unknown as Game, screen: 'choose', offer: null, ground: null, b: null, field: null, clock: new Clock(),
  scene: { cam: { x: 1500, y: 1000, z: 0.3 }, W: 800, H: 600, me: 'att', sel: new Set(), hover: null, box: null, line: null, now: 0, ping: null, colours: null },
  guardUntil: 0, dispatch: null, dispatchAge: 0, asking: false, resume: null, last: 0, trayAt: 0,
  holding: new Set(), trayFrom: 0, tag: null, wasRunning: false, met: false, rushByHand: false, before: null, colours: { att: '#888', def: '#888' },
  active: false, home: null,
};
/**
 * The two sides' colours as the campaign paints them, but where a pair stands too near to tell apart (under 20 by
 * CIEDE2000 for a normal eye or a colour-blind one), the lighter raised in lightness only (the art director, ruling
 * 226(12)). Every other pair stays as the campaign has it.
 */
const PAIR_FIX: Record<string, Record<string, string>> = { FRA: { GER: '#5f84c7' }, ITA: { UK: '#92c495' }, POL: { GER: '#cf848b' } };
function sideColours(t: FieldTicket<LandPlace>): Record<Side, string> {
  const fix = (a: SideTicketLike, b: SideTicketLike): string => PAIR_FIX[a.nat]?.[b.nat] ?? a.color;
  return { att: fix(t.att, t.def), def: fix(t.def, t.att) };
}
type SideTicketLike = { nat: string; color: string };

/** How much faster time runs on its own until the armies meet (ruling 226(1)): the clock at least this fast. */
const RUSH = 4;
const now = (): number => performance.now() / 1000;
/**
 * A speed chosen by the player, by its button or its key (1, 2, 3). Before the armies meet it ends the rush at once and
 * that speed runs, so the press is never lost (the UI director, b467, one rule with the fleet action); the pause does not.
 */
function setSpeed(sp: Speed): void {
  if (sp > 0 && S.b && S.b.contactAt === null) S.rushByHand = true;
  S.clock.set(sp);
}
const guard = (): void => { S.guardUntil = now() + KEY_GUARD; };
/** A page address flag (?setup, ?teams): read when a battle starts, never at load, so the campaign's own page is untouched. */
const flagged = (k: string): boolean => { try { return new URLSearchParams(location.search).has(k); } catch { return false; } };
/** The setup step ends when the clock is first started (release 2): Space, a speed, or the Begin button. */
function beginIfPlacing(): void { if (S.b?.setup) { S.b.command({ do: 'land:begin' }, S.scene.me); S.scene.place = null; } }

// ---- Choosing a battle --------------------------------------------------------------------------------------------

function showChooser(): void {
  S.screen = 'choose'; S.b = null; S.offer = null; S.ground = null;
  guard();
  const g = S.g;
  const list = offers(g);
  const me = g.nations[g.human!];
  const row = (o: Offer, i: number): string => {
    const t = o.ticket, p = t.place;
    // The town's guard on the field (ruling 232) is no corps: the card names it apart.
    const att = corpsOn(t.att), def = corpsOn(t.def), guard = t.def.forces.length > def ? ' and the town guard' : '';
    const bits = [TERRAIN_WORD[p.terrain], p.river ? `across the ${esc(p.river)}${p.bridgeDown ? ', bridges down' : ''}` : '', p.fort ? `fortress works, level ${p.fort}` : '', p.town ? townWords(p) : ''].filter(Boolean);
    const staff = staffSays(diceChance(o.odds, t.reckoning.dice));
    // The campaign lets a corps thrown back attack again while it has the moves for it (mark 11): the card says so.
    const back = S.g.reports.filter((x) => x.turn === S.g.turn && x.region === o.target.id && x.attNat === S.g.human && !x.taken && !x.siege).length;
    const again = back === 0 ? '' : `thrown back ${back === 1 ? 'once' : back === 2 ? 'twice' : `${back} times`} this month`;
    return `<button class="tc-card is-press offer" data-offer="${i}">
      <span class="otop"><b class="tc-title-3">${esc(o.target.name)}</b><span class="tc-data"><b>${odds(o.odds)}</b> to 1</span></span>
      <span class="tc-data omid">${facts(bits)}</span>
      <span class="tc-data olow">${facts([`${att} corps against ${def}${guard}`, ...(again ? [again] : []), staff])}</span></button>`;
  };
  // Each practice ground's card is reckoned as the campaign would reckon the two practice armies there (b400).
  const practice = (p: PracticeGround, i: number): string => {
    const t = practiceTicket(p), k = t.reckoning, worn = wornWords(p.str ?? 0.9);
    // The ground line in the campaign cards' own form, where the card's name does not already say it all.
    const ground = p.river || p.fort ? facts([TERRAIN_WORD[p.terrain], ...(p.river ? ['across a river'] : []), ...(p.fort ? [`fortress works, level ${p.fort}`] : [])]) : '';
    return `<button class="tc-card is-press offer ground" data-ground="${i}">
      <span class="otop"><b class="tc-title-3">${practiceName(p)}</b><span class="tc-data"><b>${odds(k.odds)}</b> to 1</span></span>
      ${ground ? `<span class="tc-data omid">${ground}</span>` : ''}
      <span class="tc-data olow">${facts([`${t.att.forces.length} corps against ${t.def.forces.length}`, ...(worn ? [worn] : []), staffSays(diceChance(k.odds, k.dice))])}</span></button>`;
  };
  app.innerHTML = `<div class="choose tc">
    <header class="chead"><div><div class="tc-label">Field battles · first slice</div><h1 class="tc-title-1">Choose a battle to fight</h1></div></header>
    <div class="cbody">
      <section class="tc-paper intro"><p><b>The campaign settles every battle from its Battle Ticket until you choose to fight one here.</b> The field is drawn from the province: its ground, its river and bridges, its town and its fortress works. Your divisions take the field as the campaign counts them, each drawn to its size. Where an army has few, some fight as their three regiments, named on the field as such. The two armies share out exactly the strength the attack card counted, so the card's odds are the field's. What happens here goes back to the campaign as the same kind of result, and you can hand the rest to the staff at any moment.</p></section>
      ${since(list)}
      <h2 class="tc-label shead">${facts(['Your attacks this month', `${esc(me.name)}, ${esc(list[0]?.ticket.date ?? '')}`])}</h2>
      <div class="offers">${list.length ? list.map(row).join('') : '<p class="tc-data none">No attack can be ordered now. Start the campaign again to fight another.</p>'}</div>
      <div class="again"><button class="tc-btn tc-btn--quiet" data-act="restart">Start the campaign again</button></div>
      <h2 class="tc-label shead">${facts(['Practice grounds', 'The same two armies on every ground', 'Nothing goes back to the campaign'])}</h2>
      <div class="grounds">${PRACTICE_GROUNDS.map(practice).join('')}</div>
    </div></div>`;
  app.querySelectorAll<HTMLElement>('[data-offer]').forEach((el) => el.addEventListener('click', () => {
    const o = list[Number(el.dataset.offer)];
    S.before = { offers: list, fought: o.target.id, how: '' };
    startBattle(o.ticket, o);
  }));
  app.querySelectorAll<HTMLElement>('[data-ground]').forEach((el) => el.addEventListener('click', () => {
    const p = PRACTICE_GROUNDS[Number(el.dataset.ground)];
    const t0 = practiceTicket(p, { humanAtt: true, seed: 1 + Math.floor(Math.random() * 1e6), title: practiceTitle(p) });
    // ?teams (release 2): each side brings a machine-gun team and a mortar team.
    const t = flagged('teams') ? withTeams(t0) : t0;
    startBattle(t, null, p);
  }));
  app.querySelector('[data-act="restart"]')?.addEventListener('click', () => { S.g = newCampaign(S.g.seed + 1); S.before = null; showChooser(); });
}

/**
 * SINCE YOUR LAST BATTLE (the UI director's words for ruling 226(11)): one line for each thing that changed in the
 * attacks on offer, the place first. No band when nothing changed, or before the first battle.
 */
function since(now: Offer[]): string {
  const was = S.before;
  if (!was || !was.how) return '';
  const by = new Map(now.map((o) => [o.target.id, o]));
  // One line per place, with every change on it (the UI director, slice 3): "Przemysl: the attack was thrown back;
  // now 1.56 to 1, from 1.38." The place he fought first.
  const per = new Map<string, string[]>();
  const say = (place: string, text: string): void => { const l = per.get(place) ?? []; l.push(text.replace(/\.$/, '')); per.set(place, l); };
  const offers = [...was.offers].sort((p, q) => (p.target.id === was.fought ? -1 : q.target.id === was.fought ? 1 : 0));
  for (const o of offers) {
    const n = by.get(o.target.id);
    // When the odds rose after a defeat, the line says why (the UI director, after the retest).
    if (o.target.id === was.fought) say(o.target.name, was.how === 'the attack was thrown back.' && n && n.odds > o.odds + 0.005 && was.heavier ? 'the attack was thrown back, but the defenders lost more of their strength than you' : was.how);
    else if (!n) {
      const spent = o.corps.find((c) => c.fought && !now.some((x) => x.corps.includes(c)));
      say(o.target.name, spent ? `${esc(spent.name)} has fought this month and cannot attack again` : 'out of reach this month');
    }
    if (!n) continue;
    if (n.ticket.place.bridgeDown && !o.ticket.place.bridgeDown) say(o.target.name, 'bridges down');
    const ca = corpsOn(n.ticket.att), cd = corpsOn(n.ticket.def);
    if (ca !== corpsOn(o.ticket.att) || cd !== corpsOn(o.ticket.def)) say(o.target.name, `now ${ca} corps against ${cd}`);
    // The odds never split across a fold.
    if (Math.abs(n.odds - o.odds) >= 0.005) say(o.target.name, `<span class="nobr">now ${odds(n.odds)} to 1, from ${odds(o.odds)}</span>`);
  }
  for (const n of now) if (!was.offers.some((o) => o.target.id === n.target.id)) say(n.target.name, 'now in reach');
  const lines = [...per].map(([place, parts]) => `<li><b>${esc(place)}:</b> ${parts.join('; ')}.</li>`);
  return lines.length ? `<section class="tc-paper since"><h2 class="tc-label">Since your last battle</h2><ul>${lines.join('')}</ul></section>` : '';
}

// ---- The battle screen --------------------------------------------------------------------------------------------

let canvas: HTMLCanvasElement;
let ctx: CanvasRenderingContext2D;

function startBattle(t: FieldTicket<LandPlace>, o: Offer | null, ground: PracticeGround | null = null): void {
  const b = new LandBattle(t);
  // A defence the card did not know (b453): its blocks show only where they stand until his have fought them (b464(2)),
  // all but those of the corps the card identified, which show as the card shows them (b476(3)).
  b.fog = !!o?.band;
  if (b.fog) b.seen = cardKnows(S.g, t);
  S.b = b; S.offer = o; S.ground = o ? null : ground; S.screen = 'battle';
  S.field = paintField(b.field, t.place.terrain, t.seed);
  S.scene.me = t.att.human ? 'att' : 'def';
  S.scene.sel = new Set(); S.scene.hover = null; S.scene.box = null; S.scene.line = null; S.scene.ping = null; S.scene.air = null;
  // The field opens paused: orders first, then the clock.
  S.clock = new Clock(); S.clock.set(1); S.clock.toggle();
  S.asking = false; S.holding = new Set(); S.trayFrom = 0; S.tag = null; S.wasRunning = false; S.met = false; S.rushByHand = false;
  const place = t.place.town ? `before ${t.place.town}` : `in ${t.place.name}`;
  // ?setup (release 2): his guns and support teams are placed first, dragged to where they will stand.
  const placing = flagged('setup') && b.setupOn(S.scene.me);
  S.scene.place = null;
  S.dispatch = { t: 0, what: 'contact', text: placing ? `${riverWords(t)}Your army stands ${place}: drag your guns and support teams to where they will fire from, inside your ground, then press Space to begin.` : `${riverWords(t)}Your army stands ${place}: give your orders, then press Space to begin.` };
  S.dispatchAge = 0;
  S.resume = null;
  guard();
  const mine = t[S.scene.me], theirs = t[S.scene.me === 'att' ? 'def' : 'att'];
  const col = sideColours(t);
  S.colours = col; S.scene.colours = col;
  app.innerHTML = `<div class="battle tc">
    <header class="bbar">
      <div class="btitle"><b class="tc-title-3">${esc(t.title)}</b><span class="tc-data">${esc(t.date)}</span>${placeLine(t)}</div>
      <div class="bclock" title="At ${mmss(b.ticket.limit)} the day is done, and the defender holds."><span class="tc-label" id="clockState">Paused</span><span class="tc-data" id="time">0:00</span></div>
      <div class="bspeed" role="group" aria-label="Speed">
        <button class="tc-btn tc-btn--sm" data-speed="0" title="Pause (Space)"><span id="pauseWord">Paused</span> <span class="tc-key">Space</span></button>
        <button class="tc-btn tc-btn--sm" data-speed="1" title="Normal speed (1)">×1 <span class="tc-key">1</span></button>
        <button class="tc-btn tc-btn--sm" data-speed="2" title="Twice as fast (2)">×2 <span class="tc-key">2</span></button>
        <button class="tc-btn tc-btn--sm" data-speed="4" title="Four times as fast (3)">×4 <span class="tc-key">3</span></button>
      </div>
      <div class="bbal" title="Each side's strength still standing. ${S.offer?.band ? 'The pale band is where the attack card put it: the defence is not fully known.' : 'The pale mark is where the attack card put it.'}">
        <div class="bends"><span>${S.offer ? esc(mine.adj) : 'Yours'}</span><span id="ratio" class="tc-data${S.offer?.band ? ' is-band' : ''}"></span><span>${S.offer ? esc(theirs.adj) : 'Theirs'}</span></div>
        <div class="btrack"><i id="balA" style="background:${col.att}"></i><i id="balD" style="background:${col.def}"></i><b id="balMark"></b></div>
        <div class="bbreak" title="When a bar fills, that army breaks and leaves the field. It fills with the men lost along the whole line, or with the share of it running or gone, whichever is nearer, so an army can break while every part of it is still steady."><span class="bbar2"><i id="brkMe" style="background:${col[S.scene.me]}"></i></span><span class="tc-label">How near to breaking</span><span class="bbar2 is-theirs"><i id="brkThem" style="background:${col[S.scene.me === 'att' ? 'def' : 'att']}"></i></span></div>
      </div>
      <div class="bacts">
        ${b.hasAir(S.scene.me) ? '<button class="tc-btn bair" data-act="air" id="airBtn" title="Your fighters on call: press, then click where to strike, or right-drag the run they fly. The staff call them in themselves if you leave them.">Air support <span class="tc-key">F</span><span class="tc-data airst" id="airState"></span></button>' : ''}
        <button class="tc-btn" data-act="dice" title="The staff settles the rest at once, from where the battle stands.">Hand it to the staff</button>
        <button class="tc-btn tc-btn--grave" data-act="withdraw">Withdraw</button>
        <div class="wask tc-paper" id="sask" hidden role="alertdialog" aria-label="Hand the battle to the staff?"><p id="saskText"></p><div class="wbtns"><button class="tc-btn" data-act="sgo">Hand it over</button><button class="tc-btn tc-btn--primary" data-act="stay">Keep fighting</button></div></div>
        <div class="wask tc-paper" id="wask" hidden role="alertdialog" aria-label="Withdraw the army?"><p>Withdraw the army? The battle ends and the field is theirs.</p><div class="wbtns"><button class="tc-btn" data-act="wgo">Withdraw</button><button class="tc-btn tc-btn--primary" data-act="stay">Keep fighting</button></div></div>
      </div>
    </header>
    <div class="bfield"><canvas id="field"></canvas>
      <div class="ftag tc-paper" id="ftag" hidden></div>
      <div class="ftop"><div class="dispatch tc-paper" id="dispatch"></div><div class="fwarn tc-paper" id="fwarn" hidden>Your army is near breaking: pull back the worn, or hand it to the staff.</div></div>
      <div class="zoom"><button class="tc-btn tc-btn--sm" data-zoom="1" aria-label="Closer">+</button><button class="tc-btn tc-btn--sm" data-zoom="-1" aria-label="Further">−</button><button class="tc-btn tc-btn--sm" data-zoom="0" title="The whole field">Whole field</button></div>
    </div>
    <div class="keys tc-data">${facts(['Click or drag a box: pick up', 'Right-click: march or attack', 'Right-drag: draw a line', '<span class="tc-key">H</span> hold', ...(S.b?.hasAir(S.scene.me) ? ['<span class="tc-key">F</span> air support, then click where'] : []), '<span class="tc-key">R</span> fall back off the field', '<span class="tc-key">Tab</span> next', '<span class="tc-key">Esc</span> put down', 'Arrows or two-finger drag: look round', '<span class="knarrow"><span class="tc-key">Space</span> pause</span>', '<span class="knarrow"><span class="tc-key">1</span> <span class="tc-key">2</span> <span class="tc-key">3</span> speed</span>'])}</div>
    <footer class="tray${grouped(b) ? ' is-grouped' : ''}" id="tray"><button class="tc-card is-press tmore" data-page="-1" hidden></button>${grouped(b) ? groups(b) : b.units.filter((u) => u.side === S.scene.me).map((u) => card(u, col[S.scene.me])).join('')}<button class="tc-card is-press tmore" data-page="1" hidden></button></footer>
  </div>`;
  canvas = byId('field') as HTMLCanvasElement;
  ctx = canvas.getContext('2d')!;
  // Every battle opens on the whole field, not where the last one was left zoomed in.
  S.scene.W = 0; S.scene.H = 0;
  fitBar();
  void document.fonts?.ready.then(() => { if (S.b === b && S.screen === 'battle') { fitBar(); fit(); } });
  fit();
  wireBattle();
  updateTop(true);
  updateTray(true);
  pageTray();
  S.last = now();
}

/** Width of a card in the tray with the gap after it, and of a corps' group of rows per column of four. */
const CARD_W = 196 + 8, ROW_COL_W = 148, ROWS = 4;

/**
 * A big battle's tray (the design director, b388): its blocks under their corps, one header a corps that picks up
 * the whole corps in one click, each block a row with its strength, or its state where it is not steady. A newcomer
 * gets six or seven handles in place of twenty cards; the tray pages only beyond that.
 */
const grouped = (b: LandBattle): boolean => b.units.filter((u) => u.side === S.scene.me).length > 8;

function groups(b: LandBattle): string {
  const me = S.scene.me, t = b.ticket[me];
  const order: number[] = [];
  const by = new Map<number, LandUnit[]>();
  for (const u of b.units) if (u.side === me) { if (!by.has(u.corps)) { by.set(u.corps, []); order.push(u.corps); } by.get(u.corps)!.push(u); }
  return order.map((c) => {
    const us = by.get(c)!, name = t.forces.find((f) => f.corps === c)?.name ?? us[0].parent;
    const divs = new Set(us.map((u) => (u.parent === name ? u.short : u.parent))).size;
    const count = `${divs} division${divs === 1 ? '' : 's'}`;
    const cols = Math.ceil(us.length / ROWS), w = cols * ROW_COL_W + 16;
    const rows = us.map((u) => {
      // A regiment's row carries its division, as its card does.
      const label = u.parent === name ? u.short : `${u.short}, ${u.parent}`;
      return `<button class="urow" data-unit="${u.id}" title="${esc(u.name)}"><span class="rname">${esc(label)}</span><span class="rstate"></span><i class="rbar"><b></b></i></button>`;
    }).join('');
    return `<div class="tc-card ugroup" data-item data-w="${w + 8}" data-corps="${c}" style="width:${w}px">
      <button class="ghead" data-corps="${c}" title="${esc(name)}, ${count}: click to pick up the whole corps."><b>${esc(name)}</b></button>
      <div class="grows" style="grid-template-columns:repeat(${cols}, 1fr)">${rows}</div></div>`;
  }).join('');
}

function card(u: LandUnit, color: string): string {
  return `<button class="tc-card is-press ucard" data-item data-w="${CARD_W}" data-unit="${u.id}" title="${esc(u.name)}">
    <span class="uhead">${signSvg(u.kind, color)}<span class="uname"><b>${esc(u.short)}</b><span>${esc(u.parent)}</span></span></span>
    <span class="tc-meter" data-m="str"><span class="tc-label">Strength</span><i><b></b></i><span></span></span>
    <span class="tc-meter is-rest" data-m="mor"><span class="tc-label">Morale</span><i><b></b></i><span></span></span>
    <span class="ustate tc-data"></span></button>`;
}

const STATE_WORD: Record<string, string> = { steady: 'Steady', wavering: 'Wavering', routing: 'Routing', gone: 'Fled', withdrawn: 'Off the field' };

/** The card says the order, not the walk, the moment it is given, paused or not (the UI director's words for ruling 226(8)). */
function doingWord(u: LandUnit): string {
  if (u.state === 'routing') return '';
  const o = u.order;
  if (o?.kind === 'attack') return 'attacking';
  if (o?.kind === 'withdraw') return 'falling back off the field';
  if (o?.kind === 'move') return u.act === 'run' ? 'at the double' : 'marching';
  if (S.holding.has(u.id)) return 'holding';
  return u.act === 'fight' || u.engaged !== undefined ? 'in action' : 'standing';
}

/**
 * The tray shows whole cards only (ruling 226(9)): when the army has more than fit, the last place is a card of its
 * own, "7 more ▸", and the first then reads "◂ 7 more". Tab, and picking one up on the field, bring its card into view.
 * A big battle's tray pages its corps' groups the same way, counted in corps.
 */
const MORE_W = 112 + 8, MORE_CORPS_W = 156 + 8;

/** The tray's items (cards, or corps' groups), their widths, and the room they have. */
function trayItems(): { tray: HTMLElement; items: HTMLElement[]; ws: number[]; room: number; more: number } | null {
  const tray = byId('tray');
  if (!tray) return null;
  const items = [...tray.querySelectorAll<HTMLElement>('[data-item]')];
  return { tray, items, ws: items.map((i) => Number(i.dataset.w)), room: tray.clientWidth - 24 + 8, more: tray.classList.contains('is-grouped') ? MORE_CORPS_W : MORE_W };
}

/** The first item past a page that starts at 'from': whole items only, with room kept for the "more" places it needs. */
function pageEnd(ws: number[], room: number, from: number, more = MORE_W): number {
  let used = from > 0 ? more : 0, k = from;
  while (k < ws.length && (k === from || used + ws[k] + (k + 1 < ws.length ? more : 0) <= room)) { used += ws[k]; k++; }
  return k;
}

function pageTray(): void {
  const t = trayItems();
  if (!t) return;
  const { tray, items, ws, room, more } = t;
  const back = tray.querySelector<HTMLElement>('[data-page="-1"]')!, fwd = tray.querySelector<HTMLElement>('[data-page="1"]')!;
  if (ws.reduce((a, w) => a + w, 0) <= room) {
    S.trayFrom = 0; back.hidden = true; fwd.hidden = true;
    items.forEach((c) => { c.hidden = false; });
    return;
  }
  // The last page ends with the last item: never a page with room to spare at the end.
  let from = Math.max(0, Math.min(S.trayFrom, items.length - 1));
  while (from > 0 && pageEnd(ws, room, from - 1, more) >= items.length) from--;
  S.trayFrom = from;
  const to = pageEnd(ws, room, from, more);
  items.forEach((c, i) => { c.hidden = i < from || i >= to; });
  back.hidden = from === 0; fwd.hidden = to >= items.length;
  const what = (n: number): string => (tray.classList.contains('is-grouped') ? `${n} more corps` : `${n} more`);
  back.textContent = `◂ ${what(from)}`;
  fwd.textContent = `${what(items.length - to)} ▸`;
  fwd.dataset.to = String(to);
}

/** The page before one that starts at 'from': it ends where this one begins. */
function pageBefore(ws: number[], room: number, from: number, more = MORE_W): number {
  let f = Math.max(0, from - 1);
  while (f > 0 && pageEnd(ws, room, f - 1, more) >= from) f--;
  return f;
}

/** Bring a block's card (or its corps' group) into view in the tray. */
function showCard(id: number): void {
  const t = trayItems();
  if (!t) return;
  const i = t.items.findIndex((c) => Number(c.dataset.unit) === id || !!c.querySelector(`[data-unit="${id}"]`));
  if (i < 0 || !t.items[i].hidden) return;
  let f = i;
  while (f > 0 && pageEnd(t.ws, t.room, f - 1, t.more) > i) f--;
  S.trayFrom = f;
  pageTray();
}

function updateTray(force = false): void {
  const b = S.b!;
  const t = now();
  if (!force && t - S.trayAt < 0.15) return;
  S.trayAt = t;
  for (const el of root.querySelectorAll<HTMLElement>('#tray [data-unit]')) {
    const u = b.unit(Number(el.dataset.unit));
    if (!u) continue;
    const gone = u.state === 'gone' || u.state === 'withdrawn';
    el.classList.toggle('is-on', S.scene.sel.has(u.id));
    el.classList.toggle('is-off', gone);
    if (el.classList.contains('urow')) {
      // A row: its strength, or its state in words where it is not steady; only Routing in red.
      const word = u.fellBack ? 'Falling back' : u.state === 'steady' ? '' : STATE_WORD[u.state];
      const rs = el.querySelector<HTMLElement>('.rstate')!;
      rs.textContent = word;
      // One colour for one word: Falling back is muted wherever it stands (the UI director, 3b).
      rs.className = `rstate${u.fellBack ? '' : u.state === 'routing' ? ' bad' : u.state === 'wavering' ? ' warn' : ''}`;
      // Its hover carries what the small battle's card says (the UI director, 3b): "16th Infantry · Strength 92 · attacking".
      const doingR = u.fellBack ? '' : b.over ? (live(u) ? 'battle over' : '') : live(u) ? doingWord(u) : '';
      // A tank block's named tanks, on a line of their own (b477(6)).
      const crew = crewLine(u.crew ?? null);
      el.title = [el.querySelector('.rname')!.textContent, `Strength ${Math.round(u.str * 100)}`, word, doingR].filter(Boolean).join(' · ') + (crew ? `\n${crew}` : '');
      const k = u.str / Math.max(1e-6, u.str0);
      const bar = el.querySelector<HTMLElement>('.rbar b')!;
      bar.style.width = `${Math.round(Math.max(0, Math.min(1, u.str)) * 100)}%`;
      bar.className = k < 0.35 ? 'is-low' : k < 0.65 ? 'is-mid' : '';
      continue;
    }
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
    // Carried off by its army's break, a block that still stood reads Falling back, not Routing (the design director).
    const doing = u.fellBack ? '' : b.over ? (live(u) ? 'battle over' : '') : live(u) ? doingWord(u) : '';
    st.innerHTML = `<b>${u.fellBack ? 'Falling back' : STATE_WORD[u.state]}</b>${doing ? ` · ${doing}` : ''}`;
    st.className = `ustate tc-data${u.fellBack ? ' muted' : u.state === 'routing' ? ' bad' : u.state === 'wavering' ? ' warn' : ''}`;
  }
  // A corps' header is lit while every block of it that can take orders is in hand.
  for (const h of root.querySelectorAll<HTMLElement>('#tray .ghead')) {
    const us = b.units.filter((u) => u.side === S.scene.me && u.corps === Number(h.dataset.corps) && orderable(u));
    h.classList.toggle('is-on', us.length > 0 && us.every((u) => S.scene.sel.has(u.id)));
    h.closest('.ugroup')!.classList.toggle('is-off', us.length === 0);
  }
}

/** The speed buttons' hovers while the battle runs. */
const SPEED_TIPS: Record<number, string> = { 0: 'Pause (Space)', 1: 'Normal speed (1)', 2: 'Twice as fast (2)', 4: 'Four times as fast (3)' };

function updateTop(force = false): void {
  const b = S.b!;
  const el = (id: string): HTMLElement => byId(id)!;
  el('time').textContent = `${mmss(b.t)} of ${mmss(b.ticket.limit)}`;
  const bal = b.standing();
  const k = b.ticket.reckoning, band = S.offer?.band;
  // Both shares are of the same whole, so their ratio is the odds standing now (the card's at the start).
  const nowOdds = bal.att / Math.max(1e-6, bal.def);
  const at = (o: number): string => `${((100 * o) / (1 + o)).toFixed(1)}%`;
  // Once the result is in, the balance keeps the last figure it showed before the end (the UI director, b476): a
  // withdrawal empties the field, and "now" would read nothing.
  const frozen = !!b.over && !!el('ratio').dataset.said;
  if (frozen) { /* as it stood */ } else if (band) {
    // A defence the card did not know (b453): the staff's band, moved as the fight has moved the balance since, and
    // the bar drawn at its middle. The card's own odds are not shown until the fight ends.
    const r = nowOdds / Math.max(1e-6, k.odds), mid = Math.sqrt(band.lo * band.hi) * r;
    el('balA').style.width = at(mid);
    el('balD').style.width = `${(100 - (100 * mid) / (1 + mid)).toFixed(1)}%`;
    const mark = el('balMark');
    mark.classList.add('band');
    mark.style.left = at(band.lo);
    mark.style.width = `${Math.max(0.5, (100 * band.hi) / (1 + band.hi) - (100 * band.lo) / (1 + band.lo)).toFixed(1)}%`;
    // The card's own words for a range, never a dash (the UI director, 245e366), folding at the dot where the bar is
    // narrow. The line is only rewritten when it changes, so the fold holds still while the clock runs.
    const ratio = el('ratio'), said = facts([`Card ${between(band.lo, band.hi)}`, `now ${between(band.lo * r, band.hi * r)}`]);
    if (ratio.dataset.said !== said) { ratio.innerHTML = said; ratio.dataset.said = said; }
  } else {
    const share = bal.att / Math.max(1e-6, bal.att + bal.def);
    el('balA').style.width = `${(share * 100).toFixed(1)}%`;
    el('balD').style.width = `${((1 - share) * 100).toFixed(1)}%`;
    el('balMark').style.left = at(k.odds);
    // Card over now, as the band's two lines are, so a scouted battle starts the field at the same height (the UI director).
    const ratio = el('ratio'), said = facts([`Card ${odds(k.odds)} to 1`, `now ${odds(nowOdds)} to 1`]);
    if (ratio.dataset.said !== said) { ratio.innerHTML = said; ratio.dataset.said = said; }
  }
  const sp = S.clock.speed;
  // While the rush runs no speed button is lit: the tag on the field is the one sign of the speed, and its key cap says
  // what ends it (the UI director, b467). The pause lights as ever.
  const rushing = sp > 0 && b.contactAt === null && !S.rushByHand && sp < RUSH;
  // Once the result is in (the UI director, b476): the clock reads BATTLE OVER at the time it ended, nothing is lit,
  // and the speed buttons are off, their hover saying why.
  const over = !!b.over;
  root.querySelectorAll<HTMLElement>('[data-speed]').forEach((x) => {
    x.classList.toggle('is-lit', !over && !rushing && Number(x.dataset.speed) === sp);
    x.classList.toggle('is-off', over);
    x.setAttribute('aria-disabled', over ? 'true' : 'false');
    x.title = over ? 'The battle is over.' : SPEED_TIPS[Number(x.dataset.speed)];
  });
  // Paused means paused (ruling 226(8)): the clock's label is the state, and the first segment reads Paused when lit.
  const cs = el('clockState');
  cs.textContent = over ? 'Battle over' : sp === 0 ? 'Paused' : 'Running';
  cs.classList.toggle('is-paused', !over && sp === 0);
  el('pauseWord').textContent = !over && sp === 0 ? 'Paused' : 'Pause';
  // Air support's state on its button: ready, on the way, next in, after the armies meet, all flown (b533).
  const airBtn = byId('airBtn'), airSt = byId('airState');
  if (airBtn && airSt) {
    const a = b.airView().air?.[S.scene.me];
    const said = !a || over ? '' : a.eta !== null ? `Over the field in ${mmss(a.eta)}` : a.left <= 0 ? 'All flown' : a.ready ? `Ready · ${a.left} left` : a.nextIn !== null ? `Ready in ${mmss(a.nextIn)} · ${a.left} left` : b.contactAt === null ? 'After the armies meet' : `${a.left} left`;
    if (airSt.textContent !== said) airSt.textContent = said;
    airBtn.classList.toggle('is-lit', !!S.scene.air);
    airBtn.classList.toggle('is-off', !a?.ready || over);
  }
  // How near to breaking (ruling 226(3)): a bar a side, no numbers. Under the field spread the enemy's is as the player's
  // staff reads it (the mask, shownBar); his own, and its amber warning, read the truth.
  const v = { att: b.shownBar('att', S.scene.me), def: b.shownBar('def', S.scene.me) };
  const me = S.scene.me, them: Side = me === 'att' ? 'def' : 'att';
  el('brkMe').style.width = `${(Math.min(1, v[me]) * 100).toFixed(1)}%`;
  el('brkThem').style.width = `${(Math.min(1, v[them]) * 100).toFixed(1)}%`;
  el('fwarn').hidden = !!b.over || v[me] < 0.75 || S.screen !== 'battle';
  (byId('wask') as HTMLElement).hidden = S.asking !== 'withdraw';
  (byId('sask') as HTMLElement).hidden = S.asking !== 'staff';
  // The tag on the field: PAUSED with its key while stopped, or a passing word; never while a question stands.
  const tag = el('ftag');
  const passing = S.tag && now() < S.tag.until ? S.tag.text : '';
  const tagHtml = S.asking || b.over ? '' : passing && S.tag?.air ? esc(passing) : sp === 0 ? 'Paused <span class="tc-key">Space</span>' : rushing ? `×${RUSH} until the armies meet <span class="tc-key">1</span>` : esc(passing);
  if (tag.innerHTML !== tagHtml) tag.innerHTML = tagHtml;
  tag.hidden = !tagHtml;
  if (force || S.dispatch) {
    const d = S.dispatch;
    const box = el('dispatch');
    const html = d ? `<span class="tc-label">${mmss(d.t)}</span><span>${esc(d.text)}</span>` : '';
    if (box.innerHTML !== html) box.innerHTML = html;
    // While the clock runs a dispatch fades a few seconds after it comes; while paused it stays (the UI director, 8a).
    box.classList.toggle('is-old', !!d && S.dispatchAge > DISPATCH_FOR);
  }
}

/**
 * The top bar holds one row at every width (the UI director, after 1969d7b): where the two buttons would be pushed off
 * its end they stack at the right end, small; where even that is not enough the title takes the next size down.
 * Worked out again when the window changes size.
 */
function fitBar(): void {
  const bar = root.querySelector<HTMLElement>('.bbar'), title = bar?.querySelector<HTMLElement>('.btitle'), acts = bar?.querySelector<HTMLElement>('.bacts');
  if (!bar || !title || !acts) return;
  acts.classList.remove('is-stacked');
  title.classList.remove('is-small');
  const pad = parseFloat(getComputedStyle(bar).paddingRight) || 0;
  const over = (): boolean => acts.getBoundingClientRect().right > bar.getBoundingClientRect().right - pad + 0.5;
  if (over()) acts.classList.add('is-stacked');
  if (over()) title.classList.add('is-small');
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
  // A block told to hold says so until its next order (ruling 226(8)).
  if (ok && 'units' in c && c.units) for (const id of c.units) { if (c.do === 'hold') S.holding.add(id); else S.holding.delete(id); }
  return ok;
}

function wireBattle(): void {
  const sc = S.scene;
  let down: { x: number; y: number; button: number; at: number; touch: boolean; moved: boolean; px: number; py: number } | null = null;
  let hold: number | undefined;
  let carry: number | null = null;
  const pos = (e: PointerEvent | WheelEvent | MouseEvent): [number, number] => { const r = canvas.getBoundingClientRect(); return [e.clientX - r.left, e.clientY - r.top]; };

  canvas.addEventListener('contextmenu', (e) => e.preventDefault());
  canvas.addEventListener('pointerdown', (e) => {
    if (S.screen !== 'battle') return;
    const [px, py] = pos(e);
    canvas.setPointerCapture(e.pointerId);
    const touch = e.pointerType === 'touch';
    down = { x: px, y: py, button: e.button, at: now(), touch, moved: false, px, py };
    // The setup step: a press on one of his own guns or teams picks it up to carry (release 2).
    carry = null;
    if (S.b?.setup && (touch || e.button === 0) && !sc.air) {
      const [fx, fy] = toField(sc, px, py), u = unitAt(S.b.units, fx, fy, Math.max(30, 8 / sc.cam.z), sc.me);
      if (u && u.side === sc.me && u.kind === 'art') carry = u.id;
    }
    // On a touch screen, press and hold orders, as on the campaign map.
    // Air support armed: a touch shows the run where it is, and lifting calls it; holding gives no march or attack.
    if (touch && sc.air && S.b) { const [fx, fy] = toField(sc, px, py), run = S.b.airRunAt(sc.me, fx, fy); sc.air = { run, ok: S.b.airWhyNot(sc.me, run) === null }; }
    if (touch && !sc.air && carry === null) hold = window.setTimeout(() => { if (down && !down.moved && !sc.air) { orderAt(down.x, down.y); down = null; } }, 450);
  });
  canvas.addEventListener('pointermove', (e) => {
    const [px, py] = pos(e);
    const b = S.b;
    if (!b) return;
    const [fx, fy] = toField(sc, px, py);
    // Air support armed: the run a strike would fly from here, as the field lays it.
    if (sc.air && (!down || (!down.touch && down.button === 0) || (down.button === 2 && !down.moved))) {
      const run = b.airRunAt(sc.me, fx, fy), why = b.airWhyNot(sc.me, run);
      sc.air = { run, ok: why === null };
      // The red run says why (the UI director, 21:18Z).
      if (why) S.tag = { text: why, until: now() + 0.6, air: true };
      canvas.style.cursor = 'crosshair';
    }
    if (!down) {
      // Armed, the run is what he is pointing with: no block's hover over it.
      if (sc.air) { sc.hover = null; return; }
      const u = unitAt(b.units, fx, fy, Math.max(85, 16 / sc.cam.z), sc.me);
      sc.hover = u ? u.id : null;
      canvas.style.cursor = u && u.side !== sc.me && sc.sel.size ? 'crosshair' : u && u.side === sc.me ? 'pointer' : 'default';
      return;
    }
    if (Math.hypot(px - down.x, py - down.y) > 6) down.moved = true;
    if (!down.moved) return;
    if (carry !== null) { const u = b.unit(carry)!; sc.place = { id: carry, x: fx, y: fy, ok: b.placeWhy(u, fx, fy) === null }; down.px = px; down.py = py; return; }
    if (down.touch || down.button === 1) {
      // Drag the map.
      sc.cam.x -= (px - down.px) / sc.cam.z; sc.cam.y += (py - down.py) / sc.cam.z; clampCam();
    } else if (down.button === 0 && !sc.air) {
      const [ax, ay] = toField(sc, down.x, down.y);
      sc.box = [ax, ay, fx, fy];
    } else if (down.button === 2 && sc.air) {
      // Air support armed: the right-drag is the run itself.
      const [ax, ay] = toField(sc, down.x, down.y);
      // As the field will fly it: kept on the field and to its longest.
      const run = b.airRun([ax, ay, fx, fy]), why = b.airWhyNot(sc.me, run);
      sc.air = { run, ok: why === null };
      if (why) S.tag = { text: why, until: now() + 0.6, air: true };
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
    // The setup step: a gun or team carried and let go is placed there, if it may stand there; else it stays (release 2).
    if (carry !== null) {
      const id = carry, was = sc.place;
      carry = null; sc.place = null;
      if (d.moved && was) {
        const why = b.placeWhy(b.unit(id)!, was.x, was.y);
        if (why) S.tag = { text: why, until: now() + 1.5 }; else give({ do: 'land:place', units: [id], x: was.x, y: was.y });
        updateTray(true);
        return;
      }
    }
    // Air support armed: a click (either button) strikes there; a right-drag strikes down the run drawn.
    if (sc.air && !(d.touch && d.moved) && (d.touch || d.button === 0 || d.button === 2)) {
      const [fx, fy] = toField(sc, px, py);
      const run = d.button === 2 && d.moved && sc.air.run ? sc.air.run : b.airRunAt(sc.me, fx, fy);
      sc.box = null; sc.line = null;
      callAir(run);
      return;
    }
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
  canvas.addEventListener('pointercancel', () => { down = null; carry = null; sc.place = null; sc.box = null; sc.line = null; });
  // The hover goes the moment the pointer leaves the field, as hovers do everywhere else in the game: one left standing
  // over the tray reads as a fact about a division he is no longer pointing at (the UI director).
  canvas.addEventListener('pointerleave', () => { if (!down) { sc.hover = null; canvas.style.cursor = 'default'; } });
  canvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    const [px, py] = pos(e);
    // A pinch on a trackpad, or a mouse wheel's notches, zoom; two fingers sliding on a trackpad move the map.
    const notched = e.deltaMode !== 0 || (e.deltaX === 0 && Math.abs(e.deltaY) >= 40 && Number.isInteger(e.deltaY));
    if (e.ctrlKey || notched) zoomAt(px, py, Math.exp(-e.deltaY * (e.ctrlKey ? 0.01 : 0.0015)));
    else { sc.cam.x += e.deltaX / sc.cam.z; sc.cam.y -= e.deltaY / sc.cam.z; clampCam(); }
  }, { passive: false });

  app.querySelectorAll<HTMLElement>('[data-speed]').forEach((el) => el.addEventListener('click', () => { if (S.asking || S.b?.over) return; if (Number(el.dataset.speed) > 0) beginIfPlacing(); setSpeed(Number(el.dataset.speed) as Speed); updateTop(); }));
  app.querySelectorAll<HTMLElement>('[data-zoom]').forEach((el) => el.addEventListener('click', () => {
    const k = Number(el.dataset.zoom);
    if (k === 0) whole(); else zoomAt(sc.W / 2, sc.H / 2, k > 0 ? 1.4 : 1 / 1.4);
  }));
  // HAND IT TO THE STAFF and WITHDRAW each ask once, in place, with the clock stopped; KEEP FIGHTING, the answer that
  // changes nothing, carries the brass (and Esc) (ruling 226(5)).
  app.querySelector('[data-act="air"]')?.addEventListener('click', () => { toggleAir(); updateTop(); });
  app.querySelector('[data-act="dice"]')!.addEventListener('click', () => { ask('staff'); updateTop(); });
  app.querySelector('[data-act="withdraw"]')!.addEventListener('click', () => { ask('withdraw'); updateTop(); });
  app.querySelectorAll<HTMLElement>('[data-act="stay"]').forEach((el) => el.addEventListener('click', () => { answered(); updateTop(); }));
  app.querySelector('[data-act="sgo"]')!.addEventListener('click', () => { if (now() < S.guardUntil) return; S.asking = false; S.resume = null; give({ do: 'dice' }); });
  app.querySelector('[data-act="wgo"]')!.addEventListener('click', () => { if (now() < S.guardUntil) return; S.asking = false; S.resume = null; give({ do: 'withdrawAll', side: sc.me }); });
  const tray = byId('tray')!;
  tray.addEventListener('click', (e) => {
    const more = (e.target as HTMLElement).closest<HTMLElement>('[data-page]');
    if (more) {
      const t = trayItems()!;
      S.trayFrom = more.dataset.page === '1' ? Number(tray.querySelector<HTMLElement>('[data-page="1"]')!.dataset.to ?? 0) : pageBefore(t.ws, t.room, S.trayFrom, t.more);
      pageTray(); return;
    }
    // A corps' header picks up the whole corps (b388); Shift adds it to what is in hand.
    const head = (e.target as HTMLElement).closest<HTMLElement>('.ghead');
    if (head) {
      const us = S.b!.units.filter((u) => u.side === sc.me && u.corps === Number(head.dataset.corps) && orderable(u));
      if (!e.shiftKey) sc.sel.clear();
      for (const u of us) sc.sel.add(u.id);
      updateTray(true);
      return;
    }
    const el = (e.target as HTMLElement).closest<HTMLElement>('[data-unit]');
    if (!el) return;
    const id = Number(el.dataset.unit), u = S.b!.unit(id);
    if (!u || !orderable(u)) return;
    if (e.shiftKey) { if (sc.sel.has(id)) sc.sel.delete(id); else sc.sel.add(id); } else { sc.sel.clear(); sc.sel.add(id); }
    updateTray(true);
  });
  tray.addEventListener('dblclick', (e) => {
    const head = (e.target as HTMLElement).closest<HTMLElement>('.ghead');
    if (head) {
      // Two clicks on a corps' header look at it.
      const us = S.b!.units.filter((u) => u.side === sc.me && u.corps === Number(head.dataset.corps) && live(u));
      if (us.length) { sc.cam.x = us.reduce((a, u) => a + u.x, 0) / us.length; sc.cam.y = us.reduce((a, u) => a + u.y, 0) / us.length; clampCam(); }
      return;
    }
    const el = (e.target as HTMLElement).closest<HTMLElement>('[data-unit]');
    const u = el ? S.b!.unit(Number(el.dataset.unit)) : undefined;
    if (u) { sc.cam.x = u.x; sc.cam.y = u.y; sc.cam.z = Math.max(sc.cam.z, 0.8); clampCam(); }
  });
}

/** A question on the field stops the clock while it stands (b358): no one loses men while reading. */
function ask(kind: 'withdraw' | 'staff'): void {
  if (S.asking || !S.b || S.b.over) return;
  if (kind === 'staff') {
    // The staff's chance now, from the strength each side has left (the UI director's words for ruling 226(5)).
    const b = S.b, where = b.ticket.place.name, band = S.offer?.band;
    // A defence the card did not know (b453): the chance as the staff's band reads it now, never the card's own.
    const [lo, hi] = band ? [b.chanceAt(band.lo), b.chanceAt(band.hi)] : [b.chanceNow(), b.chanceNow()];
    const pc = (x: number): number => Math.round(x * 100);
    // A practice ground is not a place to take (the UI director, after the retest).
    const what = S.offer ? `take ${esc(where)}` : 'win it';
    const odds = lo >= 0.995 ? 'They are sure of it.' : hi <= 0.005 ? 'They give it no chance.' : pc(lo) === pc(hi) ? `They give you about ${pc(lo)}% to ${what}.` : `They give you between about ${pc(lo)}% and ${pc(hi)}% to ${what}.`;
    byId('saskText')!.innerHTML = b.contactAt === null
      // True whether or not the guns have opened before the lines meet (b409, the UI director's words).
      ? `Hand the battle to the staff? The armies have not met yet: they settle the whole battle at once. ${odds}`
      : `Hand the battle to the staff? They settle the rest at once, from where it stands now. ${odds}`;
  }
  S.asking = kind;
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

/**
 * Air support (Brian, 2026-11-18; b533): F or the button arms it where a strike is ready; the next click strikes the
 * enemy block nearest it down its frontage, a right-drag down the run drawn. Pressed again, or Esc, it puts it down.
 * Where none can be called, a passing word says why.
 */
function toggleAir(): void {
  const b = S.b, sc = S.scene;
  if (!b || b.over || !b.hasAir(sc.me)) return;
  if (sc.air) { sc.air = null; canvas.style.cursor = 'default'; return; }
  const why = b.airWhyNot(sc.me);
  if (why) { S.tag = { text: why, until: now() + 2, air: true }; return; }
  sc.air = { run: null, ok: true };
  canvas.style.cursor = 'crosshair';
}
function callAir(run: [number, number, number, number]): void {
  const b = S.b!, sc = S.scene;
  const why = b.airWhyNot(sc.me, b.airRun(run));
  if (why) { S.tag = { text: why, until: now() + 2, air: true }; return; }
  if (give({ do: 'land:air', x0: run[0], y0: run[1], x1: run[2], y1: run[3] })) {
    sc.ping = { x: (run[0] + run[2]) / 2, y: (run[1] + run[3]) / 2, at: now(), kind: 'attack' };
    sc.air = null; canvas.style.cursor = 'default';
  } else S.tag = { text: b.airWhyNot(sc.me, b.airRun(run)) ?? 'Not now', until: now() + 2, air: true };
}

function pickAt(px: number, py: number, add: boolean): void {
  const b = S.b!, sc = S.scene;
  const [fx, fy] = toField(sc, px, py);
  const u = unitAt(b.units.filter((x) => x.side === sc.me && orderable(x)), fx, fy, Math.max(85, 16 / sc.cam.z), sc.me);
  if (!u) { if (!add) sc.sel.clear(); return; }
  if (add) { if (sc.sel.has(u.id)) sc.sel.delete(u.id); else sc.sel.add(u.id); } else { sc.sel.clear(); sc.sel.add(u.id); }
  showCard(u.id);
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

window.addEventListener('resize', () => { if (S.active && S.b && S.screen !== 'choose') { fitBar(); fit(); pageTray(); } });

// Keys go to the field first, and over the campaign no further: Enter would end its month, Tab pick its next army.
window.addEventListener('keydown', (e) => {
  if (!S.active) return;
  if (S.home) e.stopPropagation();
  if (e.key === 'Enter') { e.preventDefault(); return; }
  // The browser's own shortcuts stay the browser's: Ctrl+H is History, not Hold (the review, b453).
  if (e.ctrlKey || e.metaKey || e.altKey) return;
  if (now() < S.guardUntil) { if (e.key === 'Tab' || e.key === ' ') e.preventDefault(); return; }
  if (S.screen !== 'battle' || !S.b) return;
  const sc = S.scene, b = S.b;
  const key = e.key.length === 1 ? e.key.toLowerCase() : e.key;
  const pan = 120 / sc.cam.z;
  switch (key) {
    case ' ': e.preventDefault(); if (!S.asking && !b.over) { beginIfPlacing(); S.clock.toggle(); } break;
    case '1': if (!S.asking && !b.over) { beginIfPlacing(); setSpeed(1); } break;
    case '2': if (!S.asking && !b.over) { beginIfPlacing(); setSpeed(2); } break;
    case '3': if (!S.asking && !b.over) { beginIfPlacing(); setSpeed(4); } break;
    case 'f': toggleAir(); break;
    case 'h': give({ do: 'hold', units: chosen() }); break;
    case 'r': give({ do: 'withdraw', units: chosen() }); break;
    // Esc changes nothing: it closes the question if one is open, else drops the blocks in hand.
    case 'Escape': if (S.asking) answered(); else if (sc.air) { sc.air = null; canvas.style.cursor = 'default'; } else { sc.sel.clear(); sc.line = null; sc.box = null; } break;
    case 'Tab': {
      e.preventDefault();
      const mine = b.units.filter((u) => u.side === sc.me && orderable(u));
      if (!mine.length) break;
      const cur = [...sc.sel][0];
      const i = mine.findIndex((u) => u.id === cur);
      const next = mine[(i + (e.shiftKey ? mine.length - 1 : 1)) % mine.length];
      sc.sel.clear(); sc.sel.add(next.id);
      sc.cam.x = next.x; sc.cam.y = next.y; clampCam();
      showCard(next.id);
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
}, true);

// ---- The loop -----------------------------------------------------------------------------------------------------

let looping = false;
function startLoop(): void {
  if (!looping) { looping = true; requestAnimationFrame(frame); }
}

/** The dispatch, the near-breaking warning, the tag and the zoom group where they show, in the canvas's own pixels. */
function overField(): [number, number, number, number][] {
  const cv = byId('field');
  if (!cv) return [];
  const c = cv.getBoundingClientRect(), out: [number, number, number, number][] = [];
  for (const id of ['dispatch', 'fwarn', 'ftag']) {
    const el = byId(id);
    if (!el || el.hidden || el.classList.contains('is-old') || !el.textContent) continue;
    const r = el.getBoundingClientRect();
    if (r.width > 0 && r.height > 0) out.push([r.left - c.left, r.top - c.top, r.right - c.left, r.bottom - c.top]);
  }
  const zoom = (cv.parentElement?.querySelector('.zoom') as HTMLElement | null)?.getBoundingClientRect();
  if (zoom && zoom.width > 0) out.push([zoom.left - c.left, zoom.top - c.top, zoom.right - c.left, zoom.bottom - c.top]);
  return out;
}

function frame(): void {
  if (!S.active) { looping = false; return; }
  requestAnimationFrame(frame);
  if (S.screen === 'choose' || !S.b || !S.field) return;
  const t = now();
  const dt = Math.min(0.25, t - S.last);
  S.last = t;
  S.scene.now = t;
  const b = S.b;
  // Fast until the armies meet, then the speed he set, with a word on the field each way (ruling 226(1)).
  S.clock.rush = b.contactAt === null && !S.rushByHand && S.clock.speed > 0 && S.clock.speed < RUSH ? RUSH / S.clock.speed : 1;
  S.clock.advance(b, dt);
  const running = S.clock.speed > 0;
  if (running && !S.wasRunning && b.contactAt === null && S.clock.speed >= RUSH) S.tag = { text: `Running ×${S.clock.speed}`, until: t + 2 };
  else if (running && !S.wasRunning && b.contactAt !== null) S.tag = { text: `Running ×${S.clock.speed}`, until: t + 2 };
  if (b.contactAt !== null && !S.met) { S.met = true; S.tag = { text: `The armies meet · Running ×${S.clock.speed || 1}`, until: t + 2.5 }; }
  S.wasRunning = running;
  const v = b.view(S.scene.me);
  // What stands over the field's top, for a hover label to keep clear of (the UI director, b464): read only while
  // there is a hover to place.
  S.scene.avoid = S.scene.hover === null ? [] : overField();
  // The opening line says the clock is stopped: once it runs, the first fact replaces it.
  // With no orders given it stands, and says so (the UI director, after the retest).
  if (b.t > 0 && S.dispatch?.t === 0 && S.dispatch.what === 'contact' && !S.dispatch.unit) {
    const who = !S.offer ? 'Your army' : `The ${b.ticket[S.scene.me].adj} army`;
    // Orders that only turn a block where it stands are not a march (b408).
    const going = b.units.some((u) => u.side === S.scene.me && u.order !== null && Math.hypot(u.order.x - u.x, u.order.y - u.y) > 20);
    S.dispatch = { t: b.t, what: 'contact', text: going ? `${who} is moving.` : `${who} stands where it is, without orders.`, unit: -1 }; S.dispatchAge = 0;
  }
  // His own army near breaking has its amber line that stays (fwarn); everything else comes through the strip.
  for (const e of v.events) {
    // The amber line says what drives his bar (the UI director, 14:44Z): the sim's words at the warning.
    if (e.what === 'nearBreak' && e.side === S.scene.me) { byId('fwarn')!.textContent = e.text; continue; }
    if (e.text) { S.dispatch = e; S.dispatchAge = 0; }
  }
  if (S.clock.speed > 0) S.dispatchAge += dt;
  for (const id of [...S.scene.sel]) { const u = b.unit(id); if (!u || !orderable(u)) S.scene.sel.delete(id); }
  // Air support armed only while a strike can be called: once it cannot (over, called, or the battle done), put it down.
  if (S.scene.air && (b.over || b.airWhyNot(S.scene.me) !== null)) { S.scene.air = null; canvas.style.cursor = 'default'; }
  drawFrame(ctx, b, S.scene, S.field);
  updateTop();
  updateTray();
  if (v.over && S.screen === 'battle') showAfter();
}

// ---- After the battle: the campaign's own report ------------------------------------------------------------------

const FATE: Record<string, string> = { '': 'held its ground', retreated: 'retreated', surrendered: 'surrendered', destroyed: 'destroyed', advanced: 'advanced', evacuated: 'evacuated by sea', yielded: 'gave ground under orders', sheltered: 'fell back into the town' };

/** How the day went against the chance the staff gave it, in the UI director's plain words (no dice on the player's page). */
function reckoned(carried: boolean, chance: number, practice = false, said?: CardSaid, siege = false): string {
  // Where the card printed a band, its own figures (b476(4)): "60 to 100%", or its best guess where it printed one.
  const end = said ? said.end : chance >= 0.995 ? 'sure' : chance <= 0.005 ? 'none' : null;
  if (end === 'sure') return carried ? 'As the staff expected: they were sure of it.' : 'Worse than the staff expected: they were sure of it.';
  if (end === 'none') return carried ? 'Better than the staff expected: they gave it no chance.' : 'As the staff expected: they gave it no chance.';
  const figure = !said ? `about ${Math.round(chance * 100)}%` : said.guess !== null ? `about ${said.guess}%` : said.lo === said.hi ? `about ${said.lo}%` : `${said.lo} to ${said.hi}%`;
  // A practice ground is not a place to take (b408): there the staff gave him a chance to win it. Nor is a town that
  // holds out after the battle round it: the chance was the battle's.
  const p = `they gave you ${figure} to ${practice || siege ? 'win' : 'take'} it.`;
  if (carried) return chance < 0.5 ? `You did better than the staff expected: ${p}` : `As the staff expected: ${p}`;
  return chance >= 0.5 ? `You did worse than the staff expected: ${p}` : `As the staff feared: ${p}`;
}

function showAfter(): void {
  const b = S.b!, r = b.over!;
  S.screen = 'after';
  guard();
  // The after card is the one voice: the dispatch strip steps aside (the UI director, 3).
  S.dispatch = null;
  byId('dispatch')!.innerHTML = '';
  updateTray(true);
  // The top bar stays in view under the card: BATTLE OVER, nothing lit (the UI director, b476).
  updateTop();
  const t = b.ticket, k = t.reckoning;
  const g = S.g;
  const settled = S.offer ? settle(g, S.offer, r) : null;
  const rep = settled?.report ?? null;
  // Booked: the campaign keeps it at once, so a page closed on this card comes back to the field's result (b453).
  if (settled) booked?.();
  if (S.before && S.offer) {
    S.before.how = !rep ? 'the attack could not be ordered.' : rep.siege ? 'under siege.' : rep.taken ? 'taken.' : 'the attack was thrown back.';
    // Whether the defenders lost the larger share of their strength, for why the odds rose after a defeat.
    const share = (us: { men: number; lost: number }[]): number => us.reduce((a, u) => a + u.lost, 0) / Math.max(1, us.reduce((a, u) => a + u.men, 0));
    S.before.heavier = !!rep && share(rep.defenders) > share(rep.attackers);
  }
  const won = rep ? rep.taken || !!rep.siege : r.winner === 'att';
  // A practice battle handed to the staff is billed as the campaign would bill it, the guns' losses before it inside
  // the bill, and the stamp follows that bill (the design director, b410).
  // (The verdict's side of the line is the battle's own: R is rounded for the card, and may round onto the line.)
  const R = r.winner === 'att' ? Math.max(r.R, RATIO.carry) : Math.min(r.R, RATIO.carry - 1e-6);
  const bill = !rep && r.how === 'objective' && S.ground ? practiceBill(S.ground, R, r.lost, Object.fromEntries(r.forces.map((x) => [x.corps, x.lost]))) : null;
  const menOf = (s: Side): number => t[s].forces.reduce((a, f) => a + f.men, 0);
  const level = rep ? rep.level : bill ? levelOf(R, menOf('att') * bill.att > menOf('def') * bill.def) : r.level;
  // The staff speaks only its band (b464(6)): where the card did not know the defence, the chance it gave is the card's
  // best guess, never the chance at the true odds.
  const chance = S.offer?.band ? S.offer.band.guess : diceChance(k.odds, k.dice);
  const A = t.att.adj, D = t.def.adj;
  const pct = (x: number): string => (x < 1e-4 ? 'none' : x < 0.005 ? 'less than 1%' : `about ${Math.round(x * 100)}%`);
  // The army that broke, booked more than the field cost it: the pursuit's line straight after the strength line (b464).
  const pursuit = rep ? pursuitLine(t, r, rep) : null;
  // After a rout the line saying why comes second, so the strength line is the third (the sim's finish()).
  const strengthAt = r.how === 'rout' && r.brokeBy && r.brokeBy.by !== 'none' ? 2 : 1;
  const shown = bill ? [r.lines[0], `Your attack lost ${pct(bill.att)} of its strength, the defence ${pct(bill.def)}.`, ...r.lines.slice(1)]
    : pursuit ? [...r.lines.slice(0, strengthAt + 1), pursuit, ...r.lines.slice(strengthAt + 1)] : r.lines;
  const fieldLines = shown.map((l) => `<li>${esc(l)}</li>`).join('');
  let told = '', price = '', sides = '';
  if (rep) {
    const place = rep.regionName;
    // A capital or a city is fought in its outer streets (field.ts streetsOf): the heart of the city holds out (the third
    // playtester; the UI director's words, 19:06: not "the old town", which Washington or Sydney does not have).
    told = rep.siege ? (rep.streets ? `The battle in ${esc(t.place.town ?? place)}'s outer streets was won, but the heart of the city holds out: ${esc(place)} is under siege.`
      : `The country round ${esc(t.place.town ?? place)} was won, but the town holds out: ${esc(place)} is under siege.`)
      : rep.gates ? `The army outside was beaten, and ${esc(t.place.town ?? place)} opened its gates. ${esc(place)} has fallen.`
        : rep.taken ? `${esc(place)} was taken.` : `The attack on ${esc(place)} was thrown back.`;
    const sum = (us: typeof rep.attackers): number => us.reduce((s, u) => s + u.men, 0);
    const aLost = Math.max(rep.attLost, rep.attackers.reduce((s, u) => s + u.lost, 0)), dLost = Math.max(rep.defLost, rep.defenders.reduce((s, u) => s + u.lost, 0));
    const pct = (l: number, m: number): string => (m > 0 ? `, ${Math.max(1, Math.round((100 * l) / m))}% of those sent in,` : '');
    price = `It cost you ${num(aLost)} men${pct(aLost, sum(rep.attackers))} against ${num(dLost)} of theirs${rep.prisoners ? `, and ${num(rep.prisoners)} were taken prisoner` : ''}.`;
    // As the campaign's own report says it: a corps with no fate of its own fell back if its side lost.
    const list = (us: typeof rep.attackers, won: boolean): string => us.map((u) => `<li><b>${esc(u.name)}</b> <span>${facts([`${u.divs} division${u.divs === 1 ? '' : 's'}`, u.lost > 0 ? `lost ${num(u.lost)}` : 'in reserve, lost none', u.fate === '' && !won ? 'fell back' : u.fate === 'sheltered' && rep.streets ? 'fell back into the heart of the city' : FATE[u.fate] ?? u.fate])}</span></li>`).join('');
    sides = `<div class="aside2"><div><h3 class="tc-label">${esc(A)} attack</h3><ul>${list(rep.attackers, rep.taken)}</ul></div><div><h3 class="tc-label">${esc(D)} defence</h3><ul>${list(rep.defenders, !rep.taken)}</ul></div></div>`;
  }
  const paid = rep?.paid ? `<p class="paid">${esc(rep.paid.att)}</p>` : '';
  const veil = document.createElement('div');
  veil.className = 'veil';
  veil.innerHTML = `<div class="after tc-paper" role="dialog" aria-label="Battle result">
    <div class="tc-label kick">${rep ? 'Battle result' : 'Practice result'} · ${esc(t.date)}</div>
    <h2 class="tc-title-2">${esc(rep ? t.title : t.title.replace(/^Practice: /, ''))}</h2>
    <div class="verdict"><span class="tc-stamp ${won ? 'tc-stamp--won' : ''}">${esc(level)}</span></div>
    ${rep ? `<p class="told"><b>${told}</b> ${price}</p>${paid}` : ''}
    <h3 class="tc-label">On the field</h3><ul class="flines">${fieldLines}</ul>
    <h3 class="tc-label">Against the staff's reckoning</h3>
    <p class="dice">${esc(reckoned(won, chance, !rep, S.offer?.band?.said, !!rep?.siege))}${rep ? '' : ' Nothing goes back to the campaign from a practice ground.'}</p>
    ${settled?.drift ? '<p class="warn">The campaign moved while the battle was fought: its odds are not the card\'s any more.</p>' : ''}
    ${sides}
    <div class="afoot"><button class="tc-btn tc-btn--primary tc-btn--lg" data-act="continue">Continue</button></div>
  </div>`;
  (S.home ? app : document.body).appendChild(veil);
  veil.querySelector('[data-act="continue"]')!.addEventListener('click', () => {
    if (now() < S.guardUntil) return;
    veil.remove();
    if (S.home) S.home(rep); else showChooser();
  });
}

/** The page of its own: the chooser over a fresh campaign of the slice's own. */
export function bootPage(): void {
  app = document.getElementById('app')!;
  root = document;
  S.g = newCampaign();
  S.active = true;
  // A handle for the page's own checks (screenshots, the playtester's harness). Not used by the page itself.
  (window as unknown as { __field: unknown }).__field = { state: S, give, showChooser, start: startBattle };
  showChooser();
  startLoop();
}

/**
 * FIGHT IT YOURSELF on the campaign's attack card (the design director, b439): the field for that attack, opened over
 * the campaign in a layer of its own (its own looks inside, the campaign's untouched outside). The field's result goes
 * back through the campaign's own attack order (settle), and CONTINUE on the after card closes the layer and hands the
 * campaign its report. 'css' is the field's own stylesheet (the campaign's build carries it).
 */
/** Whether a field is open over the campaign: the campaign takes no order of its own until it closes. */
export const fieldOpen = (): boolean => S.active && S.home !== null;
/** Called as the field's result is booked into the campaign, before the after card shows it. */
let booked: (() => void) | null = null;

export function fightOnField(g: Game, offer: Offer, css: string, done: (report: BattleReport | null) => void, onBooked?: () => void): void {
  if (fieldOpen()) return;
  const host = document.createElement('div');
  host.style.cssText = 'position:fixed;inset:0;z-index:2000;';
  document.body.appendChild(host);
  // The campaign under the layer is out of reach while it is up: no key or Tab reaches its buttons (the review, b453).
  const shut = ([...document.body.children] as HTMLElement[]).filter((x) => x !== host && !x.inert);
  for (const x of shut) x.inert = true;
  (document.activeElement as HTMLElement | null)?.blur?.();
  booked = onBooked ?? null;
  const shadow = host.attachShadow({ mode: 'open' });
  // The stylesheet was written for a page of its own: its page-wide rules apply to the layer instead.
  const local = css.replace(/:root/g, ':host').replace(/html,\s*body\s*\{/g, ':host {').replace(/(^|\n)body\s*\{/g, '$1:host {');
  shadow.innerHTML = `<style>${local}</style><div id="app"></div>`;
  root = shadow;
  app = shadow.getElementById('app')!;
  S.g = g; S.before = null; S.active = true;
  S.home = (report) => {
    S.active = false; S.home = null; S.b = null; S.screen = 'choose';
    booked = null;
    host.remove();
    for (const x of shut) x.inert = false;
    root = document;
    done(report);
  };
  // The same handle as the page of its own, for the page's checks (screenshots, the playtester's harness).
  (window as unknown as { __field: unknown }).__field = { state: S, give };
  startBattle(offer.ticket, offer);
  startLoop();
}

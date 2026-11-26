// The convoy night's practice page (release 2; ruling 241 D2, Q9; 217(5): no press on the campaign screen until the
// Playtester finds the five practice nights easy). A page of its own, beside the fleet action's: choose a night, take the
// escorts or the pack, fight it top down on the night sea, and see how the night reads against the staff's own nights
// for the same card. Nothing a practice night does is booked.
//
// Mouse: left picks your own pieces (drag round several, Shift adds); right orders. An escort: right on a boat, or on
// the faded mark where one was last seen, to hunt her; on a wreck to pick up survivors; on the water to steam there and
// search. A boat: right on a ship to attack her. The wheel zooms. Keys: Space pause, 1 2 3 4 speed, arrows turn the view,
// F follow the convoy, Tab next piece, Esc put down, K the key, S star shell (escorts), G deck gun (boats).
//
// The clock (the playtest's question b, and the rules owner's calls on bugs 7 to 10 and round 3's 9 to 19): the
// player's chosen speed, held down for a while by a slow-down. Dusk runs at x60 until the first contact, which drops it
// to x10 whatever speed key was pressed before it: on the escorts' side the first boat an escort finds or the convoy's
// first attack, on the pack's the first of its boats found (the banner names her, and the log says so at once) or its
// first salvo. On the escorts' side a new contact (a boat no escort has held for two minutes) drops the clock to x10
// only after a quiet spell, no boat held for QUIET; in a running fight it is a line in the log and a banner. So does
// one of the player's escorts on a hunt finding her quarry again (never a new contact, however long she was lost), or
// first attacking her on that hunt. Such a slow-down follows its boat: it lasts while she is held or her last-seen
// mark is up, and a minute more, but never past SLOW_MOST from its start (its banner says so), and each order the
// player gives in it adds SLOW_MORE; a new cause on her after that brings a new one, and a hunter's finding her again
// brings one of its own at once. A new contact is never left out of the banners (contactBanner), not even at dawn
// (atDawn). The convoy under attack (the night's first salvo) drops the clock to x10 on both sides until that salvo
// has run; each later salvo's hits drop it to x60 for a minute, and on the pack's side each later salvo of the
// player's own boats from the moment she fires until it has run. A boat of the pack's player first found or first
// attacked, forced up or sunk drops it to x10 for a minute. Then the clock goes back to the speed the player last
// chose (a cause slows it only where it is slower than his speed). A speed key ends a slow-down at once; pausing does
// not. While the staff steer his side the player only watches, and nothing slows the clock.

import { againstStaff, foughtOf, ranksOf, restFrom, staffNights, type StaffNights } from '../convoy/calibrate';
import { CONVOY_BATTLE } from '../../campaign/navy';
import { CONVOY } from '../convoy/numbers';
import { CONVOY_PRACTICE, convoyPracticeTicket, type ConvoyPractice } from '../convoy/practice';
import { afterDusk, capital, ConvoySim, countWords, namesOf, sharesOf } from '../convoy/sim';
import type { ConvoyCommand, ConvoyOverExtra, ConvoySpot, ConvoyTicket, ConvoyUnit, Echo } from '../convoy/types';
import type { FieldEvent, FieldResult, Side, Zone } from '../core/types';

const esc = (s: string): string => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]!);
const cap = (k: string): string => `<span class="tc-key">${k}</span>`;
const hhmm = (sec: number): string => `${Math.floor(sec / 3600)}:${String(Math.floor((sec % 3600) / 60)).padStart(2, '0')}`;
/** A count and its noun, singular or plural, one to ten in words (the UI director, round 5): 'one torpedo', 'three torpedoes', '12 hits'. */
const nof = (k: number, one: string, many: string): string => `${countWords(k)} ${k === 1 ? one : many}`;
/** How often: 'once', 'twice', 'five times', '12 times'. */
const times = (k: number): string => k === 1 ? 'once' : k === 2 ? 'twice' : `${countWords(k)} times`;
/** Metres as the screens write them (3,200 m). */
const metres = (m: number): string => `${Math.round(m).toLocaleString('en-GB')} m`;
/** A short while of the night, for a last-seen mark: '40 s', '2 min 05 s'. */
const ago = (s: number): string => { const k = Math.max(0, Math.round(s)); return k < 60 ? `${k} s` : `${Math.floor(k / 60)} min ${String(k % 60).padStart(2, '0')} s`; };
const MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
const PEOPLE: Record<string, string> = { UK: 'British', GER: 'German', ITA: 'Italian', USA: 'American', JAP: 'Japanese', FRA: 'French', SOV: 'Soviet' };
/** Battle seconds a real second is worth at each speed (the night runs ten to eighteen hours). */
const SPEEDS = [0, 1, 10, 60, 300] as const;
/** The most battle seconds one frame may work, so a hidden tab never runs the night away. */
const MOST_A_FRAME = 600;
/** Staff nights the after card ranks the night among (the campaign's own count: handoff.ts convoyNight). */
const STAFF_NIGHTS = 30;
/** Battle seconds a slow-down outlasts its cause before the clock goes back to the player's speed. */
const SLOW_AFTER = 60;
/** Battle seconds with no boat held by any escort before a new contact slows the clock (the rules owner, round 3: a quiet spell). */
const QUIET = 300;
/** Battle seconds a slow-down that follows a boat may last at most, from its start, and the more each order the player gives in it adds. */
const SLOW_MOST = 300, SLOW_MORE = 120;
/** Real milliseconds between refreshes of the orders panel's words from the frame loop: four times a second (an order or a pick repaints it at once). */
const PANEL_EVERY = 250;
/**
 * Battle seconds of the night under the other's command that the after card passes over in naming whose night it was:
 * five minutes, five real seconds at the dusk's x60 (a player who lets the staff steer at dusk has had it for a second
 * or two of real time, and one who takes it back for a moment has not commanded the night). Who steered when is said
 * all the same, from the true spans, to the minute.
 */
const STEERED = 300;
/** Lines of the night's log on the sea. */
const LOG_LINES = 7;
/** A boat's keel, metres, past which the map draws her deep (above it, going down, at periscope depth). */
const AT_DEEP = CONVOY.boat.scope + 10;

interface Track { from: [number, number]; to: [number, number]; at: number; run: number; faint: boolean }
interface Charge { x: number; y: number; r: number; at: number; burst: number; ahead: boolean }
/** A line of the log, and how it reads for the player: plain, good news, bad news, or a warning (an order refused or overridden). */
interface Said { t: number; text: string; tone: '' | 'good' | 'bad' | 'warn'; gunners?: string[] }
/**
 * A slow-down of the clock: the speed it holds to, the banner's words for why and for how long, the battle second it
 * ends unless its cause is still in force ('until'), and the second it ends whatever ('most'). One that follows a boat
 * ('on', the escorts' side) is in force while she is held or her last-seen mark is up, and SLOW_AFTER more. 'key' joins
 * the causes that are one (the hits of one salvo).
 */
interface Slow { to: number; why: string; how: string; until: number; most: number; on?: number; key?: string }

interface State {
  screen: 'choose' | 'night' | 'after';
  card: number;
  side: Side;
  seed: number;
  t: ConvoyTicket | null;
  sim: ConvoySim | null;
  units: ConvoyUnit[];
  zones: Zone[];
  /** The other side's pieces lost from sight in the last three minutes, at their last known spots. */
  lastSeen: ConvoySpot[];
  /** Every boat the card brings (the pack's score counts them all). */
  pack: number;
  /** The name of every piece the player's side has seen this night, kept once it is out of sight (a hunt names its boat). */
  names: Map<number, string>;
  picked: Set<number>;
  /** The clock's speed now (0 paused): the player's chosen speed, held down by a slow-down. */
  speed: number;
  /** The speed the player last chose (dusk runs at x60 until he chooses). */
  chosen: number;
  paused: boolean;
  /** The slow-downs in force: the clock runs at the slowest of them and the player's own speed. */
  slows: Slow[];
  /** The battle second any escort last held a boat (the escorts' side): a new contact slows the clock only after QUIET without. */
  heldAt: number;
  /** The battle second a new contact last broke a quiet spell: another new contact in that same second broke it too. */
  spell: number;
  /** The pack's first contact told in the log by the page, for the found boat's own line to take its place (one line for one event). */
  foundLine: { on: number; said: Said } | null;
  owed: number;
  /** The camera: on the convoy, following one piece, or left where the player put it; 'fit' while it still widens to show all the player's pieces (dusk, until he moves it). */
  cam: { x: number; y: number; scale: number; follow: 'convoy' | number | null; fit: boolean };
  tracks: Track[];
  charges: Charge[];
  said: Said[];
  mode: 'starshell' | 'gun' | null;
  /**
   * The banner on the sea: its words, until when, and when it went up; where it is the new contacts' of a running fight,
   * each contact it counts and when each joined it; where it is a slow-down's for a new contact, that contact; 'clock',
   * where it only tells the clock given back (the clock note says so too).
   */
  tag: { text: string; until: number; at?: number; contacts?: string[]; joined?: number[]; contact?: string; clock?: boolean } | null;
  /** The new contacts waiting behind a banner of news (a slow-down, a hit), to be told in a banner of their own once it has gone. */
  queued: string[];
  /** The night over: the real millisecond the after card waits for, while new contacts not yet read are told (atDawn). */
  dawnAt: number | null;
  staff: StaffNights | null;
  /** The hand-over asked (the question is up), or under way (the staff are fighting out the rest). */
  asking: boolean;
  handing: boolean;
  /** The first contact's slow-down is still to come (a speed key at dusk leaves it armed). */
  rush: boolean;
  /** What has slowed the clock already this night, once each: 'charge:<boat>' (the pack's boats' first attack), 'charge:<escort>:<boat>:<hunt>' (an escort's first on a hunt). */
  alarmed: Set<string>;
  /** The night's first salvo: the boat that fired it and the second its torpedoes are home (its hits are its own slow-down's, not a later salvo's). */
  firstSalvo: { by: number; lands: number } | null;
  /** Each escort's hunt that has dropped charges on her quarry, as '<boat>:<the hunt's last second>': 'Running in on' until then, 'Attacking' after. */
  dropped: Map<number, string>;
  /** The spans of the night the staff steered the player's side, in battle seconds (the last open while they still do): the after card says who steered when. */
  reins: [number, number][];
  drag: { x0: number; y0: number; x1: number; y1: number } | null;
  /** The orders panel open beside the sea, and the key to the map shown (null until the first night: shown on it). */
  sideOpen: boolean;
  keyOn: boolean | null;
  panelAt: number;
  panelKey: string;
  detailWas: string;
}

const S: State = {
  screen: 'choose', card: 0, side: 'def', seed: 1, t: null, sim: null, units: [], zones: [], lastSeen: [], pack: 0, names: new Map(), picked: new Set(),
  speed: 60, chosen: 60, paused: false, slows: [], heldAt: -Infinity, spell: -Infinity, foundLine: null, owed: 0, cam: { x: 0, y: 0, scale: 0.06, follow: 'convoy', fit: true }, tracks: [], charges: [], said: [],
  mode: null, tag: null, queued: [], dawnAt: null, staff: null, asking: false, handing: false, rush: true, alarmed: new Set(), firstSalvo: null, dropped: new Map(), reins: [], drag: null, sideOpen: true, keyOn: null, panelAt: 0, panelKey: '', detailWas: '',
};

let app: HTMLElement;
let canvas: HTMLCanvasElement | null = null;
let last = 0;
/** Where the right button went down: an order needs both its press and its release on the sea. */
let rightDown = false;

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
    return `<button class="tc-panel offer${i === S.card ? ' is-on' : ''}" data-card="${i}" aria-pressed="${i === S.card}">
      <div class="tc-label">${esc(MONTHS[p.month - 1])} ${p.year}</div>
      <div class="otitle">${esc(p.title)}</div>
      <p class="oabout">${esc(p.about)}.</p>
      <div class="ofacts"><span class="nw">${nof(boats, 'boat', 'boats')}</span> <span class="nw">against ${nof(escorts, 'escort', 'escorts')}</span></div>
      <div class="ofacts tc-muted">The escorts have ${esc(kitWords(p))}.</div>
    </button>`;
  }).join('');
  // (the side picked takes the segment's own lit look, '.is-on': a kit button's brass is wiped by the segment's rule)
  const sideBtn = (s: Side, words: string): string => `<button class="${S.side === s ? 'is-on' : ''}" aria-pressed="${S.side === s}" data-side="${s}">${words}</button>`;
  app.innerHTML = `<div class="tc convoy choose">
    <header class="chead"><div class="tc-label">Practice nights</div><h1 class="tc-title-1">A convoy at night</h1></header>
    <section class="tc-paper intro"><p><b>One night on one convoy, from dusk to dawn.</b> The boats come up through the dark and fire into the columns. The escorts find them by radar, by sight and by sonar, run them down and drive them under with depth charges. A boat hurt badly enough is forced up, where her crew fight her deck gun, dive again if they can, or abandon her. What each side brings decides the night: the boats and their navy's habits, the escorts and the kit their navy has. Nothing a practice night does is counted in a campaign.</p></section>
    <div class="offers">${cards}</div>
    <div class="cfoot">
      <div class="cside"><span class="tc-label">Your side</span>
        <div class="tc-seg" role="group" aria-label="Your side">${sideBtn('def', 'Command the escorts')}${sideBtn('att', 'Command the pack')}</div>
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
  S.units = []; S.zones = []; S.lastSeen = []; S.names = new Map(); S.picked = new Set(); S.tracks = []; S.charges = []; S.said = []; S.mode = null; S.staff = null;
  S.asking = false; S.handing = false; S.drag = null;
  S.chosen = 60; S.paused = false; S.slows = []; S.heldAt = -Infinity; S.spell = -Infinity; S.foundLine = null; S.owed = 0; S.rush = true; S.alarmed = new Set(); S.firstSalvo = null; S.dropped = new Map(); S.reins = [];
  S.cam.follow = 'convoy'; S.cam.scale = 0.05; S.cam.fit = true;
  S.speed = 60;
  if (S.keyOn === null) S.keyOn = true;
  S.panelKey = ''; S.detailWas = ''; barWas = '';
  S.tag = { text: 'DUSK · RUNNING ×60 · THE CLOCK SLOWS TO ×10 AT THE FIRST CONTACT', until: performance.now() + 5000, clock: true }; S.queued = []; S.dawnAt = null;
  S.screen = 'night';
  const we = S.side === 'def' ? 'escorts' : 'pack';
  app.innerHTML = `<div class="tc convoy night">
    <header class="nbar">
      <div class="ntitle"><div class="tc-label">${esc(p.title)}</div><div class="nfacts" data-facts></div></div>
      <div class="nscore" data-score></div>
      <div class="nclock">
        <div class="nspeeds">
          <button class="tc-btn tc-btn--sm" data-speed="0" title="Space">Pause</button>
          <button class="tc-btn tc-btn--sm" data-speed="1" title="1">×1</button>
          <button class="tc-btn tc-btn--sm" data-speed="2" title="2">×10</button>
          <button class="tc-btn tc-btn--sm" data-speed="3" title="3">×60</button>
          <button class="tc-btn tc-btn--sm" data-speed="4" title="4">×300</button>
        </div>
        <span class="nnote tc-muted" data-clocknote></span>
        <div class="nstaffbtns">
          <button class="tc-btn tc-btn--sm" data-staffrun></button>
          <button class="tc-btn tc-btn--sm tc-btn--grave" data-hand title="The staff settle the rest of the night at once">End my command</button>
        </div>
      </div>
    </header>
    <div class="nmain">
      <div class="nsea">
        <canvas></canvas>
        <div class="ntags"><div class="nstaff" data-staffon></div><div class="ntag" data-tag></div></div>
        <div class="nlog" data-log></div>
      </div>
      <aside class="nside" data-sidebar>
        <section class="tc-panel norders" data-orders></section>
        <section class="nkey" data-key></section>
        <button class="tc-btn tc-btn--sm nunfold" data-unfold title="Show the orders">Orders</button>
      </aside>
    </div>
    <footer class="nkeys tc-muted"><span class="ncam" data-cam></span> · Click: pick up (drag or Shift for more) · Right-click: order · Wheel: zoom · ${cap('←↑↓→')} turn the view · ${cap('F')} the convoy · ${cap('Tab')} next · ${cap('Esc')} put down · ${cap('Space')} pause · ${cap('1')}–${cap('4')} speed · ${cap('K')} key</footer>
    <div class="nask" data-ask hidden>
      <section class="tc-panel askcard" role="dialog" aria-modal="true" aria-labelledby="ask-q">
        <h2 class="tc-title-3" id="ask-q">Hand the rest of the night to the staff?</h2>
        <p>The staff will fight out the rest of the night for your ${we} at once, and you go straight to how the night went. You can't take it back.</p>
        <div class="abtns"><button class="tc-btn tc-btn--grave" data-askyes>Yes</button><button class="tc-btn tc-btn--primary" data-askno>Keep command</button></div>
      </section>
    </div>
    <div class="nask" data-working hidden>
      <section class="tc-panel askcard working" role="status">
        <div class="tc-label">The staff are working</div>
        <h2 class="tc-title-3">The staff are fighting out the rest of the night.</h2>
        <p class="tc-muted">They fight this night ${STAFF_NIGHTS} times on their own to see what the rest of it would cost. This takes a few seconds.</p>
      </section>
    </div>
  </div>`;
  canvas = app.querySelector('canvas');
  app.querySelectorAll<HTMLElement>('[data-speed]').forEach((b) => b.addEventListener('click', () => { const v = SPEEDS[+b.dataset.speed!]; if (v === 0) togglePause(); else setSpeed(v); }));
  app.querySelector('[data-hand]')!.addEventListener('click', askHand);
  app.querySelector('[data-askyes]')!.addEventListener('click', () => { closeAsk(); handToStaff(); });
  app.querySelector('[data-askno]')!.addEventListener('click', keepCommand);
  // (a click on the dimmed night round the question leaves the focus on the answers)
  app.querySelector('[data-ask]')!.addEventListener('mousedown', (e) => { if (!(e.target as HTMLElement).closest('button')) e.preventDefault(); });
  app.querySelector('[data-staffrun]')!.addEventListener('click', () => {
    const s = S.sim!; if (s.over || S.handing) return;
    steer(!s.ai[S.side]);
    S.said.push({ t: s.t, text: s.ai[S.side] ? `The staff steer your ${we}. You can take it back at any time.` : `You take back command of your ${we}.`, tone: '' });
    // (watching, the clock keeps the player's speed: a slow-down in force ends, and the banner of what slows it goes)
    if (s.ai[S.side]) { S.slows = []; S.tag = null; S.queued = []; }
    clock();
    paintOrders(true); paintBar();
  });
  app.querySelector('[data-unfold]')!.addEventListener('click', () => { S.sideOpen = true; paintOrders(true); });
  wireCanvas();
  take();
  fit();
  paintOrders(true);
  paintKey();
  clock();
  last = performance.now();
  requestAnimationFrame(frame);
}

/** The staff take the player's side, or give it back: each span they steer is kept for the after card. */
function steer(on: boolean): void {
  const s = S.sim!;
  s.command({ do: 'auto', side: S.side, on }, S.side);
  // (from the second worked so far: the next second the sim works is the staff's, or the player's)
  const open = S.reins[S.reins.length - 1];
  if (on && !(open && open[1] === Infinity)) S.reins.push([s.worked, Infinity]);
  else if (!on && open && open[1] === Infinity) open[1] = s.worked;
}

/**
 * The view at dusk widens until it shows every piece on the sea, the whole convoy and every piece of the player's that
 * is up (the rules owner, round 3, 20: nothing starts off the sea), from the camera's own scale outward; never in. It
 * stops once he moves the camera, zooms or follows a piece, or at the night's first contact.
 */
function fit(): void {
  if (!S.cam.fit || S.cam.follow !== 'convoy' || !canvas) return;
  const c = canvas, W = c.clientWidth, H = c.clientHeight;
  const mv = S.units.filter((u) => u.kind === 'mv' && u.state !== 'gone');
  if (!W || !H || !mv.length) return;
  const cx = mv.reduce((a, u) => a + u.x, 0) / mv.length, cy = mv.reduce((a, u) => a + u.y, 0) / mv.length;
  // (the margin keeps a piece and its name clear of the edges, and of the log in the bottom corner)
  let scale = S.cam.scale;
  for (const u of S.units.filter(live)) {
    const dx = Math.abs(u.x - cx), dy = Math.abs(u.y - cy);
    if (dx > 1) scale = Math.min(scale, (W / 2 - 60) / dx);
    if (dy > 1) scale = Math.min(scale, (H / 2 - 70) / dy);
  }
  S.cam.scale = Math.max(0.008, scale);
}

// ---- The clock --------------------------------------------------------------------------------------------------------

/** The clock's speed now: paused, or the player's own speed held down by the slow-downs in force. */
const runSpeed = (): number => S.paused ? 0 : Math.min(S.chosen, ...S.slows.map((w) => w.to));

function clock(): void { S.speed = runSpeed(); paintClock(); }

/** A speed key: the player's choice from now on. It ends the slow-downs in force (a new cause brings a new one); the first contact's at dusk stays armed. */
function setSpeed(v: number): void {
  S.paused = false; S.chosen = v; S.slows = []; dropTag();
  clock();
}

/** Pause, or run on: running on keeps the slow-downs still in force, and the first contact's at dusk. */
function togglePause(): void { S.paused = !S.paused; dropTag(); clock(); }

/** A speed key takes down the banner of a slow-down it ended, never one of new contacts (a contact it told before it could be read comes back in one). */
const dropTag = (): void => { if (S.tag?.contacts) return; S.queued.unshift(...unread(S.tag, performance.now())); S.tag = null; };

/** Real milliseconds a banner stays up, and a banner of new contacts from when it first went up (later ones join it, never put it off). */
const TAG_MS = 4500;
/** Real milliseconds a new contact must have been told in a banner, a third of its time, before a banner of news may take its place for good (else the contact comes back after it), or dawn end the night. */
const TAG_SEEN = TAG_MS / 3;

/**
 * The new contacts a banner told that cannot have been read yet, each told in it less than TAG_SEEN: those of a banner of
 * new contacts that joined it that lately (one that joined it a moment ago, though it has been up long), a slow-down's
 * one while it is that young.
 */
const unread = (up: State['tag'], now: number): string[] => !up || up.until <= now ? []
  : up.contacts ? up.contacts.filter((_, i) => now - (up.joined?.[i] ?? up.at ?? -Infinity) < TAG_SEEN)
  : up.contact && now - (up.at ?? -Infinity) < TAG_SEEN ? [up.contact] : [];

/**
 * The banner on the sea, for a few seconds. A banner of news (a slow-down, a hit) takes the place of any, and the new
 * contacts of a banner it covers before they could be read come back after it in one of their own (nextTag); one that
 * only tells the clock given back ('back') never covers a banner of new contacts, and the clock note says it all the same.
 */
function banner(text: string, ms = TAG_MS, back = false): void {
  const now = performance.now(), up = S.tag && S.tag.until > now ? S.tag : null;
  if (up?.contacts && back) return;
  S.queued.unshift(...unread(up, now));
  S.tag = { text, until: now + ms, at: now, ...(back ? { clock: true } : {}) };
}

/** A banner of new contacts, up from now: every one counted, the last two told named, each once ('TWO NEW CONTACTS · ESCORT 3 HAS BOAT 1 · ESCORT 5 HAS BOAT 2'). */
const contactsText = (all: string[]): string => {
  const names = all.filter((c, i) => all.lastIndexOf(c) === i);
  return all.length === 1 ? `A NEW CONTACT · ${all[0]}` : `${countWords(all.length).toUpperCase()} NEW CONTACTS · ${names.slice(-2).join(' · ')}`;
};
const contactsTag = (all: string[], now: number): State['tag'] => ({ text: contactsText(all), until: now + TAG_MS, contacts: all, joined: all.map(() => now), at: now });

/**
 * A new contact in a running fight (the rules owner's (a): a line in the log and a banner, no slow-down), never left
 * out of the banners. One takes the place of a banner of the clock given back ('BACK TO ×300'); behind a banner of news
 * it waits for that to go (nextTag). While a banner of new contacts is up the next joins it rather than replacing it, so
 * that at speed none flickers past unread: 'A NEW CONTACT · ESCORT 3 HAS BOAT 1', then 'TWO NEW CONTACTS · ESCORT 3 HAS
 * BOAT 1 · ESCORT 5 HAS BOAT 2' (every contact counted, the last two named, the rest in the log). It goes TAG_MS after
 * it first went up, however many join it; a contact after that, or in its last TAG_SEEN, has a banner of its own.
 */
function contactBanner(what: string): void {
  const now = performance.now(), up = S.tag && S.tag.until > now ? S.tag : null;
  if (up && !up.contacts && !up.clock) { S.queued.push(what); return; }
  // (one in the last TAG_SEEN of a banner of new contacts could not be read in it: it waits for a banner of its own)
  if (up?.contacts) { if (up.until - now < TAG_SEEN) S.queued.push(what); else { up.contacts.push(what); up.joined?.push(now); up.text = contactsText(up.contacts); } return; }
  S.tag = contactsTag([...S.queued, what], now);
  S.queued = [];
}

/** Each frame: the new contacts kept waiting behind a banner of news come up once it has gone, in one banner of their own. */
function nextTag(): void {
  const now = performance.now();
  if (!S.queued.length || (S.tag && S.tag.until > now && !S.tag.clock)) return;
  S.tag = contactsTag(S.queued, now);
  S.queued = [];
}

/**
 * The night over: the new contacts not yet told long enough to be read (waiting behind a banner, or told less than
 * TAG_SEEN ago) are put up in a banner of their own, and the sea stays on screen TAG_SEEN for it before the after card;
 * none is left out of the banners at dawn. Whether the after card may come now.
 */
function atDawn(): boolean {
  const now = performance.now();
  if (S.dawnAt === null) {
    const up = S.tag && S.tag.until > now ? S.tag : null, owed = [...unread(up, now), ...S.queued];
    S.queued = [];
    S.dawnAt = owed.length ? now + TAG_SEEN : now;
    if (owed.length) S.tag = contactsTag(owed, now);
  }
  return now >= S.dawnAt;
}

/** Minutes in words: 'a minute', 'five minutes', '12 minutes'. */
const minutes = (m: number): string => m <= 1 ? 'a minute' : `${countWords(m)} minutes`;

/**
 * A cause to slow the clock. One that is the same as a slow-down in force (her boat's, or the same salvo's hits) keeps
 * that one, to the later end, unless it is one of its own ('alone'); else it is added. Where the clock runs below the
 * player's speed for it, the banner says why and for how long (one that follows a boat, the most it may last from now);
 * 'news' is said in a banner all the same (a new contact in a running fight, the rules owner's (a)).
 */
function slowDown(w: Slow, news = false, alone = false): void {
  const same = alone ? undefined : S.slows.filter((o) => (w.on !== undefined && o.on === w.on) || (w.key !== undefined && o.key === w.key)).sort((p, q) => q.most - p.most)[0];
  if (same) { same.until = Math.max(same.until, w.until); same.why = w.why; } else S.slows.push(w);
  const v = runSpeed(), held = same ?? w;
  const most = held.on === undefined ? '' : `, ${minutes(Math.ceil((held.most - S.sim!.t) / 60)).toUpperCase()} AT MOST`;
  if (v < S.chosen && Math.min(w.to, S.chosen) === v) banner(`${w.why} · RUNNING ×${v} ${held.how}${most}, THEN ×${S.chosen}`);
  else if (news) banner(w.why);
  clock();
}

/**
 * A slow-down to x10 that follows a boat (the escorts' side, the rules owner's (b)): while she is held or her mark is up,
 * and a minute more, at most SLOW_MOST from its cause unless orders put that off; its banner says that most
 * ('RUNNING ×10 WHILE BOAT 1 IS HELD OR HER MARK IS UP, 5 MINUTES AT MOST, THEN ×300'). Her cause again while it is in
 * force keeps it as it is; a new cause after it has run to its most brings a new one, and so does a hunter's finding
 * her own quarry again ('alone', the rules owner's (c): as in (b), with a most of its own, though another is in force).
 */
function slowBoat(why: string, on: number, t: number, news = true, alone = false): void {
  slowDown({ to: 10, why, how: `WHILE ${(nameOf(on) || 'the boat').toUpperCase()} IS HELD OR HER MARK IS UP`, until: t + SLOW_AFTER, most: t + SLOW_MOST, on }, news, alone);
}

/** A slow-down for a moment of the night: x10 (or 'to') for a minute. */
const slowAMinute = (why: string, t: number, to = 10, key?: string): void => slowDown({ to, why, how: 'FOR A MINUTE', until: t + SLOW_AFTER, most: Infinity, key });

/** A slow-down for a salvo: x10 (the night's first) or x60 (a later one of the pack's player) until its torpedoes are home. */
const slowSalvo = (why: string, to: number, lands: number, how: string): void => slowDown({ to, why, how, until: lands, most: Infinity });

/** An order given: each slow-down that follows a boat may last SLOW_MORE longer (the rules owner, round 3, (b)). */
function orderGiven(): void { for (const w of S.slows) if (w.on !== undefined) w.most += SLOW_MORE; }

/** Whether an escort holds this boat now. */
const heldNow = (on: number): boolean => S.units.some((u) => u.side === 'def' && u.kind === 'dd' && u.state !== 'gone' && u.extra.echo?.on === on);
/** Whether this boat's last-seen mark is up. */
const markedNow = (on: number): boolean => S.lastSeen.some((p) => p.id === on);

/** Each frame: a slow-down whose cause is over (a boat's a minute over), or that has run to its most, gives the clock back. */
function easeOff(): void {
  if (!S.sim || !S.slows.length) return;
  const now = S.sim.t, was = runSpeed();
  for (const w of S.slows) if (w.on !== undefined && (heldNow(w.on) || markedNow(w.on))) w.until = Math.max(w.until, now + SLOW_AFTER);
  S.slows = S.slows.filter((w) => now < Math.min(w.until, w.most));
  const v = runSpeed();
  if (!S.paused && v > was) banner(v === S.chosen ? `BACK TO ×${v}` : `NOW ×${v}, THEN ×${S.chosen}`, 2500, true);
  clock();
}

/** Once a night for each key: whether this is the first time ('charge:7', a boat's first attack). */
const firstOf = (key: string): boolean => { if (S.alarmed.has(key)) return false; S.alarmed.add(key); return true; };

/** An escort's doings on a hunt. */
const HUNTS = new Set(['run', 'search', 'attack', 'open']);

/** Whether a piece is the player's own to command (an ally's, under his side, is the staff's: ruling 241 Q4). */
const ownPiece = (u: ConvoyUnit | undefined): boolean => !!u && u.side === S.side && (S.t!.place.mine === undefined || u.nat === S.t!.place.mine);

/**
 * What slows the clock for this side, and the banners of what the player must follow (the rules owner's calls, round
 * 3, (a) to (f)). 'before' is the piece as the page last saw her, before this event: an escort's quarry as it was.
 */
function alarm(e: FieldEvent, x: Record<string, unknown>, mine: boolean, u: ConvoyUnit | undefined, before: ConvoyUnit | undefined): void {
  const name = (id: unknown): string => (nameOf(typeof id === 'number' ? id : undefined) || 'a boat').toUpperCase();
  const who = (u?.name ?? 'an escort').toUpperCase(), t = e.t;
  // (d) The convoy under attack, the night's first salvo, on both sides: x10 until it has run (its torpedoes home).
  if (e.what === 'sea:attack') {
    const run = Number(x.running ?? 0), first = S.rush;
    S.rush = false;
    S.firstSalvo = { by: x.by as number, lands: t + run };
    slowSalvo(S.side === 'def' ? `${first ? 'FIRST CONTACT · ' : ''}THE CONVOY IS UNDER ATTACK` : 'YOUR FIRST SALVO', 10, t + run, 'UNTIL THE SALVO HAS RUN');
    return;
  }
  // (d) Each later salvo's hits: x60 for a minute (a salvo has run when its torpedoes hit, in the first tick they are
  // home); the first salvo's are its own slow-down's. The hits of one salvo are one slow-down.
  if (e.what === 'sea:torpedoHit') {
    const f = S.firstSalvo;
    if (!(f && f.by === x.from && t >= f.lands && t - f.lands <= CONVOY.tick)) slowAMinute(`A TORPEDO HITS ${(u?.name ?? 'a ship').toUpperCase()}`, t, 60, `hits:${x.from}:${t}`);
    return;
  }
  if (S.side === 'att') {
    // (d) A later salvo of the player's own boats: x60 from the moment she fires until it has run (the first is the
    // convoy under attack's, told just before it).
    if (e.what === 'sea:torpedo' && ownPiece(u) && u) {
      if (!(S.firstSalvo?.by === u.id && S.firstSalvo.lands === t + Number(x.running ?? 0))) slowSalvo(`${u.name.toUpperCase()} FIRES A SALVO`, 60, t + Number(x.running ?? 0), 'UNTIL IT HAS RUN');
      return;
    }
    // (e) The first contact: told at the found boat's own line, which names her, and in the log at once (she may say it
    // herself only as she dives, a few seconds later). One made by a salvo is told by the salvo's banner.
    if (e.what === 'contact') { if (x.by === 'torpedo') S.rush = false; return; }
    if (e.what === 'sea:found') {
      if (S.rush) {
        S.rush = false;
        if (!e.text && e.unit !== undefined) { const said: Said = { t, text: `An escort has found ${nameOf(e.unit) || 'one of your boats'}.`, tone: 'bad' }; S.said.push(said); S.foundLine = { on: e.unit, said }; }
        slowAMinute(`FIRST CONTACT · AN ESCORT HAS FOUND ${name(e.unit)}`, t);
      } else if (x.first === true) slowAMinute(`${name(e.unit)} IS FOUND`, t);
      return;
    }
    if (e.what === 'sea:depthCharge' && mine && firstOf(`charge:${e.unit}`)) slowAMinute(`${name(e.unit)} IS ATTACKED`, t);
    else if (e.what === 'sea:sunk' && x.cls === 'ss' && mine) slowAMinute(`${name(e.unit)} IS LOST`, t);
    else if (e.what === 'sea:dive' && x.why === 'forced' && mine) slowAMinute(`${name(e.unit)} IS FORCED UP`, t);
    return;
  }
  if (e.what === 'sea:sonar' && 'fresh' in x) {
    // A contact gained. (f) A hunting escort finding her own quarry again is never a new contact; (c) on the player's
    // own hunt it slows the clock as in (b), however long she had lost her.
    const on = x.on as number, b = before ?? u;
    if (b && HUNTS.has(b.extra.doing as string) && (b.extra.prey === on || b.extra.chase === on)) {
      if (ownPiece(b)) slowBoat(`${who} HAS FOUND ${name(on)} AGAIN`, on, t, true, true);
      return;
    }
    // (a) A new contact (no escort held her for two minutes): the clock slows for it only after a quiet spell, no boat
    // held for QUIET; in a running fight the log and a banner tell it.
    if (x.news !== true) return;
    const first = S.rush;
    S.rush = false;
    const why = `${first ? 'FIRST CONTACT' : 'A NEW CONTACT'} · ${who} HAS ${name(on)}`;
    // (two new contacts in the second that broke the quiet spell both broke it: each slows the clock for its boat)
    // (the slow-down's banner tells the contact: covered before it could be read, it comes back in a banner of its own)
    if (t - S.heldAt >= QUIET || S.spell === t) { S.spell = t; slowBoat(why, on, t); if (S.tag?.text.startsWith(why)) S.tag.contact = `${who} HAS ${name(on)}`; }
    else contactBanner(`${who} HAS ${name(on)}`);
    return;
  }
  // One of the player's escorts dropping her first charges of a hunt: x10 as in (b), once a hunt.
  if (e.what === 'sea:depthCharge' && ownPiece(u) && u!.kind === 'dd' && typeof x.on === 'number' && firstOf(`charge:${u!.id}:${x.on}:${u!.extra.huntUntil ?? ''}`)) slowBoat(`${who} ATTACKS ${name(x.on)}`, x.on, t, false);
}

// ---- What the sim tells -----------------------------------------------------------------------------------------------

const live = (u: ConvoyUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';

/** Takes the sim's view: the pieces the player's side sees, the marks of those lately lost, and what happened since. */
function take(): void {
  const s = S.sim!;
  const v = s.view(S.side);
  // (each piece as the page saw her before these events: an escort's quarry as it was before her contact)
  const before = new Map(S.units.map((u) => [u.id, u]));
  S.units = v.units as ConvoyUnit[];
  S.zones = v.zones;
  S.lastSeen = v.lastSeen;
  S.pack = v.pack;
  for (const u of S.units) S.names.set(u.id, u.name);
  const at = new Map(S.units.map((u) => [u.id, u]));
  for (const e of v.events) note(e, at, before);
  // (a boat held now keeps the spell from being quiet: a new contact in it is the fight's, not a fresh one)
  if (S.side === 'def' && S.units.some((u) => u.side === 'def' && u.kind === 'dd' && u.state !== 'gone' && !!u.extra.echo)) S.heldAt = Math.max(S.heldAt, s.worked);
  // (handed to the staff, the player has left the night: the after card comes at once)
  if (v.over && S.screen === 'night' && (S.handing || atDawn())) after();
}

/** How a line reads for the player: his losses bad news, the other side's good, an order refused or overridden a warning. */
function toneOf(e: FieldEvent, x: Record<string, unknown>, unitSide: Side | undefined): Said['tone'] {
  const me = S.side;
  if (e.what === 'sea:order') return x.ok === false ? 'warn' : '';
  if (e.what === 'sea:dive') return x.override ? 'warn' : x.why === 'forced' ? (unitSide === me ? 'bad' : 'good') : '';
  if (e.what === 'sea:found') return 'bad';
  if (e.what === 'sea:depthCharge') return unitSide === me && me === 'att' ? 'bad' : '';
  if (e.what === 'sea:attack') return me === 'def' ? 'bad' : '';
  if (e.what === 'sea:burning') return x.limp ? (unitSide === me ? 'bad' : 'good') : me === 'def' ? 'good' : 'bad';
  if (['sea:torpedoHit', 'sea:sunk', 'sea:gunHit', 'sea:depthChargeHit', 'sea:abandon', 'sea:surrender'].includes(e.what)) return unitSide === me ? 'bad' : 'good';
  return '';
}

/** One event: its words in the log, its marks on the sea, and the clock slowed where the player must follow it. */
function note(e: FieldEvent, at: Map<number, ConvoyUnit>, before: Map<number, ConvoyUnit>): void {
  const x = (e.extra ?? {}) as Record<string, unknown>;
  const u = e.unit !== undefined ? at.get(e.unit) : undefined;
  if (e.what === 'sea:torpedo') {
    if (u && typeof x.x === 'number' && typeof x.y === 'number') S.tracks.push({ from: [u.x, u.y], to: [x.x, x.y as number], at: e.t, run: Number(x.running ?? 60), faint: !!x.wakeless });
  }
  if (e.what === 'sea:depthCharge' && typeof x.x === 'number') {
    S.charges.push({ x: x.x, y: x.y as number, r: Math.max(20, Number(x.spread ?? 60) / 2), at: e.t, burst: e.t + Number(x.sinking ?? 20), ahead: x.kind === 'ahead' });
  }
  const found = S.foundLine;
  if (e.what === 'sea:gun' && x.why === 'defend' && u) {
    // (the armed merchants open fire together: one line for those of one second, never a log full of them)
    const prev = S.said[S.said.length - 1];
    if (prev?.gunners && prev.t === e.t) { prev.gunners.push(u.name); prev.text = `The gunners of ${namesOf(prev.gunners)} open fire.`; }
    else S.said.push({ t: e.t, text: e.text, tone: '', gunners: [u.name] });
  } else if (e.text && (e.what === 'sea:dive' || e.what === 'sea:depth') && found && found.on === e.unit && x.reason === 'an escort has found her' && e.t - found.said.t <= 60) {
    // (the found boat's own line, 'Boat 1 dives: an escort has found her.', takes the place of the page's first-contact
    // line in the log: one line for one event, the first contact still told at once)
    found.said.text = e.text; found.said.t = e.t; S.foundLine = null;
  } else if (e.text) S.said.push({ t: e.t, text: e.text, tone: toneOf(e, x, u?.side ?? e.side) });
  if (S.said.length > 40) S.said.splice(0, S.said.length - 40);
  // (an escort's first charges of a hunt on her quarry: the panel says 'Attacking' from then, 'Running in on' before)
  if (e.what === 'sea:depthCharge' && u?.kind === 'dd' && typeof x.on === 'number') S.dropped.set(u.id, `${x.on}:${u.extra.huntUntil ?? ''}`);
  const mine = e.side === S.side || u?.side === S.side;
  // (the staff steering, the player only watches: nothing slows the clock, and the first contact passes)
  if (S.sim!.ai[S.side]) { if (e.what === 'contact' || e.what === 'sea:attack' || e.what === 'sea:found' || (e.what === 'sea:sonar' && x.news === true)) S.rush = false; }
  else alarm(e, x, mine, u, e.unit !== undefined ? before.get(e.unit) : undefined);
  // (any contact told is a boat held at that second: a later one within QUIET is no fresh contact)
  if (e.what === 'sea:sonar') S.heldAt = Math.max(S.heldAt, e.t);
}

function frame(nowMs: number): void {
  if (S.screen !== 'night' || !S.sim) return;
  const real = Math.min(0.25, (nowMs - last) / 1000);
  last = nowMs;
  const s = S.sim;
  easeOff();
  nextTag();
  // (the dusk view widens to show every piece of the player's as it comes up, until the first contact or until he moves it)
  if (S.cam.fit) { if (S.rush) fit(); else S.cam.fit = false; }
  if (!s.over && S.speed > 0 && !S.asking && !S.handing) {
    S.owed = Math.min(MOST_A_FRAME, S.owed + real * S.speed);
    const until = s.t + S.owed;
    let guard = 0;
    // (a slow-down met inside the frame takes hold at once: the rest of the frame's seconds are not worked at the old speed)
    const was = S.speed;
    while (!s.over && s.t + 1e-9 < until && guard++ < 20000) { s.step(); if (guard % 20 === 0) { take(); if (S.speed < was || S.screen !== 'night') break; } }
    // (the seconds worked past what was owed are owed back: the screen steps whole DTs, and at a slow speed a frame owes
    // less than one, so the clock would otherwise run fast, x1 at about x6)
    S.owed = S.speed < was ? 0 : until - s.t;
    if (S.screen === 'night') take();
  } else if (s.over && !S.handing) take();
  if (S.screen !== 'night') return;
  draw();
  paintBar();
  if (nowMs - S.panelAt >= PANEL_EVERY) { S.panelAt = nowMs; paintOrders(); }
  requestAnimationFrame(frame);
}

// ---- Orders ----------------------------------------------------------------------------------------------------------

function own(): ConvoyUnit[] { return S.units.filter((u) => u.side === S.side && live(u) && u.kind !== 'mv'); }
function picked(): ConvoyUnit[] { return own().filter((u) => S.picked.has(u.id)); }
const nameOf = (id: number | undefined): string => id === undefined ? '' : S.names.get(id) ?? S.lastSeen.find((p) => p.id === id)?.name ?? '';

function order(c: ConvoyCommand | { do: 'move'; units: number[]; x: number; y: number }, follow = false): void {
  const s = S.sim!;
  if (s.over || S.handing) return;
  if (s.ai[S.side]) { steer(false); S.said.push({ t: s.t, text: `You take back command of your ${S.side === 'def' ? 'escorts' : 'pack'}.`, tone: '' }); clock(); }
  s.command(c as ConvoyCommand, S.side);
  orderGiven();
  // (shown at once, paused or not: the answer in the log, the piece's new look, the panel; or the after card, where the
  // order sent the last boat home and the night ended with it)
  take();
  if (S.screen !== 'night') return;
  if (follow) { const u = picked().find((p) => c.units.includes(p.id)); if (u) { S.cam.follow = u.id; S.cam.fit = false; } }
  paintOrders(true);
  paintBar();
}

/** The orders panel: rebuilt when the pieces picked change, its words refreshed from the frame loop. */
function paintOrders(force = false): void {
  const el = app.querySelector<HTMLElement>('[data-orders]');
  if (!el || !S.sim) return;
  // (a piece sunk, gone home or not yet come is put down, and its buttons go with it)
  const mine = new Set(own().map((u) => u.id));
  for (const id of [...S.picked]) if (!mine.has(id)) S.picked.delete(id);
  const ps = picked();
  const staff = S.sim.ai[S.side];
  const we = S.side === 'def' ? 'escorts' : 'pack';
  const sb = app.querySelector<HTMLElement>('[data-staffrun]');
  if (sb) {
    const want = staff ? 'Take back command' : 'Let the staff steer <small>(you can take it back)</small>';
    if (sb.innerHTML !== want) sb.innerHTML = want;
    sb.classList.toggle('is-lit', staff);
    sb.title = staff ? `The staff are steering your ${we}. Press to command them yourself again.` : `The staff steer your ${we} and you watch. You can take it back at any time.`;
  }
  const on = app.querySelector<HTMLElement>('[data-staffon]');
  if (on) { const w = staff ? `THE STAFF ARE STEERING YOUR ${we.toUpperCase()} · GIVE ANY ORDER OR PRESS TAKE BACK COMMAND` : ''; if (on.textContent !== w) on.textContent = w; on.style.display = w ? 'block' : 'none'; }
  const side = app.querySelector<HTMLElement>('[data-sidebar]');
  side?.classList.toggle('is-folded', !S.sideOpen);
  // (boats forced up take only the deck gun, and those their crews have left nothing: the buttons follow what they can take)
  const up = S.side === 'att' && ps.length > 0 && ps.every(forcedUp), manned = ps.filter((u) => !abandoned(u));
  const key = `${S.side}|${S.sideOpen}|${ps.map((u) => u.id).join(',')}|${up}|${manned.length}`;
  if (force || key !== S.panelKey) {
    S.panelKey = key; S.detailWas = '';
    const head = ps.length === 0 ? (S.side === 'def' ? 'Your escorts' : 'Your pack') : ps.length === 1 ? esc(ps[0].name) : esc(namesOf(ps.map((u) => u.name)));
    const btn = (k: string, label: string, title = ''): string => `<button class="tc-btn tc-btn--sm" data-o="${k}" title="${esc(title)}">${label}</button>`;
    const gun = btn('gun', `Deck gun ${cap('G')}`, `Then click a ship within ${metres(CONVOY.surface.gunAt)}`);
    const btns = !ps.length ? '' : S.side === 'def'
      ? `<div class="obtns">${btn('screen', ps.length === 1 ? 'Back to its station' : 'Back to their stations', 'Return to the screen round the convoy')}${btn('starshell', `Star shell ${cap('S')}`, 'Then click where to light the sea')}</div>
        <p class="tc-faint ohint">Right-click a boat, or the faded mark where one was last seen, to hunt her. Right-click a wreck to pick up survivors, or the open sea to steam there and search.</p>`
      : up ? (!manned.length ? '' : `<div class="obtns">${gun}</div>
        <p class="tc-faint ohint">Forced to the surface, ${ps.length === 1 ? 'she can take no order but to fire her deck gun' : 'they can take no order but to fire their deck guns'}. Right-click a ship within ${metres(CONVOY.surface.gunAt)} to fire at her.</p>`)
      : `<div class="obtns">${btn('attack', 'Attack', 'On the surface')}${btn('under', 'Attack submerged')}${btn('dive', 'Dive', 'To periscope depth')}${btn('deep', 'Go deep')}${btn('surface', 'Surface')}${btn('evade', 'Evade')}${gun}${btn('leave', 'Leave the night')}</div>
        <p class="tc-faint ohint">Right-click a ship to attack her at the depth you last chose.</p>`;
    el.innerHTML = `<div class="ohead"><div class="tc-label">${head}</div><button class="tc-btn tc-btn--quiet ofold" data-fold title="Hide the orders and widen the sea">Hide</button></div>
      <div class="odetail" data-detail></div>${btns}`;
    el.querySelector('[data-fold]')!.addEventListener('click', () => { S.sideOpen = false; paintOrders(true); });
    const ids = ps.map((u) => u.id);
    el.querySelectorAll<HTMLElement>('[data-o]').forEach((b) => b.addEventListener('click', () => {
      const o = b.dataset.o!;
      if (o === 'starshell' || o === 'gun') { S.mode = o; return; }
      if (o === 'screen') order({ do: 'convoy:screen', units: ids });
      else if (o === 'attack') order({ do: 'convoy:attack', units: ids, under: false });
      else if (o === 'under') order({ do: 'convoy:attack', units: ids, under: true });
      else if (o === 'dive') order({ do: 'convoy:dive', units: ids });
      else if (o === 'deep') order({ do: 'convoy:dive', units: ids, deep: true });
      else if (o === 'surface') order({ do: 'convoy:surface', units: ids });
      else if (o === 'evade') order({ do: 'convoy:evade', units: ids });
      else if (o === 'leave') order({ do: 'convoy:leave', units: ids });
    }));
  }
  const detail = el.querySelector<HTMLElement>('[data-detail]');
  const words = detailOf(ps, staff);
  if (detail && words !== S.detailWas) { S.detailWas = words; detail.innerHTML = words; }
}

/** A boat forced to the surface (her damage, or her crew giving her up): she takes no order but the deck gun's. */
const forcedUp = (u: ConvoyUnit): boolean => u.kind === 'ss' && (u.extra.doing === 'up' || u.extra.doing === 'gun');
/** A boat her crew have left: she takes no order at all. */
const abandoned = (u: ConvoyUnit): boolean => forcedUp(u) && u.extra.ships === 0;

/** The panel's words: the side's state with nothing picked, a piece's own state, or the pieces picked. */
function detailOf(ps: ConvoyUnit[], staff: boolean): string {
  if (!ps.length) {
    if (staff) return `<p class="tc-muted">The staff are steering your ${S.side === 'def' ? 'escorts' : 'boats'}. Give any order, or press Take back command, to command them yourself again.</p>`;
    if (S.side === 'def') return '<p class="tc-muted">Your escorts keep their stations round the convoy and report what they find, but they do not leave the screen to hunt unless you send them. Pick one up and right-click a boat to send it hunting.</p>';
    const up = own().filter((u) => u.kind === 'ss').length;
    const lost = Object.values(S.sim!.counts.ss).reduce((a, b) => a + b, 0);
    const left = S.units.filter((u) => u.kind === 'ss' && u.side === 'att' && u.state === 'withdrawn').length;
    const coming = Math.max(0, S.pack - up - lost - left);
    const first = up === 0 && lost + left === 0;
    const where = up > 0 ? `${capital(nof(up, 'of your boats is', 'of your boats are'))} up with the convoy.` : first ? 'None of your boats is up with the convoy yet.' : 'None of your boats is up with the convoy now.';
    const more = !coming ? '' : first ? ` ${coming === 1 ? 'Your boat is' : `All ${countWords(coming)} are`} on the way and will come up during the night.` : ` ${capital(nof(coming, 'more is', 'more are'))} on the way and will come up during the night.`;
    // (true on every night: what a captain left to himself does when found, and what an Attack order makes of it)
    const radar = !!S.t!.place.kit[S.t!.def.nat]?.radar;
    // (the sim's own rules, round 3 (ii) and (iii): a boat found and not hunted ducks to periscope depth and keeps on)
    const pressed = ' An Attack order makes a captain stay up while she is only held; forced down by an escort closing, she keeps at periscope depth to attack submerged if the convoy comes within reach, and comes up to press in as soon as no escort holds her and none is in her sight. Ordered up from deep while an escort holds her or is in her sight, she comes up only to periscope depth and does the same.';
    const habit = (radar ? ' Left to themselves, your captains close the convoy, fire and turn away to reload. The escorts\' radar finds a boat on the surface far out: a boat found dives to periscope depth and keeps closing at her slower dived speed, and comes up again once no escort holds her and none is in her sight (one that cannot get in submerged first draws off out of the escorts\' sight); she goes deep only when an escort hunts her, and then lies quiet a long while.'
      : ' Left to themselves, your captains close the convoy, fire and turn away to reload; a boat an escort finds dives to periscope depth and keeps closing, and goes deep only when an escort hunts her.') + pressed
      // (C1 (a): every rise to the surface but one forced by damage waits for her look round)
      + ' A boat takes one look round at periscope depth before she surfaces, unless damage forces her up or she is called back up before she reaches it.';
    return `<p class="tc-muted">${where}${more}${habit} Pick one up to give her orders.</p>`;
  }
  if (ps.length > 1) return `<p class="tc-muted">${capital(nof(ps.length, S.side === 'def' ? 'escort' : 'boat', S.side === 'def' ? 'escorts' : 'boats'))} picked. An order goes to every one of them.</p>`;
  return `<p class="tc-muted">${pieceWords(ps[0])}</p>`;
}

/**
 * A boat's depth as she really is, by her keel and where she is going, never as ordered: 'Going deep, now at 4 m',
 * 'Coming up to the surface, now at 40 m'. She is on the surface only once her keel is (the rules owner, round 3, 6:
 * only there do radar and lookouts find her and her gun fire), as the log says it.
 */
function depthWords(u: ConvoyUnit): string {
  const keel = u.extra.keel ?? 0, at = metres(keel);
  if (u.extra.dive === 'surfaced') return keel <= 0 ? 'On the surface' : `Coming up to the surface, now at ${at}`;
  // (up from below for her look round before she surfaces, C1 (a): said as the log says it, 'comes up to periscope depth to look round')
  if (u.extra.dive === 'periscope') return keel < CONVOY.boat.scope - 1 ? `Diving to periscope depth, now at ${at}` : keel > CONVOY.boat.scope + 1 ? `Coming up to periscope depth${u.extra.rise ? ' to look round' : ''}, now at ${at}` : `At periscope depth, ${at}`;
  return keel < CONVOY.boat.deep - 2 ? `Going deep${u.extra.doing === 'deep' ? ' to lie quiet' : ''}, now at ${at}` : `Deep, at ${at}`;
}

/** From now to a battle second, in words: 'in a minute', 'in 12 minutes', 'in 2 hours and 15 minutes'. */
const inMinutes = (at: number): string => {
  const m = Math.max(1, Math.ceil((at - S.sim!.t) / 60));
  if (m < 90) return m === 1 ? 'in a minute' : `in ${countWords(m)} minutes`;
  const h = Math.floor(m / 60), r = m % 60;
  return `in ${nof(h, 'hour', 'hours')}${r ? ` and ${nof(r, 'minute', 'minutes')}` : ''}`;
};

function senseWords(e: Echo, name: string): string {
  return e.kind === 'sight' ? `Has ${name} in sight at ${metres(e.range)}` : `Holding ${name} by ${e.kind === 'asdic' ? 'sonar' : 'radar'} at ${metres(e.range)}`;
}

function pieceWords(u: ConvoyUnit): string {
  const x = u.extra, now = S.sim!.t;
  if (u.kind === 'ss') {
    // (one going deep is said so by her depth: 'Going deep to lie quiet, now at 4 m')
    const goingDeep = x.doing === 'deep' && (x.keel ?? 0) < CONVOY.boat.deep - 2;
    // (forced up too badly hurt to save, her crew will give her up whatever they try first: said so)
    const doing = x.doing === 'close' ? (x.off ? 'Drawing off out of the escorts\' sight to come up and run round on the surface, since she cannot get in submerged' : 'Closing the convoy to attack') : x.doing === 'away' ? 'Turning away to reload' : x.doing === 'evade' ? 'Evading the escorts'
      : x.doing === 'deep' ? (goingDeep ? '' : 'Lying deep and quiet')
      : abandoned(u) ? 'Her crew have left her, and she is going down'
      : x.doing === 'up' ? (x.doomed ? 'Forced to the surface too badly hurt to dive again; her crew will have to abandon her' : 'Forced to the surface; her crew are trying to make good the damage, and she may dive again once it is repaired')
      : x.doing === 'gun' ? (x.doomed ? 'Forced to the surface too badly hurt to dive again; her crew fight her deck gun until they must abandon her' : 'Forced to the surface and fighting her deck gun while her damage is repaired') : '';
    const aim = nameOf(x.aim as number | undefined);
    // (the rules owner, round 3, (iii): an Attack order at periscope depth keeps her there, to attack dived where the
    // convoy comes within reach, and up again as soon as no escort holds her and none is in her sight (turning away to
    // reload under an order to fire her gun, as soon as none is in her sight). Said by what keeps her there now, in the
    // present and only while it does (the UI director, round 5, 3): held, an escort in her sight, else nothing but her
    // look round (C1 (a)), with how she came down where an escort closing forced her. 'Keeps to periscope depth', true on
    // her way there as well as there (the UI director, round 6, 1); her look done, nothing but the gate's next tick (2))
    const free = x.doing === 'close' ? 'no escort holds her and none is in her sight' : 'no escort is in her sight';
    const press = x.order === 'attack' ? 'press in' : 'fire her gun', holders = x.doing === 'close' ? (x.heard ?? []).length : 0;
    const stays = `so she keeps to periscope depth${x.order === 'attack' ? ', attacks submerged if the convoy comes within her reach,' : ''} and comes up to ${press} as soon as ${free}.`;
    const none = x.doing === 'close' ? 'holds her or is in her sight now' : 'is in her sight now';
    const kept = (x.order === 'attack' || x.order === 'gun') && x.dive === 'periscope' && (x.doing === 'close' || x.doing === 'away')
      ? ` ${holders ? `${holders === 1 ? 'An escort holds' : `${capital(countWords(holders))} escorts hold`} her, ${stays}` : x.sees ? `She has an escort in her sight, ${stays}`
        : `${x.duck ? `An escort closing forced her down. None ${none}` : `No escort ${none}`}, so she ${x.looked ? '' : 'takes one look round at periscope depth and '}comes ${x.duck ? 'back ' : ''}up to ${press}.`}` : '';
    // (any other rise to the surface held at periscope depth for her look round, C1 (a): her own with nothing else keeping
    // her down. A Surface order's says it in the order while she is still below (the UI director, round 6, 3))
    const look = !kept && x.order !== 'surface' && x.look ? ' She takes one look round at periscope depth before she surfaces.' : '';
    const ord = (x.order === 'attack' ? `Ordered to attack${aim ? ` ${aim}` : ''} on the surface.` : x.order === 'under' ? `Ordered to attack${aim ? ` ${aim}` : ''} submerged.`
      : x.order === 'gun' ? `Ordered to fire her deck gun${aim ? ` at ${aim}` : ''}.` : x.order === 'dive' ? 'Ordered to periscope depth.' : x.order === 'deep' ? 'Ordered deep.'
      : x.order === 'surface' ? (x.rise ? 'Ordered to the surface: she takes one look round at periscope depth first.' : 'Ordered to stay on the surface.') : x.order === 'evade' ? 'Ordered to evade.' : '') + kept + look;
    const heard = (x.heard ?? []).map((id) => nameOf(id)).filter(Boolean), held = (x.heard ?? []).length;
    const by = !held ? '' : heard.length === held ? ` ${namesOf(heard)} ${held === 1 ? 'holds' : 'hold'} her.` : ` ${capital(nof(held, 'escort holds', 'escorts hold'))} her.`;
    // (down of her captain's own accord with an order standing: when she goes back to it, unless an escort hears her first)
    const surfaceOrder = x.order === 'attack' || x.order === 'gun';
    // (held, the wait starts again each time she is heard: said as how long she must go unheard)
    const quiet = held && typeof x.upAt === 'number' ? inMinutes(x.upAt).replace(/^in /, '') : '';
    const up = typeof x.upAt !== 'number' ? '' : surfaceOrder ? ` Her captain keeps her down while the escorts are about: ${held ? `she comes up to carry out the order once no escort has heard her for ${quiet}` : `unless one hears her again, she comes up to carry out the order ${inMinutes(x.upAt)}`}.`
      : x.order === 'dive' ? ` Her captain keeps her deep while the escorts are about: ${held ? `she comes back up to periscope depth once no escort has heard her for ${quiet}` : `unless one hears her again, she comes back up to periscope depth ${inMinutes(x.upAt)}`}.`
      : (x.tubes ?? 0) === 0 ? '' : held ? ` She takes up the attack again once no escort has heard her for ${quiet}.` : ` Unless an escort hears her again, she takes up the attack again ${inMinutes(x.upAt).replace(/^in /, 'within ')}.`;
    return `${depthWords(u)}.${doing ? ` ${doing}.` : ''}${ord ? ` ${ord}` : ''}${by}${up}${abandoned(u) ? '' : ` ${x.tubes ? capital(nof(x.tubes, 'torpedo', 'torpedoes')) : 'No torpedoes'} in the tubes.`}`;
  }
  // (the boat it holds; else the one its hunt is after, kept by name once it has lost her; else the one it was sent after)
  const e = x.echo, held = e ? nameOf(e.on) || 'a boat' : '', prey = nameOf(x.prey as number | undefined), chase = nameOf(x.chase as number | undefined);
  const boat = held || prey || chase;
  const d = x.doing as string | undefined, moved = x.order === 'move';
  // (the boat it is after, held by another escort and drawn on the map: said so, not 'where she was last known')
  const quarry = !e ? (x.prey ?? x.chase) as number | undefined : undefined;
  const others = quarry === undefined || !S.units.some((v) => v.id === quarry && live(v)) ? [] : S.units.filter((v) => v.id !== u.id && v.kind === 'dd' && v.state !== 'gone' && v.extra.echo?.on === quarry).map((v) => v.name);
  const heldBy = others.length ? `, held by ${namesOf(others)}` : '';
  // (in on her until its first charges of the hunt are down: attacking her from then)
  const dropped = !!boat && S.dropped.get(u.id) === `${e?.on ?? quarry}:${x.huntUntil ?? ''}`;
  const phase = d === 'screen' ? 'On station round the convoy' : d === 'back' ? 'Going back to its station round the convoy'
    : d === 'move' ? 'Steaming to the spot you gave it, to search there' : d === 'rescue' ? (u.speed < 0.5 ? 'Picking up survivors' : 'Steaming to pick up survivors')
    : d === 'run' ? (held ? `Running in on ${held}` : heldBy ? `Running to ${boat}${heldBy}` : boat ? `Running to where ${boat} was last known` : 'Running in')
    : d === 'search' ? (held ? `Searching for ${held}` : heldBy ? `Searching for ${boat}${heldBy}` : boat ? `Searching for ${boat} where she was last known` : moved ? 'Searching the sea where you sent it' : 'Searching for a boat')
    : d === 'attack' ? (x.huntPhase === 'blind' ? `Running over ${boat || 'her'} to drop charges; its sonar can't hear her this close` : dropped ? `Attacking ${boat}` : `Running in on ${boat || 'a boat'}`)
    : d === 'open' ? `Opening the range after an attack on ${boat || 'a boat'}, to come round again` : 'On station round the convoy';
  const holding = e ? ` ${senseWords(e, held)}.` : heldBy ? '' : boat && d !== 'screen' && d !== 'back' ? ` It has no contact on ${boat} now.` : ' It holds no boat.';
  const giveUp = typeof x.giveUp === 'number' ? x.giveUp : undefined;
  const until = giveUp === undefined ? '' : d === 'search' && !held ? ` Unless it finds ${boat ? 'her' : 'a boat'}, it gives up and goes back to its station ${inMinutes(giveUp)}.`
    : ` It must give up the hunt and go back ${inMinutes(giveUp)}.`;
  return `${phase}.${holding}${until}${x.ships > 1 ? ` ${capital(countWords(x.ships))} ships.` : ''}`;
}

// ---- Handing the night to the staff -----------------------------------------------------------------------------------

let pausedBefore = false;

/** The night behind a question on the page: nothing in it takes a click, a key or the focus while the question is up. */
function behind(on: boolean): void { app.querySelectorAll<HTMLElement>('.night > header, .night > .nmain, .night > footer').forEach((el) => { el.inert = on; }); }

/** End my command: asked on the page first, the clock stopped while the question is up. */
function askHand(): void {
  if (!S.sim || S.sim.over || S.handing || S.asking) return;
  S.asking = true; pausedBefore = S.paused; S.paused = true; S.mode = null; clock();
  behind(true);
  app.querySelector<HTMLElement>('[data-ask]')!.hidden = false;
  app.querySelector<HTMLElement>('[data-askno]')!.focus();
}

function closeAsk(): void { S.asking = false; behind(false); app.querySelector<HTMLElement>('[data-ask]')!.hidden = true; }

function keepCommand(): void { closeAsk(); S.paused = pausedBefore; clock(); }

function handToStaff(): void {
  if (!S.sim || S.sim.over || S.handing) return;
  S.handing = true;
  S.paused = true; clock();
  behind(true);
  app.querySelector<HTMLElement>('[data-working]')!.hidden = false;
  // (the staff's nights take a few seconds and hold the page: the notice is painted first)
  requestAnimationFrame(() => requestAnimationFrame(() => setTimeout(() => {
    S.staff = staffNights(S.t!, STAFF_NIGHTS);
    S.sim!.rest = restFrom(S.staff, S.sim!.limit);
    S.sim!.command({ do: 'dice' }, S.side);
    take();
  }, 30)));
}

// ---- The after card ---------------------------------------------------------------------------------------------------

/**
 * The night's result in one plain line, from the game's own record: its level (levelOfNight, the one source of it) and
 * its counts. 'A close win for the pack: 10 of 44 ships sunk, no boats lost.' The escorts' close win in which the pack
 * sank no more than one ship in twenty, which only that twentieth kept from being the pack's, is the convoy getting
 * through (the rules owner, round 3, 3): 'The convoy got through: 1 of 44 ships sunk, no boats lost.'
 */
function winnerWords(over: FieldResult, mv: number, ships: number, lostB: number, lostE: number): string {
  const how = /Heroic|Crushing/.test(over.level) ? 'An overwhelming win' : /Decisive/.test(over.level) ? 'A clear win' : /Pyrrhic/.test(over.level) ? 'A costly win' : 'A close win';
  const through = over.winner === 'def' && over.level === 'Close defeat' && mv <= ships / 20;
  const bits = [mv === 0 ? `none of the ${countWords(ships)} ships sunk` : `${countWords(mv)} of ${countWords(ships)} ships sunk`, lostB === 0 ? 'no boats lost' : `${nof(lostB, 'boat', 'boats')} lost`];
  if (lostE) bits.push(`${nof(lostE, 'escort', 'escorts')} lost`);
  return `${through ? 'The convoy got through' : `${how} for the ${over.winner === 'att' ? 'pack' : 'escorts'}`}: ${bits.length > 2 ? `${bits.slice(0, -1).join(', ')} and ${bits[bits.length - 1]}` : bits.join(', ')}.`;
}

/** A while of the night in words, to the nearest minute: '4 minutes', '1 hour and 5 minutes', '9 hours'. */
const lengthOf = (sec: number): string => {
  const m = Math.round(sec / 60), h = Math.floor(m / 60), r = m % 60;
  return h === 0 ? nof(m, 'minute', 'minutes') : `${nof(h, 'hour', 'hours')}${r ? ` and ${nof(r, 'minute', 'minutes')}` : ''}`;
};

/**
 * Who commanded the player's side through the night. Whose night it was, for the rank's words, from the sim's own count
 * of the seconds he steered it: his (the staff had it for under STEERED), the staff's (he had it for under STEERED: a
 * player who lets the staff steer at dusk has had it a minute or two), or both. Who steered when, from the true spans
 * (the rules owner, round 3, 4 and 22), to the minute: 'The staff steered your escorts from 0:01 to 11:16; you had the
 * rest.' A hand-over alone is said with its hour, in the sim's own words for it; after it the staff's dice fought the rest.
 */
function commandOf(x: ConvoyOverExtra, night: number): { who: 'mine' | 'staff' | 'both'; words: string } {
  const we = S.side === 'def' ? 'escorts' : 'boats', had = x.steered[S.side];
  const who = had < STEERED ? 'staff' : night - had < STEERED ? 'mine' : 'both';
  // The staff's spans, in minutes of the night: those the player let them steer and a hand-over's rest, joined.
  const end = Math.round(night / 60), min = (sec: number): number => Math.min(end, Math.round(Math.min(sec, night) / 60));
  const handed = x.handed && x.handedAt !== undefined ? x.handedAt : undefined;
  const spans: [number, number][] = [];
  for (const [a, b] of [...S.reins, ...(handed !== undefined ? [[handed, night]] : [])].map(([a, b]) => [min(a), min(b)]).filter(([a, b]) => b > a).sort((p, q) => p[0] - q[0])) {
    const l = spans[spans.length - 1];
    if (l && a <= l[1]) l[1] = Math.max(l[1], b); else spans.push([a, b]);
  }
  if (!spans.length) return { who, words: '' };
  // His spans: the rest of the night.
  const his: [number, number][] = [];
  let at = 0;
  for (const [a, b] of spans) { if (a > at) his.push([at, a]); at = b; }
  if (at < end) his.push([at, end]);
  // (handed over within the first minute: the staff fought the night from dusk, as the sim's own line says)
  if (!his.length || (handed !== undefined && afterDusk(handed) === 'dusk')) return { who, words: `The staff steered your ${we} all night.` };
  if (handed !== undefined && spans.length === 1 && spans[0][0] === min(handed)) return { who, words: `You commanded your ${we} until ${afterDusk(handed)}, and the staff from then to dawn.` };
  const when = (m: number): string => m === 0 ? 'dusk' : m === end && night >= S.sim!.limit - 60 ? 'dawn' : hhmm(m * 60);
  // ('the first minute', 'the last 3 minutes', 'the first hour and 5 minutes')
  const span = (m: number): string => lengthOf(m * 60).replace(/^one /, '');
  const yours = his.length > 1 ? 'the rest' : his[0][0] === 0 ? `the first ${span(his[0][1])}` : his[0][1] === end ? `the last ${span(his[0][1] - his[0][0])}` : 'the rest';
  return { who, words: `The staff steered your ${we} ${andOf(spans.map(([a, b]) => `from ${when(a)} to ${when(b)}`))}; you had ${yours}.` };
}

function after(): void {
  S.screen = 'after';
  const s = S.sim!, t = S.t!, over = s.over!;
  const x = over.extra as unknown as ConvoyOverExtra;
  const sum = (r: Record<number, number>): number => Object.values(r).reduce((a, b) => a + b, 0);
  const lostB = sum(x.counts.ss), lostE = sum(x.counts.dd), mv = x.counts.mv;
  const ships = S.units.filter((u) => u.kind === 'mv').length || t.place.ships;
  const lines = over.lines.map((l) => `<p>${esc(l)}</p>`).join('');
  const tl = x.tally;
  // (the night as fought: to dawn, or to when it ended with no boat left in it; a night handed over runs to dawn)
  const night = Math.min(s.limit, over.seconds ?? s.limit);
  const cmd = commandOf(x, night);
  const pack = S.side === 'att' && cmd.who === 'mine' ? 'Your boats' : 'The boats';
  const escorts = S.side === 'def' && cmd.who === 'mine' ? 'Your escorts' : 'The escorts';
  // (after a hand-over, each line says inside itself how much of it the staff's dice gave for the rest of the night)
  const st = x.handed ? x.staff?.tally : undefined;
  const dice = (parts: [number, number, string, string][]): string => {
    if (!st || parts.every(([all]) => all === 0)) return '';
    const of = parts.filter(([all]) => all > 0).map(([all, k, one, many]) => k === 0 ? `none of the ${many}` : k === all ? (all === 1 ? `the ${one}` : `all ${countWords(all)} ${many}`) : `${countWords(k)} of the ${many}`);
    return `; the staff's dice for the rest of the night gave ${andOf(of)}`;
  };
  const facts = [
    tl.salvoes === 0 ? `${pack} fired no torpedoes.`
      : `${pack} fired ${nof(tl.salvoes, 'salvo', 'salvoes')}, and ${tl.hits === 0 ? 'none hit' : `${nof(tl.hits, 'torpedo', 'torpedoes')} hit`}${dice([[tl.salvoes, st?.salvoes ?? 0, 'salvo', 'salvoes'], [tl.hits, st?.hits ?? 0, 'hit', 'hits']])}.`,
    tl.hunts === 0 ? `${escorts} never went hunting a boat.`
      : `${escorts} went hunting ${times(tl.hunts)}${tl.attacks === 0 ? ' but never got to attack a boat with depth charges' : ` and attacked boats with depth charges ${times(tl.attacks)}`}${dice([[tl.hunts, st?.hunts ?? 0, 'hunt', 'hunts'], [tl.attacks, st?.attacks ?? 0, 'attack', 'attacks']])}.`,
  ];
  app.innerHTML = `<div class="tc convoy afterc"><section class="tc-panel acard">
    <div class="tc-label">${esc(t.title)} · ${hhmm(s.limit)} of dark${night < s.limit - 60 ? ` · it ended at ${hhmm(night)}, with no boat left in it` : ''}</div>
    <h1 class="tc-title-2 awin">${esc(winnerWords(over, mv, ships, lostB, lostE))}</h1>
    <p class="averdict" data-verdict><b>The staff are fighting the same night ${STAFF_NIGHTS} times to see how yours compares…</b></p>
    <div class="alines">${lines}</div>
    <ul class="afacts tc-muted">${facts.map((f) => `<li>${esc(f)}</li>`).join('')}</ul>
    <p class="acamp tc-muted" data-camp></p>
    <div class="abtns">
      <button class="tc-btn" data-again>Same night again</button>
      <button class="tc-btn" data-new>A new night</button>
      <button class="tc-btn tc-btn--primary" data-choose>Choose another</button>
    </div>
  </section></div>`;
  app.querySelector('[data-again]')!.addEventListener('click', () => begin(S.seed));
  app.querySelector('[data-new]')!.addEventListener('click', () => begin(Math.floor(Math.random() * 1e9)));
  app.querySelector('[data-choose]')!.addEventListener('click', choose);
  requestAnimationFrame(() => setTimeout(() => {
    if (S.screen !== 'after' || S.sim !== s) return;
    const staff = S.staff ?? staffNights(t, STAFF_NIGHTS);
    const el = app.querySelector('[data-verdict]'), camp = app.querySelector('[data-camp]');
    if (el) el.innerHTML = `<b>${esc(rankWords(t, x, staff, cmd, leanOf(t, x, staff), over.winner))}</b>`;
    if (camp) camp.textContent = campaignWords(t, x, staff);
  }, 30));
}

/** And-joined: 'a', 'a and b', 'a, b and c'. */
const andOf = (xs: string[]): string => xs.length > 1 ? `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}` : xs[0] ?? '';

/**
 * One of the night's totals against the staff's nights, in true counts, the parts that are nought left out: the first
 * part in full ('more ships than the staff's pack did in 21 of the 30 times it fought this night'), the rest short ('as
 * many as in 3 and fewer than in 6'). 'lead' is how the first part names the staff's nights; a part that covers them
 * all says so.
 */
function versus(parts: [number, string, string][], n: number, lead: (k: number) => string): string {
  const nz = parts.filter(([k]) => k > 0);
  if (!nz.length) return '';
  const [[k0, full], ...rest] = nz;
  return andOf([`${full} ${lead(k0)}`, ...rest.map(([k, , short]) => `${short} in ${countWords(k)}`)]);
}

/** The night's place among the staff's nights by the ships sunk, 0 to 1 as the pack reads it: the booked swing's own (calibrate.ts foughtOf on the night's ranks). */
function placeOf(t: ConvoyTicket, x: ConvoyOverExtra, staff: StaffNights): number {
  const { low, high } = CONVOY_BATTLE.swing;
  return (foughtOf(t, ranksOf(t, x.counts, staff)).swing - low) / Math.max(1e-6, high - low);
}

/**
 * The side the night favours by the ships sunk (the campaign line's better and worse nights): the pack's where its place
 * is 0.6 or more and it sank more ships than at least half the staff's nights did, the escorts' where its place is 0.4
 * or less and it sank fewer than at least half did, else neither. A tie drawn by lot may place a night high among the
 * staff's nights it ties, but a night that sank no more ships than most of them is never one of the better ones for the
 * pack, nor one that sank no fewer one of the better ones for the escorts.
 */
function leanOf(t: ConvoyTicket, x: ConvoyOverExtra, staff: StaffNights): Side | null {
  const place = placeOf(t, x, staff), a = againstStaff(t, x.counts, staff), half = a.n / 2;
  return place >= 0.6 && a.mv.less >= half ? 'att' : place <= 0.4 && a.mv.more >= half ? 'def' : null;
}

/**
 * The night against the staff's own nights at it, in true counts: 'Your pack sank more ships than the staff's pack did
 * in 21 of the 30 times it fought this night…'. Where it points the other way from the headline (a win for one side on
 * a night that was one of the better ones for the other, by the ships sunk), it starts 'Even so,' with the count that
 * points that way, so the two never read as one against the other (the rules owner, round 3, 3); who steered when
 * follows it.
 */
function rankWords(t: ConvoyTicket, x: ConvoyOverExtra, staff: StaffNights, cmd: { who: 'mine' | 'staff' | 'both'; words: string }, lean: Side | null, winner: Side): string {
  const a = againstStaff(t, x.counts, staff), n = a.n;
  const lostE = Object.values(x.counts.dd).reduce((s, k) => s + k, 0);
  const escortsCount = lostE > 0 || a.dd.same < n;
  const mine = cmd.who === 'mine', pack = S.side === 'att';
  // (the first count names the staff's nights; the later ones say only how many)
  const first = (k: number): string => mine ? `the staff's ${pack ? 'pack' : 'escorts'} did in ${k === n ? 'every one' : countWords(k)} of the ${countWords(n)} times ${pack ? 'it' : 'they'} fought this night`
    : `in ${k === n ? 'every one' : countWords(k)} of the ${countWords(n)} times the staff fought this night`;
  const later = (k: number): string => k === n ? `in all ${countWords(n)}` : `in ${countWords(k)}`;
  const subject = pack ? (mine ? 'Your pack' : 'Your boats') : 'Your escorts';
  // (one subject through the sentence: 'Your boats sank …; they lost …', 'Your pack sank …; it lost …')
  const it = pack && mine ? 'it' : 'they';
  // (the staff's nights that sank fewer ships than this one lean it the pack's way, those that sank more the escorts')
  const even = lean !== null && lean !== winner && (lean === 'att' ? a.mv.less : a.mv.more) > 0;
  const lead = (parts: [number, string, string][], way: Side): [number, string, string][] => even && lean !== way ? [...parts].reverse() : parts;
  let body: string;
  if (pack) {
    const ships = versus(lead([[a.mv.less, 'sank more ships than', 'more than'], [a.mv.same, 'sank as many ships as', 'as many as'], [a.mv.more, 'sank fewer ships than', 'fewer than']], 'att'), n, first);
    const boats = versus([[a.ss.more, 'lost fewer boats than', 'fewer than'], [a.ss.same, 'lost as many boats as', 'as many as'], [a.ss.less, 'lost more boats than', 'more than']], n, later);
    const escortsSunk = escortsCount ? versus([[a.dd.less, 'sank more escorts than', 'more than'], [a.dd.same, 'sank as many escorts as', 'as many as'], [a.dd.more, 'sank fewer escorts than', 'fewer than']], n, later) : '';
    body = `${subject} ${ships}; ${it} ${boats}${escortsSunk ? `, and ${it} ${escortsSunk}` : ''}.`;
  } else {
    const ships = versus(lead([[a.mv.more, 'lost fewer ships than', 'fewer than'], [a.mv.same, 'lost as many ships as', 'as many as'], [a.mv.less, 'lost more ships than', 'more than']], 'def'), n, first);
    const boats = versus([[a.ss.less, 'sank more boats than', 'more than'], [a.ss.same, 'sank as many boats as', 'as many as'], [a.ss.more, 'sank fewer boats than', 'fewer than']], n, later);
    const escortsLost = escortsCount ? versus([[a.dd.more, 'lost fewer escorts than', 'fewer than'], [a.dd.same, 'lost as many escorts as', 'as many as'], [a.dd.less, 'lost more escorts than', 'more than']], n, later) : '';
    body = `${subject} ${ships}; ${it} ${boats}${escortsLost ? `, and ${it} ${escortsLost}` : ''}.`;
  }
  const said = even ? `Even so, ${body[0].toLowerCase()}${body.slice(1)}` : body;
  return cmd.words ? `${said} ${cmd.words}` : said;
}

/**
 * What a campaign would make of the night, in plain words: what the month would book by the rules (calibrate.ts
 * foughtOf on the night's ranks, unchanged) and so where the night falls among the staff's nights by the ships sunk,
 * which is what sets the tonnage booked: the share of the usual tonnage, and the boats and escorts lost booked the same
 * way, by their own counts, where the month expects any. Where staff nights sank as many ships as this one, it says so,
 * and that a lot settles the tie, the share saying where it fell (words only: the booking is the ranks' own).
 */
function campaignWords(t: ConvoyTicket, x: ConvoyOverExtra, staff: StaffNights): string {
  const a = againstStaff(t, x.counts, staff), n = Math.max(1, a.n);
  const ranks = ranksOf(t, x.counts, staff), fought = foughtOf(t, ranks);
  const { low, high } = CONVOY_BATTLE.swing;
  // (never printed as a bound it did not reach (the UI director, round 5, 5): sharesOf)
  const pc = sharesOf(fought.swing, high, low);
  // (the place is the booked swing's own, held to the counts: never one of the better nights for a side whose count most of the staff's nights matched or beat)
  const lean = leanOf(t, x, staff);
  const who = S.side === 'att' ? 'the pack, by the ships it sank' : 'the escorts, by the ships they lost';
  const place = lean === S.side ? `one of the better nights the staff could have had for ${who}` : lean ? `one of the worse nights the staff could have had for ${who}` : `an ordinary night for ${who}, about as the staff would have fought it`;
  // (the staff's nights by one name, the verdict's (the UI director, round 5, 2))
  const tons = `the month would book ${pc.share}% of the tonnage a convoy usually loses (${pc.high}% after a night that sinks more ships than every one of the ${countWords(n)} times the staff fought it, ${pc.low}% after one that sinks fewer than every one).`;
  // (tied: the lot settles where the night falls among the staff's nights it ties, and 'this time' carries the chance;
  // the share beside it says where the lot put it, never 'high' or 'low' (the UI director, round 5, 1))
  const tie = a.mv.same > 0 ? ` ${S.side === 'att' ? 'The pack sank' : 'The escorts lost'} as many ships as in ${a.mv.same === n ? 'every one' : countWords(a.mv.same)} of the ${countWords(n)} times the staff fought this night, and a lot settles the tie: this time ${tons}` : '';
  // Boats and escorts lost: booked at the lower or the higher of the two whole counts around what the month expects, by
  // the same ranks. A practice card expects none, so there is nothing to book.
  const booked = (side: Side): string => {
    const fs = t.place.fleets.filter((f) => f.side === side && f.n > 0);
    if (!fs.some((f) => f.expect > 0)) return '';
    const lo = fs.reduce((k, f) => k + Math.floor(f.expect), 0), hi = fs.reduce((k, f) => k + Math.ceil(f.expect), 0);
    const got = fs.reduce((k, f) => k + ((side === 'att' ? fought.ss[f.id] : fought.dd[f.id]) ?? 0), 0);
    const what = side === 'att' ? ['boat', 'boats'] : ['escort', 'escorts'];
    return `${nof(got, what[0], what[1])} lost${lo === hi ? '' : ` (the dice allow ${countWords(lo)} or ${countWords(hi)})`}`;
  };
  const bits = [booked('att'), booked('def')].filter(Boolean);
  const lost = bits.length ? ` Its boats and escorts lost, placed the same way by their own counts, would book ${andOf(bits)}.`
    : ' Boats and escorts lost are booked the same way where the month expects some to be lost; a practice night has no month behind it, so it books none.';
  return `In a campaign this night would count as ${place}${tie ? `.${tie}` : `: ${tons}`}${lost}`;
}

// ---- The sea ----------------------------------------------------------------------------------------------------------

function wireCanvas(): void {
  const c = canvas!;
  c.addEventListener('contextmenu', (e) => e.preventDefault());
  c.addEventListener('mousedown', (e) => {
    if (S.asking || S.handing) return;
    const [x, y] = local(e);
    if (e.button === 2) { rightDown = true; return; }
    if (e.button === 0) {
      if (S.mode) { modeAt(x, y); return; }
      S.drag = { x0: x, y0: y, x1: x, y1: y };
    }
  });
  c.addEventListener('mousemove', (e) => { if (S.drag) { const [x, y] = local(e); S.drag.x1 = x; S.drag.y1 = y; } });
  c.addEventListener('mouseup', (e) => {
    const [x, y] = local(e);
    if (e.button === 2) { if (rightDown) rightAt(x, y); rightDown = false; return; }
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
  c.addEventListener('wheel', (e) => {
    e.preventDefault();
    S.cam.fit = false;
    const k = e.deltaY > 0 ? 0.85 : 1.18, to = Math.max(0.008, Math.min(1.2, S.cam.scale * k));
    // (on the pointer while the camera is free; on the middle while it follows something)
    if (S.cam.follow === null) { const [sx, sy] = local(e), [wx, wy] = toSea(sx, sy); S.cam.scale = to; const [nx, ny] = toSea(sx, sy); S.cam.x += wx - nx; S.cam.y += wy - ny; }
    else S.cam.scale = to;
  }, { passive: false });
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

function nearest<T extends { x: number; y: number }>(sx: number, sy: number, us: T[], within: number): T | null {
  let best: T | null = null, bd = within;
  for (const u of us) { const [x, y] = toScreen(u.x, u.y); const d = Math.hypot(x - sx, y - sy); if (d < bd) { bd = d; best = u; } }
  return best;
}

function rightAt(sx: number, sy: number): void {
  if (S.asking || S.handing) return;
  if (S.mode) { modeAt(sx, sy); return; }
  const ps = picked();
  if (!ps.length) return;
  const ids = ps.map((u) => u.id);
  if (S.side === 'def') {
    const boat = nearest(sx, sy, S.units.filter((u) => u.kind === 'ss' && live(u)), 18);
    if (boat) { order({ do: 'convoy:hunt', units: ids, target: boat.id }, true); return; }
    // (the faded mark where a boat was last seen: the hunt goes to it)
    const spot = nearest(sx, sy, S.lastSeen.filter((p) => p.cls === 'ss'), 18);
    if (spot) { order({ do: 'convoy:hunt', units: ids, target: spot.id }, true); return; }
    const wreck = nearest(sx, sy, S.units.filter((u) => u.state === 'gone' && u.kind !== 'ss'), 18);
    if (wreck) { order({ do: 'convoy:rescue', units: ids, target: wreck.id }, true); return; }
    const [x, y] = toSea(sx, sy);
    order({ do: 'move', units: ids, x, y }, true);
  } else {
    // (no depth sent: the boat keeps the one her commander last chose, Attack submerged staying submerged; boats forced
    // up, whose only order is the deck gun, turn it on the ship)
    const ship = nearest(sx, sy, S.units.filter((u) => u.side === 'def' && live(u)), 18);
    if (ship) order(ps.every(forcedUp) ? { do: 'convoy:gun', units: ids, target: ship.id } : { do: 'convoy:attack', units: ids, target: ship.id });
  }
}

function modeAt(sx: number, sy: number): void {
  const ids = picked().map((u) => u.id);
  const m = S.mode; S.mode = null;
  if (!ids.length) return;
  if (m === 'starshell') { const [x, y] = toSea(sx, sy); order({ do: 'convoy:starshell', units: ids, x, y }); }
  if (m === 'gun') {
    const ship = nearest(sx, sy, S.units.filter((u) => u.side === 'def' && live(u)), 18);
    if (ship) order({ do: 'convoy:gun', units: ids, target: ship.id });
    else S.said.push({ t: S.sim!.t, text: 'No ship there for the deck gun: click on a ship.', tone: 'warn' });
  }
}

/** A boat's look by her real depth, by her keel: on the surface (keel and all), diving, rising or at periscope depth, deep (her keel past it). */
const lookOf = (u: ConvoyUnit): 'surfaced' | 'periscope' | 'deep' => u.extra.dive === 'surfaced' && (u.extra.keel ?? 0) <= 0 ? 'surfaced' : (u.extra.keel ?? 0) < AT_DEEP ? 'periscope' : 'deep';

function draw(): void {
  const c = canvas!, s = S.sim!, t = S.t!;
  const dpr = window.devicePixelRatio || 1;
  if (c.width !== Math.round(c.clientWidth * dpr) || c.height !== Math.round(c.clientHeight * dpr)) { c.width = Math.round(c.clientWidth * dpr); c.height = Math.round(c.clientHeight * dpr); }
  const g = c.getContext('2d')!;
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  const W = c.clientWidth, H = c.clientHeight, now = s.t;
  const mv = S.units.filter((u) => u.kind === 'mv' && u.state !== 'gone');
  const f = S.cam.follow;
  if (typeof f === 'number') {
    const u = S.units.find((v) => v.id === f && live(v));
    if (u) { S.cam.x = u.x; S.cam.y = u.y; } else S.cam.follow = 'convoy';
  }
  if (S.cam.follow === 'convoy' && mv.length) { S.cam.x = mv.reduce((a, u) => a + u.x, 0) / mv.length; S.cam.y = mv.reduce((a, u) => a + u.y, 0) / mv.length; }
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
    const p = Math.min(1, (now - k.at) / Math.max(1, k.run));
    const [ax, ay] = toScreen(k.from[0], k.from[1]), [bx, by] = toScreen(k.to[0], k.to[1]);
    g.strokeStyle = k.faint ? 'rgba(237,230,214,0.25)' : 'rgba(237,230,214,0.6)'; g.lineWidth = 1;
    g.beginPath(); g.moveTo(ax, ay); g.lineTo(ax + (bx - ax) * p, ay + (by - ay) * p); g.stroke();
  }
  // Orders under way: a dashed brass line from the piece to where her order sends her (a hunt, a spot, a wreck, an attack's ship).
  g.strokeStyle = 'rgba(224,184,114,0.55)'; g.setLineDash([5, 4]); g.lineWidth = 1;
  for (const u of S.units) {
    if (u.side !== S.side || !live(u)) continue;
    let to: { x: number; y: number } | undefined;
    if (u.kind === 'ss' && u.extra.aim !== undefined && u.extra.gun === undefined) to = S.units.find((v) => v.id === u.extra.aim && live(v));
    if (u.kind === 'dd') to = (u.extra.chase !== undefined ? S.units.find((v) => v.id === u.extra.chase && live(v)) : undefined) ?? (u.extra.goal as { x: number; y: number } | undefined);
    if (!to) continue;
    const [ax, ay] = toScreen(u.x, u.y), [bx, by] = toScreen(to.x, to.y);
    g.beginPath(); g.moveTo(ax, ay); g.lineTo(bx, by); g.stroke();
    g.beginPath(); g.arc(bx, by, 4, 0, Math.PI * 2); g.stroke();
  }
  g.setLineDash([]);
  // Contacts: an escort's line to the boat it holds, by the sense that holds her.
  for (const u of S.units) {
    const e = u.extra.echo;
    if (!e || u.state === 'gone') continue;
    const b = S.units.find((v) => v.id === e.on);
    if (!b) continue;
    const [ax, ay] = toScreen(u.x, u.y), [bx, by] = toScreen(b.x, b.y);
    g.strokeStyle = LINE[e.kind]; g.lineWidth = 1.5;
    g.setLineDash(e.kind === 'asdic' ? [2, 3] : []); g.beginPath(); g.moveTo(ax, ay); g.lineTo(bx, by); g.stroke(); g.setLineDash([]);
  }
  // The marks of pieces lately lost from sight: faded, at their last known spots, with the time since.
  placed = [];
  const lg = app.querySelector<HTMLElement>('[data-log]');
  if (lg && lg.childElementCount) { const a = lg.getBoundingClientRect(), b = c.getBoundingClientRect(); logBox = { x: a.left - b.left, y: a.top - b.top, w: a.width, h: a.height }; } else logBox = null;
  // (the pieces' names first, the picked ones before the rest, so a mark's name never takes a piece's place)
  const order = [...S.units].filter((u) => u.state !== 'withdrawn').sort((a, b) => (S.picked.has(b.id) ? 1 : 0) - (S.picked.has(a.id) ? 1 : 0) || (a.kind === 'mv' ? 1 : 0) - (b.kind === 'mv' ? 1 : 0));
  for (const p of S.lastSeen) drawSpot(g, p, now, false);
  // The pieces (a boat gone home is no longer on the sea), and their names.
  for (const u of S.units) if (u.state !== 'withdrawn') drawPiece(g, u, false);
  for (const u of order) drawPiece(g, u, true);
  for (const p of S.lastSeen) drawSpot(g, p, now, true);
  // The box being dragged.
  if (S.drag) { const d = S.drag; g.strokeStyle = 'rgba(224,184,114,0.8)'; g.strokeRect(Math.min(d.x0, d.x1), Math.min(d.y0, d.y1), Math.abs(d.x1 - d.x0), Math.abs(d.y1 - d.y0)); }
  // The tag.
  const tag = app.querySelector<HTMLElement>('[data-tag]');
  if (tag) {
    const show = S.mode ? (S.mode === 'starshell' ? 'CLICK WHERE TO FIRE STAR SHELL · ESC TO CANCEL' : 'CLICK A SHIP FOR THE DECK GUN · ESC TO CANCEL') : S.tag && S.tag.until > performance.now() ? S.tag.text : '';
    if (tag.textContent !== show) tag.textContent = show;
    tag.style.display = show ? 'block' : 'none';
  }
}

/** The contact lines by the sense that holds the boat, each its own colour: radar a clear blue, sight a warm white, sonar a dotted green. */
const LINE: Record<Echo['kind'], string> = { asdic: 'rgba(140,205,120,0.9)', radar: 'rgba(90,185,250,0.9)', sight: 'rgba(250,228,180,0.85)' };

interface Box { x: number; y: number; w: number; h: number }
const meets = (a: Box, b: Box): boolean => a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
/** The names drawn on the sea this frame, and the log's box over it: a name is put where it meets none of them. */
let placed: Box[] = [];
let logBox: Box | null = null;

/**
 * A piece's name beside her, clear of her mark ('off' pixels from her middle), on a dark plate so that no line runs
 * through it: to the right, or the left first where 'left' (an escort whose contact line runs to the right), else
 * above or below her, or a line up or down, at the first place that meets no other name and is clear of the log; a
 * ship in the columns ('above') above or below her first. Always whole on the sea. None for a piece whose mark is off
 * the sea or under the log.
 */
function label(g: CanvasRenderingContext2D, text: string, sx: number, sy: number, bright: boolean, faint = false, off = 9, above = false, left = false): void {
  const W = (g.canvas as HTMLCanvasElement).clientWidth, H = (g.canvas as HTMLCanvasElement).clientHeight;
  if (sx < -6 || sx > W + 6 || sy < -6 || sy > H + 6) return;
  if (logBox && meets(logBox, { x: sx - 1, y: sy - 1, w: 2, h: 2 })) return;
  g.font = '11px "Courier Prime", "Courier New", monospace';
  g.textBaseline = 'middle';
  const w = g.measureText(text).width;
  const R: [number, number] = [sx + off, sy], Lf: [number, number] = [sx - off - w, sy], U: [number, number] = [sx - w / 2, sy - off - 4], D: [number, number] = [sx - w / 2, sy + off + 4];
  const sides = left ? [Lf, R] : [R, Lf];
  const tries: [number, number][] = above ? [U, D, ...sides] : [...sides, U, D, ...sides.flatMap(([x, y]) => [[x, y - 14], [x, y + 14]] as [number, number][])];
  const box = ([x, y]: [number, number]): Box => { const cx = Math.max(4, Math.min(W - 4 - w, x)), cy = Math.max(8, Math.min(H - 8, y)); return { x: cx - 3, y: cy - 7, w: w + 6, h: 14 }; };
  // (never over her own mark: a place pushed back inside the sea's edge onto it is no place)
  const mark: Box = { x: sx - off + 4, y: sy - off + 4, w: 2 * off - 8, h: 2 * off - 8 };
  const free = tries.map(box).find((b) => !meets(mark, b) && !placed.some((p) => meets(p, b)) && !(logBox && meets(logBox, b)));
  const b = free ?? box(tries[0]);
  placed.push(b);
  g.fillStyle = 'rgba(8,10,14,0.78)'; g.fillRect(b.x, b.y, b.w, b.h);
  g.fillStyle = faint ? 'rgba(168,159,141,0.8)' : bright ? '#f4dea8' : 'rgba(226,215,188,0.9)';
  g.fillText(text, b.x + 3, b.y + 7);
}

/** The corners of a box round a mark ('r' pixels out): the faded 'last seen' mark, never a ring (a ring is a boat deep). */
function corners(g: CanvasRenderingContext2D, sx: number, sy: number, r: number): void {
  const k = r * 0.45;
  g.beginPath();
  for (const [dx, dy] of [[-1, -1], [1, -1], [1, 1], [-1, 1]]) { g.moveTo(sx + dx * r, sy + dy * (r - k)); g.lineTo(sx + dx * r, sy + dy * r); g.lineTo(sx + dx * (r - k), sy + dy * r); }
  g.stroke();
}

function boatShape(g: CanvasRenderingContext2D, look: 'surfaced' | 'periscope' | 'deep', solid: boolean): void {
  if (look === 'deep' && !solid) { g.setLineDash([2, 2]); g.beginPath(); g.arc(0, 0, 5, 0, Math.PI * 2); g.stroke(); g.setLineDash([]); return; }
  g.beginPath(); g.moveTo(0, -6); g.lineTo(3.5, 5); g.lineTo(-3.5, 5); g.closePath();
  if (look === 'surfaced' || solid) g.fill(); else g.stroke();
}

function drawSpot(g: CanvasRenderingContext2D, p: ConvoySpot, now: number, named: boolean): void {
  const [sx, sy] = toScreen(p.x, p.y);
  if (named) { label(g, `${p.name} · last seen ${ago(now - p.t)} ago`, sx, sy, false, true, 14); return; }
  g.save(); g.globalAlpha = 0.45;
  g.strokeStyle = p.cls === 'ss' ? (S.side === 'att' ? '#e0b872' : '#e08a6a') : '#8fb0c8'; g.fillStyle = g.strokeStyle; g.lineWidth = 1.2;
  corners(g, sx, sy, 10);
  g.translate(sx, sy);
  if (p.cls === 'ss') boatShape(g, p.dive === 'deep' ? 'deep' : 'periscope', false);
  else { g.beginPath(); g.moveTo(0, -7); g.lineTo(4, 5); g.lineTo(-4, 5); g.closePath(); g.stroke(); }
  g.restore();
}

/** A piece's mark, or ('named') her name: the names are put after every mark is drawn, so none is drawn over. */
function drawPiece(g: CanvasRenderingContext2D, u: ConvoyUnit, named: boolean): void {
  const [sx, sy] = toScreen(u.x, u.y);
  const gone = u.state === 'gone';
  const on = S.picked.has(u.id);
  const len = Math.max(u.kind === 'mv' ? 7 : 9, u.d * S.cam.scale), beam = Math.max(3, u.w * S.cam.scale * 1.6);
  const ring = Math.max(9, len / 2 + 4);
  if (named) {
    // A name on every escort and boat; a merchant's only where an order of the player's names her. An escort's on
    // the side away from the line to the boat she holds.
    const shown = u.kind !== 'mv' || S.units.some((v) => v.side === S.side && live(v) && (v.extra.aim === u.id || v.extra.gun === u.id));
    const e = u.extra.echo, to = e ? S.units.find((v) => v.id === e.on) : undefined;
    const left = !!to && toScreen(to.x, to.y)[0] > sx;
    if (shown) label(g, gone ? `${u.name} (sunk)` : u.name, sx, sy, on, gone, on ? ring + 4 : Math.max(9, len / 2 + 3), u.kind === 'mv', left);
    return;
  }
  g.save();
  g.translate(sx, sy);
  if (gone && u.kind === 'mv') {
    // (a wreck: a red cross plain enough to right-click for survivors)
    g.strokeStyle = 'rgba(224,110,90,0.85)'; g.lineWidth = 2;
    g.beginPath(); g.moveTo(-4, -4); g.lineTo(4, 4); g.moveTo(4, -4); g.lineTo(-4, 4); g.stroke();
    g.restore();
    return;
  }
  g.rotate(u.face);
  if (u.kind === 'mv') {
    g.fillStyle = '#b7ad98';
    g.fillRect(-beam / 2, -len / 2, beam, len);
  } else if (u.kind === 'dd') {
    g.fillStyle = gone ? 'rgba(140,47,34,0.7)' : '#8fb0c8';
    g.beginPath(); g.moveTo(0, -len / 2 - 3); g.lineTo(beam / 2 + 1.5, len / 2); g.lineTo(-beam / 2 - 1.5, len / 2); g.closePath(); g.fill();
  } else {
    const col = gone ? 'rgba(140,47,34,0.7)' : u.side === S.side ? '#e0b872' : '#e08a6a';
    g.strokeStyle = col; g.fillStyle = col; g.lineWidth = 1.5;
    boatShape(g, lookOf(u), gone);
  }
  g.restore();
  if (on) { g.strokeStyle = '#f4dea8'; g.lineWidth = 1; g.beginPath(); g.arc(sx, sy, ring, 0, Math.PI * 2); g.stroke(); }
  if (u.extra.gun !== undefined && !gone) {
    const t = S.units.find((v) => v.id === u.extra.gun);
    if (t) { const [tx, ty] = toScreen(t.x, t.y); g.strokeStyle = 'rgba(240,166,58,0.5)'; g.lineWidth = 1; g.beginPath(); g.moveTo(sx, sy); g.lineTo(tx, ty); g.stroke(); }
  }
}

// ---- The key ----------------------------------------------------------------------------------------------------------

function paintKey(): void {
  const el = app.querySelector<HTMLElement>('[data-key]');
  if (!el) return;
  el.style.display = S.keyOn ? 'block' : 'none';
  if (!S.keyOn) return;
  const line = (stroke: string, dash = ''): string => `<svg width="28" height="10" aria-hidden="true"><line x1="1" y1="5" x2="27" y2="5" stroke="${stroke}" stroke-width="1.5"${dash ? ` stroke-dasharray="${dash}"` : ''}/></svg>`;
  const tri = (fill: string, solid: boolean, op = 1): string => `<svg width="28" height="14" aria-hidden="true" opacity="${op}"><path d="M14 1 L18 12 L10 12 Z" fill="${solid ? fill : 'none'}" stroke="${fill}" stroke-width="1.5"/></svg>`;
  // (the 'last seen' mark as the map draws it: a faint outline in the corners of a box)
  const seen = (c: string): string => `<svg width="28" height="16" aria-hidden="true" opacity="0.6"><path d="M7 4 V1 H10 M18 1 H21 V4 M21 12 V15 H18 M10 15 H7 V12" fill="none" stroke="${c}" stroke-width="1.2"/><path d="M14 3 L17 12 L11 12 Z" fill="none" stroke="${c}" stroke-width="1.2"/></svg>`;
  const ring = (c: string): string => `<svg width="28" height="14" aria-hidden="true"><circle cx="14" cy="7" r="5" fill="none" stroke="${c}" stroke-width="1.5" stroke-dasharray="2 2"/></svg>`;
  const boat = S.side === 'att' ? '#e0b872' : '#e08a6a', boats = S.side === 'att' ? 'Your boat' : 'A boat';
  const rows = [
    [line(LINE.radar), `An escort holds ${S.side === 'att' ? 'your' : 'a'} boat by radar`], [line(LINE.asdic, '2 3'), '… by sonar'], [line(LINE.sight), '… by sight'],
    [line('rgba(224,184,114,0.8)', '5 4'), 'Where an order sends a ship'],
    [tri(boat, true), `${boats} on the surface`], [tri(boat, false), `${boats} at periscope depth, diving or coming up`], [ring(boat), `${boats} deep`],
    [seen(S.side === 'att' ? '#8fb0c8' : boat), S.side === 'att' ? 'Where an escort was last seen' : 'Where a boat was last seen: right-click it to hunt there'],
    [tri('#8fb0c8', true), 'An escort'], ['<svg width="28" height="14" aria-hidden="true"><rect x="12" y="2" width="4" height="10" fill="#b7ad98"/></svg>', 'A merchant ship'],
    ['<svg width="28" height="14" aria-hidden="true"><path d="M10 3 L18 11 M18 3 L10 11" stroke="rgba(224,110,90,0.9)" stroke-width="2"/></svg>', 'A wreck'],
  ];
  el.innerHTML = `<div class="tc-label">Key to the map · ${cap('K')} hides it</div>${rows.map(([i, w]) => `<div class="krow">${i}<span>${w}</span></div>`).join('')}`;
}

// ---- The bar ----------------------------------------------------------------------------------------------------------

function paintClock(): void {
  app.querySelectorAll<HTMLElement>('[data-speed]').forEach((b) => {
    const v = SPEEDS[+b.dataset.speed!];
    b.classList.toggle('tc-btn--primary', v === 0 ? S.paused : !S.paused && v === S.speed);
    // (the player's own speed, lamp-lit while a slow-down holds the clock below it: the clock goes back to it)
    b.classList.toggle('is-lit', v !== 0 && v === S.chosen && (S.paused || S.speed !== S.chosen));
  });
  const note = app.querySelector<HTMLElement>('[data-clocknote]');
  if (note) {
    const watching = !!S.sim?.ai[S.side];
    // (two lines at most: the box beside the speeds is one size, so the bar never moves as the note changes)
    // (of the slow-downs holding it, the one that may last longest: one that follows a boat says the latest it gives the
    // clock back, and that an order puts that off; one for a moment of the night, for how long)
    const end = (w: Slow): number => w.on !== undefined ? w.most : w.until;
    // (paused, the same: the clock runs on at it)
    const v = Math.min(S.chosen, ...S.slows.map((o) => o.to)), held = S.slows.filter((o) => o.to === v).sort((p, q) => end(q) - end(p))[0];
    const w = watching ? 'The staff are steering: the clock keeps your speed.'
      : v < S.chosen && held ? (held.on !== undefined ? `Slowed to ×${v} for ${nameOf(held.on)}, until ${hhmm(held.most)} at most; orders add time.` : `Slowed to ×${v} ${held.how.toLowerCase()}, then back to ×${S.chosen}.`)
      // (running at his speed: said, since a banner of news may cover 'BACK TO ×300' (the UI director, round 5, 4))
      : S.chosen <= 10 ? '' : `At ×${S.chosen}. ${S.rush ? 'Slows to ×10 at the first contact.'
      : S.side === 'def' ? 'Slows for new contacts after a quiet spell, your hunts and hits.' : 'Slows when your boats fire, or are found, attacked or lost.'}`;
    if (note.textContent !== w) note.textContent = w;
  }
}

let barWas = '';
function paintBar(): void {
  const s = S.sim!, t = S.t!, L = t.place.light;
  const sunk = s.counts.mv, boats = S.pack || s.packSize;
  const ships = S.units.filter((u) => u.kind === 'mv').length || t.place.ships;
  const lostB = Object.values(s.counts.ss).reduce((a, b) => a + b, 0), lostE = Object.values(s.counts.dd).reduce((a, b) => a + b, 0);
  const facts = `${esc(t.place.seaName)} · ${esc(MONTHS[t.place.when.month])} ${t.place.when.year} · dusk +${hhmm(s.t)} · dawn in ${hhmm(Math.max(0, s.limit - s.t))} · moon ${Math.round(L.moon.lit * 100)}%${L.moon.up > 0 ? '' : ' (down)'} · a boat on the surface seen at ${L.vis.toLocaleString('en-GB')} m · you are the ${S.side === 'def' ? `${PEOPLE[t.def.nat] ?? t.def.nat} escorts` : `${PEOPLE[t.att.nat] ?? t.att.nat} pack`}`;
  const score = `<span class="nw">Merchants sunk <b>${sunk}</b> of ${ships}</span> <span class="nw">Boats lost <b>${lostB}</b>${S.side === 'att' ? ` of ${boats}` : ''}</span> <span class="nw">Escorts lost <b>${lostE}</b></span>`;
  const log = S.said.slice(-LOG_LINES).map((m) => `<div class="${m.tone ? `is-${m.tone}` : ''}"><span class="tc-faint">${hhmm(m.t)}</span> ${esc(m.text)}</div>`).join('');
  const f = S.cam.follow;
  const cam = f === 'convoy' ? 'Camera on the convoy' : typeof f === 'number' ? `Camera following ${esc(nameOf(f))}` : 'Camera free';
  const now = facts + score + log + cam;
  if (now === barWas) return;
  barWas = now;
  // (A campaign's night, with Units decide and the route's count laid: the month's convoys on the route, the card's sighted line, and the pack.)
  const k = t.place.card?.count;
  // (The boats that fire a convoy where the escort's radar before a boat fires is laid, so the line agrees with the torpedoes.)
  const by = k ? k.fired ?? k.pack : 0;
  // (Release 2, once the sea patrol is live: what the player's side last saw, with its month, in his panel's words: the
  // boats last seen on the route's seas, and where he holds boats the aircraft they last met.)
  const seen = (t.place.card?.sight ?? []).map((w) => ` · <span class="nw">${esc(w)}</span>`).join('');
  app.querySelector('[data-facts]')!.innerHTML = (k ? `${facts} · <span class="nw">of about ${countWords(Math.round(k.sailed))} convoys this month, ${countWords(Math.round(k.sighted))} sighted, ${countWords(Math.round(k.attacked))} attacked${by > 0 ? `, by about ${by < 10 ? by.toFixed(1) : Math.round(by)} boats each` : ''}${k.watched !== undefined ? `, aircraft watching about ${Math.round(k.watched * 100)}% of its waters` : ''}${k.sunk !== undefined ? `, losing about ${k.sunk < 10 ? k.sunk.toFixed(1) : Math.round(k.sunk)} ships each${k.torps !== undefined && k.sunk >= k.torps - 0.01 ? ', as many as their torpedoes sink on an average night' : ''}` : ''}</span>` : facts) + seen;
  app.querySelector('[data-score]')!.innerHTML = score;
  app.querySelector('[data-log]')!.innerHTML = log;
  app.querySelector('[data-cam]')!.innerHTML = cam;
  paintClock();
}

// ---- Keys -------------------------------------------------------------------------------------------------------------

function keys(e: KeyboardEvent): void {
  if (S.screen !== 'night') return;
  if (S.handing) { e.preventDefault(); return; }
  if (S.asking) {
    // (the question keeps the keys: Esc keeps command; Enter and Space press the answer that has the focus, Keep command
    // unless the player moved it; Tab goes between the two answers and nowhere else; no other key does anything)
    const yes = app.querySelector<HTMLElement>('[data-askyes]')!, no = app.querySelector<HTMLElement>('[data-askno]')!;
    const onAnswer = document.activeElement === yes || document.activeElement === no;
    if (e.key === 'Escape') { e.preventDefault(); keepCommand(); }
    else if (e.key === 'Tab') { e.preventDefault(); (document.activeElement === no ? yes : no).focus(); }
    else if (!onAnswer || (e.key !== 'Enter' && e.key !== ' ')) { e.preventDefault(); if (!onAnswer) no.focus(); }
    return;
  }
  if (e.key === ' ') { e.preventDefault(); togglePause(); }
  else if (e.key >= '1' && e.key <= '4') setSpeed(SPEEDS[+e.key]);
  else if (e.key === 'f' || e.key === 'F') S.cam.follow = 'convoy';
  // (with the column folded the key is out of sight: K unfolds it and shows the key, never turns it off unseen)
  else if (e.key === 'k' || e.key === 'K') { if (!S.sideOpen) { S.sideOpen = true; S.keyOn = true; } else S.keyOn = !S.keyOn; paintKey(); paintOrders(true); }
  else if (e.key === 'Escape') { S.mode = null; S.picked.clear(); paintOrders(); }
  else if (e.key === 'Tab') {
    e.preventDefault();
    const us = own();
    if (!us.length) return;
    const i = us.findIndex((u) => S.picked.has(u.id));
    const u = us[(i + 1) % us.length];
    S.picked = new Set([u.id]); S.cam.follow = u.id; S.cam.fit = false;
    paintOrders();
  } else if ((e.key === 's' || e.key === 'S') && S.side === 'def' && picked().length) S.mode = 'starshell';
  else if ((e.key === 'g' || e.key === 'G') && S.side === 'att' && picked().length) S.mode = 'gun';
  else if (e.key.startsWith('Arrow')) {
    e.preventDefault();
    S.cam.follow = null; S.cam.fit = false;
    const d = 120 / S.cam.scale;
    if (e.key === 'ArrowLeft') S.cam.x -= d; if (e.key === 'ArrowRight') S.cam.x += d; if (e.key === 'ArrowUp') S.cam.y += d; if (e.key === 'ArrowDown') S.cam.y -= d;
  }
}

export function bootConvoyPage(): void {
  app = document.getElementById('app')!;
  document.addEventListener('keydown', keys);
  // (a press that began on the sea and ends off it, on the panel or the bar, picks and orders nothing)
  window.addEventListener('mouseup', (e) => { if (e.target !== canvas) { S.drag = null; rightDown = false; } });
  const open = (window as unknown as { CONVOY_OPEN?: { practice?: number; side?: Side; seed?: number } }).CONVOY_OPEN;
  if (open?.practice !== undefined) { S.card = Math.max(0, Math.min(CONVOY_PRACTICE.length - 1, open.practice)); S.side = open.side ?? 'def'; begin(open.seed ?? 1); }
  else choose();
}

(window as unknown as { convoyState?: State }).convoyState = S;
bootConvoyPage();

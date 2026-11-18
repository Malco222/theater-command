// FIRST COMMAND in the browser game: the staff's note, its slip beside a card, the pointer, the DONE stamp, a tip,
// the Guide's list of the ten lessons, and the door's button. The lessons, their words and the tests of their deeds
// are the shared rules' (../lessons, data/campaign/lessons.json); the look is the art director's
// (art-director/first-hour/first-hour-look-brief.md) in the UI kit's parts. Display only: a note never moves a piece,
// takes a key or locks the screen. Deeds are read off the game as the page is drawn (a battle fought, a raid flown,
// an order placed, a corps moved, the month turned), or reported by the page where only it can see them (the month
// card put down, a line of it followed).
import {
  BOMBING, CLOSING, FIRST_COMMAND, LESSONS, LESSON_DOOR, LESSON_GROUPS, LESSON_SETTING, TIPS, allDone, bringBack, closingOwed, currentLesson,
  doneCount, firstCommand, isDone, lessonDeed, lessonWords, lessonsAgain, lessonsOn, notesWanted, pickupPlace, putAway, readNotebook, showAgain, siegeOf,
  trainGround, waitingCount, type Deed, type Lesson, type Moment, type Notebook,
} from '../lessons';
import { markFor, monthCard } from '../monthcard';
import { railReach } from '../game';
import { cutThroughGuns } from '../rails';
import { humanUp, onEve, reach, stormChoice } from '../turn';
import { planRaid, type RaidKind } from '../airwar';
import type { Corps, Game } from '../types';

/** What the page lends the notes. */
export interface Page {
  game(): Game;
  ui: { sel: number; sea: number; raid: string; lead: number; picked: Set<number>; tab: string; modal: null | { kind: string; [k: string]: any }; mark: any; lastCard: any; live: boolean; menu: string };
  /** The corps the player has in hand. */
  held(): Corps[];
  /** True while a desk's tab is tucked away because the staff runs it. */
  tucked(tab: string): boolean;
  /** True while the other nations move, or anything else keeps the player's hands off the map. */
  busy(): boolean;
  openModal(m: { kind: string; [k: string]: any } | null): void;
  refresh(): void;
  /** Brings these provinces into view, as SHOW ON MAP does. */
  look(ids: number[]): void;
  esc(s: string): string;
}

const STORE = 'tc-lessons';
const STAMP_MS = 2000;
let page: Page;
let book: Notebook = readNotebook(null);
/** The note just stamped DONE: it stays until `until`, or until a click sends it. */
let stamped: { id: string; until: number } | null = null;
/** The tip on screen, and the month it came in: it goes when he acts or the month ends. */
let tip: { id: string; turn: number; acts: number } | null = null;
/** The note last drawn, so that a new one slides in and an old one does not. */
let lastDrawn = '';
/** Lesson 1 lies on the month card that is open. */
let laid = false;
/** What the game looked like at the last drawing, to read the player's deeds against. */
let snap: Snap | null = null;
interface Snap {
  g: Game; turn: number; up: boolean; reports: number; air: number; queue: Set<number>;
  /** The corps in hand, where each stood, and where each could march this month. */
  hand: { id: number; reg: number; reach: Set<number>; rail: Set<number> }[];
  kept: Set<string>; cuts: number;
  /** The month he last had open (-1 for none yet), and whether a siege of his stood then. */
  upTurn: number; siegeUp: boolean;
}

function save(): void {
  try { localStorage.setItem(STORE, JSON.stringify(book)); } catch { /* the notebook lasts as long as the page */ }
}
function load(): void {
  try { book = readNotebook(JSON.parse(localStorage.getItem(STORE) ?? 'null')); } catch { book = readNotebook(null); }
}

const $ = (s: string): HTMLElement | null => document.querySelector(s) as HTMLElement | null;
const cardOpen = (): boolean => !!page.ui.modal && document.getElementById('veil')?.classList.contains('open') === true;
const monthCardOpen = (): boolean => cardOpen() && page.ui.modal?.kind === 'news' && !!page.ui.modal.card;
const moment = (): Moment => ({ card: monthCardOpen(), inHand: page.ui.tab === 'region' ? page.held().map((c) => c.id) : [] });
/** The notes stand aside on the front door, while the other nations move, and when nobody is at the controls. */
const quiet = (): boolean => {
  const g = page.game();
  return !page.ui.live || !g.human || page.busy() || page.ui.modal?.kind === 'start' || page.ui.modal?.kind === 'replace' || page.ui.modal?.kind === 'guide';
};

export function initFirstHour(p: Page): void {
  page = p;
  load();
  // (Capture: the notes' own buttons are theirs alone, and a line of the month card is read before the page acts on it.)
  document.addEventListener('click', onClick, true);
  addEventListener('resize', () => redraw());
  document.addEventListener('scroll', (e) => { if ((e.target as Element | null)?.id === 'pane') awayCheck = true; }, true);
  requestAnimationFrame(follow);
}

/** The notebook, for the Guide and for checks. */
export const notebook = (): Notebook => book;

// ---- Deeds -----------------------------------------------------------------------------------------------------

/** A deed the page saw: stamped on its lesson, if it earns one. */
export function lessonDone(does: Deed, m: Moment = moment()): void {
  const g = page.game();
  // (The note on the desk is the one last drawn: by the time a deed is read off the game its moment may have passed,
  // as a march's has once the army is put down.)
  const was = lastDrawn || (currentLesson(g, book, m)?.id ?? '');
  const got = lessonDeed(g, book, does, m);
  if (!got.length) return;
  save();
  // The note on screen takes the stamp; a lesson done while it was put away is ticked in the Guide and says nothing.
  const shown = got.find((l) => l.id === was) ?? (got[0].id === 'month' ? got[0] : null);
  if (shown && shown.id !== 'month') {
    stamped = { id: shown.id, until: performance.now() + STAMP_MS };
    setTimeout(() => { if (stamped && performance.now() >= stamped.until - 5) { stamped = null; redraw(); } }, STAMP_MS + 20);
  }
}

/** Reads the deeds off the game since the last drawing. Called as the page is drawn. */
function readDeeds(): void {
  const g = page.game();
  const me = g.human;
  const s = snap;
  const up = !!me && humanUp(g) && !onEve(g);
  if (s && s.g === g && me && lessonsOn(g, book)) {
    if (s.upTurn >= 0 && g.turn > s.upTurn) {
      // The month turned since his own was last open: a siege standing as he handed it over earns lesson 7; then
      // the month's end. (While the other nations move the page is drawn many times with nobody at the controls.)
      if (s.siegeUp) lessonDone('siege', { ended: s.upTurn });
      lessonDone('end', { ended: s.upTurn });
      if (tip && tip.turn < g.turn) tip = null;
    } else if (up && g.turn === s.turn) {
      const fought = g.reports.slice(s.reports).filter((r) => r.attNat === me);
      if (fought.some((r) => r.town)) lessonDone('siege', {});
      if (fought.length) lessonDone('attack', {});
      if (g.airReports.slice(s.air).some((r) => r.attNat === me && BOMBING.includes(r.mission))) lessonDone('raid', {});
      if ((g.nations[me]?.queue ?? []).some((b) => !s.queue.has(b.id))) lessonDone('order', {});
      // A corps that was in hand has moved: by a kept march, or to ground it could march to, a march; to ground
      // only a train reaches, by rail.
      const kept = Object.keys(g.flags).filter((k) => k.startsWith('march:') && !s.kept.has(k));
      const keptIds = new Set(kept.map((k) => Number(k.slice(6))));
      let march = kept.length > 0, rail = false, moved = false;
      for (const h of s.hand) {
        const c = g.corps.find((x) => x.id === h.id);
        if (!c || c.dead || c.reg === h.reg) continue;
        moved = true;
        if (!keptIds.has(c.id) && !h.reach.has(c.reg) && h.rail.has(c.reg)) rail = true; else march = true;
      }
      // (Lesson 3's words send an army "farther off", which at home is often a train: while it is the lesson to do,
      // the ride is its march, and lesson 8 waits to be taught.)
      if (rail && !isDone(book, 'march')) { rail = false; march = true; }
      // (Any order he gives puts away a tip that came before it.)
      if (tip && (fought.length || moved || kept.length || g.airReports.length > s.air || (g.nations[me]?.queue.length ?? 0) !== s.queue.size)) tip.acts++;
      if (!fought.length) {
        if (kept.length) tipDue('kept');
        if (march) lessonDone('march', {});
        if (rail) lessonDone('rail', {});
      }
    }
    // A line of his cut for the first time, found in his own month: a cut made while the others move is counted
    // against the lines as he last saw them, so that its tip comes when he can read it.
    if (up && cutsOf(g) > s.cuts) tipDue('cut');
  }
  if (up && page.ui.tab === 'region' && page.held().length && lessonsOn(g, book)) lessonDone('pick');
  const hand = up && page.ui.tab === 'region' ? page.held().map((c) => ({ id: c.id, reg: c.reg, reach: new Set([...reach(g, c)].filter(([id, x]) => id !== c.reg && !x.attack).map(([id]) => id)), rail: railReach(g, c) })) : [];
  snap = {
    g, turn: g.turn, up, reports: g.reports.length, air: g.airReports.length, queue: new Set((me ? g.nations[me]?.queue ?? [] : []).map((b) => b.id)),
    hand, kept: new Set(Object.keys(g.flags).filter((k) => k.startsWith('march:'))), cuts: up || !s || s.g !== g ? cutsOf(g) : s.cuts,
    // (His own month as it last stood open, carried through the other nations' moves.)
    upTurn: up ? g.turn : s && s.g === g && g.turn >= s.upTurn ? s.upTurn : -1,
    siegeUp: up ? siegeOf(g) !== null : s && s.g === g ? s.siegeUp : false,
  };
}
/**
 * The staff's own work, done at once as he hands it a desk in the middle of his month (orderDesk, orderDepth): the
 * raids it flies and the orders it places are taken into the picture as they stand, never read as his deeds.
 */
export function lessonsAside(): void {
  const g = page?.game();
  if (!snap || !g || snap.g !== g) return;
  const me = g.human;
  snap.reports = g.reports.length;
  snap.air = g.airReports.length;
  snap.queue = new Set((me ? g.nations[me]?.queue ?? [] : []).map((b) => b.id));
  snap.kept = new Set(Object.keys(g.flags).filter((k) => k.startsWith('march:')));
}

/** How many of the player's provinces have their railway cut by bombers where no guns stood: the moment of the tip that says so. */
const cutsOf = (g: Game): number => (g.human ? g.regions.filter((r) => r.ctrl === g.human && cutThroughGuns(g, r.id) === 0).length : 0);

/** A tip is shown once, the first time its moment comes, and never while a lesson waits on the same thing. */
function tipDue(id: string): void {
  const g = page.game();
  if (book.tips.includes(id) || !lessonsOn(g, book) || !TIPS.some((t) => t.id === id)) return;
  if (id === 'kept' && !isDone(book, 'march')) return;
  book.tips.push(id);
  save();
  // (The order that brought it is already counted: the next one puts it away.)
  tip = { id, turn: g.turn, acts: 0 };
}

// ---- The month card --------------------------------------------------------------------------------------------

/** A campaign has begun from the door. FIRST COMMAND marks its campaign; a war from its first month lays out the first card for lesson 1. */
export function lessonsBegun(first: boolean): void {
  const g = page.game();
  if (first) {
    g.flags.firstCommand = 1;
    // (A player who had switched the notes off and comes back through FIRST COMMAND has them in its campaign again,
    // and in no other until he says so. Read before the lessons are set again, which switches them on.)
    const off = !book.on;
    if (allDone(book)) lessonsAgain(book);
    if (off) book.set = false;
    book.on = true;
    save();
  }
  stamped = null; tip = null; snap = null; laid = false; lastDrawn = '';
  // (The game as it stands now is the picture deeds are read against: a month ended at once after CONTINUE is then
  // seen to turn, though the next drawing comes while the others move.)
  readDeeds();
  // (A war played from its first month has no card until its second: lesson 1 needs one, so the first card is
  // opened as his first month comes up, headed "As September 1939 opens", as a campaign taken over later opens with its own.)
  opening = g.turn === 1 && lessonsOn(g, book) && !isDone(book, 'month');
  redraw();
  if (opening) openFirstCard();
}
/** The first month's card is owed to lesson 1. */
let opening = false;
function openFirstCard(): void {
  const g = page.game();
  if (!opening || !lessonsOn(g, book) || isDone(book, 'month') || g.turn !== 1) { opening = false; return; }
  if (quiet() || page.ui.modal || !g.human || !humanUp(g) || onEve(g)) return;
  opening = false;
  setTimeout(() => {
    if (quiet() || page.ui.modal || !g.human || !humanUp(g) || page.game() !== g) { opening = true; return; }
    const card = monthCard(g, page.ui.mark ?? markFor(g), g.human, { opening: true });
    page.ui.lastCard = card;
    page.openModal({ kind: 'news', from: g.headlines.length, reports: g.reports.length, sea: g.seaReports.length, air: g.airReports.length, card });
  }, 0);
}

/** Called as a card is about to change: lesson 1 is done when the card it lay on is put down. */
export function cardChanging(was: Page['ui']['modal'], next: Page['ui']['modal']): void {
  // (A card put down: the slip goes back to the panel once the page has done with it.)
  if (was && !next) setTimeout(() => { if (!page.ui.modal) redraw(); }, 0);
  if (!laid || !was || was.kind !== 'news' || !was.card || (next && next.kind === 'news' && next.card === was.card)) return;
  laid = false;
  lessonDone('card', { card: true });
}

/** Called once a card is drawn: the door's button, lesson 1 laid on the first month's card, a slip in the margin. */
export function cardDrawn(card: HTMLElement, m: NonNullable<Page['ui']['modal']>): void {
  if (m.kind === 'start') { doorButton(card); laid = false; redraw(); return; }
  const g = page.game();
  laid = false;
  if (m.kind === 'news' && m.card && !quiet()) {
    const l = currentLesson(g, book, { card: true });
    if (l?.id === 'month') {
      const w = lessonWords(g, l, 'browser');
      card.querySelector('.mc-head')?.insertAdjacentHTML('afterend', `<div class="tc-note tc-paper tc-note--laid fh-laid" data-lesson="month"><div class="tc-note__head"><span class="tc-label">Staff note</span><span class="tc-micro">${l.n} of ${LESSONS.length}</span></div><h4>${page.esc(w.head)}</h4><p>${page.esc(w.text)}</p></div>`);
      laid = true;
    }
  }
  redraw();
}

// ---- Drawing ---------------------------------------------------------------------------------------------------

/** After the page is drawn: the deeds since the last drawing, the GUIDE button, the note and the pointer. */
export function lessonsDrawn(): void {
  if (!page) return;
  readDeeds();
  guideButton();
  redraw();
  if (opening) openFirstCard();
}

/** What stands on the desk now: a stamped note, a tip, the lesson due, or the closing note. */
function onDesk(): { kind: 'lesson' | 'tip' | 'closing'; lesson?: Lesson; id: string } | null {
  const g = page.game();
  if (quiet() || !lessonsOn(g, book) || opening) return null;
  if (stamped) { const l = LESSONS.find((x) => x.id === stamped!.id); if (l) return { kind: 'lesson', lesson: l, id: l.id }; }
  const l = currentLesson(g, book, moment());
  if (!(tip && tip.acts < 1 && tip.turn === g.turn)) tip = null;
  // (A lesson that comes only with the month card goes before a tip, which waits until the card is put down.)
  else if (!(l?.due === 'card' && monthCardOpen())) return { kind: 'tip', id: tip.id };
  // (Lesson 1 lies on the card itself and nowhere else.)
  if (l && l.id !== 'month') return { kind: 'lesson', lesson: l, id: l.id };
  if (!l && closingOwed(g, book)) return { kind: 'closing', id: CLOSING.id };
  return null;
}

const TICK = '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M3 8.4l3.2 3.1L13 4.6"/></svg>';

function noteHtml(d: NonNullable<ReturnType<typeof onDesk>>, slip: boolean): string {
  const g = page.game(), esc = page.esc;
  const cls = `tc-note tc-paper fh-note${slip ? ' tc-note--slip' : ''}`;
  if (d.kind === 'tip') {
    const t = TIPS.find((x) => x.id === d.id)!;
    return `<div class="${cls} tc-note--tip" data-note="${d.id}"><div class="tc-note__head"><span class="fh-tiptag">Tip</span><span class="tc-micro"></span><button class="tc-btn--quiet fh-quiet" data-act="fhtipoff" aria-label="Put the tip away">Close</button></div><p>${esc(t.text)}</p></div>`;
  }
  if (d.kind === 'closing') {
    return `<div class="${cls}" data-note="${CLOSING.id}"><div class="tc-note__head"><span class="tc-label">Staff note</span><span class="tc-micro"></span><button class="tc-btn--quiet fh-quiet" data-act="fhclose">Close</button></div><h4>${esc(CLOSING.head)}</h4><p>${esc(closingWords())}</p></div>`;
  }
  const l = d.lesson!;
  // (Lesson 10 moves on once the BUILD page is open: its note then asks for the artillery corps.)
  const w = lessonWords(g, l, 'browser', page.tucked('build') ? ' under STAFF' : '', l.id === 'build' && page.ui.tab === 'build');
  const done = stamped?.id === l.id;
  return `<div class="${cls}${done ? ' is-done' : ''}" data-note="${l.id}"${done ? ' data-act="fhsend" title="Click to put it away now"' : ''}><div class="tc-note__head"><span class="tc-label">Staff note</span><span class="tc-micro">${l.n} of ${LESSONS.length}</span>${done ? '' : '<button class="tc-btn--quiet fh-quiet" data-act="fhlater" title="Put this note away. It waits in the GUIDE.">Later</button>'}</div><h4>${esc(w.head)}</h4><p>${esc(w.text)}</p>${w.deed ? `<div class="tc-deed"><i>${done ? TICK : ''}</i><span>${esc(w.deed)}</span></div>` : ''}${done ? '<span class="tc-stamp tc-stamp--won tc-stamp--sm fh-stamp">Done</span>' : ''}</div>`;
}
const closingWords = (): string => lessonWords(page.game(), { ...LESSONS[0], text: CLOSING.text, deed: '' }, 'browser').text;

/**
 * Draws the note in the panel, or as the slip beside an open card. Where it stands and what it points at are measured
 * on the next frame, never here: a measure in the middle of the page's redraw makes the browser lay the whole map out
 * again then and there (ui-v82l, "one redraw of the page").
 */
function redraw(): void {
  if (!page) return;
  document.querySelectorAll('.fh-note').forEach((e) => e.remove());
  const d = onDesk();
  const slipOn = cardOpen();
  if (d && !slipOn) {
    const pane = $('#pane');
    if (pane) {
      pane.insertAdjacentHTML('afterbegin', noteHtml(d, false));
      arrive(pane.firstElementChild as HTMLElement, d.id);
    }
  } else if (d && slipOn) {
    const veil = document.getElementById('veil');
    if (veil) {
      veil.insertAdjacentHTML('beforeend', noteHtml(d, true));
      const el = veil.lastElementChild as HTMLElement;
      el.classList.add('fh-slip');
      arrive(el, d.id);
    }
  }
  if (!d) lastDrawn = '';
  due = { d };
}
/** The redraw's measuring, waiting for the next frame. */
let due: { d: ReturnType<typeof onDesk> } | null = null;
let toTop = false;
/** A note slides in the first time it is drawn, and stays still after. */
function arrive(el: HTMLElement, id: string): void {
  if (lastDrawn === id) return;
  el.classList.add('fh-in');
  lastDrawn = id;
  // (A new note in a panel scrolled down: the panel goes back to its top, where the note is, on the next frame.)
  if (el.parentElement?.id === 'pane') toTop = true;
}
/**
 * The slip stands in the card's left margin, never on the card. Where the margin is too narrow for it (the month
 * card on any screen under about 1,550 px), the note is laid in the card instead, under its heading, as lesson 1 is:
 * a strip across a wide card, the note itself in a narrow one. It is never hidden.
 */
function placeSlip(): void {
  const el = document.querySelector<HTMLElement>('#veil > .fh-slip');
  const card = document.getElementById('card');
  if (!el || !card) return;
  const r = card.getBoundingClientRect();
  const room = r.left - 14;
  if (room >= 112) {
    el.style.width = `${Math.min(148, room)}px`;
    el.style.top = `${Math.max(14, r.top)}px`;
    return;
  }
  el.classList.remove('fh-slip', 'tc-note--slip');
  el.classList.add('fh-inlaid', r.width >= 900 && !el.classList.contains('tc-note--tip') ? 'tc-note--laid' : 'tc-note--inlaid');
  el.removeAttribute('style');
  const head = card.querySelector('.fh-laid') ?? card.querySelector('.mc-head');
  if (head) head.insertAdjacentElement('afterend', el);
  else (card.querySelector(':scope > .body') ?? card).prepend(el);
}

// ---- The pointer -----------------------------------------------------------------------------------------------

/** Where the pointer stands: a thing on the page (by its element) or on the map (by its box on the screen). */
let aimAt: { el: Element; map: boolean; spot?: DOMPoint } | null = null;

/**
 * A point inside a province's lit ground that shows on the screen and takes a click: its middle where it can, else
 * the nearest point to it that is not hidden. Returned on the screen and in the map's own measure.
 */
const CORNERS: [number, number][] = [[-24, -24], [24, -24], [-24, 24], [24, 24]];
function openSpot(path: SVGGeometryElement): { x: number; y: number; user: DOMPoint } | null {
  const r = path.getBoundingClientRect();
  const ctm = path.getScreenCTM();
  if (!r.width || !ctm) return null;
  const inv = ctm.inverse();
  const cx = r.left + r.width / 2, cy = r.top + r.height / 2;
  const tries: [number, number][] = [];
  for (let j = 1; j < 8; j++) for (let i = 1; i < 8; i++) tries.push([r.left + (r.width * i) / 8, r.top + (r.height * j) / 8]);
  tries.sort((a, b) => Math.hypot(a[0] - cx, a[1] - cy) - Math.hypot(b[0] - cx, b[1] - cy));
  for (const [x, y] of tries) {
    if (x < 0 || y < 0 || x > innerWidth || y > innerHeight) continue;
    const user = new DOMPoint(x, y).matrixTransform(inv);
    if (!path.isPointInFill(user)) continue;
    if (!document.elementFromPoint(x, y)?.closest('#map')) continue;
    // (The clamps stand 22 px round the spot: each corner must be on the map too, not under the zoom buttons, the
    // compass, a tray or the legend, and not off the screen's edge.)
    if (!CORNERS.every(([dx, dy]) => document.elementFromPoint(x + dx, y + dy)?.closest('#map'))) continue;
    return { x, y, user };
  }
  return null;
}
let lastBox = '';

/** The one thing the note points at, for the lesson on the desk and where the player stands now. */
function target(d: ReturnType<typeof onDesk>): typeof aimAt {
  const q = (s: string): Element | null => document.querySelector(s);
  const vis = (e: Element | null): Element | null => (e && (e as HTMLElement).getClientRects().length ? e : null);
  const g = page.game();
  if (laid && monthCardOpen()) return pick(q('#card .mc-fly'));
  if (!d || d.kind !== 'lesson' || stamped) return null;
  const id = d.lesson!.id, open = cardOpen(), kind = page.ui.modal?.kind;
  // (Of the lit ground, the province nearest the army in hand, at a point of it he can see and click: not off the
  // map's edge, nor under the army's card or a tray.)
  const ground = (cls: string): typeof aimAt => {
    const from = document.querySelector(`#units .army.chosen[data-nat="${g.human}"]`)?.getBoundingClientRect();
    const at: [number, number] | null = from ? [from.left + from.width / 2, from.top + from.height / 2] : null;
    // (Nearest first by its box, and the first with an open spot is taken: a spot is sought by asking the page what
    // stands at each point, and asking it of every lit province each time the page is drawn costs a frame.)
    const near = [...document.querySelectorAll<SVGGeometryElement>(`#marks path.mk.${cls}`)].map((e) => {
      const r = e.getBoundingClientRect();
      return { e, d: at ? Math.hypot(r.left + r.width / 2 - at[0], r.top + r.height / 2 - at[1]) : 0 };
    }).sort((a, b) => a.d - b.d);
    for (const { e } of near) {
      const spot = openSpot(e);
      if (spot) return { el: e, map: true, spot: spot.user };
    }
    return null;
  };
  const flag = (reg: number): typeof aimAt => { const e = vis(q(`#units .army[data-reg="${reg}"][data-nat="${g.human}"]`)); return e ? { el: e, map: true } : null; };
  const tab = (t: string): Element | null => vis(q(`#tabs [data-tab="${t}"]`));
  if (open) {
    if (id === 'card' && kind === 'news') return pick(q('#card .mc-go:not(.none)')) ?? pick(q('#card .mc-banner .mc-alt'));
    if (id === 'attack' && kind === 'attack') return pick(q('#card footer [data-act="attack"]'));
    // (The order the card lights by the staff's verdict: the storm, or keeping up the siege. Either earns the lesson.)
    if (id === 'city' && kind === 'siege') return pick(q('#card footer .btn.go:is([data-act="dosiege"], [data-act="close"], [data-act="dotake"], [data-act="domarch"])'));
    if (id === 'raid' && kind === 'raid' && BOMBING.includes(page.ui.modal?.raid)) return pick(q('#card footer .btn.go[data-act="doraid"], #card footer [data-act="doraid"]'));
    return null;
  }
  const inHand = page.ui.tab === 'region' && page.held().length > 0;
  switch (id) {
    case 'pickup': return flag(pickupPlace(g));
    case 'march': return ground('move');
    case 'attack': {
      if (inHand) { const e = ground('hit'); if (e) return e; }
      const me = g.human!;
      const at = page.held()[0]?.reg ?? -1;
      const beside = g.regions.find((r) => r.units.some((c) => c.nat === me && c.ap > 0 && !c.dead) && r.nb.some((k) => g.regions[k].units.some((u) => g.nations[me].atWar.includes(u.nat))));
      return flag(beside?.id ?? at);
    }
    // (END MONTH, the deed: the art director's picture of the browser game. The note names NEXT ARMY on its way.)
    case 'end': return pick(q('#barbtns [data-act="end"]'));
    case 'city': {
      const s = siegeOf(g);
      // (A storm the staff would not order is not pointed at: holding the siege a month is the deed then, at END MONTH.)
      if (s && !stormFavoured(g, s.region)) return pick(q('#barbtns [data-act="end"]'));
      const storm = vis(q('#pane [data-act="storm"]'));
      return storm ? pick(storm) : s ? flag(s.region) : null;
    }
    case 'rail': {
      const at = ground('railfar');
      if (!at) lookForRail();
      return at;
    }
    case 'raid': {
      if (page.tucked('air')) return page.ui.tab === 'staff' ? pick(q('#pane [data-act="desk"][data-desk="air"][data-on="0"]')) : pick(tab('staff'));
      if (page.ui.tab !== 'air') return pick(tab('air'));
      // The planner: PLAN A RAID, then the bombing raid, then a target. (Another kind open, Fight for the sky: back
      // to the bombing raid, since only a bombing raid earns the lesson.)
      const open = (q('#pane .planner [data-act="raidkind"].on') as HTMLElement | null)?.dataset.kind ?? '';
      return pick((BOMBING.includes(open) ? bestRaid(open) : null) ?? vis(q('#pane .planner [data-act="raidkind"][data-kind="bomb"]:not([disabled]):not(.on)')) ?? vis(q('#pane .btn.go[data-act="raidmenu"][data-id="-1"]:not([disabled])')));
    }
    case 'build': {
      if (page.ui.tab === 'build') return pick(vis(q('#pane button[data-act="arsenal"]:not([disabled])')));
      if (page.tucked('build')) return page.ui.tab === 'staff' ? pick(q('#pane .desk [data-act="tab"][data-tab="build"]')) : pick(tab('staff'));
      return pick(tab('build'));
    }
  }
  return null;
}
/**
 * Lesson 8 with no lit railway in view (off the screen's edge, or only under the map's own buttons): the map is
 * brought round to the army and the nearest ground a train takes it to, once for each army picked up.
 */
let lookedFor = '';
function lookForRail(): void {
  const g = page.game();
  const held = page.held();
  const key = `${g.turn}:${held.map((c) => c.id).join(',')}`;
  if (!held.length || key === lookedFor) return;
  lookedFor = key;
  const from = g.regions[held[0].reg];
  const to = [...trainGround(g, held)].sort((a, b) => Math.hypot(g.regions[a].x - from.x, g.regions[a].y - from.y) - Math.hypot(g.regions[b].x - from.x, g.regions[b].y - from.y))[0];
  if (to === undefined) return;
  page.look([from.id, to]);
  setTimeout(() => redraw(), 450);
}
/** Whether the staff would storm the town now (the card's lit order), asked once a month and place. */
let stormMemo: { g: Game; key: string; yes: boolean } | null = null;
function stormFavoured(g: Game, region: number): boolean {
  const key = `${g.turn}:${region}:${g.reports.length}`;
  if (stormMemo?.g !== g || stormMemo.key !== key) stormMemo = { g, key, yes: stormChoice(g, region)?.favours === 'storm' };
  return stormMemo.yes;
}
/** The planner's row with the best chance of success for the kind of raid open, as the raid card would give it. */
let raidMemo: { g: Game; key: string; id: string } | null = null;
function bestRaid(kind: string): Element | null {
  const g = page.game();
  const rows = [...document.querySelectorAll<HTMLElement>('#pane .planner [data-act="raidtarget"]')].filter((e) => e.getClientRects().length);
  if (!rows.length) return null;
  const lead = page.ui.lead;
  const key = `${g.turn}:${kind}:${lead}:${g.airReports.length}:${rows.map((e) => e.dataset.target).join(',')}`;
  if (raidMemo?.g !== g || raidMemo.key !== key) {
    let id = rows[0].dataset.target ?? '', best = -1;
    for (const e of rows) {
      const p = planRaid(g, kind as RaidKind, Number(e.dataset.target), lead >= 0 ? [lead] : undefined);
      if (!p.why && p.success > best) { best = p.success; id = e.dataset.target ?? ''; }
    }
    raidMemo = { g, key, id };
  }
  return rows.find((e) => e.dataset.target === raidMemo!.id) ?? rows[0];
}
const pick = (e: Element | null): typeof aimAt => (e && (e as HTMLElement).getClientRects().length ? { el: e, map: false } : null);

/** The thing last pointed at, by its words: it is brought into view once, the first time, and never again. */
let shown = '';
function aim(d: ReturnType<typeof onDesk>): void {
  aimAt = target(d);
  lastBox = '';
  document.body.classList.toggle('has-pointer', !!aimAt);
  if (!aimAt) { hidePointer(); shown = ''; return; }
  // A button below the fold of the panel or a card is scrolled up to where he can see it.
  const key = `${d?.id ?? ''}:${aimAt.el.outerHTML.slice(0, 120)}`;
  if (!aimAt.map && key !== shown) {
    shown = key;
    const box = aimAt.el.closest('#pane, #card');
    if (box) {
      const r = aimAt.el.getBoundingClientRect(), b = box.getBoundingClientRect();
      if (r.top < b.top || r.bottom > b.bottom) {
        // (In the panel the note is the first thing: the panel goes to its top where the button shows with it, else
        // only as far as brings the button up from the bottom, so that as much of the note stays in view as can.)
        const pane = box as HTMLElement;
        const bottom = pane.scrollTop + r.bottom - b.top + 16;
        if (box.id === 'pane') pane.scrollTop = bottom <= pane.clientHeight ? 0 : bottom - pane.clientHeight;
        else (aimAt.el as HTMLElement).scrollIntoView({ block: 'center' });
      }
    }
  }
  drawPointer();
}
function hidePointer(): void {
  const p = document.getElementById('fhpointer');
  if (p) p.style.display = 'none';
  document.getElementById('fhground')?.remove();
}

/**
 * The note in the panel scrolled out of view (a long STAFF, AIR or BUILD page): it stands as the slip at the map's
 * left edge until it is back (ruling 147(a): where its home would hide it, it becomes the slip at the left edge). Not
 * while the pointer is on the map, where the slip could lie on the thing pointed at.
 */
let awayCheck = false;
function keepNote(): void {
  const note = document.querySelector<HTMLElement>('#pane > .fh-note');
  const copy = document.getElementById('fhaway');
  const map = document.getElementById('map')?.getBoundingClientRect();
  const pane = note?.parentElement?.getBoundingClientRect();
  const r = note?.getBoundingClientRect();
  const away = !!note && !!map && !!pane && !!r && r.bottom - pane.top < Math.min(r.height, 90) && !aimAt?.map;
  if (!away) { copy?.remove(); return; }
  if (copy && copy.dataset.note === note!.dataset.note && copy.dataset.done === String(note!.classList.contains('is-done'))) return;
  copy?.remove();
  const el = note!.cloneNode(true) as HTMLElement;
  el.id = 'fhaway';
  el.dataset.done = String(note!.classList.contains('is-done'));
  el.classList.remove('fh-in');
  el.classList.add('fh-slip', 'tc-note--slip', 'fh-away');
  el.style.left = `${map!.left + 14}px`;
  el.style.top = `${map!.top + 14}px`;
  document.body.appendChild(el);
}

/** Each frame: the pointer follows its thing as the page scrolls and the map moves. */
function follow(): void {
  if (due) {
    const { d } = due;
    due = null;
    if (toTop) { toTop = false; const pane = $('#pane'); if (pane && pane.scrollTop > 0) pane.scrollTop = 0; }
    placeSlip();
    aim(d);
    keepNote();
  } else if (awayCheck) {
    awayCheck = false;
    keepNote();
  }
  if (aimAt && !due) {
    if (!aimAt.el.isConnected) aim(onDesk());
    else drawPointer();
  }
  requestAnimationFrame(follow);
}

function drawPointer(): void {
  if (!aimAt) return;
  const r = aimAt.el.getBoundingClientRect();
  if (!r.width && !r.height) { hidePointer(); return; }
  // (Scrolled out of its panel or card, it is not pointed at: the clamps would hang over the tabs round nothing.)
  const frame = aimAt.map ? null : aimAt.el.closest('#pane, #card')?.getBoundingClientRect();
  const mid = r.top + r.height / 2;
  const away = !!frame && (mid < frame.top || mid > frame.bottom);
  const box = `${aimAt.map ? 'm' : 'p'}${Math.round(r.left)},${Math.round(r.top)},${Math.round(r.width)},${Math.round(r.height)}${away ? 'x' : ''}`;
  if (box === lastBox) return;
  lastBox = box;
  if (away) { const p = document.getElementById('fhpointer'); if (p) p.style.display = 'none'; return; }
  if (aimAt.map) { mapPointer(r, aimAt.spot); const p = document.getElementById('fhpointer'); if (p) p.style.display = 'none'; return; }
  document.getElementById('fhground')?.remove();
  let p = document.getElementById('fhpointer');
  if (!p) { p = document.createElement('div'); p.id = 'fhpointer'; p.innerHTML = '<div class="tc-pointer"></div>'; document.body.appendChild(p); }
  p.style.display = '';
  // (At the screen's edge the clamps close in on the part of the thing that shows, so that none hangs off it.)
  const L = Math.max(r.left, 12), T = Math.max(r.top, 12), R = Math.min(r.right, innerWidth - 12), B = Math.min(r.bottom, innerHeight - 12);
  const fit = R - L >= 8 && B - T >= 8;
  p.style.left = `${fit ? L : r.left}px`; p.style.top = `${fit ? T : r.top}px`; p.style.width = `${fit ? R - L : r.width}px`; p.style.height = `${fit ? B - T : r.height}px`;
  p.firstElementChild!.classList.toggle('tc-pointer--lg', r.width >= 60 && r.height >= 60);
  // (It breathes out 4 px a side: the scale that takes its frame, 9 px clear, to 13.)
  p.style.setProperty('--fh-s', ((Math.min(r.width, r.height) + 26) / (Math.min(r.width, r.height) + 18)).toFixed(3));
}

/**
 * On the map the pointer is drawn on the ground, under every piece, flag and plate (the art director's brief,
 * section 3): four brass clamps in the map's own layers, just under the pieces, 6 px clear of the thing. It does not
 * breathe there: nothing that moves by itself is drawn on the map's own drawing (84.2.1's standing rule).
 * A province is pointed at by a frame 44 px square at its middle.
 */
function mapPointer(r: DOMRect, spot?: DOMPoint): void {
  const svg = document.getElementById('map') as unknown as SVGSVGElement | null;
  const units = document.getElementById('units');
  const ctm = svg?.getScreenCTM();
  if (!svg || !units || !ctm) return;
  const inv = ctm.inverse();
  const px = 1 / ctm.a;
  let x0 = r.left - 6, y0 = r.top - 6, x1 = r.right + 6, y1 = r.bottom + 6;
  if (spot) { const c = spot.matrixTransform(ctm); x0 = c.x - 22; x1 = c.x + 22; y0 = c.y - 22; y1 = c.y + 22; }
  const a = new DOMPoint(x0, y0).matrixTransform(inv), b = new DOMPoint(x1, y1).matrixTransform(inv);
  const arm = ((x1 - x0) < 60 ? 12 : 16) * px;
  const d = `M${a.x} ${a.y + arm}V${a.y}H${a.x + arm}M${b.x - arm} ${a.y}H${b.x}V${a.y + arm}M${b.x} ${b.y - arm}V${b.y}H${b.x - arm}M${a.x + arm} ${b.y}H${a.x}V${b.y - arm}`;
  let gEl: Element | null = document.getElementById('fhground');
  if (!gEl) { gEl = document.createElementNS('http://www.w3.org/2000/svg', 'g'); gEl.id = 'fhground'; units.parentNode!.insertBefore(gEl, units); }
  gEl.innerHTML = `<path class="fh-case" d="${d}" stroke-width="${(6 * px).toFixed(3)}"/><path class="fh-arm" d="${d}" stroke-width="${(3 * px).toFixed(3)}"/>`;
}

// ---- The Guide -------------------------------------------------------------------------------------------------

/** GUIDE on the top bar, with the count of notes put away on its corner. */
function guideButton(): void {
  const bar = document.getElementById('barbtns');
  const g = page.game();
  if (!bar || !g.human || !page.ui.live) return;
  const n = waitingCount(book);
  const btn = `<button class="btn fh-guide" data-act="fhguide" title="The ten lessons of FIRST COMMAND: done, waiting and to come, and how a month works.">Guide${n ? `<span class="tc-count fh-count">${n}</span>` : ''}</button>`;
  const last = bar.querySelector('[data-act="new"]');
  if (last) last.insertAdjacentHTML('beforebegin', btn); else bar.insertAdjacentHTML('beforeend', btn);
}

const WORD = ['None', 'One', 'Two', 'Three', 'Four', 'Five', 'Six', 'Seven', 'Eight', 'Nine', 'Ten'];

/** A note asked for from the Guide in a campaign where the notes stand aside switches the setting on: he asked for it. */
function wantNotes(): void {
  if (notesWanted(page.game(), book)) return;
  book.on = true;
  book.set = true;
}

/** The Guide's first page: the ten lessons, each done, waiting or to come, and the setting. */
export function guideCard(): string {
  const g = page.game(), esc = page.esc;
  const first = firstCommand(g), wanted = notesWanted(g, book);
  const cur = lessonsOn(g, book) ? currentLesson(g, book, moment())?.id ?? '' : '';
  const waiting = LESSONS.filter((l) => !isDone(book, l.id) && (book.later.includes(l.id) || l.id === cur));
  const done = doneCount(book);
  const state = `${done ? `${WORD[done]} done.` : 'None done yet.'}${waiting.length ? ` ${waiting.length === 1 ? 'One is' : `${WORD[waiting.length]} are`} waiting for you.` : ''}`;
  let lit = false;
  const row = (l: Lesson): string => {
    if (isDone(book, l.id)) return `<div class="tc-step is-done"><i>${TICK}</i><b>${esc(l.head)}</b><button class="tc-btn--quiet fh-quietdark" data-act="fhagain" data-lesson="${l.id}">Show me again</button></div>`;
    if (waiting.includes(l)) {
      const glow = !lit; lit = true;
      return `<button class="tc-step is-now${glow ? ' is-lit' : ''}" data-act="fhback" data-lesson="${l.id}"><i>${l.n}</i><b>${esc(l.head)}</b><u>Waiting for you</u></button>`;
    }
    return `<div class="tc-step is-later"><i>${l.n}</i><b>${esc(l.head)}</b><u>when its moment comes</u></div>`;
  };
  const groups = LESSON_GROUPS.map((gr, k) => {
    const to = LESSON_GROUPS[k + 1]?.from ?? LESSONS.length;
    return `<div class="fh-group"><div class="fh-glabel">${esc(first ? gr.label : gr.plain)}</div>${LESSONS.slice(gr.from, to).map(row).join('')}</div>`;
  }).join('');
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">The Guide${first ? ' · First command' : ''}</div><h2>The ten lessons</h2></header>
    <div class="body fh-guidebody"><p class="fh-state">${esc(state)}</p>${groups}
      <div class="fh-setting"><span>${esc(LESSON_SETTING)}</span><span class="fh-seg" role="group" aria-label="${esc(LESSON_SETTING)}"><button class="${wanted ? 'on' : ''}" data-act="fhset" data-on="1" aria-pressed="${wanted}">On</button><button class="${wanted ? '' : 'on'}" data-act="fhset" data-on="0" aria-pressed="${!wanted}">Off</button></span></div>
      <p class="fh-rules"><button class="link" data-act="fhrules">How a month works ›</button> <small>The typed page of rules, on the Region page when no province is picked.</small></p></div>
    <footer><button class="btn go" data-act="close">Back to the war</button></footer>`;
}

// ---- The door --------------------------------------------------------------------------------------------------

/** FIRST COMMAND on the door's title band, above CONTINUE YOUR CAMPAIGN, lit until the ten lessons are done (ruling 141). */
function doorButton(card: HTMLElement): void {
  const head = card.querySelector('header');
  if (!head || head.querySelector('.fh-door') || page.ui.menu === 'scenarios') return;
  const done = allDone(book);
  const c = FIRST_COMMAND;
  head.classList.add('fh-band');
  head.insertAdjacentHTML('beforeend', `<button class="btn fh-door${done ? '' : ' is-lit'}" data-act="start" data-first="1" data-nat="${c.nat}" data-from="${c.from}" data-view="grand" data-level="${c.level}" data-depth="${c.depth}" title="Germany, September 1939, at Cadet and Field command: your staff runs every desk but the armies, and ten staff notes teach the war as you play it."><b>${page.esc(LESSON_DOOR.label)}</b><span>${page.esc(done ? LESSON_DOOR.again : LESSON_DOOR.fresh)}</span></button>`);
}

// ---- Clicks ----------------------------------------------------------------------------------------------------

function onClick(e: MouseEvent): void {
  const t = (e.target as HTMLElement | null)?.closest?.('[data-act]') as HTMLElement | null;
  if (!t || !page) return;
  const act = t.dataset.act!;
  // A line of the month card followed to its place: lesson 6's deed (the page then goes there as it always does).
  if ((act === 'showmap' || act === 'showsea' || act === 'homealarm') && t.closest('#card .mc')) { lessonDone('line', { card: true }); return; }
  if (!act.startsWith('fh')) return;
  e.stopPropagation();
  e.preventDefault();
  const id = t.dataset.lesson ?? '';
  switch (act) {
    case 'fhlater': { const d = onDesk(); if (d?.lesson) { putAway(book, d.lesson.id); save(); } page.refresh(); break; }
    case 'fhsend': stamped = null; redraw(); break;
    case 'fhtipoff': tip = null; redraw(); break;
    case 'fhclose': if (!book.done.includes(CLOSING.id)) book.done.push(CLOSING.id); save(); redraw(); break;
    case 'fhguide': page.openModal({ kind: 'guide' }); break;
    case 'fhback': bringBack(book, id); wantNotes(); save(); page.openModal(null); page.refresh(); break;
    case 'fhagain': showAgain(book, id); wantNotes(); save(); redrawGuide(); break;
    case 'fhset': book.on = t.dataset.on === '1'; book.set = true; save(); redrawGuide(); break;
    case 'fhrules': {
      page.openModal(null);
      // (A sea picked would show its own page, not the rules.)
      page.ui.sel = -1; page.ui.sea = -1; page.ui.picked.clear(); page.ui.tab = 'region';
      page.refresh();
      const h = [...document.querySelectorAll('#pane h3')].find((x) => x.textContent === 'How a month works');
      h?.scrollIntoView({ block: 'start' });
      break;
    }
  }
}
function redrawGuide(): void {
  if (page.ui.modal?.kind !== 'guide') { page.refresh(); return; }
  const card = document.getElementById('card');
  const at = card?.scrollTop ?? 0;
  page.openModal({ kind: 'guide' });
  if (card) card.scrollTop = at;
  page.refresh();
}

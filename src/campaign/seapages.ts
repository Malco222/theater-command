// The sandbox at sea as pages (Brian, 19 November 2026, "yes do all 3"): rooms a general panel can draw, beside
// airpages.ts, so that the 3D game's bridge needs only to hand its page() and act() calls through. Three of them are
// Brian's: a named ship's card with her battle stars, the navy's memorial, the carrier decks as cards of squadrons,
// and the drawing board as a blueprint whose outline follows the dials.
//
// Pages:
//   'memorial'                          the navy's ships of its own classes lost, year by year, with their stars
//   'deck:<fleet>'                      a carrier fleet's decks as they are; 'deck:<fleet>:<f>.<d>.<t>' a draft of them
//   'board'                             the drawing board at a battleship; 'board:s:<7 dials>' a surface ship,
//                                       'board:u:<4 dials>' a submarine (the dials of sandbox.ts, joined by dots)
// For a fleet's own page: fleetExtras (each named ship with her battle stars and the refits she could have, and the
// door to the decks). For the list of rooms: seaDoors. For the last act: memorialDoor.
// Actions: 'deckplan:<fleet>:<f>:<d>:<t>', 'boardsign:<s|u>:<dials>', 'shiprefit:<fleet>:<ship>:<refit>'.
//
// A page carries what a list of rows cannot: `deck`, the squadrons as cards in their lanes with the page each drag
// opens, and `sheet`, the blueprint (blueprint.ts). A row may carry `stars`, drawn as gold chevrons. Everything is
// behind the sandbox switch: with it off every page here is null and every action unknown, so the game is as it was.

import { airPool, carrierRules } from './air';
import { drawShip, drawSub, type Blueprint, type StepWords, type SubWords } from './blueprint';
import { deckView, planSquadrons, setDeckPlan, whyNoDeckPlan } from './carriers';
import { CLASS_ROWS, classRule, sandbox, type ClassRow } from './classes';
import { deckPlan, navyWing, planeFlak, planeHurt, torpShare, type DeckPlan } from './decks';
import { BUDGET } from './economy';
import { NAVY } from './navy';
import { PRESETS, designCard, drawDesign, refitOffers, subCard, type DesignCard, type Dials, type SubDials } from './sandbox';
import { SANDBOX_SHIPS } from './sandbox-data';
import { decks as deckCount, standard } from './shiplist';
import { humanUp, lastAct, orderShipRefit } from './turn';
import type { Fleet, Game, ShipClass, ShipHull } from './types';
import { turnToDate } from '../core/ticket';

export interface PageButton { label: string; act: string; ok: boolean; tip: string; danger?: boolean }
/** A row; `stars` are a named ship's battle stars, drawn as gold chevrons beside her name. */
export interface PageRow { nat: string; title: string; lines: string[]; buttons: PageButton[]; stars?: number; meter?: number; alarm?: string }
export interface Page {
  id: string; title: string; headline: string; note: string; top: PageButton[]; sections: { heading: string; text: string; rows: PageRow[] }[];
  deck?: DeckSheet;
  sheet?: Blueprint;
}

/** One lane of a carrier fleet's decks: its squadrons, and how many of them have their aircraft aboard. */
export interface DeckLane { role: 'ftr' | 'dive' | 'torp'; label: string; n: number; aboard: number; tip: string }
/** What the reserve holds of a kind, in squadrons (cards) and aircraft. */
export interface DeckStock { role: 'ftr' | 'bmr'; label: string; n: number; planes: number; tip: string }
/**
 * The decks as cards. A card dragged from one place to another opens the page `moves['<from>><to>']`: from 'ftr',
 * 'dive', 'torp', 'res-ftr' or 'res-bmr', to 'ftr', 'dive', 'torp' or 'res' (the reserve). A move the rules refuse is
 * in `refused` with the reason instead.
 */
export interface DeckSheet {
  fleet: number; name: string; slots: number; squadron: number; empty: number;
  lanes: DeckLane[]; reserve: DeckStock[];
  moves: Record<string, string>; refused: Record<string, string>;
  /** Why the plan as drawn cannot be signed now; empty when it can. */
  why: string;
  changed: boolean;
}

const shown = (pp: number): string => Math.round(pp * BUDGET.scale).toLocaleString('en-US');
const pct = (x: number): string => `${Math.round(Math.abs(x) * 100)}%`;
const dateOf = (turn: number): string => turnToDate(Math.max(1, turn)).label;
const cap = (t: string): string => (t ? t[0].toUpperCase() + t.slice(1) : t);
const plural = (n: number, one: string, many = `${one}s`): string => `${n} ${n === 1 ? one : many}`;
const ONE = (k: ShipClass): string => NAVY.classes[k].one;
const MANY = (k: ShipClass): string => NAVY.classes[k].many;
const fleetOf = (g: Game, id: number): Fleet | undefined => g.fleets.find((f) => f.id === id);
const mine = (g: Game, f: Fleet | undefined): f is Fleet => !!f && !!g.human && f.nat === g.human;
const starWords = (n: number): string => (n ? `${plural(n, 'battle star')}: she came through ${n === 1 ? 'a battle' : `${n} battles`} afloat.` : 'No battle yet.');

// ---- A named ship's card ---------------------------------------------------------------------------------------

/** A ship of the list in words, against her kind's standard ship (classes.ts says it the same way for a class). */
function hullWords(h: ShipHull): string {
  const row = { id: h.cls, of: h.kind, label: h.label, power: h.power, tough: h.tough, escort: h.escort, raid: h.raid, ...(h.deck !== undefined && h.kind !== 'cv' ? { deck: h.deck } : {}), ...(h.aa ? { aa: h.aa } : {}), ...(h.eyes ? { eyes: h.eyes } : {}), ...(h.oil !== undefined ? { oil: h.oil } : {}) } as ClassRow;
  return classRule(row);
}

const REFIT_WORD = (id: string): string => SANDBOX_SHIPS.refits.find((r) => r.id === id)?.label.toLowerCase() ?? id;

/**
 * What the sandbox adds to one of the player's fleets' own page: a row for each ship of its list (her name, her class,
 * her battle stars as chevrons, the refits she has had and could have), a row for each kind's standard ships with the
 * refits that would give one a name of her own, and the door to its decks when it has carriers. Null with the switch off.
 */
export function fleetExtras(g: Game, fleetId: number): { rows: PageRow[]; top: PageButton[] } | null {
  const f = fleetOf(g, fleetId);
  if (!sandbox(g) || !mine(g, f)) return null;
  const up = humanUp(g);
  const offers = refitOffers(g, f.nat, f);
  const buttons = (target: string): PageButton[] => offers.filter((o) => o.key.startsWith(`refit:${f.id}:${target}:`)).map((o) => ({
    label: `REFIT: ${o.label.toUpperCase()}`, act: `ship${o.key}`, ok: up && !o.why,
    tip: (up ? o.why : 'It is not your month.') ?? `${o.blurb} ${shown(o.cost)}, and ${plural(o.months, 'month')} in the yard: she leaves the fleet until it is done.`,
  }));
  const rows: PageRow[] = [];
  for (const h of f.hulls ?? []) {
    if (h.n <= 0) continue;
    const words = hullWords(h);
    rows.push({
      nat: f.nat, title: h.n > 1 ? `${h.name} (${h.n} ${MANY(h.kind)})` : h.name, stars: h.stars ?? 0,
      lines: [`${h.label}.${words ? ` ${words}` : ''}`, `Joined the fleet in ${dateOf(h.born)}. ${starWords(h.stars ?? 0)}`, ...(h.refits?.length ? [`Refitted: ${h.refits.map(REFIT_WORD).join(', ')}.`] : [])],
      buttons: buttons(h.name),
    });
  }
  for (const k of ['bb', 'cv', 'ca', 'dd'] as ShipClass[]) {
    const n = standard(f, k);
    const b = buttons(`std:${k}`);
    if (n > 0 && b.length) rows.push({ nat: '', title: `${n} ${n === 1 ? ONE(k) : MANY(k)} of the standard pattern`, lines: ['A refit sends one to the yard: she comes back with a name of her own, and her record starts.'], buttons: b });
  }
  const top: PageButton[] = f.ships.cv > 0 && carrierRules(g)
    ? [{ label: 'THE DECKS', act: `open:deck:${f.id}`, ok: true, tip: 'Her carriers\' squadrons as cards: fighters, dive bombers and torpedo planes, changed over in harbour.' }]
    : [];
  return { rows, top };
}

// ---- The memorial -------------------------------------------------------------------------------------------------

/** The door to the memorial from the last act's page: once the war between the alliances is decided, with the switch on. */
export function memorialDoor(g: Game): PageButton | null {
  if (!sandbox(g) || !g.human || !lastAct(g)?.decided) return null;
  return { label: 'THE MEMORIAL', act: 'open:memorial', ok: true, tip: 'The ships that did not come home, by name, with the battle stars they earned.' };
}

function memorialPage(g: Game): Page | null {
  const n = g.human ? g.nations[g.human] : undefined;
  if (!sandbox(g) || !n) return null;
  const wall = n.memorial ?? [];
  const over = !!lastAct(g)?.decided;
  const stars = wall.reduce((s, m) => s + m.stars, 0);
  const years = new Map<number, PageRow[]>();
  for (const m of wall) {
    const year = turnToDate(Math.max(1, m.turn)).year;
    const where = m.sea >= 0 ? ` in the ${g.seas[m.sea]?.name ?? 'open sea'}` : '';
    (years.get(year) ?? years.set(year, []).get(year)!).push({
      nat: g.human!, title: m.name, stars: m.stars,
      lines: [`${m.label}. Lost${m.how ? ` ${m.how}` : ''}${where}, ${dateOf(m.turn)}.`, m.stars ? `${plural(m.stars, 'battle star')}.` : 'Lost before her first battle was done.'],
      buttons: [],
    });
  }
  return {
    id: 'memorial', title: 'THE MEMORIAL',
    headline: !wall.length ? (over ? 'The war is over, and every ship of the navy\'s own classes came home.' : 'No ship of the navy\'s own classes has been lost.')
      : over ? `The war is over. ${plural(wall.length, 'ship')} did not come home.` : `${plural(wall.length, 'ship')} lost so far.`,
    note: `Every ship of ${n.adj ?? n.name} classes of her own that went down, by name, with the battle stars she earned: one for each battle she came through afloat.${stars ? ` Between them they earned ${plural(stars, 'battle star')}.` : ''}`,
    top: [], sections: [...years.entries()].sort((a, b) => a[0] - b[0]).map(([year, rows]) => ({ heading: String(year), text: '', rows })),
  };
}

// ---- The decks ------------------------------------------------------------------------------------------------------

const LANES: { role: 'ftr' | 'dive' | 'torp'; label: string; tip: string }[] = [
  { role: 'ftr', label: 'FIGHTERS', tip: 'They guard the fleet from the enemy\'s strikes and fly as the escort of its own.' },
  { role: 'dive', label: 'DIVE BOMBERS', tip: 'Surest against carriers and light ships, and they lose fewer to the ships\' guns.' },
  { role: 'torp', label: 'TORPEDO PLANES', tip: 'They do the most harm to battleships, and lose more to the ships\' guns.' },
];
const key = (p: DeckPlan): string => `${p.ftr}.${p.dive}.${p.torp}`;
const same = (a: DeckPlan, b: DeckPlan): boolean => a.ftr === b.ftr && a.dive === b.dive && a.torp === b.torp;

/** Reads a draft from its page id's last part ('4.3.5'), or null when it is not three whole numbers. */
function draftOf(part: string | undefined): DeckPlan | null {
  if (!part) return null;
  const n = part.split('.').map(Number);
  return n.length === 3 && n.every((x) => Number.isInteger(x) && x >= 0) ? { ftr: n[0], dive: n[1], torp: n[2] } : null;
}

function deckPage(g: Game, fleetId: number, draftPart?: string): Page | null {
  const c = fleetOf(g, fleetId);
  if (!mine(g, c)) return null;
  const v = deckView(g, c.nat, c);
  if (!v) return null;
  const N = v.squadrons, sq = v.squadron;
  const now = v.plan;
  // The navy's own mix in squadrons, as the decks fly it until the player signs a plan of his own.
  const own = planSquadrons(c, deckPlan({ ...c, deckPlan: undefined }));
  const draft = draftOf(draftPart) ?? now;
  const changed = !same(draft, now);
  const sum = draft.ftr + draft.dive + draft.torp;
  const base = `deck:${c.id}`;

  // What signing would do to the aircraft: the decks take what the plan wants from the reserve, as far as it has them,
  // and send back what they no longer have room for (setDeckPlan does it the same way).
  const w = navyWing(c.nat), all = w.ftr + w.bmr, decks = deckCount(c), pool = airPool(g, c.nat);
  const est = { ftr: Math.round((decks * all * draft.ftr) / Math.max(1, N)), bmr: Math.round((decks * all * (draft.dive + draft.torp)) / Math.max(1, N)) };
  const hasFtr = Math.max(0, Math.min(pool.ftr, pool.pilots));
  const after = changed
    ? { ftr: est.ftr <= v.aboard.ftr ? est.ftr : v.aboard.ftr + Math.min(est.ftr - v.aboard.ftr, hasFtr), bmr: est.bmr <= v.aboard.bmr ? est.bmr : v.aboard.bmr + Math.min(est.bmr - v.aboard.bmr, Math.max(0, pool.bmr)) }
    : v.aboard;
  const back = { ftr: Math.max(0, v.aboard.ftr - after.ftr), bmr: Math.max(0, v.aboard.bmr - after.bmr) };
  const drawn = { ftr: Math.max(0, after.ftr - v.aboard.ftr), bmr: Math.max(0, after.bmr - v.aboard.bmr) };
  const left = { ftr: Math.max(0, hasFtr - drawn.ftr + back.ftr), bmr: Math.max(0, pool.bmr - drawn.bmr + back.bmr) };
  const strike = draft.dive + draft.torp;
  const lanes: DeckLane[] = LANES.map((l) => {
    const n = draft[l.role];
    const aboard = l.role === 'ftr' ? after.ftr / sq : strike > 0 ? ((after.bmr / sq) * n) / strike : 0;
    return { role: l.role, label: l.label, n, aboard: Math.round(Math.min(n, aboard) * 100) / 100, tip: l.tip };
  });
  const reserve: DeckStock[] = [
    { role: 'ftr', label: 'FIGHTERS IN THE RESERVE', n: Math.floor(left.ftr / sq), planes: Math.round(left.ftr), tip: 'Fighters with pilots, waiting at home. They fill a place in the fighters\' lane.' },
    { role: 'bmr', label: 'STRIKE AIRCRAFT IN THE RESERVE', n: Math.floor(left.bmr / sq), planes: Math.round(left.bmr), tip: 'Bombers waiting at home. They fill a place in the dive bomber or torpedo lane.' },
  ];

  // Every drag the cards allow, and the page each one opens.
  const moves: Record<string, string> = {}, refused: Record<string, string> = {};
  const roles = ['ftr', 'dive', 'torp'] as const;
  const full = 'The decks are full: drag a squadron off to the reserve first.';
  for (const from of roles) {
    if (draft[from] <= 0) continue;
    for (const to of roles) if (to !== from) moves[`${from}>${to}`] = `${base}:${key({ ...draft, [from]: draft[from] - 1, [to]: draft[to] + 1 })}`;
    moves[`${from}>res`] = `${base}:${key({ ...draft, [from]: draft[from] - 1 })}`;
  }
  for (const to of roles) {
    const fits = to === 'ftr' ? 'res-ftr' : 'res-bmr', wrong = to === 'ftr' ? 'res-bmr' : 'res-ftr';
    if (sum < N) moves[`${fits}>${to}`] = `${base}:${key({ ...draft, [to]: draft[to] + 1 })}`;
    else refused[`${fits}>${to}`] = full;
    refused[`${wrong}>${to}`] = to === 'ftr' ? 'Strike aircraft go in the dive bomber or torpedo lane.' : 'Fighters go in the fighters\' lane.';
  }

  const why = whyNoDeckPlan(g, c.nat, c)
    ?? (sum < N ? `${plural(N - sum, 'place')} on the decks ${N - sum === 1 ? 'stands' : 'stand'} empty: fill ${N - sum === 1 ? 'it' : 'them'} from the reserve.` : sum > N ? `The decks hold ${plural(N, 'squadron')}.` : null)
    ?? (!changed ? 'This is the plan the decks fly now.' : null)
    ?? (!humanUp(g) ? 'It is not your month.' : null);

  // What the plan would do in a strike, beside the decks as they are.
  const tNow = torpShare(c), tNew = strike > 0 ? draft.torp / strike : tNow, sNow = Math.max(1e-6, now.dive + now.torp);
  const effect = (k: ShipClass): number => (planeHurt(tNew, k) * strike) / (planeHurt(tNow, k) * sNow) - 1;
  const than = (x: number, more: string, less: string): string => (Math.abs(x) < 0.005 ? 'the same as the decks as they are' : `${pct(x)} ${x > 0 ? more : less} than the decks as they are`);
  const strikeRows: PageRow[] = [
    {
      nat: '', title: `The strike: ${plural(strike, 'squadron')} of strike aircraft`,
      lines: [
        `${plural(draft.dive, 'squadron')} of dive bombers and ${plural(draft.torp, 'squadron')} of torpedo planes: ${strike ? `${pct(tNew)} of the strike carries torpedoes` : 'the decks would fly no strike'}.`,
        ...(changed ? [
          `Against battleships: ${than(effect('bb'), 'harder', 'softer')}. Against carriers: ${than(effect('cv'), 'harder', 'softer')}.`,
          `Against cruisers and destroyers: ${than((effect('ca') + effect('dd')) / 2, 'harder', 'softer')}. To the ships' guns it would lose ${than(planeFlak(tNew) / planeFlak(tNow) - 1, 'more', 'fewer')}.`,
        ] : []),
      ],
      buttons: [],
    },
    {
      nat: '', title: `Fighters: ${plural(draft.ftr, 'squadron')}`,
      lines: [`They guard the fleet and fly as the strike's escort.${changed && draft.ftr !== now.ftr ? ` ${plural(Math.abs(draft.ftr - now.ftr), 'squadron')} ${draft.ftr > now.ftr ? 'more' : 'fewer'} than the decks have now.` : ''}`],
      buttons: [],
    },
  ];
  const craft = (x: { ftr: number; bmr: number }): string => [x.ftr ? plural(Math.round(x.ftr), 'fighter') : '', x.bmr ? plural(Math.round(x.bmr), 'strike aircraft', 'strike aircraft') : ''].filter(Boolean).join(' and ');
  const short = { ftr: Math.max(0, est.ftr - after.ftr), bmr: Math.max(0, est.bmr - after.bmr) };
  const aircraftRow: PageRow = {
    nat: '', title: 'The aircraft',
    lines: [
      `Aboard now: ${craft(v.aboard) || 'none'}.`,
      ...(changed ? [
        back.ftr + back.bmr > 0 ? `Signing sends ${craft(back)} back to the reserve.` : '',
        drawn.ftr + drawn.bmr > 0 ? `Signing draws ${craft(drawn)} from the reserve.` : '',
        short.ftr + short.bmr > 0 ? `The reserve is short of ${craft(short)}: they come as the decks are made up in port.` : '',
      ].filter(Boolean) : []),
    ],
    buttons: [],
  };

  const draftKey = key(draft);
  return {
    id: draftPart ? `${base}:${draftKey}` : base,
    title: `${c.name.toUpperCase()}  ·  THE DECKS`,
    headline: `${plural(N, 'squadron')} of ${sq}: ${draft.ftr} of fighters, ${draft.dive} of dive bombers, ${draft.torp} of torpedo planes.`,
    note: 'Drag a squadron to another lane to change what it flies, or to the reserve to free its place; fill an empty place from the reserve. The plan is signed in harbour: aircraft the decks no longer need go home, and the rest come from the reserve.',
    top: [
      { label: 'SIGN THE DECK PLAN', act: `deckplan:${c.id}:${draft.ftr}:${draft.dive}:${draft.torp}`, ok: !why, tip: why ?? 'The decks take this plan now: what they no longer need flies home, and what they want comes from the reserve.' },
      { label: 'AS THEY ARE', act: `open:${base}`, ok: changed, tip: 'Back to the plan the decks fly now.' },
      { label: 'THE NAVY\'S OWN MIX', act: `open:${base}:${key(own)}`, ok: !same(draft, own), tip: 'The mix the navy flies when it is left to itself.' },
      { label: 'THE FLEET', act: `open:fleet:${c.id}`, ok: true, tip: '' },
    ],
    deck: { fleet: c.id, name: c.name, slots: N, squadron: sq, empty: Math.max(0, N - sum), lanes, reserve, moves, refused, why: why ?? '', changed },
    sections: [
      { heading: 'WHAT THE PLAN WOULD DO', text: '', rows: strikeRows },
      { heading: 'AIRCRAFT', text: '', rows: [aircraftRow] },
    ],
  };
}

// ---- The drawing board ------------------------------------------------------------------------------------------

const BOARD = SANDBOX_SHIPS.board;
const label = (x: { label: string }[]): string[] => x.map((s) => s.label);
const WORDS: StepWords = { hull: label(BOARD.hull), guns: label(BOARD.guns), armour: label(BOARD.armour), speed: label(BOARD.speed), aa: label(BOARD.aa), air: label(BOARD.air) };
const SUB_WORDS: SubWords = { size: label(BOARD.sub.size), tubes: label(BOARD.sub.tubes), range: label(BOARD.sub.range), hangar: label(BOARD.sub.hangar) };
const SURFACE: (keyof Dials)[] = ['hull', 'guns', 'turrets', 'armour', 'speed', 'aa', 'air'];
const UNDER: (keyof SubDials)[] = ['size', 'tubes', 'range', 'hangar'];
const isSub = (d: Dials | SubDials): d is SubDials => (d as SubDials).size !== undefined;
const boardId = (d: Dials | SubDials): string => (isSub(d) ? `board:u:${UNDER.map((k) => d[k]).join('.')}` : `board:s:${SURFACE.map((k) => d[k]).join('.')}`);

/** The dials a board page's id names, or null when they are not the board's. */
function dialsOf(id: string): Dials | SubDials | null {
  const [, kind, list] = id.split(':');
  if (!kind) return { ...(PRESETS.bb as Dials) };
  const n = (list ?? '').split('.').map(Number);
  if (!n.every((x) => Number.isInteger(x) && x >= 0)) return null;
  if (kind === 's' && n.length === SURFACE.length) return Object.fromEntries(SURFACE.map((k, i) => [k, n[i]])) as unknown as Dials;
  if (kind === 'u' && n.length === UNDER.length) return Object.fromEntries(UNDER.map((k, i) => [k, n[i]])) as unknown as SubDials;
  return null;
}

/** Each dial of the board: its steps, the one it stands at, and what each step does in a few words. */
function dialRows(d: Dials | SubDials): PageRow[] {
  const row = (title: string, at: number, steps: string[], go: (i: number) => string, tips: (i: number) => string, lines: string[]): PageRow => ({
    nat: '', title: `${title}: ${steps[at] ?? '?'}`, lines,
    buttons: steps.map((s, i) => ({ label: `${i === at ? '● ' : ''}${s.toUpperCase()}`, act: `open:${go(i)}`, ok: i !== at, tip: i === at ? 'As drawn.' : tips(i) })),
  });
  const set = <T extends Dials | SubDials>(k: keyof T, i: number): string => boardId({ ...(d as T), [k]: i } as T);
  const needs = (x: { needs?: string }): string => (x.needs ? ` Needs the technology ${x.needs === 'superbattleships' ? 'Super battleships' : x.needs === 'battlecarriers' ? 'Battle carriers' : x.needs === 'carriers' ? 'Carriers' : x.needs === 'aircraftsubs' ? 'Aircraft-carrying submarines' : x.needs}.` : '');
  if (isSub(d)) {
    const b = BOARD.sub;
    return [
      row('Size', d.size, SUB_WORDS.size, (i) => set<SubDials>('size', i), (i) => `${b.size[i].count > 1 ? `A flotilla of ${b.size[i].count}` : 'One boat'}, ${b.size[i].months} months on the slip.`, ['The hull: a coastal boat, an ocean-going one, or a cruiser submarine big enough for a hangar.']),
      row('Torpedo tubes', d.tubes, SUB_WORDS.tubes, (i) => set<SubDials>('tubes', i), () => 'More tubes, a heavier salvo against shipping.', ['Tubes in the bow, and in the stern from six.']),
      row('Range', d.range, SUB_WORDS.range, (i) => set<SubDials>('range', i), (i) => `Burns ${Math.round(b.range[i].oil * 100)}% of a boat's oil.`, ['Saddle tanks for fuel: further out on the shipping lanes.']),
      row('Hangar', d.hangar, SUB_WORDS.hangar, (i) => set<SubDials>('hangar', i), (i) => `${b.hangar[i].eyes ? 'A seaplane that scouts a sea beyond her own.' : 'No aircraft.'}${needs(b.hangar[i])}`, ['A seaplane in a watertight hangar, only on a cruiser submarine.']),
    ];
  }
  const tur = [BOARD.turrets[0], BOARD.turrets[1]];
  return [
    row('Hull', d.hull, WORDS.hull, (i) => set<Dials>('hull', i), (i) => `Room for ${BOARD.hull[i].room}; ${BOARD.hull[i].months} months on the slip; a yard of level ${BOARD.hull[i].yard}.${BOARD.hull[i].count > 1 ? ` Comes as a flotilla of ${BOARD.hull[i].count}.` : ''}${needs(BOARD.hull[i])}`, ['Everything else must fit in her hull\'s room.']),
    row('Main guns', d.guns, WORDS.guns, (i) => set<Dials>('guns', i), (i) => `Takes ${BOARD.guns[i].room} of her room a turret.${needs(BOARD.guns[i])}`, ['Heavier guns hit harder in a fleet action and take more room.']),
    row('Turrets', d.turrets, Array.from({ length: tur[1] + 1 }, (_, i) => (i === 0 ? 'None' : String(i))), (i) => set<Dials>('turrets', i), (i) => (i === 0 ? 'No main guns at all.' : `${plural(i, 'turret')} of her main guns.`), ['How many turrets of her main guns she carries.']),
    row('Armour', d.armour, WORDS.armour, (i) => set<Dials>('armour', i), (i) => `Takes ${Math.round(BOARD.armour[i].room * 100)}% of her room.`, ['A thicker belt: harder to sink, slower to build.']),
    row('Speed', d.speed, WORDS.speed, (i) => set<Dials>('speed', i), (i) => `Takes ${Math.round(BOARD.speed[i].room * 100)}% of her room; burns ${Math.round(BOARD.speed[i].oil * 100)}% of the oil.`, ['More boilers and more funnels: better at raiding and escorting, hungrier for oil.']),
    row('Air defence', d.aa, WORDS.aa, (i) => set<Dials>('aa', i), (i) => `Takes ${BOARD.aa[i].room} of her room.`, ['Guns for the air: they cut her fleet\'s losses to air strikes.']),
    row('Aircraft', d.air, WORDS.air, (i) => set<Dials>('air', i), (i) => `${BOARD.air[i].roomShare !== undefined ? `Takes ${Math.round(BOARD.air[i].roomShare * 100)}% of her room.` : `Takes ${BOARD.air[i].room} of her room.`}${needs(BOARD.air[i])}`, ['A catapult seaplane scouts; half a flight deck makes a battle carrier, a full one a carrier.']),
  ];
}

const KIND_WORD: Record<ShipClass, string> = { bb: 'BATTLESHIP', cv: 'AIRCRAFT CARRIER', ca: 'CRUISER', dd: 'DESTROYER', ss: 'SUBMARINE' };

function boardPage(g: Game, id: string): Page | null {
  const nat = g.human;
  if (!nat) return null;
  const d = dialsOf(id);
  if (!d) return null;
  const n = g.nations[nat];
  const card: DesignCard = isSub(d) ? subCard(g, nat, d) : designCard(g, nat, d);
  const row = card.row;
  const flotilla = row.count > 1;
  const kind = `${KIND_WORD[card.kind]}${flotilla ? ` FLOTILLA OF ${row.count}` : ''}`;
  const up = humanUp(g);
  const why = card.why ?? (!up ? 'It is not your month.' : null);
  const std = NAVY.classes[card.kind];
  const per = (k: 'power' | 'tough' | 'escort' | 'raid'): number => (std[k] > 0 ? row[k] / std[k] : 0);
  const sheet = isSub(d)
    ? drawSub(d, SUB_WORDS, kind, `${(n.adj ?? n.name).toUpperCase()} NAVY · ${dateOf(g.turn).toUpperCase()}`)
    : drawShip(d, WORDS, kind, `${(n.adj ?? n.name).toUpperCase()} NAVY · ${dateOf(g.turn).toUpperCase()}`);
  const words = classRule({ ...row, id: '', of: card.kind } as ClassRow);
  const room = card.room.has > 0 ? card.room.used / card.room.has : 0;
  const what: PageRow[] = [
    {
      nat: '', title: `${cap(kind.toLowerCase())}: ${shown(row.cost)} and ${plural(row.months, 'month')} on the slip`,
      lines: [
        `A shipyard of level ${row.yard} lays her down.${flotilla ? ` The price is for all ${row.count}.` : ''}`,
        card.capped ? `Her price is raised: for the money she would be more than a tenth better than a standard ${std.one}, and the board holds every design to that.` : '',
        words || `Much like a standard ${std.one}.`,
      ].filter(Boolean),
      buttons: [],
    },
    ...(!isSub(d) ? [{
      nat: '', title: `Room: ${card.room.used} of ${card.room.has}`, meter: Math.min(1, room),
      lines: [room > 1 ? 'Her guns, armour, engines and the rest take more room than her hull has.' : `${Math.round((1 - room) * 100)}% of her hull's room is left over.`],
      ...(room > 1 + 1e-9 ? { alarm: 'She will not fit.' } : {}),
      buttons: [],
    } as PageRow] : []),
    {
      nat: '', title: `Against a standard ${std.one}`,
      lines: [`In a fleet action ${pct(per('power'))}; hard to sink ${pct(std.tough / Math.max(0.01, row.tough))}; escort ${std.escort ? pct(per('escort')) : row.escort ? 'yes' : 'none'}; raiding ${std.raid ? pct(per('raid')) : 'none'}.`],
      buttons: [],
    },
  ];
  const presetLabel = (k: string): string => CLASS_ROWS.find((r) => r.id === k)?.label ?? (k === 'dd' ? 'Destroyers' : cap(NAVY.classes[k as ShipClass]?.one ?? k));
  const designs = n.designs ?? [];
  return {
    id: boardId(d), title: 'THE DRAWING BOARD',
    headline: `${cap(kind.toLowerCase())}: ${why ?? 'she can be signed for.'}`,
    note: 'Turn a dial and the drawing follows. Every design is held to a tenth better than her kind\'s standard ship for the money: above that, her price rises. A design signed for is ordered from the Navy column of the arsenal like any other class.',
    top: [
      { label: 'SIGN FOR HER', act: `boardsign:${boardId(d).slice(6)}`, ok: !why, tip: why ?? `She joins the navy's classes as Design ${designs.length + 1}, to be ordered from the arsenal.` },
      isSub(d) ? { label: 'A SURFACE SHIP', act: `open:${boardId(PRESETS.bb)}`, ok: true, tip: 'Draw a ship that sails on the sea.' } : { label: 'A SUBMARINE', act: `open:${boardId(PRESETS.scoutsub)}`, ok: true, tip: 'Draw a submarine.' },
    ],
    sheet,
    sections: [
      { heading: 'THE DIALS', text: '', rows: dialRows(d) },
      { heading: 'WHAT SHE WOULD BE', text: '', rows: what },
      {
        heading: 'START FROM', text: '',
        rows: [{ nat: '', title: 'A class to start the drawing from', lines: ['The board opens at her dials, ready to change.'], buttons: Object.keys(PRESETS).map((k) => ({ label: presetLabel(k).toUpperCase(), act: `open:${boardId(PRESETS[k])}`, ok: boardId(PRESETS[k]) !== boardId(d), tip: `Start from the ${presetLabel(k).toLowerCase()}.` })) }],
      },
      {
        heading: 'THE NAVY\'S DESIGNS', text: designs.length ? '' : 'None signed for yet.',
        rows: designs.map((r) => {
          const dd = (r as ClassRow & { dials?: Dials; sub?: SubDials }).dials ?? (r as ClassRow & { sub?: SubDials }).sub;
          return { nat: nat, title: r.label, lines: [`A ${NAVY.classes[r.of as ShipClass]?.one ?? r.of}. ${classRule(r)}`.trim()], buttons: dd ? [{ label: 'DRAW IT AGAIN', act: `open:${boardId(dd)}`, ok: true, tip: 'Open the board at her dials, to draw a new class from her.' }] : [] };
        }),
      },
    ],
  };
}

// ---- The doors and the actions ------------------------------------------------------------------------------------

/** The sandbox's rooms for the list of rooms: the drawing board, and the memorial once a ship is on it or the war is over. */
export function seaDoors(g: Game): { id: string; label: string }[] {
  if (!sandbox(g) || !g.human) return [];
  const out = [{ id: 'board', label: 'THE DRAWING BOARD' }];
  if ((g.nations[g.human]?.memorial?.length ?? 0) > 0 || lastAct(g)?.decided) out.push({ id: 'memorial', label: 'THE MEMORIAL' });
  return out;
}

/** One of this file's pages, or null for any other id (and for all of them with the switch off). */
export function seaPage(g: Game, id: string): Page | null {
  if (!sandbox(g) || !g.human) return null;
  const part = id.split(':');
  if (id === 'memorial') return memorialPage(g);
  if (part[0] === 'deck' && part.length >= 2) return deckPage(g, Number(part[1]), part[2]);
  if (part[0] === 'board') return boardPage(g, id);
  return null;
}

/** Carries out one of this file's actions: null for any other (and for all of them with the switch off). */
export function seaAct(g: Game, action: string): { ok: boolean; note: string; open?: string } | null {
  if (!sandbox(g) || !g.human) return null;
  const part = action.split(':');
  if (part[0] === 'deckplan') {
    const c = fleetOf(g, Number(part[1]));
    const sq = { ftr: Number(part[2]), dive: Number(part[3]), torp: Number(part[4]) };
    if (!mine(g, c)) return { ok: false, note: 'That is not your fleet.' };
    if (!humanUp(g)) return { ok: false, note: 'It is not your month.' };
    const why = whyNoDeckPlan(g, c.nat, c);
    if (why) return { ok: false, note: why };
    const ok = setDeckPlan(g, c.nat, c.id, sq);
    return ok
      ? { ok, note: `The ${c.name}'s decks are set: ${plural(sq.ftr, 'squadron')} of fighters, ${sq.dive} of dive bombers and ${sq.torp} of torpedo planes.`, open: `deck:${c.id}` }
      : { ok, note: 'Those squadrons do not fill the decks.' };
  }
  if (part[0] === 'boardsign') {
    const d = dialsOf(`board:${part[1]}:${part[2]}`);
    if (!d) return { ok: false, note: 'Those dials do not make a ship.' };
    if (!humanUp(g)) return { ok: false, note: 'It is not your month.' };
    const r = drawDesign(g, g.human, d, '');
    return r.row
      ? { ok: true, note: `Signed: ${r.row.label}, a ${NAVY.classes[r.row.of as ShipClass]?.one ?? r.row.of}. Order her from the Navy column of the arsenal.`, open: boardId(d) }
      : { ok: false, note: r.why ?? 'The board cannot sign for her.' };
  }
  if (part[0] === 'shiprefit') {
    const b = orderShipRefit(g, action.slice(4));
    return b ? { ok: true, note: `${b.name}: back in ${plural(b.eta - g.turn, 'month')}.`, open: `fleet:${part[1]}` } : { ok: false, note: 'The yard cannot take her.' };
  }
  return null;
}

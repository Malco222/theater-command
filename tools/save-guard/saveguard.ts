// The save guard: Brian's own saved campaigns, loaded on every new version of the rules and played on for a few
// months the way the page plays them when he presses END THE MONTH and gives no orders. It changes nothing in the
// tree and nothing in the save. It prints one line a check, "ok", "WATCH" or "FAIL", and ends with "errors: [...]",
// which lists every FAIL: a FAIL is something that would break his campaign when he loads it; a WATCH is worth a look
// but breaks nothing.
//
//   node saveguard.mjs SAVE [months]       (run.sh builds saveguard.mjs from this file against any tree)
//
// Beside SAVE an optional SAVE-without-.json + ".expect.json" holds what that campaign was told it would see
// (Britain's answer, towns that must stay his). Rules a tree does not have yet are passed over, and say so.

import { readFileSync, existsSync } from 'node:fs';
import { loadGame, peekSave, saveGame, staleSave } from '../../src/campaign/save';
import * as T from '../../src/campaign/turn';
import * as G from '../../src/campaign/game';
import * as SIEGE from '../../src/campaign/siege';
import * as GOALS from '../../src/campaign/goals';
import * as MARCH from '../../src/campaign/marches';
import * as ROUND from '../../src/campaign/round';
import * as CARD from '../../src/campaign/monthcard';
import * as DANGER from '../../src/campaign/danger';
import { turnToDate } from '../../src/core/ticket';
import worldData from '../../data/campaign/world.json';
import type { Corps, Game } from '../../src/campaign/types';

const [SAVE, MONTHS = '5'] = process.argv.slice(2);
const months = Math.max(1, Number(MONTHS) || 5);
const errors: string[] = [];
const say = (kind: 'ok' | 'WATCH' | 'FAIL' | 'note', what: string): void => {
  console.log(`${kind === 'ok' ? 'ok   ' : kind === 'note' ? '     ' : kind === 'WATCH' ? 'WATCH' : 'FAIL '} ${what}`);
  if (kind === 'FAIL') errors.push(what.slice(0, 300));
};
const check = (ok: boolean, what: string, got?: unknown): boolean => {
  say(ok ? 'ok' : 'FAIL', `${what}${got !== undefined && !ok ? `: ${typeof got === 'string' ? got : JSON.stringify(got)}` : ''}`);
  return ok;
};
const label = (turn: number): string => turnToDate(turn).label;
const any = (m: unknown): any => m as any;

// ---- What the text of the game may never read --------------------------------------------------------------------

/** Words a player must never see: a hole in a sentence. */
const BAD_TEXT = /\bundefined\b|\bNaN\b|\[object|\bnull\b|\bInfinity\b/;
/** The old names of a group of ships (Brian, 17 November 2026: "ships should be called fleets"). An air group's own "Air Squadron" stands. */
const OLD_SHIPS = /(?<! Air) Squadron\b| Escort Group\b| Submarine Flotilla\b/;
/** "the United States" lost its "the" (ruling 211(f)). */
const NO_THE = /\b(?:of|to|on|by|with|for|against|from|at|into|beside) United States\b/;
/** Every string inside a value, for reading a card. */
const strings = (v: unknown, out: string[] = []): string[] => {
  if (typeof v === 'string') out.push(v);
  else if (Array.isArray(v)) for (const x of v) strings(x, out);
  else if (v && typeof v === 'object') for (const x of Object.values(v)) strings(x, out);
  return out;
};

// ---- The save --------------------------------------------------------------------------------------------------

if (!SAVE || !existsSync(SAVE)) { console.log(`no save at ${SAVE}`); console.log('errors:', JSON.stringify([`no save at ${SAVE}`])); process.exit(0); }
const text = readFileSync(SAVE, 'utf8');
const expectPath = SAVE.replace(/\.json$/, '.expect.json');
const expect: { britain?: { dominion: string; answer: 'answers' | 'stays out'; told?: string }; hold?: string[]; alarm?: { army: string } } = existsSync(expectPath) ? JSON.parse(readFileSync(expectPath, 'utf8')) : {};
const raw = JSON.parse(text);
console.log(`save ${SAVE.split('/').pop()}: ${raw.human} in ${label(raw.turn)} (turn ${raw.turn}), seed ${raw.seed}, ${Math.round(text.length / 1024)} KB`);

const peek = peekSave(text);
check(!!peek && !staleSave(text), 'the start screen offers it (not a save from another map)', { peek, stale: staleSave(text) });

let g: Game | null = null;
try { g = loadGame(text); } catch (e) { say('FAIL', `it does not load: ${(e as Error).stack?.split('\n').slice(0, 4).join(' | ')}`); }
if (!g) {
  if (!errors.length) say('FAIL', 'it does not load: loadGame gives nothing');
  console.log('errors:', JSON.stringify(errors));
  process.exit(0);
}
const me = g.human!;
say('ok', `it loads: ${me}, ${label(g.turn)}, ${me} ${T.humanUp(g) ? 'to play' : 'not up (the computer plays on to his month)'}`);

// ---- Nothing lost on the way in ----------------------------------------------------------------------------------

{
  const lost: string[] = [], moved: string[] = [];
  const now = new Map(g.corps.map((c) => [c.id, c]));
  for (const c of raw.corps as Corps[]) {
    const n = now.get(c.id);
    if (!n) { lost.push(`${c.name} (${c.nat})`); continue; }
    if (n.nat !== c.nat || n.reg !== c.reg || n.divs !== c.divs || n.kind !== c.kind || !!n.dead !== !!c.dead) moved.push(`${c.name}: ${c.nat} ${c.kind} ${c.divs} at ${c.reg} became ${n.nat} ${n.kind} ${n.divs} at ${n.reg}`);
  }
  const mine = (raw.corps as Corps[]).filter((c) => c.nat === me && !c.dead);
  check(!lost.length && !moved.length, `every army is there as saved: ${raw.corps.length} corps, his own ${mine.length} with ${mine.reduce((s, c) => s + c.divs, 0)} divisions`, { lost: lost.slice(0, 6), changed: moved.slice(0, 6) });
  // Every corps stands on one list of its own province: in the field or inside the town.
  const twice: string[] = [];
  const seen = new Map<number, number>();
  g.regions.forEach((r) => { for (const c of [...r.units, ...(r.garr ?? [])]) { seen.set(c.id, (seen.get(c.id) ?? 0) + 1); if (c.reg !== r.id) twice.push(`${c.name} listed at ${r.name} but stands at ${g!.regions[c.reg]?.name}`); } });
  for (const c of g.corps) if (!c.dead && seen.get(c.id) !== 1) twice.push(`${c.name} is on ${seen.get(c.id) ?? 0} lists`);
  check(!twice.length, 'every corps stands on its own province\'s list once', twice.slice(0, 6));

  const fleetsGone: string[] = [];
  const fl = new Map(g.fleets.map((f) => [f.id, f]));
  for (const f of raw.fleets as any[]) {
    const n = fl.get(f.id);
    if (!n) fleetsGone.push(`${f.name} (${f.nat})`);
    else if (n.nat !== f.nat || JSON.stringify(n.ships) !== JSON.stringify(f.ships)) fleetsGone.push(`${f.name}: ships or flag changed`);
  }
  const myFleets = g.fleets.filter((f) => f.nat === me);
  check(!fleetsGone.length, `every fleet is there as saved: ${raw.fleets.length}, his own ${myFleets.length} (${myFleets.map((f) => f.name).join(', ')})`, fleetsGone.slice(0, 6));
  const airWas = (raw.airFleets ?? []) as any[];
  const airGone = airWas.filter((a) => !g!.airFleets.some((b: any) => b.id === a.id));
  check(!airGone.length, `every air group is there as saved: ${airWas.length} (${g.airFleets.length} now)`, airGone.slice(0, 6).map((a) => a.name));
  const ground = raw.regions.map((r: any[], i: number) => (g!.regions[i].owner !== r[0] || g!.regions[i].ctrl !== r[1] ? g!.regions[i].name : '')).filter(Boolean);
  check(!ground.length, `every province is held as saved: ${me} holds ${g.regions.filter((r) => r.ctrl === me).length}`, ground.slice(0, 8));
  const wars = Object.values(g.nations).filter((n) => JSON.stringify(n.atWar) !== JSON.stringify(raw.nations[n.id]?.atWar ?? n.atWar) || n.alive !== (raw.nations[n.id]?.alive ?? n.alive)).map((n) => n.id);
  check(!wars.length, `every war and every country stands as saved: ${me} at war with ${g.nations[me].atWar.join(', ') || 'nobody'}`, wars);
}

// ---- Names ------------------------------------------------------------------------------------------------------

const namesNow = (gm: Game, from = { h: 0, s: 0 }): string[] => [
  ...gm.fleets.map((f) => f.name),
  ...gm.headlines.slice(from.h).map((h) => h.text),
  ...gm.seaReports.slice(from.s).flatMap((r) => strings(r)),
].filter((s) => OLD_SHIPS.test(s));
{
  const old = namesNow(g);
  check(!old.length, `ships are fleets: no Squadron, Escort Group or Submarine Flotilla left in names or news (the save itself had ${(text.match(/(?<! Air) Squadron\b| Escort Group\b| Submarine Flotilla\b/g) ?? []).length})`, old.slice(0, 4));
}

// ---- Save, load, save again ---------------------------------------------------------------------------------------

/** The keys in which two saves differ, keys of a table taken in any order. */
const canon = (v: unknown): string => JSON.stringify(v, (_k, x) => (x && typeof x === 'object' && !Array.isArray(x) ? Object.fromEntries(Object.entries(x).sort(([a], [b]) => (a < b ? -1 : a > b ? 1 : 0))) : x));
function differ(a: string, b: string): string[] {
  if (a === b) return [];
  const A = JSON.parse(a), B = JSON.parse(b);
  return [...new Set([...Object.keys(A), ...Object.keys(B)])].filter((k) => canon(A[k]) !== canon(B[k]));
}
/** Where two saves of the same game differ, in a few words: the first corps, nation, region or flag that does. */
function where(a: string, b: string, key: string): string {
  const A = JSON.parse(a)[key], B = JSON.parse(b)[key];
  if (Array.isArray(A) && Array.isArray(B)) {
    if (A.length !== B.length) return `${key}: ${A.length} against ${B.length}`;
    const i = A.findIndex((x: unknown, k: number) => canon(x) !== canon(B[k]));
    const x = A[i], y = B[i];
    if (x && y && typeof x === 'object' && !Array.isArray(x)) {
      const f = Object.keys({ ...x, ...y }).filter((k) => canon(x[k]) !== canon(y[k]));
      return `${key}[${i}] ${x.name ?? x.id ?? ''}: ${f.slice(0, 4).map((k) => `${k} ${JSON.stringify(x[k])?.slice(0, 40)} / ${JSON.stringify(y[k])?.slice(0, 40)}`).join('; ')}`;
    }
    return `${key}[${i}]: ${JSON.stringify(x)?.slice(0, 80)} / ${JSON.stringify(y)?.slice(0, 80)}`;
  }
  if (A && B && typeof A === 'object') {
    const f = Object.keys({ ...A, ...B }).filter((k) => canon(A[k]) !== canon(B[k]));
    return `${key}: ${f.slice(0, 4).map((k) => `${k} ${JSON.stringify(A[k])?.slice(0, 60)} / ${JSON.stringify(B[k])?.slice(0, 60)}`).join('; ')}`;
  }
  return `${key}: ${JSON.stringify(A)?.slice(0, 60)} / ${JSON.stringify(B)?.slice(0, 60)}`;
}
function roundTrip(gm: Game, when: string): void {
  const s1 = saveGame(gm);
  const back = loadGame(s1);
  if (!back) { say('FAIL', `${when}: the game does not load from its own save`); return; }
  const keys = differ(s1, saveGame(back));
  check(!keys.length, `${when}: save, load and save again give the same save`, keys.map((k) => where(s1, saveGame(back), k)));
}
roundTrip(g, `as loaded (${label(g.turn)})`);

// ---- The month, as the page plays it --------------------------------------------------------------------------------

/** Called after each country's month, with the country. */
type Step = (nat: string) => void;
/**
 * One press of END THE MONTH with no orders given, as the page does it (main.ts, 'end', advanceToHuman, arrive):
 * kept marches settled, goals judged, the player's month closed, every other country's month played, then his own
 * month opened, his kept marches walked and last month's orders to stand let go. Returns the month's card.
 */
function endMonth(gm: Game, mark: any, onStep?: Step): { card: any; fell: boolean } {
  const who = gm.human!;
  any(MARCH).settleMarches?.(gm);
  any(GOALS).judgeGoals?.(gm, who);
  T.endHuman(gm);
  let guard = 0;
  while (!T.humanUp(gm) && guard++ < 80) {
    if (!gm.nations[who].alive) break;
    const month = gm.turn;
    const nat = T.stepAI(gm);
    if (gm.turn !== month) any(GOALS).judgeGoals?.(gm, who);
    onStep?.(nat);
  }
  any(GOALS).judgeGoals?.(gm, who);
  if (!gm.nations[who].alive) return { card: null, fell: true };
  if (T.humanUp(gm)) T.beginHuman(gm);
  if (T.humanUp(gm)) { any(MARCH).advanceMarches?.(gm); any(ROUND).settleStands?.(gm); }
  const card = any(CARD).monthCard ? any(CARD).monthCard(gm, mark, who, { opening: any(CARD).opensCampaign?.(gm, mark) ?? false }) : null;
  return { card, fell: false };
}
const markOf = (gm: Game): any => any(CARD).markFor?.(gm, gm.human) ?? null;

// ---- The rules a version may carry ---------------------------------------------------------------------------------

/** Ruling 211(a), Version 84.2.5: guns and engineers never go forward alone. */
const gunsRule: ((gm: Game) => boolean) | undefined = any(G).gunsRule;
const fights = (c: { kind: string }): boolean => c.kind !== 'art' && c.kind !== 'eng' && c.kind !== 'fort';
/** Ruling 211(b), Version 84.2.5: Britain stands by its dominions six times in ten. */
const standsBy = any(G).standsBy;
const dominionAnswers: ((gm: Game, p: string, a: string, b: string, war: number) => boolean) | undefined = any(G).dominionAnswers;
const dominions = ((worldData as any).dominions ?? {}) as Record<string, { of: string[]; answers: number }>;

const guns: string[] = [];
const watchGuns = (gm: Game): Step => {
  let before = gm.regions.map((r) => r.ctrl);
  let reports = gm.reports.length;
  return (nat) => {
    if (!gunsRule?.(gm)) { before = gm.regions.map((r) => r.ctrl); reports = gm.reports.length; return; }
    // An attack or a storm made by guns and engineers alone.
    for (const r of gm.reports.slice(reports)) {
      const atk = r.attackers.filter((u) => !u.guard);
      if (atk.length && !atk.some(fights) && !r.sally) guns.push(`${label(r.turn)}: ${r.attNat} attacked ${r.regionName} with ${atk.map((u) => `${u.name} (${u.kind})`).join(', ')}${r.taken ? ' and took it' : ''}`);
    }
    reports = gm.reports.length;
    // Ground taken with no fighting corps of the new holder's side on it or next to it.
    gm.regions.forEach((r, i) => {
      if (r.ctrl === before[i]) return;
      const side = [...r.units, ...(r.garr ?? [])].filter((c) => !c.dead && (c.nat === r.ctrl || G.allied(gm, c.nat, r.ctrl)));
      if (!side.length || side.some(fights)) return;
      const near = r.nb.some((j) => gm.regions[j].units.some((c) => !c.dead && fights(c) && (c.nat === r.ctrl || G.allied(gm, c.nat, r.ctrl))));
      if (!near) guns.push(`${label(gm.turn)}: ${r.name} went from ${before[i]} to ${r.ctrl} in ${nat}'s month, held by ${side.map((c) => `${c.name} (${c.kind})`).join(', ')} alone`);
    });
    before = gm.regions.map((r) => r.ctrl);
  };
};
/** Towns kept under siege by guns and engineers alone, at the moment the player's month opens. */
const gunSieges = (gm: Game): string[] => (!gunsRule?.(gm) ? [] : gm.regions.filter((r) => SIEGE.besieged(gm, r)).flatMap((r) => {
  const ring = r.units.filter((c) => !c.dead && G.atWar(gm, c.nat, r.ctrl));
  return ring.length && !ring.some(fights) ? [`${r.name} (${r.ctrl}) ringed by ${ring.map((c) => `${c.name} (${c.kind})`).join(', ')} alone`] : [];
}));

/** The wars the player made on a dominion that its mother country has not answered yet: [mother, dominion, month the war began]. */
const dominionWars = (gm: Game): [string, string, number][] => {
  const out: [string, string, number][] = [];
  for (const [p, row] of Object.entries(dominions)) for (const d of row.of) {
    if (!gm.nations[d]?.alive || !G.atWar(gm, me, d) || G.atWar(gm, p, me)) continue;
    const since = gm.flags[`war:${me}:${d}`];
    if (since !== undefined) out.push([p, d, since]);
  }
  return out;
};
const motherNews = (gm: Game, from: number): { head: string; kind: string; text: string; turn: number }[] =>
  gm.headlines.slice(from).filter((h: any) => typeof h.head === 'string' && /^Britain (stands by|stays out of)|^\S+ (stands by|stays out of) .+('s war)?$/.test(h.head)).map((h: any) => ({ head: h.head, kind: h.kind, text: h.text, turn: h.turn }));

/**
 * Ruling 211(c), Version 84.2.6: the red row, the home front's alarm, one rules reading both games draw
 * (danger.ts homeAlarms). Every alarm reads whole, and the army its press picks up is his own and alive. In the
 * save as loaded, the first alarm names the army `expect.alarm.army` says (December 1940: the army round Vancouver).
 */
const homeAlarms: ((gm: Game, nat: string) => any[]) | undefined = any(DANGER).homeAlarms;
function alarms(gm: Game, when: string, first: boolean): void {
  if (!homeAlarms) return;
  let list: any[];
  try { list = homeAlarms(gm, me) ?? []; } catch (e) { say('FAIL', `${when}: the red rows throw: ${(e as Error).stack?.split('\n').slice(0, 4).join(' | ')}`); return; }
  const holes = list.flatMap((a) => strings(a)).filter((s) => BAD_TEXT.test(s));
  const bad: string[] = [];
  for (const a of list) {
    const h = a?.help;
    if (!h) continue;
    for (const id of (h.corps ?? []) as number[]) { const c = gm.corps.find((x) => x.id === id); if (!c || c.dead || c.nat !== me) bad.push(`corps ${id} is not a live corps of ${me}'s`); }
    if (h.to !== undefined && h.to !== null && !gm.regions[h.to]) bad.push(`no province ${h.to}`);
  }
  check(!holes.length && !bad.length, `${when}: the red rows (${list.length}) read whole, and each press picks up an army of his`, { holes: holes.slice(0, 3), bad: bad.slice(0, 3) });
  if (!first || !expect.alarm) return;
  const at = gm.byName[expect.alarm.army];
  if (!list.length) { say('WATCH', `${when}: no red row at all; the save was to show one naming the army round ${expect.alarm.army}`); return; }
  const a = list[0], ids: number[] = a?.help?.corps ?? [];
  const where = [...new Set(ids.map((id) => gm.corps.find((c) => c.id === id)).filter(Boolean).map((c) => gm.regions[c!.reg].name))];
  check(!!at && ids.length > 0 && where.length === 1 && where[0] === expect.alarm.army, `${when}: the first red row names the army round ${expect.alarm.army}: "${String(a?.hand ?? '').slice(0, 140)}"`, { corps: ids, standing: where, to: a?.help?.to !== undefined ? gm.regions[a.help.to]?.name : undefined });
}

// ---- Play on -----------------------------------------------------------------------------------------------------------

const pending = standsBy && Object.keys(dominions).length ? dominionWars(g).filter(([, d]) => g!.flags[`domroll:${me}:${d}`] === undefined) : [];
if (!standsBy) say('note', 'Britain and her dominions (ruling 211(b)): not in this tree, passed over');
else if (!pending.length) say('note', `Britain and her dominions (ruling 211(b)): no war of ${me}'s on a dominion waits for an answer in this save`);
else for (const [p, d, since] of pending) say('note', `Britain and her dominions: ${me} has been at war with ${d} since ${label(since)}; ${p}'s one roll falls at the end of ${label(g.turn)}${dominionAnswers ? `, and on this campaign's seed it says she ${dominionAnswers(g, p, me, d, since) ? 'answers' : 'stays out'}` : ''}`);
if (!gunsRule) say('note', 'guns never go forward alone (ruling 211(a)): not in this tree, passed over');
if (!homeAlarms) say('note', 'the red row (ruling 211(c)): not in this tree, passed over');
else alarms(g, `as loaded (${label(g.turn)})`, true);

// The same month loaded twice gives the same month.
{
  try {
    const a = loadGame(text)!, b = loadGame(text)!;
    endMonth(a, markOf(a)); endMonth(b, markOf(b));
    const sa = saveGame(a), sb = saveGame(b);
    const keys = differ(sa, sb);
    check(!keys.length, `the same month loaded twice plays the same way (${label(g.turn)})`, keys.map((k) => where(sa, sb, k)));
  } catch (e) { say('FAIL', `the month after loading throws: ${(e as Error).stack?.split('\n').slice(0, 5).join(' | ')}`); }
}

const startTurn = g.turn, h0 = g.headlines.length, s0 = g.seaReports.length;
const myFleetIds = new Set(g.fleets.filter((f) => f.nat === me).map((f) => f.id));
const holds = (expect.hold ?? []).filter((n) => g!.byName[n]);
const holder = new Map(holds.map((n) => [n, g!.byName[n].ctrl]));
let mark = markOf(g);
const onStep = watchGuns(g);
let continuity: string | null = null;
for (let m = 1; m <= months; m++) {
  const t0 = Date.now(), turn = g.turn, h = g.headlines.length;
  let res: { card: any; fell: boolean };
  // Month 2 is also played from a save made at the start of it: the loaded game must play it as the kept one does.
  const fork = m === 2 ? saveGame(g) : null;
  try { res = endMonth(g, mark, onStep); } catch (e) { say('FAIL', `the month of ${label(turn)} throws: ${(e as Error).stack?.split('\n').slice(0, 6).join(' | ')}`); break; }
  if (fork) {
    try {
      const k = loadGame(fork)!;
      endMonth(k, markOf(k));
      const keys = differ(saveGame(g), saveGame(k));
      continuity = keys.length ? keys.map((x) => where(saveGame(g), saveGame(k), x)).join(' || ') : '';
    } catch (e) { continuity = `throws: ${(e as Error).message}`; }
  }
  const N = g.nations[me];
  const corps = g.corps.filter((c) => c.nat === me && !c.dead);
  const fleets = g.fleets.filter((f) => f.nat === me);
  console.log(`      month ${m}, ${label(turn)} played in ${((Date.now() - t0) / 1000).toFixed(1)} s: ${me} ${res.fell ? 'FELL' : 'stands'}, ${corps.length} corps of ${corps.reduce((s, c) => s + c.divs, 0)} divisions, ${fleets.length} fleets, ${g.regions.filter((r) => r.ctrl === me).length} provinces; at war with ${N.atWar.join(', ') || 'nobody'}; ${g.headlines.length - h} lines of news`);
  if (res.fell) { say('WATCH', `${me} fell in ${label(turn)}: nothing more to play`); break; }
  // Nothing in the month's news or on its card has a hole in it.
  const news = g.headlines.slice(h).map((x) => x.text);
  const cardText = res.card ? strings(res.card) : [];
  const holes = [...news, ...cardText].filter((s) => BAD_TEXT.test(s));
  check(!holes.length, `${label(turn)}: the news and the month's card read whole (${news.length} lines of news, ${cardText.length} on the card)`, holes.slice(0, 3));
  if (!res.card && any(CARD).monthCard) say('FAIL', `${label(turn)}: no month's card`);
  const the = [...news, ...cardText].filter((s) => NO_THE.test(s));
  if (the.length) say('WATCH', `${label(turn)}: "United States" without its "the": ${the.slice(0, 2).map((s) => `"${s.slice(0, 120)}"`).join('; ')}`);
  const old = namesNow(g, { h, s: s0 });
  if (old.length) say('FAIL', `${label(turn)}: a group of ships called by its old name: ${old.slice(0, 2).join('; ')}`);
  // The player's fleets: any that is gone went down in a fight the news reports.
  for (const id of [...myFleetIds]) if (!g.fleets.some((f) => f.id === id)) { myFleetIds.delete(id); say('WATCH', `${label(turn)}: fleet ${id} of ${me}'s is gone (sunk or merged)`); }
  // Corps on their lists.
  const off = g.corps.filter((c) => !c.dead && ![...g!.regions[c.reg].units, ...(g!.regions[c.reg].garr ?? [])].includes(c));
  check(!off.length, `${label(turn)}: every corps stands on its province's list`, off.slice(0, 4).map((c) => c.name));
  const nan = Object.values(g.nations).filter((n) => !Number.isFinite(n.pp) || (n as any).treasury !== undefined && !Number.isFinite((n as any).treasury)).map((n) => n.id);
  check(!nan.length, `${label(turn)}: every country's books are numbers`, nan);
  for (const n of holds) { const now = g.byName[n].ctrl; if (now !== holder.get(n)) { say('WATCH', `${label(turn)}: ${n} went from ${holder.get(n)} to ${now}`); holder.set(n, now); } }
  for (const s of gunSieges(g)) say('FAIL', `${label(g.turn)}: guns alone keep up a siege (ruling 211(a)): ${s}`);
  roundTrip(g, `after ${label(turn)}`);
  alarms(g, `${label(g.turn)} opens`, false);
  mark = markOf(g);
  // Britain's one roll falls at the end of the first month after loading.
  if (m === 1 && pending.length) {
    for (const [p, d, since] of pending) {
      const told = motherNews(g, h0).filter((x) => x.head.includes(g!.nations[d].name));
      const went = G.atWar(g, p, me);
      const want = dominionAnswers ? dominionAnswers(g, p, me, d, since) : went;
      check(g.flags[`domroll:${me}:${d}`] === since, `${label(turn)}: ${p}'s one roll for ${d} is made as the first month ends`, Object.keys(g.flags).filter((k) => k.startsWith('domroll')));
      check(told.length === 1, `${label(turn)}: one line of news says which way it went: "${told[0]?.head ?? ''}"`, told);
      check(went === want, `${label(turn)}: ${p} ${want ? 'answers' : 'stays out'}, as this campaign's seed says`, { atWar: went });
      if (expect.britain && expect.britain.dominion === d) check((went ? 'answers' : 'stays out') === expect.britain.answer, `${label(turn)}: what Brian was told: Britain ${expect.britain.answer}`, `she ${went ? 'answers' : 'stays out'}${expect.britain.told ? ` (${expect.britain.told})` : ''}`);
    }
  }
}
if (continuity !== null) {
  if (continuity) say('WATCH', `a game loaded from a save made as ${label(startTurn + 1)} opened plays that month differently from the game kept in hand: ${continuity.slice(0, 600)}`);
  else say('ok', `a game loaded from a save made as ${label(startTurn + 1)} opened plays that month as the game kept in hand does`);
}
// After the first month nobody rolls again for the same war, and a Britain that stayed out stays out by this rule.
if (pending.length && g.turn > startTurn + 1) {
  for (const [p, d] of pending) {
    const told = motherNews(g, h0).filter((x) => x.head.includes(g!.nations[d].name));
    check(told.length === 1, `to ${label(g.turn - 1)}: no second roll for ${p} over the same war`, told.map((x) => `${label(x.turn)} ${x.head}`));
    if (told[0]?.kind === 'info' && G.atWar(g, p, me)) say('WATCH', `${p} stayed out, then was at war with ${me} by ${label(g.turn)} (by some other road?)`);
  }
}
if (gunsRule) check(!guns.length, `${label(startTurn)} to ${label(g.turn - 1)}: no ground taken and no attack made by guns or engineers alone (ruling 211(a))`, guns.slice(0, 4));
if (holds.length) say('note', `held at the end: ${holds.map((n) => `${n} ${g!.byName[n].ctrl}`).join(', ')}`);
console.log('errors:', JSON.stringify(errors));

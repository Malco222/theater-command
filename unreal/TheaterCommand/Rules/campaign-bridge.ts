// The doorway between the campaign rules and the Unreal game.
//
// The rules are the browser prototype's own (src/campaign). Nothing here decides anything about
// the war: it asks the rules, gives them the player's orders, and hands back plain pictures of
// the result as JSON. Tools/build_rules.sh bundles this file and everything it imports into
// Content/Rules/campaign-rules.js, which the game runs.
//
// The Unreal side calls these by name through a global object, TC:
//
//   TC.starts()                         -> { groups: [{ title, from, view, sides: [{ nat, name, blurb, aims }] }] }
//   TC.start({ human, from, view, seed }) -> view
//   TC.save()                           -> { text }: the whole campaign as text
//   TC.peek(text)                       -> { human, name, date, open } for a saved campaign, or null
//   TC.load(text)                       -> view, or null if the save cannot be read
//   TC.view()                           -> view
//   TC.reach([corpsId, ...])            -> { regions: [{ id, cost, attack, prev }] }
//   TC.move([corpsId, ...], regionId)   -> { moved: [{ id, path }], view }
//   TC.previewAttack([corpsId, ...], regionId) -> the odds and the Battle Ticket, or null
//   TC.attack([corpsId, ...], regionId) -> { report, note, view }
//   TC.endMonth()                       -> { steps, headlines, events, fallen, view }

import { allied, atWar, createGame, men } from '../../../src/campaign/game';
import {
  beginEve, beginHuman, canOrder, candidates, current, endEve, endHuman, humanUp, onEve, orderAttack, orderMove,
  postureOf, previewAttack, reach, runTurnAI, stepAI,
} from '../../../src/campaign/turn';
import { generalOf } from '../../../src/campaign/commanders';
import { GOALS, GOAL_KEYS, judgeGoals, openGoals } from '../../../src/campaign/goals';
import { isOpen, startOpen } from '../../../src/campaign/staff';
import { loadGame, peekSave, saveGame } from '../../../src/campaign/save';
import { STARTS } from './starts';
import { turnToDate } from '../../../src/core/ticket';
import type { BattleReport, Corps, Game } from '../../../src/campaign/types';

let g: Game | null = null;
/** The player's nation, once it has been beaten. The war goes on with nobody at the controls. */
let fallenNation: string | null = null;

function game(): Game {
  if (!g) throw new Error('No campaign has been started');
  return g;
}

/** Where a nation stands with the player: own, ally, enemy or neutral. */
function standingOf(game: Game, nat: string): string {
  const me = game.human;
  if (!me) return 'neutral';
  if (nat === me) return 'own';
  if (atWar(game, me, nat)) return 'enemy';
  return allied(game, me, nat) ? 'ally' : 'neutral';
}

/** The picture of the war the game draws. */
function view() {
  const s = game();
  const up = humanUp(s);
  return {
    rules: 'campaign',
    turn: s.turn,
    week: s.week,
    date: turnToDate(s.turn).label,
    human: s.human,
    current: current(s),
    humanUp: up,
    eve: onEve(s),
    open: isOpen(s),
    fallen: fallenNation,
    regions: s.regions.map((r) => ({ id: r.id, ctrl: r.ctrl, stand: standingOf(s, r.ctrl) })),
    corps: s.corps.filter((c) => !c.dead).map((c) => ({
      id: c.id, nat: c.nat, name: c.name, kind: c.kind, divs: c.divs, str: c.str, org: c.org, reg: c.reg,
      ap: c.ap, apMax: c.apMax, gen: generalOf(s, c)?.name ?? null, posture: postureOf(c),
      canOrder: up && canOrder(s, c), supplied: c.supplied, men: men(c),
    })),
    nations: Object.fromEntries(Object.values(s.nations).map((n) => [n.id, {
      name: n.name, adj: n.adj, alive: n.alive, standing: standingOf(s, n.id), atWar: n.atWar,
    }])),
  };
}

const corpsById = (s: Game, ids: number[]): Corps[] =>
  ids.map((id) => s.corps.find((c) => c.id === id)).filter((c): c is Corps => !!c && !c.dead);

/** One computer nation's month. If it did something worth seeing, that is noted. */
function stepOne(s: Game, steps: unknown[] | null): void {
  {
    const ctrl = s.regions.map((r) => r.ctrl);
    const r0 = s.reports.length;
    const month = s.turn;
    const nat = stepAI(s);
    if (s.human && s.turn !== month) judgeGoals(s, s.human);
    if (!steps) return;
    const taken = s.regions.filter((r, i) => r.ctrl !== ctrl[i]).map((r) => r.id);
    const reps = s.reports.slice(r0);
    if (!taken.length && !reps.length) return;
    const some = (names: string[]): string => (names.length <= 3 ? names.join(', ') : `${names.slice(0, 3).join(', ')} and ${names.length - 3} more`);
    const won = reps.filter((r) => r.taken).map((r) => r.regionName);
    const held = reps.filter((r) => !r.taken).map((r) => r.regionName);
    const walked = taken.filter((id) => !reps.some((r) => r.taken && r.region === id)).map((id) => s.regions[id].name);
    const said = [
      won.length ? `took ${some(won)}` : '',
      walked.length ? `moved into ${some(walked)}` : '',
      held.length ? `${won.length || walked.length ? 'and were' : 'were'} thrown back at ${some(held)}` : '',
    ].filter(Boolean);
    steps.push({
      nat,
      note: `${s.nations[nat].adj} forces ${said.join(', ')}.`,
      taken,
      battles: reps.map((r) => r.region),
      focus: [...new Set([...taken, ...reps.map((r) => r.region)])],
      // The player's own ground or troops were involved.
      mine: reps.some((r) => r.defNat === s.human || r.defenders.some((u) => u.nat === s.human)) || taken.some((id) => ctrl[id] === s.human),
      view: view(),
    });
  }
}

/** Every computer nation's month until the player is up again, or has fallen. */
function runOthers(s: Game, steps: unknown[] | null): void {
  for (let guard = 0; guard < 80 && !humanUp(s); guard++) {
    if (s.human && !s.nations[s.human].alive) break;
    stepOne(s, steps);
  }
}

/** Opens the player's month once the others have had theirs. Returns the nation that fell, if the player's did. */
function arrive(s: Game): string | null {
  if (s.human) judgeGoals(s, s.human);
  const lost = s.human && !s.nations[s.human].alive ? s.human : null;
  if (lost) {
    // The month the country fell in is played out to its end. The war goes on without the player.
    fallenNation = lost;
    s.human = null;
    while (s.phase > 0) stepAI(s);
  }
  if (humanUp(s)) beginHuman(s);
  return lost;
}

/** The places a campaign can begin, with each playable nation's aims for the campaigns that start in September 1939. */
function starts() {
  return {
    groups: STARTS.map((group) => ({
      ...group,
      sides: group.sides.map((side) => ({
        ...side,
        aims: side.nat && group.from === 1
          ? GOAL_KEYS.map((key) => {
            const goal = GOALS[side.nat!]?.[key];
            return goal ? `${key === 'short' ? 'Short campaign' : 'Long campaign'}: ${goal.text}` : '';
          }).filter(Boolean)
          : [],
      })),
    })),
  };
}

/** Starts a campaign. With a later starting month, the computer plays history up to it and then hands over. */
function start(options?: { human?: string | null; from?: number; view?: string; seed?: number }) {
  const human = options?.human === undefined ? 'GER' : options.human;
  const from = options?.from ?? 1;
  fallenNation = null;
  g = createGame(options?.seed ?? 1939, from > 1 ? null : human);
  if (options?.view === 'open') startOpen(g);
  while (g.turn < from) runTurnAI(g);
  g.human = human;
  if (human) {
    openGoals(g, human, from);
    // A country that is attacked before its first month comes round places its army on the eve of war.
    if (!(from === 1 && beginEve(g))) {
      runOthers(g, null);
      arrive(g);
    }
  }
  return view();
}

/** The whole campaign as text, to be kept and picked up again later. */
function save() {
  return { text: saveGame(game()) };
}

/** A line about a saved campaign for the opening screen, without loading it. */
function peek(text: string) {
  const p = peekSave(text);
  if (!p) return null;
  const sample = g ?? createGame(1, null);
  return { human: p.human, name: p.human ? sample.nations[p.human]?.name ?? p.human : null, date: turnToDate(p.turn).label, open: p.open };
}

function load(text: string) {
  const loaded = loadGame(text);
  if (!loaded) return null;
  g = loaded;
  fallenNation = null;
  if (g.human && !humanUp(g)) {
    runOthers(g, null);
    arrive(g);
  }
  return view();
}

/** Where a picked-up army could go: anywhere any one of its corps could, and what it could attack. */
function reachOf(ids: number[]) {
  const s = game();
  const merged = new Map<number, { id: number; cost: number; attack: boolean; prev: number }>();
  if (humanUp(s)) {
    for (const c of corpsById(s, ids)) {
      for (const [id, r] of reach(s, c)) {
        if (id === c.reg) continue;
        const old = merged.get(id);
        if (!old || r.cost < old.cost) merged.set(id, { id, cost: r.cost, attack: r.attack || !!old?.attack, prev: r.prev });
        else if (r.attack) old.attack = true;
      }
    }
  }
  return { regions: [...merged.values()] };
}

function move(ids: number[], to: number) {
  const s = game();
  const moved: { id: number; path: number[] }[] = [];
  for (const c of corpsById(s, ids)) {
    const m = reach(s, c);
    const end = m.get(to);
    if (!end || end.attack || to === c.reg) continue;
    const path: number[] = [];
    for (let at = to; at !== c.reg && at >= 0; at = m.get(at)!.prev) path.unshift(at);
    const n = orderMove(s, c, to);
    if (n > 0) moved.push({ id: c.id, path: path.slice(0, n) });
  }
  return { moved, view: view() };
}

/** The player's corps that could join an attack on a region: the ones picked that stand next to it. */
function attackers(s: Game, ids: number[], to: number): Corps[] {
  const able = new Set(candidates(s, s.regions[to]).map((c) => c.id));
  return corpsById(s, ids).filter((c) => able.has(c.id));
}

function preview(ids: number[], to: number) {
  const s = game();
  const corps = attackers(s, ids, to);
  const a = previewAttack(s, corps, s.regions[to]);
  if (!a) return null;
  return {
    region: to,
    attack: a.A,
    defence: a.D,
    odds: a.odds,
    lines: a.lines,
    attackers: corps.map((c) => ({ id: c.id, name: c.name, kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null })),
    defenders: a.defenders.map((c) => ({ id: c.id, nat: c.nat, name: c.name, kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null })),
  };
}

/** What a battle came to, in a sentence. */
function battleNote(s: Game, r: BattleReport): string {
  if (r.yielded) return `The ${s.nations[r.defNat].adj} troops fell back from ${r.regionName} without a fight.`;
  const cost = `${r.attLost.toLocaleString('en')} lost against ${r.defLost.toLocaleString('en')}`;
  const prisoners = r.prisoners > 0 ? `, ${r.prisoners.toLocaleString('en')} prisoners` : '';
  return `${r.level} at ${r.regionName}: ${r.taken ? 'the ground is taken' : 'the attack is held'}, ${cost}${prisoners}.`;
}

function attack(ids: number[], to: number) {
  const s = game();
  const before = new Map(s.corps.map((c) => [c.id, c.reg]));
  const report = orderAttack(s, attackers(s, ids, to), s.regions[to]);
  // Corps that followed up into the ground they won, for the game to march across.
  const moved = report
    ? s.corps.filter((c) => !c.dead && c.nat === s.human && c.reg === to && before.get(c.id) !== to).map((c) => ({ id: c.id, path: [to] }))
    : [];
  return { report, note: report ? battleNote(s, report) : '', moved, view: view() };
}

function endMonth() {
  const s = game();
  const h0 = s.headlines.length;
  const steps: unknown[] = [];
  let fallen: string | null = null;
  if (!s.human) {
    // Nobody at the controls (the player's country has fallen): one whole month of the war, every nation on the computer.
    const month = s.turn;
    for (let guard = 0; guard < 80 && s.turn === month; guard++) stepOne(s, steps);
  } else {
    if (onEve(s)) endEve(s);
    else {
      judgeGoals(s, s.human);
      endHuman(s);
    }
    runOthers(s, steps);
    fallen = arrive(s);
  }
  const headlines = s.headlines.slice(h0).map((h) => ({ kind: h.kind, text: h.text, nat: h.nat ?? null, turn: h.turn, week: h.week }));
  // The few lines worth telling first: declarations of war, countries falling, and events.
  const first = headlines.filter((h) => h.kind === 'war' || h.kind === 'fall' || h.kind === 'event' || h.kind === 'goal');
  const events = (first.length ? first : headlines).slice(0, 12).map((h) => ({ kind: h.kind, text: h.text }));
  return { steps, headlines, events, fallen, view: view() };
}

(globalThis as Record<string, unknown>).TC = { starts, start, save, peek, load, view, reach: reachOf, move, previewAttack: preview, attack, endMonth };

// The air war as pages: rooms a general panel can draw with no screen of its own (the grand campaign).
//
// A page is a title, a headline, a note, buttons across the top, and sections of rows; each row has lines of
// words and buttons. A button's `act` is either 'open:<page id>', which opens another page, or an action for
// airAct(). The 3D game's bridge hands its page() and act() calls straight to airPage() and airAct().
//
// Pages: 'airfield' (the province selected), 'group:<id>', 'raids', 'raid:<kind>', 'raid:<kind>:<group id>',
// 'plan:<kind>:<target>:<ids joined by dots>', 'scout:<group id>', 'rebase:<group id>', and for the carriers
// 'wings:<fleet id>' (the aircraft one fleet's carriers bring) and 'search:<group id>' (the seas a group could search).
// A raid that has been flown: 'airreport:<report id>' (its report) and 'airreports' (the month's raids, each with its report).
// Actions: 'raid:<kind>:<target>:<ids joined by dots>', 'job:<group id>:<job key>', 'scout:<group id>:<target>',
// 'airbase:<group id>:<region>', 'search:<group id>:<sea>', 'aboard:<group id>:<fleet id>'.
// A carrier wing is sent ashore by 'airbase' (its 'rebase' page lists the fields on the shore where its fleet is),
// and a wing ashore goes back by 'aboard' (its page 'aboard:<group id>' lists the fleets with deck room for it).
// A carrier wing is an air group: 'group:<its id>' is its page, and the kinds 'fleet' (target: a fleet id) and
// 'harbour' (target: a province) are raids like the rest.

import type { AirFleet, AirReport, AirSide, Game } from './types';
import { AIR, SHIP_AIR_TIP, airRange, airWar, carrierRules, closeRules, groupAt, hasAirfield, km, overOcean, skyWords } from './air';
import { offers } from './arsenal';
import { turnToDate } from '../core/ticket';
import { fieldCanBeLaid, isEngineer } from './engineers';
import { atWar, placeName, theName } from './game';
import { campaignWhy, type GroupRow, type RaidKind, type RaidPlan, type RaidTarget } from './airwar';
import {
  airGroupBases, airGroupDecks, airGroupJobs, airfieldAt, fleetView, humanUp, myAirGroups, orderAboard, orderAirBase, orderGroupJob, orderRaid, orderScout, orderSearch, previewRaid, raidMenu,
  raidTargetsFor, searchOptionsFor, wingsOfFleet,
} from './turn';
import { FLIP, afterCard, type AfterCard, type FoughtAir } from './airfought';
import { allied } from './game';
import { shipList } from './navy';
import { townName } from './towns';

export interface PageButton { label: string; act: string; ok: boolean; tip: string; danger?: boolean }
export interface PageRow { nat: string; title: string; lines: string[]; buttons: PageButton[] }
export interface Page { id: string; title: string; headline: string; note: string; top: PageButton[]; sections: { heading: string; text: string; rows: PageRow[] }[] }
/** What came of something pressed: `open` is the page to show next, `fly` aircraft to draw leaving for their target, `report` the id of the air report just filed (its page is 'airreport:<that id>'). */
export interface AirActResult {
  ok: boolean;
  note: string;
  open?: string;
  /** from is the province the lead group is based in (a carrier wing's port), and fromX, fromY where it took off: its airfield, or its fleet at sea. */
  fly?: { from: number; fromX: number; fromY: number; x: number; y: number; nat: string; bombers: boolean; region: number };
  report?: number;
  /** A raid fought out as an air battle: its after card, written from what the campaign booked. */
  after?: AfterCard;
}

const num = (n: number): string => Math.round(n).toLocaleString('en-US');
const pct = (x: number): string => `${Math.round(x * 100)}%`;
const craft = (x: { ftr: number; bmr: number }): string => [x.ftr >= 1 ? `${num(x.ftr)} fighters` : '', x.bmr >= 1 ? `${num(x.bmr)} bombers` : ''].filter(Boolean).join(', ') || 'no aircraft fit to fly';
const KINDS: RaidKind[] = ['bomb', 'night', 'ground', 'line', 'town', 'superiority', 'strike', 'fleet', 'harbour', 'recon'];
const isKind = (k: string): k is RaidKind => (KINDS as string[]).includes(k);
const ids = (s: string | undefined): number[] => (s ? s.split('.').map(Number).filter((n) => Number.isFinite(n)) : []);
const BACK: PageButton = { label: 'BACK TO THE AIRFIELD', act: 'open:airfield', ok: true, tip: '' };

/** A group's lines. On its airfield's own page the crowding is said once, in the page's head, so its rows leave it out (the UI director, build 1668). */
export function groupLines(x: GroupRow, crowd = true): string[] {
  return [
    `${craft(x)}${x.command ? `, of ${x.command}` : ''}${x.leader ? `. ${x.leader} commands` : ''}. Reaches ${num(x.range)} km. Readiness ${pct(x.ready)}.`,
    `${x.doing}.${crowd && x.crowd < 1 ? ` The airfield is crowded: only ${pct(x.crowd)} of it can fly.` : ''}`,
  ];
}

function airfieldPage(g: Game, region: number): Page | null {
  const c = airfieldAt(g, region);
  if (!c) return null;
  const up = humanUp(g);
  const rows: PageRow[] = c.rows.map((x) => ({
    nat: x.nat, title: x.name, lines: groupLines(x, false),
    buttons: x.mine ? [{ label: 'ORDERS', act: `open:group:${x.id}`, ok: true, tip: 'Its standing orders, the raids it can fly, a reconnaissance, or a move to another airfield.' }] : [],
  }));
  return {
    id: 'airfield', title: `${(c.field?.word ?? 'NO AIRFIELD').toUpperCase()} AT ${c.name.toUpperCase()}`,
    headline: !c.field ? 'No aircraft can be based here' : c.hidden ? 'What flies from here is not known' : rows.length ? `${rows.length} air group${rows.length === 1 ? '' : 's'} based here` : 'No air group is based here',
    note: `${c.note} ${c.build} Aircraft take off from an airfield and come back to it. A fighter group patrols its sky, sticks with a bomber group as escort, ${closeRules(g) ? 'flies ground support for the armies, ' : ''}or rests; any group can fly one raid a month, planned from its target.`,
    top: [{ label: 'PLAN A RAID', act: 'open:raids', ok: up, tip: 'Pick the kind of raid, then the target: you see the chance of success and the losses to expect, then choose which groups fly it together.' }, reportsDoor(g)],
    sections: [{ heading: 'THE AIR GROUPS', text: rows.length || c.hidden ? '' : c.field ? 'Order aircraft from the ARSENAL, or move a group here from another airfield.' : '', rows }],
  };
}

function groupPage(g: Game, id: number): Page | null {
  const x = myAirGroups(g).find((r) => r.id === id);
  if (!x) return null;
  const up = humanUp(g);
  const jobs: PageRow[] = airGroupJobs(g, id).map((j) => ({
    nat: x.nat, title: j.label, lines: [j.forecast, j.why ?? ''].filter(Boolean),
    // (A dim button says why: the order it has now, a reason from the rules, or that it is not the player's month.)
    buttons: [{ label: j.on ? 'ITS ORDERS NOW' : 'GIVE THIS ORDER', act: `job:${id}:${j.key}`, ok: up && !j.on && !j.why, tip: j.on ? 'This is its standing order now. Give it one of the others to change it.' : j.why ?? (up ? '' : 'It is not your month.') }],
  }));
  const menu = raidMenu(g);
  // (A group on a standing campaign by night or by day has no raid of its own: its rows say so, and how to free it,
  // where freeing it would give that kind a target. Finding 4 of 1.5.1.)
  const held = campaignWhy(g, id);
  const raids: PageRow[] = menu.filter((k) => k.key !== 'recon').map((k) => {
    const n = raidTargetsFor(g, k.key, id).length;
    const cannot = x.flew ? 'It has flown its raid this month.' : k.why ?? (n ? null : x.kind === 'ftr' && (k.key === 'night') ? 'Fighters do not fly with the night bombers.' : (held && heldWhy(g, k.key, id)) ?? 'Nothing in its reach.');
    return { nat: x.nat, title: k.label, lines: [k.blurb, cannot ?? `${n} target${n === 1 ? '' : 's'} in its reach.`], buttons: [{ label: 'PICK A TARGET', act: `open:raid:${k.key}:${id}`, ok: up && !cannot, tip: cannot ?? '' }] };
  });
  const deck = x.ship >= 0;
  const top: PageButton[] = [
    deck ? { label: 'BACK TO THE FLEET', act: `open:wings:${x.ship}`, ok: true, tip: '' } : BACK,
    { label: 'SCOUT FROM THE AIR', act: `open:scout:${id}`, ok: up && !x.scouted, tip: x.scouted ? 'It has flown its reconnaissance this month.' : 'A few aircraft photograph one province: its armies are seen whole until the month ends.' },
  ];
  if (carrierRules(g)) top.push({ label: 'SEARCH A SEA', act: `open:search:${id}`, ok: up && !x.scouted, tip: x.scouted ? 'It has flown its reconnaissance this month.' : 'A few aircraft quarter one sea for the month: enemy ships there may be reported, or identified. A fleet has to be found before it can be struck.' });
  if (!deck) top.push({ label: 'MOVE TO ANOTHER AIRFIELD', act: `open:rebase:${id}`, ok: up, tip: 'Settling in costs it some readiness for a month.' });
  // A carrier wing's own order (ruling 175(b)): ashore from its fleet, or back aboard. With one place to go the button gives the order; with several it opens the list.
  const mv = x.wingMove;
  if (mv?.order === 'ashore') {
    const to = airGroupBases(g, id).filter((b) => !b.why);
    top.push({ label: 'FLY ASHORE', act: to.length === 1 ? `airbase:${id}:${to[0].region}` : `open:rebase:${id}`, ok: up && !mv.why, tip: mv.why ?? `${to.length === 1 ? `To the airfield at ${placeName(to[0].name)}. ` : ''}${mv.price}` });
  }
  if (mv?.order === 'aboard') {
    const to = airGroupDecks(g, id).filter((d) => !d.why);
    top.push({ label: 'BACK ABOARD', act: to.length === 1 ? `aboard:${id}:${to[0].fleet}` : `open:aboard:${id}`, ok: up && !mv.why, tip: mv.why ?? `${to.length === 1 ? `To the ${to[0].name}, ${to[0].where}. ` : ''}${mv.price}` });
  }
  return {
    id: `group:${id}`, title: x.name.toUpperCase(), headline: `${craft(x)} ${deck ? 'aboard the' : x.ashore >= 0 ? 'ashore at' : 'at'} ${deck ? x.baseName : placeName(x.baseName)}`,
    note: `${groupLines(x).join(' ')}${x.ashore >= 0 ? ` A carrier wing${x.fleetName ? ` of the ${x.fleetName}` : ''}, ordered ashore: while it is here its carriers fight as empty carriers.` : ''}`,
    top,
    sections: [{ heading: 'STANDING ORDERS', text: 'What it does between raids.', rows: jobs }, { heading: 'RAIDS', text: 'One raid a month. A raid is flown the moment you order it, and the group is away until next month.', rows: raids }],
  };
}

function raidsPage(g: Game): Page | null {
  const menu = raidMenu(g);
  if (!menu.length) return null;
  const up = humanUp(g);
  return {
    id: 'raids', title: 'PLAN A RAID', headline: 'Pick the kind of raid, then its target',
    note: 'You pick the target and see the chance of success and the losses to expect. Then you choose which groups fly it, from one airfield or several: bombers that meet up lose fewer of their number, and fighters in range go with them.',
    top: [BACK, reportsDoor(g)],
    sections: [{ heading: 'KINDS OF RAID', text: '', rows: menu.map((k) => ({ nat: '', title: k.label, lines: [k.blurb, k.why ?? `${k.targets} target${k.targets === 1 ? '' : 's'} in reach this month.`], buttons: [{ label: 'PICK A TARGET', act: `open:raid:${k.key}`, ok: up && !k.why, tip: k.why ?? '' }] })) }],
  };
}

const lossLine = (p: RaidPlan): string => `Chance of success ${pct(p.success)}: ${p.verdict.toLowerCase()}. Likely losses: ${[p.bombers ? `${num(p.bomberLossN)} of ${num(p.bombers)} bombers (${pct(p.bomberLoss)})` : '', p.fighters ? `${num(p.fighterLossN)} of ${num(p.fighters)} fighters` : ''].filter(Boolean).join(', ') || 'none'}${p.foeLossN ? `, for about ${num(p.foeLossN)} enemy fighters` : ''}.`;

/**
 * Every target of a raid as the sections of its page (Brian, 24 November 2026: "you cant bomb germany from london").
 * The list used to stop at the twelve nearest, and from London in 1942 those were all in occupied France and
 * Belgium: no German city could be picked, though the rules have always let bombers fly at any enemy ground in
 * reach, seen or not. Now nothing in reach is left off. Targets on land that lie in more than one country have a
 * section for each country and whoever holds it: for a raid on factories and railways, by day or by night, the
 * enemy's own ground first and then the ground he holds; for every other raid the nearest country first. Inside
 * a section the nearest target is first, as it always was. One country, or targets at sea, is one list as before.
 */
function targetSections(g: Game, kind: RaidKind, all: RaidTarget[], row: (t: RaidTarget) => PageRow, none: string, word = 'TARGET'): Page['sections'] {
  if (!all.length) return [{ heading: 'TARGETS', text: none, rows: [] }];
  const by = all.every((t) => !t.sea && g.regions[t.id]) ? countrySections(g, all, (t) => t.id, (t) => t.holder, row, word, kind === 'bomb' || kind === 'night') : null;
  return by ?? [{ heading: 'TARGETS', text: '', rows: all.map(row) }];
}

/** Provinces gathered by country and whoever holds it, in the order the list first reaches each, each keeping the list's own order. */
function countryGroups<T>(g: Game, all: T[], at: (t: T) => number, holderOf: (t: T) => string): { owner: string; holder: string; list: T[] }[] {
  const groups: { owner: string; holder: string; list: T[] }[] = [];
  for (const t of all) {
    const holder = holderOf(t), owner = g.regions[at(t)].owner || holder;
    const x = groups.find((y) => y.owner === owner && y.holder === holder);
    if (x) x.list.push(t); else groups.push({ owner, holder, list: [t] });
  }
  return groups;
}

/**
 * Provinces as a section for each country and whoever holds it, in the order the list has them, each section keeping
 * the list's own order (`ownFirst`: a country held by its own nation first). Null where they all lie in one.
 */
function countrySections<T>(g: Game, all: T[], at: (t: T) => number, holderOf: (t: T) => string, row: (t: T) => PageRow, word: string, ownFirst: boolean): Page['sections'] | null {
  const groups = countryGroups(g, all, at, holderOf);
  if (groups.length < 2) return null;
  // (the sort is stable: among the enemy's own countries, and among the lands he holds, the nearest stays first)
  if (ownFirst) groups.sort((a, b) => Number(b.owner === b.holder) - Number(a.owner === a.holder));
  const name = (n: string): string => (g.nations[n]?.name ?? n).toUpperCase();
  return groups.map((x) => ({
    heading: `${x.owner === x.holder ? name(x.owner) : `${name(x.owner)}, HELD BY ${name(x.holder)}`}  \u00B7  ${x.list.length} ${word}${x.list.length === 1 ? '' : 'S'}`,
    text: '', rows: x.list.map(row),
  }));
}

/**
 * The player's own province, nearest the enemy, where an engineer corps could lay out an airfield (the rule of
 * engineers.ts) and from which enemy ground would lie within the longest reach of his aircraft (airRange, km): its
 * name, or null where there is none (finding 13 of 1.5.1; the UI director's words of 25 November 2026, 05:10Z). Only
 * the groups that could fly a raid this month are weighed, and only `group` where the page is one group's.
 */
function fieldToLay(g: Game, group: number): string | null {
  const me = g.human;
  if (!me) return null;
  const fleets = g.airFleets.filter((f) => f.nat === me && f.ftr + f.bmr > 0 && f.flew !== g.turn && f.mission === 'support' && (group < 0 || f.id === group));
  const reach = fleets.reduce((m, f) => { const R = airRange(f, g); return Math.max(m, f.bmr > 0 ? R.bmr : 0, f.ftr > 0 ? R.ftr : 0); }, 0);
  const foes = g.regions.filter((r) => atWar(g, me, r.ctrl));
  if (reach <= 0 || !foes.length) return null;
  let best = -1, near = Infinity;
  for (const r of g.regions) {
    if (r.ctrl !== me || hasAirfield(g, r.id) || !fieldCanBeLaid(g, me, r)) continue;
    let d = Infinity;
    for (const e of foes) d = Math.min(d, km(g, r.id, e.id));
    if (d < near) { near = d; best = r.id; }
  }
  return best >= 0 && near <= reach ? g.regions[best].name : null;
}

/**
 * The end of finding 13's line, by whether the player has an engineer corps (the UI director, 25 November 2026,
 * 14:06Z): one in the field, the clause; one on order in the arsenal and not yet raised, the clause and the month it
 * is due, the month the arsenal's order list gives it (15:04Z); none, but the arsenal offers him one (the arsenal's own
 * list, offers()), the clause and where to raise one; and where his nation can never raise one, no clause.
 */
function layBy(g: Game): string {
  const me = g.human!;
  if (g.corps.some((c) => c.nat === me && !c.dead && isEngineer(c))) return ': an engineer corps can lay one there.';
  const etas = g.nations[me].queue.filter((b) => b.what === 'corps' && b.kind === 'eng').map((b) => b.eta).sort((a, b) => a - b);
  // (Two or more on order, the UI director's final read of 15:42Z: how many, and the month the first is due.)
  if (etas.length > 1) return `: an engineer corps can lay one there. You have ${etas.length} on order in the ARSENAL, the first due ${turnToDate(etas[0]).label}.`;
  if (etas.length === 1) return `: an engineer corps can lay one there. You have one on order in the ARSENAL, due ${turnToDate(etas[0]).label}.`;
  if (offers(g, me).some((o) => o.key === 'corps:eng')) return ': an engineer corps can lay one there. You have none: raise one in the ARSENAL.';
  return '.';
}

/** True when no kind of raid has a target in reach of any of the player's groups. */
const reachNothing = (g: Game): boolean => raidMenu(g).every((k) => raidTargetsFor(g, k.key, -1).length === 0);

/**
 * The targets of a kind a group would have, were it taken out of its standing campaign ('Support the armies'). A
 * reading only: the saved group is never written to.
 */
function reachOnceFreed(g: Game, kind: RaidKind, id: number): number {
  const i = g.airFleets.findIndex((x) => x.id === id);
  if (i < 0) return 0;
  const f = g.airFleets[i];
  // (A copy of the group, freed, stands in its place in the list while the targets are read; the group itself is put
  // back after, untouched, so that the save is byte for byte as it was, its keys in their order.)
  const free: AirFleet = { ...f, mission: 'support', target: -1 };
  delete free.escorts; delete free.close;
  g.airFleets[i] = free;
  try {
    return raidTargetsFor(g, kind, id).length;
  } finally {
    g.airFleets[i] = f;
  }
}

/** The campaign's way out (campaignWhy) for a group's page or targets page of one kind: only where freeing the group would give that kind a target. */
const heldWhy = (g: Game, kind: RaidKind, id: number): string | null => {
  const why = campaignWhy(g, id);
  return why && reachOnceFreed(g, kind, id) > 0 ? why : null;
};

function targetsPage(g: Game, kind: RaidKind, group: number): Page | null {
  const k = raidMenu(g).find((x) => x.key === kind);
  if (!k) return null;
  const up = humanUp(g);
  const lead = group >= 0 ? [group] : undefined;
  const all = raidTargetsFor(g, kind, group);
  const row = (t: RaidTarget): PageRow => {
    const p = previewRaid(g, kind, t.id, lead);
    const lines = [`${t.far[0].toUpperCase()}${t.far.slice(1)}. ${t.what ? `${t.what[0].toUpperCase()}${t.what.slice(1)}.` : ''} ${t.groups} of your groups could fly it.`];
    if (p.why) lines.push(p.why);
    else if (kind === 'recon') lines.push(`${p.verdict}. ${p.effects[0]}`);
    else lines.push(`With ${p.seats.filter((s) => s.picked).map((s) => s.name).join(', ')}: ${lossLine(p)}`, ...p.effects.slice(0, 1));
    const act = kind === 'recon' ? `scout:${p.picked[0]}:${t.id}` : `open:plan:${kind}:${t.id}:${p.picked.join('.')}`;
    return { nat: t.holder, title: t.name, lines, buttons: [{ label: kind === 'recon' ? 'SEND THE FLIGHT' : 'PLAN THIS RAID', act, ok: up && !p.why, tip: p.why ?? (kind === 'recon' ? '' : 'Choose which groups fly it, and see what meeting up is worth, before you give the order.') }] };
  };
  const none = kind === 'fleet' ? 'No enemy fleet at sea has been found within reach. Search a sea from a group\'s page, or send a fleet to look: a fleet has to be found before it can be struck.' : kind === 'harbour' ? 'No enemy warships lie in a harbour within reach.' : kind === 'town' ? 'No enemy town that your armies stand round or beside is within reach of a bomber group that can still fly.' : 'Move a group to an airfield nearer the enemy, or wait until one that has flown is back.';
  // (A group held by its standing campaign is told why it has none, and how to free it: finding 4 of 1.5.1. Only
  // where freeing it would give it a target of this kind; otherwise the page says what it said before.)
  const held = !all.length && group >= 0 ? heldWhy(g, kind, group) : null;
  const sections = targetSections(g, kind, all, row, held ?? none);
  // (With nothing in reach of any group, the province a field would put the enemy in reach from: finding 13 of 1.5.1.
  // Only where no kind of raid has a target in reach of any of his groups: where his aircraft already reach the enemy
  // for another kind, the page says what it said before.)
  const lay = all.length || held || kind === 'fleet' || kind === 'harbour' || kind === 'town' || kind === 'strike' || kind === 'recon' || !reachNothing(g) ? null : fieldToLay(g, group);
  return {
    id: group >= 0 ? `raid:${kind}:${group}` : `raid:${kind}`, title: k.label.toUpperCase(),
    headline: all.length ? `${all.length} target${all.length === 1 ? '' : 's'} in reach, ${sections.length > 1 ? 'by country' : 'nearest first'}`
      : lay ? `No target is in reach. Your aircraft could reach the enemy from ${placeName(lay)} if it had an airfield${layBy(g)}` : 'No target is in reach',
    note: k.blurb, top: [{ label: 'BACK', act: group >= 0 ? `open:group:${group}` : 'open:raids', ok: true, tip: '' }],
    sections,
  };
}

function planPage(g: Game, kind: RaidKind, target: number, picked: number[]): Page | null {
  if (kind === 'recon') return null;
  const p = previewRaid(g, kind, target, picked);
  const up = humanUp(g);
  const link = (list: number[]): string => `open:plan:${kind}:${target}:${list.join('.')}`;
  const seat = (s: RaidPlan['seats'][number]): PageRow => ({
    nat: g.human ?? '', title: s.name,
    lines: [`${num(s.aircraft)} ${s.kind === 'ftr' ? 'fighters' : s.kind === 'bmr' ? 'bombers' : 'aircraft'} ${s.ship >= 0 ? 'aboard the' : 'at'} ${s.ship >= 0 ? s.baseName : placeName(s.baseName)}. ${s.why ?? `${s.note[0].toUpperCase()}${s.note.slice(1)}.`}${s.locked ? ' It goes because it is ordered to stick with its bombers.' : ''}`],
    buttons: s.why ? [] : s.picked
      ? [{ label: 'LEAVE IT OUT', act: link(p.picked.filter((id) => id !== s.id)), ok: !s.locked, tip: s.locked ? 'Change its standing order to take it off escort.' : '' }]
      : [{ label: 'ADD IT TO THE RAID', act: link([...p.picked, s.id]), ok: true, tip: s.kind === 'ftr' ? 'It goes as escort, and leaves its patrol for the rest of the month.' : 'Bombers that meet up lose fewer of their number.' }],
  });
  const going = p.seats.filter((s) => s.picked), could = p.seats.filter((s) => !s.picked && !s.why), cannot = p.seats.filter((s) => s.why);
  return {
    id: `plan:${kind}:${target}:${p.picked.join('.')}`, title: p.title.toUpperCase(),
    headline: p.why ?? `${p.verdict}: ${pct(p.success)}`,
    note: p.why ? '' : [lossLine(p), ...p.effects, p.tooFew ?? '', p.massed, p.offPatrol].filter(Boolean).join(' '),
    top: [
      { label: 'FLY THE RAID', act: `raid:${kind}:${target}:${p.picked.join('.')}`, ok: up && !p.why, tip: p.why ?? 'Flown at once. Every group that goes is away for the rest of the month.' },
      { label: 'ANOTHER TARGET', act: `open:raid:${kind}`, ok: true, tip: '' },
    ],
    sections: [
      { heading: 'FLYING THE RAID', text: going.length ? '' : 'No group is picked.', rows: going.map(seat) },
      { heading: 'COULD JOIN IT', text: could.length ? '' : 'No other group can reach it this month.', rows: could.map(seat) },
      { heading: 'RISING TO MEET THEM', text: p.meets.length ? '' : 'No enemy fighters are in reach.', rows: p.meets.map((m) => ({ nat: m.nat, title: m.name, lines: [`${num(m.ftr)} fighters ${m.afloat ? 'aboard the' : 'at'} ${m.afloat ? m.baseName : placeName(m.baseName)}: about ${num(m.able)} can meet the raid.`], buttons: [] })) },
      { heading: 'THE BATTLE TICKET', text: p.lines.map((l) => `${l.label}: ${l.value}${l.effect ? ` (${l.effect})` : ''}`).join('\n'), rows: [] },
      { heading: 'CANNOT GO', text: '', rows: cannot.slice(0, 8).map(seat) },
    ].filter((s) => s.rows.length || s.text),
  };
}

function scoutPage(g: Game, id: number): Page | null {
  const x = myAirGroups(g).find((r) => r.id === id);
  if (!x) return null;
  const up = humanUp(g);
  const all = raidTargetsFor(g, 'recon', id);
  const row = (t: RaidTarget): PageRow => {
    const p = previewRaid(g, 'recon', t.id, [id]);
    return { nat: t.holder, title: t.name, lines: [`${t.far[0].toUpperCase()}${t.far.slice(1)}. ${t.what}. ${p.verdict}.`], buttons: [{ label: 'SEND THE FLIGHT', act: `scout:${id}:${t.id}`, ok: up, tip: p.effects[0] ?? '' }] };
  };
  // (every province in its reach, by country where there is more than one: targetSections)
  const sections = all.length ? targetSections(g, 'recon', all, row, '', 'PROVINCE') : [{ heading: 'PROVINCES', text: '', rows: [] as PageRow[] }];
  if (sections.length === 1) sections[0].heading = 'PROVINCES';
  return {
    id: `scout:${id}`, title: `A RECONNAISSANCE  ·  ${x.name.toUpperCase()}`, headline: all.length ? `${all.length} province${all.length === 1 ? '' : 's'} it could photograph, ${sections.length > 1 ? 'by country' : 'nearest first'}` : x.scouted ? 'It has flown its reconnaissance this month' : 'Nothing in its reach is hidden',
    note: 'A few aircraft photograph one province and the ground beside it: every army there is seen whole until the month ends. One flight a month for each group, beside its raid, and it keeps its patrol.',
    top: [{ label: 'BACK', act: `open:group:${id}`, ok: true, tip: '' }], sections,
  };
}

function rebasePage(g: Game, id: number): Page | null {
  const x = myAirGroups(g).find((r) => r.id === id);
  if (!x) return null;
  const up = humanUp(g);
  // A wing aboard: the airfields on the shore where its fleet is.
  if (x.ship >= 0) {
    const fields: PageRow[] = airGroupBases(g, id).slice(0, 14).map((b) => ({
      nat: g.regions[b.region].ctrl, title: b.name,
      lines: [`${b.word}: serves ${num(b.holds)} aircraft, room for ${num(b.free)} more.${b.why ? ` ${b.why}` : ''}`],
      buttons: [{ label: 'FLY ASHORE HERE', act: `airbase:${id}:${b.region}`, ok: up && !b.why, tip: b.why ?? x.wingMove?.price ?? '' }],
    }));
    return {
      id: `rebase:${id}`, title: `ASHORE  ·  ${x.name.toUpperCase()}`,
      headline: fields.length ? `${fields.length} airfield${fields.length === 1 ? '' : 's'} on the shore where its fleet is` : 'No friendly airfield is in reach of its fleet',
      note: `${x.name} is aboard the ${x.baseName}. A wing can fly ashore to the airfield of the harbour its fleet lies in, or from sea to a friendly airfield on that sea. Ashore it is an air group like any other. ${x.wingMove?.why ?? x.wingMove?.price ?? ''}`,
      top: [{ label: 'BACK', act: `open:group:${id}`, ok: true, tip: '' }], sections: [{ heading: 'AIRFIELDS', text: '', rows: fields }],
    };
  }
  const all = airGroupBases(g, id);
  const rows: PageRow[] = all.filter((b) => !b.sea).slice(0, 14).map((b) => ({
    nat: g.regions[b.region].ctrl, title: b.name,
    lines: [`${b.word}, ${num(b.km)} km away: serves ${num(b.holds)} aircraft, room for ${num(b.free)} more.${b.free < x.aircraft ? ' It would be crowded: not all of the group could fly.' : ''}${b.why ? ` ${b.why}` : ''}`],
    buttons: [{ label: 'MOVE HERE', act: `airbase:${id}:${b.region}`, ok: up && !b.why, tip: b.why ?? 'Settling in costs it some readiness for a month.' }],
  }));
  // Across an ocean a group goes by sea (ruling 133(b)): every field it could be shipped to (finding 5 of 1.5.1: the
  // eight nearest left Britain off from the Canal Zone), country by country in the order the list of a raid's targets
  // keeps them, and within each country the fields it could be shipped to now first, then the nearest. No heading for
  // each country in 1.5.2 (tests/america-842.test.ts holds this page to its two sections): the first field of each
  // country's run names the country on its second line, as the target list's heading would, unless the field's own
  // name is the country's (the UI director, 25 November 2026, 05:45Z).
  const far = all.filter((b) => b.sea).sort((a, b) => (a.why ? 1 : 0) - (b.why ? 1 : 0) || a.km - b.km);
  const nameOf = (n: string): string => g.nations[n]?.name ?? n;
  const over: PageRow[] = countryGroups(g, far, (b) => b.region, (b) => g.regions[b.region].ctrl).flatMap((c) => c.list.map((b, i) => {
    const country = c.owner === c.holder ? nameOf(c.owner) : `${nameOf(c.owner)}, held by ${g.nations[c.holder] ? theName(g.nations[c.holder]) : c.holder}`;
    return {
      nat: g.regions[b.region].ctrl, title: b.name,
      lines: [`${b.word}, ${num(b.km)} km away across the ocean: serves ${num(b.holds)} aircraft, room for ${num(b.free)} more.${b.why ? ` ${b.why}` : ''}`, ...(i === 0 && b.name !== nameOf(c.owner) ? [country] : [])],
      buttons: [{ label: 'SHIP IT HERE', act: `airbase:${id}:${b.region}`, ok: up && !b.why, tip: b.why ?? SHIP_AIR_TIP }],
    };
  }));
  const sections = [{ heading: 'AIRFIELDS', text: '', rows }];
  if (over.length) sections.push({ heading: 'ACROSS THE OCEAN, BY SEA', text: SHIP_AIR_TIP, rows: over });
  return {
    id: `rebase:${id}`, title: `A NEW AIRFIELD  ·  ${x.name.toUpperCase()}`, headline: `${rows.length} airfield${rows.length === 1 ? '' : 's'} it could move to, nearest first`,
    note: `${x.name} is based at ${placeName(x.baseName)}. Aircraft can be based only where there is an airfield: engineers lay new ones out, and the treasury enlarges them. A group reaches farther into enemy country from a field nearer the front.`,
    top: [{ label: 'BACK', act: `open:group:${id}`, ok: true, tip: '' }], sections,
  };
}

/** The fleets a carrier wing ashore could go back aboard. */
function aboardPage(g: Game, id: number): Page | null {
  const x = myAirGroups(g).find((r) => r.id === id);
  if (!x || x.ashore < 0) return null;
  const up = humanUp(g);
  const rows: PageRow[] = airGroupDecks(g, id).slice(0, 14).map((d) => ({
    nat: x.nat, title: d.name,
    lines: [`${d.where[0].toUpperCase()}${d.where.slice(1)}: deck room standing empty for ${num(d.room)} ${x.kind === 'ftr' ? 'fighters' : 'strike aircraft'}.${d.why ? ` ${d.why}` : ''}`],
    buttons: [{ label: 'GO ABOARD', act: `aboard:${id}:${d.fleet}`, ok: up && !d.why, tip: d.why ?? x.wingMove?.price ?? '' }],
  }));
  return {
    id: `aboard:${id}`, title: `BACK ABOARD  ·  ${x.name.toUpperCase()}`,
    headline: rows.length ? `${rows.length} fleet${rows.length === 1 ? '' : 's'} with deck room for it` : 'No carrier has deck room standing empty for it',
    note: `${x.name} is ashore at ${placeName(x.baseName)}. It goes back aboard a fleet of its navy that has deck room standing empty, when that fleet lies in the harbour here or in the sea off this coast.`,
    top: [{ label: 'BACK', act: `open:group:${id}`, ok: true, tip: '' }], sections: [{ heading: 'FLEETS', text: '', rows }],
  };
}

/** The aircraft one fleet's carriers bring: its wings as rows, each opening as an air group. */
function wingsPage(g: Game, fleet: number): Page | null {
  const c = carrierRules(g) ? fleetView(g, fleet) : null;
  if (!c) return null;
  const up = humanUp(g), mine = c.nat === g.human;
  const carriers = c.cards.find((x) => x.cls === 'cv')?.count ?? 0;
  const rows: PageRow[] = wingsOfFleet(g, fleet).map((w) => ({
    nat: c.nat, title: w.name,
    lines: [`${num(w.aircraft)} of ${num(w.est)} ${w.kind === 'ftr' ? 'fighters' : 'strike aircraft'}. Reaches ${num(w.range)} km. Readiness ${pct(w.ready)}.`, `${w.doing}. ${w.note}`],
    buttons: mine ? [{ label: 'ORDERS', act: `open:group:${w.id}`, ok: true, tip: 'Its standing orders, the strikes it can fly, a search of a sea, or a reconnaissance.' }] : [],
  }));
  const ashore: PageRow[] = c.ashore.map((w) => ({
    nat: c.nat, title: w.name, lines: [`${num(w.aircraft)} aircraft ashore at ${placeName(w.at)}.`],
    buttons: mine ? [{ label: 'ORDERS', act: `open:group:${w.id}`, ok: true, tip: 'Its orders as an air group ashore, and the order that sends it back aboard.' }] : [],
  }));
  return {
    id: `wings:${fleet}`, title: `THE AIRCRAFT OF THE ${c.name.toUpperCase()}`,
    headline: carriers ? `${carriers} carrier${carriers === 1 ? '' : 's'}: ${num(c.aboard)} of ${num(c.deckRoom)} aircraft aboard${c.bare ? `. ${c.bare}` : ''}` : 'This fleet has no carriers',
    note: 'A carrier is a floating airfield: its fighters and strike aircraft go where the fleet goes. At sea they reach every coast of the sea the fleet is in, the ground inland of it, and the seas next to it. The fighters cover the fleet; the strike aircraft go for a fleet your side has found, ships in a harbour, or a target ashore. New aircraft come aboard only in harbour, and a carrier that is sunk takes most of its aircraft with it.',
    top: [{ label: 'PLAN A STRIKE', act: 'open:raids', ok: up && mine, tip: 'Pick the kind of raid, then the target: you see the chance of success and the losses to expect before you give the order.' }],
    sections: [
      { heading: 'THE WINGS', text: rows.length ? '' : ashore.length ? 'None aboard.' : 'Carriers are built in the ARSENAL, in a port with a great yard.', rows },
      ...(ashore.length ? [{ heading: 'ASHORE', text: '', rows: ashore }] : []),
    ],
  };
}

function searchPage(g: Game, id: number): Page | null {
  const x = myAirGroups(g).find((r) => r.id === id);
  if (!x || !carrierRules(g)) return null;
  const up = humanUp(g);
  const rows: PageRow[] = searchOptionsFor(g, id).slice(0, 14).map((s) => ({
    nat: '', title: s.name,
    lines: [`${s.km > 0 ? `${num(s.km)} km` : 'Its own waters'}. ${s.word}.${s.reported ? ` ${s.reported} enemy force${s.reported === 1 ? '' : 's'} reported there, not identified.` : ''}${s.searched ? ' Already searched from the air this month.' : ''}`],
    buttons: [{ label: 'SEARCH THIS SEA', act: `search:${id}:${s.zone}`, ok: up, tip: 'It adds to your search of that sea for the month: enough may report a fleet there, or identify it.' }],
  }));
  return {
    id: `search:${id}`, title: `A SEARCH AT SEA  ·  ${x.name.toUpperCase()}`, headline: rows.length ? `${rows.length} sea${rows.length === 1 ? '' : 's'} it could search, nearest first` : x.scouted ? 'It has flown its reconnaissance this month' : 'No sea is within its reach',
    note: 'A fleet has to be found before it can be struck. Your ships in a sea, your carriers in the seas next to it and your aircraft on its coasts all search it; a search flown from here adds to that for the month. A fleet is first reported (ships are there, not what), then identified. One flight a month for each group, beside its raid.',
    top: [{ label: 'BACK', act: `open:group:${id}`, ok: true, tip: '' }], sections: [{ heading: 'SEAS', text: '', rows }],
  };
}

// ---- A raid that has been flown: its report ---------------------------------------------------------------
//
// (Brian, 24 November 2026: "i did a fighter raid from germany and it was intercepted by the UK but it didnt't tell
// me the results of the battle.") A raid his crews fly is settled the moment it is ordered and its report is filed
// (g.airReports), but the 3D game showed him one line under the map for twelve seconds and nothing more. The report
// is now a page of its own, in the words the battles list of the browser game has always used for it.

const ours = (g: Game, nat: string): boolean => !!g.human && !!nat && (nat === g.human || allied(g, nat, g.human));

/** The side a raid's report is read from: 'att' a raid his side flew, 'def' one that fell on his side, null a raid that is neither's (not his to read). */
function raidSide(g: Game, r: AirReport): 'att' | 'def' | null {
  if (ours(g, r.attNat) || r.att.some((u) => u.nat === g.human)) return 'att';
  return ours(g, r.defNat) || r.def.some((u) => ours(g, u.nat)) ? 'def' : null;
}

/**
 * How it went, as that side reads it. The rules judge a raid by what it did against what it cost (the harm done and
 * the defenders shot down, against the raiders lost), which is not whether the bombs fell: a raid can strike its
 * target and still cost more than it was worth, so a raid that did not pay is never said to have been "beaten off"
 * above a note that says where its bombs landed. One that no fighter rose to meet was no battle. Only a fight for
 * the sky is won or lost outright, on its losses; and since a sweep can win its fight and still not win the sky it
 * was flown for (it takes fighters enough left over it), the verdict says both, the fight and the sky.
 */
function raidVerdict(r: AirReport, side: 'att' | 'def'): string {
  if (r.mission === 'superiority') {
    // (The fight and the sky can disagree: a sweep can win its fight and not the sky. Then the verdict says "but",
    // so the two halves do not read as a contradiction; where they agree a comma is enough.)
    const fight = side === 'def' ? FLIP[r.level] ?? r.level : r.level;
    const good = side === 'def' ? !r.sky : r.sky;
    const agree = /victory/i.test(fight) === good;
    const skyWord = side === 'def' ? (r.sky ? 'lost' : 'held') : r.sky ? 'won' : 'not won';
    return agree ? `${fight}, sky ${skyWord}` : `${fight}, but the sky ${skyWord}`;
  }
  if (!r.def.length) return side === 'def' ? 'Not intercepted' : 'Unopposed';
  return r.won ? (side === 'def' ? 'Raid got through' : 'Raid succeeded') : side === 'def' ? 'Raid made to pay' : 'Raid cost too much';
}

function raidTitle(g: Game, r: AirReport): string {
  if (r.mission === 'fleet') return `Air strike on the ${r.ships?.[0]?.fleet ?? 'fleet'}`;
  if (r.mission === 'harbour') return `Raid on the harbour of ${placeName(r.place)}`;
  if (r.mission === 'town') return `Bombing of the defences of ${placeName(r.region >= 0 ? townName(g.regions[r.region]) : r.place)}`;
  return r.mission === 'superiority' ? `The sky over ${placeName(r.place)}` : r.mission === 'bomb' ? `Bombing of ${placeName(r.place)}` : r.mission === 'night' ? `Night raid on ${placeName(r.place)}`
    : r.mission === 'ground' ? `Raid on the troops at ${placeName(r.place)}` : r.mission === 'line' ? `Bombing of the line at ${placeName(r.place)}` : `Air strikes, ${r.place.replace(/^the /, '')}`;
}

/** "1 fighter, 53 bombers": aircraft counted with their nouns, the singular where it is one. */
const planes = (ftr: number, bmr: number): string => ([[ftr, 'fighter'], [bmr, 'bomber']] as [number, string][]).filter(([n]) => n >= 1).map(([n, w]) => `${num(n)} ${w}${Math.round(n) === 1 ? '' : 's'}`).join(', ') || 'no aircraft';
const fell = (ss: AirSide[]): number => ss.reduce((n, x) => n + x.lostFtr + x.lostBmr, 0);
const lostWord = (n: number): string => (n >= 1 ? num(n) : 'no');
/**
 * What each side lost, in a sentence or two. Never "of" a number sent: a report keeps each group's whole strength,
 * not the share of it that flew or rose (a night raid is met by a few of a group's fighters), so "lost 0 of 476"
 * would say that 476 fought. The groups and their strengths are in the rows below.
 */
const raidBill = (r: AirReport): string => `The raid lost ${lostWord(fell(r.att))} aircraft. ${r.def.length ? `The defence lost ${lostWord(fell(r.def))}.` : 'No fighters came up.'}`;

/**
 * The raids whose reports are the player's to read now, newest first: every raid flown by his side or against it
 * since he last ended a month. The 3D game's bridge marks the last report filed as he ends his month
 * (flags.raidsFrom), so the raids his own side flies as his month closes (an air campaign's, a standing order's,
 * the escorts that follow) are his to read with those the other nations fly after it and those he orders next month.
 * A war with no mark yet (its first month, or one saved before the mark) reads by the month: this month's raids, and
 * last month's by the nations that move after him.
 */
export function raidsToRead(g: Game): { r: AirReport; side: 'att' | 'def' }[] {
  const me = g.human;
  if (!me) return [];
  const at = g.order.indexOf(me);
  const from: number | undefined = g.flags.raidsFrom;
  const out: { r: AirReport; side: 'att' | 'def' }[] = [];
  for (const r of g.airReports) {
    if (from !== undefined ? r.id <= from : r.turn !== g.turn && !(r.turn === g.turn - 1 && g.order.indexOf(r.attNat) > at)) continue;
    const side = raidSide(g, r);
    if (side) out.push({ r, side });
  }
  return out.reverse();
}

/** The door to the month's raids and their reports: dim, and saying why, while there is none to read. */
function reportsDoor(g: Game): PageButton {
  const n = raidsToRead(g).length;
  return { label: "THE MONTH'S RAIDS", act: 'open:airreports', ok: n > 0, tip: n ? 'Every raid flown by your side or against it since you last ended a month, each with its report.' : 'No raid has been flown by your side or against it since you last ended a month.' };
}

function raidReportPage(g: Game, id: number): Page | null {
  const r = g.airReports.find((x) => x.id === id);
  const side = r ? raidSide(g, r) : null;
  if (!r || !side) return null;
  const adj = (n: string): string => g.nations[n]?.adj ?? n;
  const row = (x: AirSide): PageRow => ({
    // (the group's strength beside its name: the row is the report's, not the group's own page)
    nat: x.nat, title: `${x.fleet}  ·  ${planes(x.ftr, x.bmr)}`,
    lines: [`${x.leader ? `${x.leader} commands. ` : ''}${x.lostFtr + x.lostBmr >= 1 ? `Lost ${planes(x.lostFtr, x.lostBmr)}.` : 'No losses.'}`],
    buttons: [],
  });
  const ships: PageRow[] = (r.ships ?? []).map((x) => {
    const had = shipList(x.ships), sunk = shipList(x.sunk);
    return { nat: x.nat, title: x.fleet, lines: [`${had[0].toUpperCase()}${had.slice(1)}${x.admiral ? `. ${x.admiral} commands` : ''}. ${sunk === 'no ships' ? 'No ships lost.' : `Lost ${sunk}.`}`], buttons: [] };
  });
  // A fight for the sky: how the fighters stood at its end, against what it takes to win it, in the one set of words
  // the browser's report card reads too (air.ts skyWords, the UI director's rulings of 04:55Z to 15:42Z on 25 November):
  // the game's page and the browser's card say the same sentence of the same raid.
  const sky = skyWords(g, r);
  const unready = r.surprise ? ' Caught unready: war was declared this month, so the harbour had no warning, its fighters and guns were not manned, and the ships lay at their moorings.' : '';
  const where: PageButton[] = r.zone >= 0 ? [{ label: `GO TO ${r.place.replace(/^the /i, 'THE ').toUpperCase()}`, act: `goto:sea:${r.zone}`, ok: true, tip: 'Looks at the place on the map.' }]
    : r.region >= 0 ? [{ label: `GO TO ${r.place.toUpperCase()}`, act: `goto:region:${r.region}`, ok: true, tip: 'Looks at the place on the map.' }] : [];
  return {
    id: `airreport:${id}`, title: `THE RAID REPORT  ·  ${r.place.replace(/^the /i, '').toUpperCase()}`,
    headline: `${raidTitle(g, r)}: ${raidVerdict(r, side).toLowerCase()}`,
    note: `${turnToDate(r.turn).label}. ${[r.result, raidBill(r)].filter(Boolean).join(' ')}${sky}${unready}`,
    top: [
      reportsDoor(g),
      { label: 'PLAN A RAID', act: 'open:raids', ok: humanUp(g), tip: humanUp(g) ? '' : 'It is not your month.' },
      ...where,
    ],
    sections: [
      { heading: side === 'att' ? 'FLEW THE RAID' : 'THE RAIDERS', text: '', rows: r.att.map(row) },
      { heading: 'ROSE TO MEET THEM', text: r.def.length ? '' : 'No fighters came up.', rows: r.def.map(row) },
      { heading: 'THE SHIPS UNDER ATTACK', text: '', rows: ships },
      { heading: 'THE BATTLE TICKET', text: r.lines.map((l) => `${l.label}: ${l.value}${l.effect ? ` (${l.effect})` : ''}`).join('\n'), rows: [] },
    ].filter((s) => s.rows.length || s.text),
  };
}

function raidReportsPage(g: Game): Page | null {
  const all = raidsToRead(g);
  const row = ({ r, side }: { r: AirReport; side: 'att' | 'def' }): PageRow => ({
    nat: r.attNat, title: raidTitle(g, r), lines: [`${raidVerdict(r, side)}. ${raidBill(r)}`],
    buttons: [{ label: 'READ THE REPORT', act: `open:airreport:${r.id}`, ok: true, tip: 'Who flew, who rose to meet them, what each lost, and the battle ticket.' }],
  });
  const flown = all.filter((x) => x.side === 'att'), met = all.filter((x) => x.side === 'def');
  const up = humanUp(g);
  return {
    id: 'airreports', title: "THE MONTH'S RAIDS",
    // (Never a bare 0: "none" reads better beside a count.)
    headline: !all.length ? 'No raid to read' : flown.length ? `${flown.length} flown by your side, ${met.length || 'none'} against it` : `None flown by your side, ${met.length} against it`,
    note: 'Every raid flown by your side, and every raid flown against it, since you last ended a month. Each has its report: who flew, who rose to meet them, and what each lost.',
    top: [{ label: 'PLAN A RAID', act: 'open:raids', ok: up, tip: up ? '' : 'It is not your month.' }],
    sections: [
      { heading: 'FLOWN BY YOUR SIDE', text: flown.length ? '' : 'None yet.', rows: flown.map(row) },
      { heading: 'FLOWN AGAINST YOUR SIDE', text: met.length ? '' : 'None yet.', rows: met.map(row) },
    ],
  };
}

/** One of the air war's pages, or null if the id is not one of them (or the game is not a grand campaign). region is the province selected on the map. */
export function airPage(g: Game, id: string, region = -1): Page | null {
  if (!airWar(g) || !g.human) return null;
  const part = id.split(':');
  if (id === 'airfield') return airfieldPage(g, region);
  if (id === 'raids') return raidsPage(g);
  if (part[0] === 'group') return groupPage(g, Number(part[1]));
  if (part[0] === 'raid' && isKind(part[1])) return targetsPage(g, part[1], part[2] === undefined ? -1 : Number(part[2]));
  if (part[0] === 'plan' && isKind(part[1])) return planPage(g, part[1], Number(part[2]), ids(part[3]));
  if (part[0] === 'scout') return scoutPage(g, Number(part[1]));
  if (part[0] === 'rebase') return rebasePage(g, Number(part[1]));
  if (part[0] === 'aboard') return aboardPage(g, Number(part[1]));
  if (part[0] === 'wings') return wingsPage(g, Number(part[1]));
  if (part[0] === 'search') return searchPage(g, Number(part[1]));
  if (id === 'airreports') return raidReportsPage(g);
  if (part[0] === 'airreport') return raidReportPage(g, Number(part[1]));
  return null;
}

/** Something pressed on one of those pages. Null if the action is not one of the air war's. */
export function airAct(g: Game, action: string, fought?: FoughtAir): AirActResult | null {
  if (!airWar(g) || !g.human) return null;
  const part = action.split(':');
  if (part[0] === 'raid' && isKind(part[1]) && part.length >= 4) {
    const kind = part[1], target = Number(part[2]), list = ids(part[3]);
    const first = g.airFleets.find((f) => f.id === list[0]);
    const from = first ? groupAt(g, first) : null;
    const h0 = g.headlines.length;
    // A raid fought out as an air battle (src/battle/air) is paid with what the battle reports, inside the dice's run.
    const rep = orderRaid(g, kind, target, list, fought);
    if (!rep) return { ok: false, note: previewRaid(g, kind, target, list).why ?? 'That raid cannot be flown now.' };
    // A strike at sea is drawn to the sea it was fought over, a raid on land to its province.
    const p = rep.zone >= 0 ? g.seas[rep.zone] : g.regions[target];
    return {
      ok: true, note: g.headlines.slice(h0).map((h) => h.text).join(' '), open: first?.ship !== undefined ? `wings:${first.ship}` : 'airfield', report: rep.id,
      fly: first && from ? { from: first.base, fromX: from.x, fromY: from.y, x: p.x, y: p.y, nat: first.nat, bombers: rep.att.some((s) => s.bmr > 0), region: rep.zone >= 0 ? -1 : target } : undefined,
      ...(fought ? { after: afterCard(g, rep, fought) } : {}),
    };
  }
  if (part[0] === 'search') {
    const r = orderSearch(g, Number(part[1]), Number(part[2]));
    return { ok: r.ok, note: r.text, open: `group:${part[1]}` };
  }
  if (part[0] === 'job') {
    const id = Number(part[1]), key = part.slice(2).join(':');
    const ok = orderGroupJob(g, id, key);
    return { ok, note: ok ? `${g.airFleets.find((f) => f.id === id)?.name ?? 'The group'} has its new orders.` : 'That order cannot be given now.', open: `group:${id}` };
  }
  if (part[0] === 'scout') {
    const r = orderScout(g, Number(part[1]), Number(part[2]));
    return { ok: r.ok, note: r.text, open: `group:${part[1]}` };
  }
  if (part[0] === 'airbase') {
    const wing = g.airFleets.find((f) => f.id === Number(part[1]) && f.ship !== undefined);
    const grp = g.airFleets.find((f) => f.id === Number(part[1])), to = g.regions[Number(part[2])];
    const sea = !!grp && !!to && overOcean(g, grp, to);
    const ok = orderAirBase(g, Number(part[1]), Number(part[2]));
    const where = to ? placeName(to.name) : 'its new airfield';
    return { ok, note: ok ? (wing ? `The ${wing.name} flies ashore to ${where}.` : sea ? `The group is shipped across the ocean to ${where}.` : `The group moves to ${where}.`) : wing ? 'It cannot fly ashore there now.' : sea ? 'It cannot be shipped there now.' : 'It cannot move there now.', open: `group:${part[1]}` };
  }
  if (part[0] === 'aboard') {
    const id = Number(part[1]), fleet = g.fleets.find((c) => c.id === Number(part[2]));
    const name = g.airFleets.find((f) => f.id === id)?.name ?? 'The wing';
    const ok = orderAboard(g, id, Number(part[2]));
    // Where the fleet had raised a wing of this kind meanwhile the two are one again, and the page is the fleet's.
    return { ok, note: ok ? `The ${name} is back aboard the ${fleet?.name ?? 'fleet'}.` : 'It cannot go aboard there now.', open: ok && !g.airFleets.some((f) => f.id === id) ? `wings:${part[2]}` : `group:${id}` };
  }
  return null;
}

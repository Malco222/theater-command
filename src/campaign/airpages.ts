// The air war as pages: rooms a general panel can draw with no screen of its own (the grand campaign).
//
// A page is a title, a headline, a note, buttons across the top, and sections of rows; each row has lines of
// words and buttons. A button's `act` is either 'open:<page id>', which opens another page, or an action for
// airAct(). The 3D game's bridge hands its page() and act() calls straight to airPage() and airAct().
//
// Pages: 'airfield' (the province selected), 'group:<id>', 'raids', 'raid:<kind>', 'raid:<kind>:<group id>',
// 'plan:<kind>:<target>:<ids joined by dots>', 'scout:<group id>', 'rebase:<group id>', and for the carriers
// 'wings:<fleet id>' (the aircraft one fleet's carriers bring) and 'search:<group id>' (the seas a group could search).
// Actions: 'raid:<kind>:<target>:<ids joined by dots>', 'job:<group id>:<job key>', 'scout:<group id>:<target>',
// 'airbase:<group id>:<region>', 'search:<group id>:<sea>'.
// A carrier wing is an air group: 'group:<its id>' is its page, and the kinds 'fleet' (target: a fleet id) and
// 'harbour' (target: a province) are raids like the rest.

import type { Game } from './types';
import { airWar, carrierRules, groupAt } from './air';
import type { GroupRow, RaidKind, RaidPlan } from './airwar';
import {
  airGroupBases, airGroupJobs, airfieldAt, fleetView, humanUp, myAirGroups, orderAirBase, orderGroupJob, orderRaid, orderScout, orderSearch, previewRaid, raidMenu,
  raidTargetsFor, searchOptionsFor, wingsOfFleet,
} from './turn';

export interface PageButton { label: string; act: string; ok: boolean; tip: string; danger?: boolean }
export interface PageRow { nat: string; title: string; lines: string[]; buttons: PageButton[] }
export interface Page { id: string; title: string; headline: string; note: string; top: PageButton[]; sections: { heading: string; text: string; rows: PageRow[] }[] }
/** What came of something pressed: `open` is the page to show next, `fly` aircraft to draw leaving for their target, `report` the id of the air report just filed. */
export interface AirActResult {
  ok: boolean;
  note: string;
  open?: string;
  /** from is the province the lead group is based in (a carrier wing's port), and fromX, fromY where it took off: its airfield, or its fleet at sea. */
  fly?: { from: number; fromX: number; fromY: number; x: number; y: number; nat: string; bombers: boolean; region: number };
  report?: number;
}

const num = (n: number): string => Math.round(n).toLocaleString('en-US');
const pct = (x: number): string => `${Math.round(x * 100)}%`;
const craft = (x: { ftr: number; bmr: number }): string => [x.ftr >= 1 ? `${num(x.ftr)} fighters` : '', x.bmr >= 1 ? `${num(x.bmr)} bombers` : ''].filter(Boolean).join(', ') || 'no aircraft fit to fly';
const KINDS: RaidKind[] = ['bomb', 'night', 'ground', 'superiority', 'strike', 'fleet', 'harbour', 'recon'];
const isKind = (k: string): k is RaidKind => (KINDS as string[]).includes(k);
const ids = (s: string | undefined): number[] => (s ? s.split('.').map(Number).filter((n) => Number.isFinite(n)) : []);
const BACK: PageButton = { label: 'BACK TO THE AIRFIELD', act: 'open:airfield', ok: true, tip: '' };

function groupLines(x: GroupRow): string[] {
  return [
    `${craft(x)}${x.command ? `, of ${x.command}` : ''}${x.leader ? `. ${x.leader} commands` : ''}. Reaches ${num(x.range)} km. Readiness ${pct(x.ready)}.`,
    `${x.doing}.${x.crowd < 1 ? ` The airfield is crowded: only ${pct(x.crowd)} of it can fly.` : ''}`,
  ];
}

function airfieldPage(g: Game, region: number): Page | null {
  const c = airfieldAt(g, region);
  if (!c) return null;
  const up = humanUp(g);
  const rows: PageRow[] = c.rows.map((x) => ({
    nat: x.nat, title: x.name, lines: groupLines(x),
    buttons: x.mine ? [{ label: 'ORDERS', act: `open:group:${x.id}`, ok: true, tip: 'Its standing orders, the raids it can fly, a reconnaissance, or a move to another airfield.' }] : [],
  }));
  return {
    id: 'airfield', title: `${(c.field?.word ?? 'NO AIRFIELD').toUpperCase()} AT ${c.name.toUpperCase()}`,
    headline: !c.field ? 'No aircraft can be based here' : c.hidden ? 'What flies from here is not known' : rows.length ? `${rows.length} air group${rows.length === 1 ? '' : 's'} based here` : 'No air group is based here',
    note: `${c.note} ${c.build} Aircraft take off from an airfield and come back to it. A fighter group patrols its sky, sticks with a bomber group as escort, or rests; any group can fly one raid a month, planned from its target.`,
    top: [{ label: 'PLAN A RAID', act: 'open:raids', ok: up, tip: 'Pick the kind of raid, then the target: you see the chance of success and the losses to expect, then choose which groups fly it together.' }],
    sections: [{ heading: 'THE AIR GROUPS', text: rows.length || c.hidden ? '' : c.field ? 'Order aircraft from the ARSENAL, or move a group here from another airfield.' : '', rows }],
  };
}

function groupPage(g: Game, id: number): Page | null {
  const x = myAirGroups(g).find((r) => r.id === id);
  if (!x) return null;
  const up = humanUp(g);
  const jobs: PageRow[] = airGroupJobs(g, id).map((j) => ({
    nat: x.nat, title: j.label, lines: [j.forecast, j.why ?? ''].filter(Boolean),
    buttons: [{ label: j.on ? 'ITS ORDERS NOW' : 'GIVE THIS ORDER', act: `job:${id}:${j.key}`, ok: up && !j.on && !j.why, tip: j.why ?? '' }],
  }));
  const menu = raidMenu(g);
  const raids: PageRow[] = menu.filter((k) => k.key !== 'recon').map((k) => {
    const n = raidTargetsFor(g, k.key, id).length;
    const cannot = x.flew ? 'It has flown its raid this month.' : k.why ?? (n ? null : x.kind === 'ftr' && (k.key === 'night') ? 'Fighters do not fly with the night bombers.' : 'Nothing in its reach.');
    return { nat: x.nat, title: k.label, lines: [k.blurb, cannot ?? `${n} target${n === 1 ? '' : 's'} in its reach.`], buttons: [{ label: 'PICK A TARGET', act: `open:raid:${k.key}:${id}`, ok: up && !cannot, tip: cannot ?? '' }] };
  });
  const deck = x.ship >= 0;
  const top: PageButton[] = [
    deck ? { label: 'BACK TO THE FLEET', act: `open:wings:${x.ship}`, ok: true, tip: '' } : BACK,
    { label: 'SCOUT FROM THE AIR', act: `open:scout:${id}`, ok: up && !x.scouted, tip: x.scouted ? 'It has flown its reconnaissance this month.' : 'A few aircraft photograph one province: its armies are seen whole until the month ends.' },
  ];
  if (carrierRules(g)) top.push({ label: 'SEARCH A SEA', act: `open:search:${id}`, ok: up && !x.scouted, tip: x.scouted ? 'It has flown its reconnaissance this month.' : 'A few aircraft quarter one sea for the month: enemy ships there may be reported, or identified. A fleet has to be found before it can be struck.' });
  if (!deck) top.push({ label: 'MOVE TO ANOTHER AIRFIELD', act: `open:rebase:${id}`, ok: up, tip: 'Settling in costs it some readiness for a month.' });
  return {
    id: `group:${id}`, title: x.name.toUpperCase(), headline: `${craft(x)} ${deck ? 'aboard the' : 'at'} ${x.baseName}`,
    note: groupLines(x).join(' '),
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
    top: [BACK],
    sections: [{ heading: 'KINDS OF RAID', text: '', rows: menu.map((k) => ({ nat: '', title: k.label, lines: [k.blurb, k.why ?? `${k.targets} target${k.targets === 1 ? '' : 's'} in reach this month.`], buttons: [{ label: 'PICK A TARGET', act: `open:raid:${k.key}`, ok: up && !k.why, tip: k.why ?? '' }] })) }],
  };
}

const lossLine = (p: RaidPlan): string => `Chance of success ${pct(p.success)}: ${p.verdict.toLowerCase()}. Likely losses: ${[p.bombers ? `${num(p.bomberLossN)} of ${num(p.bombers)} bombers (${pct(p.bomberLoss)})` : '', p.fighters ? `${num(p.fighterLossN)} of ${num(p.fighters)} fighters` : ''].filter(Boolean).join(', ') || 'none'}${p.foeLossN ? `, for about ${num(p.foeLossN)} enemy fighters` : ''}.`;

function targetsPage(g: Game, kind: RaidKind, group: number): Page | null {
  const k = raidMenu(g).find((x) => x.key === kind);
  if (!k) return null;
  const up = humanUp(g);
  const lead = group >= 0 ? [group] : undefined;
  const all = raidTargetsFor(g, kind, group);
  const rows: PageRow[] = all.slice(0, 12).map((t) => {
    const p = previewRaid(g, kind, t.id, lead);
    const lines = [`${num(t.km)} km. ${t.what ? `${t.what[0].toUpperCase()}${t.what.slice(1)}.` : ''} ${t.groups} of your groups could fly it.`];
    if (p.why) lines.push(p.why);
    else if (kind === 'recon') lines.push(`${p.verdict}. ${p.effects[0]}`);
    else lines.push(`With ${p.seats.filter((s) => s.picked).map((s) => s.name).join(', ')}: ${lossLine(p)}`, ...p.effects.slice(0, 1));
    const act = kind === 'recon' ? `scout:${p.picked[0]}:${t.id}` : `open:plan:${kind}:${t.id}:${p.picked.join('.')}`;
    return { nat: t.holder, title: t.name, lines, buttons: [{ label: kind === 'recon' ? 'SEND THE FLIGHT' : 'PLAN THIS RAID', act, ok: up && !p.why, tip: p.why ?? (kind === 'recon' ? '' : 'Choose which groups fly it, and see what meeting up is worth, before you give the order.') }] };
  });
  return {
    id: group >= 0 ? `raid:${kind}:${group}` : `raid:${kind}`, title: k.label.toUpperCase(),
    headline: all.length ? `${all.length} target${all.length === 1 ? '' : 's'} in reach, nearest first` : 'No target is in reach',
    note: k.blurb, top: [{ label: 'BACK', act: group >= 0 ? `open:group:${group}` : 'open:raids', ok: true, tip: '' }],
    sections: [{ heading: 'TARGETS', text: all.length ? '' : kind === 'fleet' ? 'No enemy fleet at sea has been found within reach. Search a sea from a group\'s page, or send a fleet to look: a fleet has to be found before it can be struck.' : kind === 'harbour' ? 'No enemy warships lie in a harbour within reach.' : 'Move a group to an airfield nearer the enemy, or wait until one that has flown is back.', rows }],
  };
}

function planPage(g: Game, kind: RaidKind, target: number, picked: number[]): Page | null {
  if (kind === 'recon') return null;
  const p = previewRaid(g, kind, target, picked);
  const up = humanUp(g);
  const link = (list: number[]): string => `open:plan:${kind}:${target}:${list.join('.')}`;
  const seat = (s: RaidPlan['seats'][number]): PageRow => ({
    nat: g.human ?? '', title: s.name,
    lines: [`${num(s.aircraft)} ${s.kind === 'ftr' ? 'fighters' : s.kind === 'bmr' ? 'bombers' : 'aircraft'} ${s.ship >= 0 ? 'aboard the' : 'at'} ${s.baseName}. ${s.why ?? `${s.note[0].toUpperCase()}${s.note.slice(1)}.`}${s.locked ? ' It goes because it is ordered to stick with its bombers.' : ''}`],
    buttons: s.why ? [] : s.picked
      ? [{ label: 'LEAVE IT OUT', act: link(p.picked.filter((id) => id !== s.id)), ok: !s.locked, tip: s.locked ? 'Change its standing order to take it off escort.' : '' }]
      : [{ label: 'ADD IT TO THE RAID', act: link([...p.picked, s.id]), ok: true, tip: s.kind === 'ftr' ? 'It goes as escort, and leaves its patrol for the rest of the month.' : 'Bombers that meet up lose fewer of their number.' }],
  });
  const going = p.seats.filter((s) => s.picked), could = p.seats.filter((s) => !s.picked && !s.why), cannot = p.seats.filter((s) => s.why);
  return {
    id: `plan:${kind}:${target}:${p.picked.join('.')}`, title: p.title.toUpperCase(),
    headline: p.why ?? `${p.verdict}: ${pct(p.success)}`,
    note: p.why ? '' : [lossLine(p), ...p.effects, p.massed, p.offPatrol].filter(Boolean).join(' '),
    top: [
      { label: 'FLY THE RAID', act: `raid:${kind}:${target}:${p.picked.join('.')}`, ok: up && !p.why, tip: p.why ?? 'Flown at once. Every group that goes is away for the rest of the month.' },
      { label: 'ANOTHER TARGET', act: `open:raid:${kind}`, ok: true, tip: '' },
    ],
    sections: [
      { heading: 'FLYING THE RAID', text: going.length ? '' : 'No group is picked.', rows: going.map(seat) },
      { heading: 'COULD JOIN IT', text: could.length ? '' : 'No other group can reach it this month.', rows: could.map(seat) },
      { heading: 'RISING TO MEET THEM', text: p.meets.length ? '' : 'No enemy fighters are in reach.', rows: p.meets.map((m) => ({ nat: m.nat, title: m.name, lines: [`${num(m.ftr)} fighters ${m.afloat ? 'aboard the' : 'at'} ${m.baseName}: about ${num(m.able)} can meet the raid.`], buttons: [] })) },
      { heading: 'THE BATTLE TICKET', text: p.lines.map((l) => `${l.label}: ${l.value}${l.effect ? ` (${l.effect})` : ''}`).join('\n'), rows: [] },
      { heading: 'CANNOT GO', text: '', rows: cannot.slice(0, 8).map(seat) },
    ].filter((s) => s.rows.length || s.text),
  };
}

function scoutPage(g: Game, id: number): Page | null {
  const x = myAirGroups(g).find((r) => r.id === id);
  if (!x) return null;
  const up = humanUp(g);
  const rows: PageRow[] = raidTargetsFor(g, 'recon', id).slice(0, 14).map((t) => ({ t, p: previewRaid(g, 'recon', t.id, [id]) })).map(({ t, p }) => ({
    nat: t.holder, title: t.name, lines: [`${num(t.km)} km. ${t.what}. ${p.verdict}.`], buttons: [{ label: 'SEND THE FLIGHT', act: `scout:${id}:${t.id}`, ok: up, tip: p.effects[0] ?? '' }],
  }));
  return {
    id: `scout:${id}`, title: `A RECONNAISSANCE  ·  ${x.name.toUpperCase()}`, headline: rows.length ? `${rows.length} province${rows.length === 1 ? '' : 's'} it could photograph, nearest first` : x.scouted ? 'It has flown its reconnaissance this month' : 'Nothing in its reach is hidden',
    note: 'A few aircraft photograph one province and the ground beside it: every army there is seen whole until the month ends. One flight a month for each group, beside its raid, and it keeps its patrol.',
    top: [{ label: 'BACK', act: `open:group:${id}`, ok: true, tip: '' }], sections: [{ heading: 'PROVINCES', text: '', rows }],
  };
}

function rebasePage(g: Game, id: number): Page | null {
  const x = myAirGroups(g).find((r) => r.id === id);
  if (!x) return null;
  const up = humanUp(g);
  const rows: PageRow[] = airGroupBases(g, id).slice(0, 14).map((b) => ({
    nat: g.regions[b.region].ctrl, title: b.name,
    lines: [`${b.word}, ${num(b.km)} km away: serves ${num(b.holds)} aircraft, room for ${num(b.free)} more.${b.free < x.aircraft ? ' It would be crowded: not all of the group could fly.' : ''}${b.why ? ` ${b.why}` : ''}`],
    buttons: [{ label: 'MOVE HERE', act: `airbase:${id}:${b.region}`, ok: up && !b.why, tip: b.why ?? 'Settling in costs it some readiness for a month.' }],
  }));
  return {
    id: `rebase:${id}`, title: `A NEW AIRFIELD  ·  ${x.name.toUpperCase()}`, headline: `${rows.length} airfield${rows.length === 1 ? '' : 's'} it could move to, nearest first`,
    note: `${x.name} is based at ${x.baseName}. Aircraft can be based only where there is an airfield: engineers lay new ones out, and the treasury enlarges them. A group reaches farther into enemy country from a field nearer the front.`,
    top: [{ label: 'BACK', act: `open:group:${id}`, ok: true, tip: '' }], sections: [{ heading: 'AIRFIELDS', text: '', rows }],
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
  return {
    id: `wings:${fleet}`, title: `THE AIRCRAFT OF THE ${c.name.toUpperCase()}`,
    headline: carriers ? `${carriers} carrier${carriers === 1 ? '' : 's'}: decks ${pct(c.decks)} full` : 'This fleet has no carriers',
    note: 'A carrier is a floating airfield: its fighters and strike aircraft go where the fleet goes. At sea they reach every coast of the sea the fleet is in, the ground inland of it, and the seas next to it. The fighters cover the fleet; the strike aircraft go for a fleet your side has found, ships in a harbour, or a target ashore. New aircraft come aboard only in harbour, and a carrier that is sunk takes most of its aircraft with it.',
    top: [{ label: 'PLAN A STRIKE', act: 'open:raids', ok: up && mine, tip: 'Pick the kind of raid, then the target: you see the chance of success and the losses to expect before you give the order.' }],
    sections: [{ heading: 'THE WINGS', text: rows.length ? '' : 'Carriers are built in the ARSENAL, in a port with a great yard.', rows }],
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
  if (part[0] === 'wings') return wingsPage(g, Number(part[1]));
  if (part[0] === 'search') return searchPage(g, Number(part[1]));
  return null;
}

/** Something pressed on one of those pages. Null if the action is not one of the air war's. */
export function airAct(g: Game, action: string): AirActResult | null {
  if (!airWar(g) || !g.human) return null;
  const part = action.split(':');
  if (part[0] === 'raid' && isKind(part[1]) && part.length >= 4) {
    const kind = part[1], target = Number(part[2]), list = ids(part[3]);
    const first = g.airFleets.find((f) => f.id === list[0]);
    const from = first ? groupAt(g, first) : null;
    const h0 = g.headlines.length;
    const rep = orderRaid(g, kind, target, list);
    if (!rep) return { ok: false, note: previewRaid(g, kind, target, list).why ?? 'That raid cannot be flown now.' };
    // A strike at sea is drawn to the sea it was fought over, a raid on land to its province.
    const p = rep.zone >= 0 ? g.seas[rep.zone] : g.regions[target];
    return {
      ok: true, note: g.headlines.slice(h0).map((h) => h.text).join(' '), open: first?.ship !== undefined ? `wings:${first.ship}` : 'airfield', report: rep.id,
      fly: first && from ? { from: first.base, fromX: from.x, fromY: from.y, x: p.x, y: p.y, nat: first.nat, bombers: rep.att.some((s) => s.bmr > 0), region: rep.zone >= 0 ? -1 : target } : undefined,
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
    const ok = orderAirBase(g, Number(part[1]), Number(part[2]));
    return { ok, note: ok ? `The group moves to ${g.regions[Number(part[2])]?.name ?? 'its new airfield'}.` : 'It cannot move there now.', open: `group:${part[1]}` };
  }
  return null;
}

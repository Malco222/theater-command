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
// 'airbase:<group id>:<region>', 'search:<group id>:<sea>', 'aboard:<group id>:<fleet id>'.
// A carrier wing is sent ashore by 'airbase' (its 'rebase' page lists the fields on the shore where its fleet is),
// and a wing ashore goes back by 'aboard' (its page 'aboard:<group id>' lists the fleets with deck room for it).
// A carrier wing is an air group: 'group:<its id>' is its page, and the kinds 'fleet' (target: a fleet id) and
// 'harbour' (target: a province) are raids like the rest.

import type { Game } from './types';
import { SHIP_AIR_TIP, airWar, carrierRules, closeRules, groupAt, overOcean } from './air';
import type { GroupRow, RaidKind, RaidPlan, RaidTarget } from './airwar';
import {
  airFerryRoutes, airGroupBases, airGroupDecks, airGroupJobs, airfieldAt, fleetView, humanUp, myAirGroups, orderAboard, orderAirBase, orderFerryOff, orderFerryTo, orderGroupJob, orderRaid,
  orderScout, orderSearch, previewRaid, raidMenu, raidTargetsFor, searchOptionsFor, wingsOfFleet,
} from './turn';
import { FERRY, ferryOn } from './ferry';
import { afterCard, type AfterCard, type FoughtAir } from './airfought';
import { band, raidFogOn } from './airfog';

export interface PageButton { label: string; act: string; ok: boolean; tip: string; danger?: boolean }
/** (Release 2) An air group's row may carry its role word, its short form and its earned name as its arm sets it (airwar.ts airWords). */
export interface PageRow { nat: string; title: string; lines: string[]; buttons: PageButton[]; role?: string; short?: string; called?: string }
export interface Page { id: string; title: string; headline: string; note: string; top: PageButton[]; sections: { heading: string; text: string; rows: PageRow[] }[] }
/** What came of something pressed: `open` is the page to show next, `fly` aircraft to draw leaving for their target, `report` the id of the air report just filed. */
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
/** Fog on raids (airfog.ts, its switch on): a reconnaissance flight's sight lasts the month flown and the next. */
const SEEN_TWO = 'A few aircraft photograph one province and the ground beside it: it is seen whole this month and next month.';
const BACK: PageButton = { label: 'BACK TO THE AIRFIELD', act: 'open:airfield', ok: true, tip: '' };

/** A group's lines. On its airfield's own page the crowding is said once, in the page's head, so its rows leave it out (the UI director, build 1668). */
export function groupLines(x: GroupRow, crowd = true): string[] {
  return [
    `${craft(x)}${x.command ? `, of ${x.command}` : ''}${x.leader ? `. ${x.leader} commands` : ''}. Reaches ${num(x.range)} km. Readiness ${pct(x.ready)}.${x.pilots ? ` ${x.pilots}` : ''}${x.joined ? ` ${x.joined}` : ''}`,
    `${x.doing}.${crowd && x.crowd < 1 ? ` The airfield is crowded: only ${pct(x.crowd)} of it can fly.` : ''}`,
  ];
}

function airfieldPage(g: Game, region: number): Page | null {
  const c = airfieldAt(g, region);
  if (!c) return null;
  const up = humanUp(g);
  const rows: PageRow[] = c.rows.map((x) => ({
    nat: x.nat, title: x.name, lines: groupLines(x, false),
    ...(x.role ? { role: x.role } : {}), ...(x.short ? { short: x.short } : {}), ...(x.called ? { called: x.called } : {}),
    buttons: x.mine ? [{ label: 'ORDERS', act: `open:group:${x.id}`, ok: true, tip: 'Its standing orders, the raids it can fly, a reconnaissance, or a move to another airfield.' }] : [],
  }));
  return {
    id: 'airfield', title: `${(c.field?.word ?? 'NO AIRFIELD').toUpperCase()} AT ${c.name.toUpperCase()}`,
    headline: !c.field ? 'No aircraft can be based here' : c.hidden ? 'What flies from here is not known' : rows.length ? `${rows.length} air group${rows.length === 1 ? '' : 's'} based here` : 'No air group is based here',
    note: `${c.note} ${c.build} Aircraft take off from an airfield and come back to it. A fighter group patrols its sky, sticks with a bomber group as escort, ${closeRules(g) ? 'flies ground support for the armies, ' : ''}or rests; any group can fly one raid a month, planned from its target.`,
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
    // (A dim button says why: the order it has now, a reason from the rules, or that it is not the player's month.)
    buttons: [{ label: j.on ? 'ITS ORDERS NOW' : 'GIVE THIS ORDER', act: `job:${id}:${j.key}`, ok: up && !j.on && !j.why, tip: j.on ? 'This is its standing order now. Give it one of the others to change it.' : j.why ?? (up ? '' : 'It is not your month.') }],
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
    { label: 'SCOUT FROM THE AIR', act: `open:scout:${id}`, ok: up && !x.scouted, tip: x.scouted ? 'It has flown its reconnaissance this month.' : raidFogOn(g) ? SEEN_TWO : 'A few aircraft photograph one province: its armies are seen whole until the month ends.' },
  ];
  if (carrierRules(g)) top.push({ label: 'SEARCH A SEA', act: `open:search:${id}`, ok: up && !x.scouted, tip: x.scouted ? 'It has flown its reconnaissance this month.' : 'A few aircraft quarter one sea for the month: enemy ships there may be reported, or identified. A fleet has to be found before it can be struck.' });
  if (!deck) top.push({ label: 'MOVE TO ANOTHER AIRFIELD', act: `open:rebase:${id}`, ok: up, tip: 'Settling in costs it some readiness for a month.' });
  // On its way to another field by stages (release 2, ferry.ts): it can be called off, and stays where it has got to.
  if (x.way) top.push({ label: 'CALL IT OFF', act: `ferryoff:${id}`, ok: up, tip: `It stays at ${x.baseName}, the field it has reached, and is back in the fight.` });
  // A carrier wing's own order (ruling 175(b)): ashore from its fleet, or back aboard. With one place to go the button gives the order; with several it opens the list.
  const mv = x.wingMove;
  if (mv?.order === 'ashore') {
    const to = airGroupBases(g, id).filter((b) => !b.why);
    top.push({ label: 'FLY ASHORE', act: to.length === 1 ? `airbase:${id}:${to[0].region}` : `open:rebase:${id}`, ok: up && !mv.why, tip: mv.why ?? `${to.length === 1 ? `To the airfield at ${to[0].name}. ` : ''}${mv.price}` });
  }
  if (mv?.order === 'aboard') {
    const to = airGroupDecks(g, id).filter((d) => !d.why);
    top.push({ label: 'BACK ABOARD', act: to.length === 1 ? `aboard:${id}:${to[0].fleet}` : `open:aboard:${id}`, ok: up && !mv.why, tip: mv.why ?? `${to.length === 1 ? `To the ${to[0].name}, ${to[0].where}. ` : ''}${mv.price}` });
  }
  return {
    id: `group:${id}`, title: x.name.toUpperCase(), headline: `${craft(x)} ${deck ? 'aboard the' : x.ashore >= 0 ? 'ashore at' : 'at'} ${x.baseName}`,
    note: `${x.schools ? `${x.schools} ` : ''}${groupLines(x).join(' ')}${x.ashore >= 0 ? ` A carrier wing${x.fleetName ? ` of the ${x.fleetName}` : ''}, ordered ashore: while it is here its carriers fight as empty carriers.` : ''}`,
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

const lossLine = (p: RaidPlan): string => (p.fog && !p.fog.live ? fogLossLine(p, p.fog) : `Chance of success ${pct(p.success)}: ${p.verdict.toLowerCase()}. Likely losses: ${[p.bombers ? `${num(p.bomberLossN)} of ${num(p.bombers)} bombers (${pct(p.bomberLoss)})` : '', p.fighters ? `${num(p.fighterLossN)} of ${num(p.fighters)} fighters` : ''].filter(Boolean).join(', ') || 'none'}${p.foeLossN ? `, for about ${num(p.foeLossN)} enemy fighters` : ''}.`);

/** Fog on raids (airfog.ts, its switch on): out of sight the same line gives each figure as a band, low end to high. */
const fogLossLine = (p: RaidPlan, f: NonNullable<RaidPlan['fog']>): string =>
  `Chance of success ${band(f.success[0], f.success[1])}, not scouted. Likely losses: ${[p.bombers ? `${band(f.bomberLossN[0], f.bomberLossN[1], false)} of ${num(p.bombers)} bombers (${band(f.loss[0], f.loss[1])})` : '', p.fighters ? `${band(f.fighterLossN[0], f.fighterLossN[1], false)} of ${num(p.fighters)} fighters` : ''].filter(Boolean).join(', ') || 'none'}.`;

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
  const groups: { owner: string; holder: string; list: RaidTarget[] }[] = [];
  if (all.every((t) => !t.sea && g.regions[t.id])) {
    for (const t of all) {
      const owner = g.regions[t.id].owner || t.holder;
      const at = groups.find((x) => x.owner === owner && x.holder === t.holder);
      if (at) at.list.push(t); else groups.push({ owner, holder: t.holder, list: [t] });
    }
  }
  if (groups.length < 2) return [{ heading: 'TARGETS', text: '', rows: all.map(row) }];
  // (the sort is stable: among the enemy's own countries, and among the lands he holds, the nearest stays first)
  if (kind === 'bomb' || kind === 'night') groups.sort((a, b) => Number(b.owner === b.holder) - Number(a.owner === a.holder));
  const name = (n: string): string => (g.nations[n]?.name ?? n).toUpperCase();
  return groups.map((x) => ({
    heading: `${x.owner === x.holder ? name(x.owner) : `${name(x.owner)}, HELD BY ${name(x.holder)}`}  \u00B7  ${x.list.length} ${word}${x.list.length === 1 ? '' : 'S'}`,
    text: '', rows: x.list.map(row),
  }));
}

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
  const sections = targetSections(g, kind, all, row, none);
  return {
    id: group >= 0 ? `raid:${kind}:${group}` : `raid:${kind}`, title: k.label.toUpperCase(),
    headline: all.length ? `${all.length} target${all.length === 1 ? '' : 's'} in reach, ${sections.length > 1 ? 'by country' : 'nearest first'}` : 'No target is in reach',
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
    lines: [`${num(s.aircraft)} ${s.kind === 'ftr' ? 'fighters' : s.kind === 'bmr' ? 'bombers' : 'aircraft'} ${s.ship >= 0 ? 'aboard the' : 'at'} ${s.baseName}. ${s.why ?? `${s.note[0].toUpperCase()}${s.note.slice(1)}.`}${s.locked ? ' It goes because it is ordered to stick with its bombers.' : ''}`],
    buttons: s.why ? [] : s.picked
      ? [{ label: 'LEAVE IT OUT', act: link(p.picked.filter((id) => id !== s.id)), ok: !s.locked, tip: s.locked ? 'Change its standing order to take it off escort.' : '' }]
      : [{ label: 'ADD IT TO THE RAID', act: link([...p.picked, s.id]), ok: true, tip: s.kind === 'ftr' ? 'It goes as escort, and leaves its patrol for the rest of the month.' : 'Bombers that meet up lose fewer of their number.' }],
  });
  const going = p.seats.filter((s) => s.picked), could = p.seats.filter((s) => !s.picked && !s.why), cannot = p.seats.filter((s) => s.why);
  return {
    id: `plan:${kind}:${target}:${p.picked.join('.')}`, title: p.title.toUpperCase(),
    // (Fog on raids: out of sight the forecast is a band, so the headline gives the band its note gives, not the high end alone.)
    headline: p.why ?? (p.fog && !p.fog.live ? `Chance of success ${band(p.fog.success[0], p.fog.success[1])}: not scouted` : `${p.verdict}: ${pct(p.success)}`),
    note: p.why ? '' : [lossLine(p), ...p.effects, p.massed, p.offPatrol].filter(Boolean).join(' '),
    top: [
      { label: 'FLY THE RAID', act: `raid:${kind}:${target}:${p.picked.join('.')}`, ok: up && !p.why, tip: p.why ?? 'Flown at once. Every group that goes is away for the rest of the month.' },
      { label: 'ANOTHER TARGET', act: `open:raid:${kind}`, ok: true, tip: '' },
    ],
    sections: [
      { heading: 'FLYING THE RAID', text: going.length ? '' : 'No group is picked.', rows: going.map(seat) },
      // (Release 2, a raid on troops with the strafers' rule on, the UI director's words of 15:04Z: at the foot of the
      // raid's forces, the groups likely to go down, a row each under the muted line; or the line for none. No colour:
      // the rows carry no nation, so no flag or picture is set on them.)
      ...(p.strafe && !p.why ? [{ heading: p.strafe.heading, text: p.strafe.names.length ? p.strafe.note : p.strafe.none, rows: p.strafe.names.map((n): PageRow => ({ nat: '', title: n, lines: [], buttons: [] })) }] : []),
      { heading: 'COULD JOIN IT', text: could.length ? '' : 'No other group can reach it this month.', rows: could.map(seat) },
      // (Fog on raids: what the side knows of the target's defences, and the forecast's band.)
      ...(p.fog && !p.why ? [{ heading: 'WHAT IS KNOWN OF ITS DEFENCES', text: [p.fog.fighters, p.fog.guns, p.fog.damage, p.fog.band].filter(Boolean).join('\n'), rows: [] as PageRow[] }] : []),
      { heading: 'RISING TO MEET THEM', text: p.fog && !p.fog.live ? (p.meets.length ? 'Only the groups whose airfields are in sight are named: more may come.' : 'No enemy fighter is known for sure to be in reach. Scout it to see what is there.') : p.meets.length ? '' : 'No enemy fighters are in reach.', rows: p.meets.map((m) => ({ nat: m.nat, title: m.name, lines: [`${num(m.ftr)} fighters ${m.afloat ? 'aboard the' : 'at'} ${m.baseName}: about ${num(m.able)} can meet the raid.`], buttons: [] })) },
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
    note: `${raidFogOn(g) ? `${SEEN_TWO} Its fighters, its guns and the damage to it are seen too, and remembered after.` : 'A few aircraft photograph one province and the ground beside it: every army there is seen whole until the month ends.'} One flight a month for each group, beside its raid, and it keeps its patrol.`,
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
  // Across an ocean a group goes by sea (ruling 133(b)): the fields it could be shipped to now first, then the nearest.
  const over: PageRow[] = all.filter((b) => b.sea).sort((a, b) => (a.why ? 1 : 0) - (b.why ? 1 : 0) || a.km - b.km).slice(0, 8).map((b) => ({
    nat: g.regions[b.region].ctrl, title: b.name,
    lines: [`${b.word}, ${num(b.km)} km away across the ocean: serves ${num(b.holds)} aircraft, room for ${num(b.free)} more.${b.why ? ` ${b.why}` : ''}`],
    buttons: [{ label: 'SHIP IT HERE', act: `airbase:${id}:${b.region}`, ok: up && !b.why, tip: b.why ?? SHIP_AIR_TIP }],
  }));
  const sections = [{ heading: 'AIRFIELDS', text: '', rows }];
  // With the switch 'ferry' (release 2, ferry.ts): every other airfield of his side's or his allies', with the way there
  // by stages or crated by sea, the turns it takes and the enemy fighters known on the way. The fields across the ocean
  // are among them, so the old section by sea is not shown beside it. Off, the page is as it was.
  const far = ferryOn(g) ? farRows(g, x, rows.map((r) => r.title)) : [];
  if (far.length) sections.push({ heading: 'FARTHER, BY STAGES', text: `It flies from field to field, up to ${FERRY.perMonth} hops a month, and is out of the fight until it gets there. A closed sky holds it where it is. Where enemy fighters are known within reach of the way it can be caught, and fights.`, rows: far });
  else if (over.length) sections.push({ heading: 'ACROSS THE OCEAN, BY SEA', text: SHIP_AIR_TIP, rows: over });
  return {
    id: `rebase:${id}`, title: `A NEW AIRFIELD  ·  ${x.name.toUpperCase()}`, headline: `${rows.length} airfield${rows.length === 1 ? '' : 's'} it could move to, nearest first`,
    note: `${x.name} is based at ${x.baseName}. Aircraft can be based only where there is an airfield: engineers lay new ones out, and the treasury enlarges them. A group reaches farther into enemy country from a field nearer the front.`,
    top: [{ label: 'BACK', act: `open:group:${id}`, ok: true, tip: '' }], sections,
  };
}

/** The fields a group could be sent to by stages, those it can reach soonest first (ferry.ts): every one but those it can move to this month. */
function farRows(g: Game, x: GroupRow, direct: string[]): PageRow[] {
  const up = humanUp(g);
  return airFerryRoutes(g, x.id).filter((r) => !direct.includes(r.name))
    .sort((a, b) => (a.how === 'none' ? 1 : 0) - (b.how === 'none' ? 1 : 0) || a.turns - b.turns || a.hops.length - b.hops.length || a.name.localeCompare(b.name))
    .map((r) => ({
      nat: g.regions[r.to].ctrl, title: r.name,
      lines: [r.words, ...(r.hops.length > 1 ? [`The way: ${r.hops.map((h) => `${g.regions[h.to].name} (${num(h.km)} km)`).join(', ')}.`] : r.hops.length ? [`One hop of ${num(r.hops[0].km)} km.`] : [])],
      buttons: [{ label: 'SEND IT THERE', act: `ferry:${x.id}:${r.to}`, ok: up && r.how !== 'none', tip: r.why ?? (r.how === 'sea' ? SHIP_AIR_TIP : `It sets off at once. Settling in at ${r.name} costs it some readiness for a month.`) }],
    }));
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
    note: `${x.name} is ashore at ${x.baseName}. It goes back aboard a fleet of its navy that has deck room standing empty, when that fleet lies in the harbour here or in the sea off this coast.`,
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
    nat: c.nat, title: w.name, lines: [`${num(w.aircraft)} aircraft ashore at ${w.at}.`],
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
    const where = to?.name ?? 'its new airfield';
    return { ok, note: ok ? (wing ? `The ${wing.name} flies ashore to ${where}.` : sea ? `The group is shipped across the ocean to ${where}.` : `The group moves to ${where}.`) : wing ? 'It cannot fly ashore there now.' : sea ? 'It cannot be shipped there now.' : 'It cannot move there now.', open: `group:${part[1]}` };
  }
  // By stages to a far field, and calling that off (release 2, ferry.ts; never offered with the switch off).
  if (part[0] === 'ferry' && ferryOn(g)) {
    const route = orderFerryTo(g, Number(part[1]), Number(part[2]));
    const f = g.airFleets.find((y) => y.id === Number(part[1]));
    const where = g.regions[Number(part[2])]?.name ?? 'that airfield';
    return { ok: !!route, note: route ? (f?.way ? `${f.name} sets off for ${where}: ${route.words}` : `${f?.name ?? 'The group'} reaches ${where}.`) : 'There is no way there now.', open: `group:${part[1]}` };
  }
  if (part[0] === 'ferryoff' && ferryOn(g)) {
    const ok = orderFerryOff(g, Number(part[1]));
    return { ok, note: ok ? 'Called off: it stays at the field it has reached.' : 'It is not on its way anywhere.', open: `group:${part[1]}` };
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

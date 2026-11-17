// TRIAL (far-side seats rig, never for main). Proves that a campaign can be played from a seat (ruling 209(e)):
// "it starts, an order is given, a year passes and nothing breaks".
//
//   node dist/trial/seat-play.mjs SEAT [SEED] [MONTHS] [LEVEL]     the grand campaign from September 1939 with SEAT
//                                                                  as the player, who gives one order and then passes
//   node dist/trial/seat-play.mjs watch [SEED] [MONTHS] [CODES]    nobody playing: what the computer does with each
//                                                                  country (CODES: comma list; default every seat)
//   node dist/trial/seat-play.mjs list                             the seats this build knows
//
// Build first: node tools/trial/build.mjs (see tools/trial/README.md). The month is advanced by the Playtester's
// own harness (lib.ts: start, advance, check), so a seat is opened and played as every seat has been since Version 53.
// Exit code 0 when the verdict is PASS, 1 otherwise.
import { T, advance, armySize, check, date, ledger, loadGame, saveGame, start, type Game } from '@pt/lib';
import { offers } from '@tc/campaign/arsenal';
import { friendsOf } from '@tc/campaign/diplomacy';
import { allied, bloc } from '@tc/campaign/game';
import { formOf } from '@tc/campaign/government';
import { founders, partName, partOf, small } from '@tc/campaign/league';
import { whyNotShip } from '@tc/campaign/navy';
import { eliteOf, traitsOf } from '@tc/campaign/traits';
import { TRIAL_FAR, WORLD, WORLD_FIRST } from '@tc/campaign/world';

declare const TRIAL_SEATS: { id: string; from: string; placeholder: boolean }[];
declare const TRIAL_FILES: string[];
declare const TRIAL_WARNINGS: number;

const out = (s = ''): void => { console.log(s); };
const money = (g: Game, nat: string): { scale: number; treasury: number; gross: number; upkeep: number; net: number } => {
  const L = ledger(g, nat);
  return { scale: L.scale, treasury: L.treasury * L.scale, gross: L.gross * L.scale, upkeep: L.upkeepTotal * L.scale, net: L.net * L.scale };
};
const r1 = (x: number): string => (Math.round(x * 10) / 10).toString();
const wars = (g: Game, nat: string): string => g.nations[nat].atWar.filter((e) => g.nations[e]?.alive).join(',') || 'none';
const corpsOf = (g: Game, nat: string) => g.corps.filter((c) => c.nat === nat && !c.dead);
const heldBy = (g: Game, nat: string) => g.regions.filter((r) => r.ctrl === nat);

/** What a country looks like on a given day, as one line. */
function monthLine(g: Game, nat: string): string {
  const n = g.nations[nat], m = money(g, nat), cs = corpsOf(g, nat);
  const divs = cs.reduce((s, c) => s + c.divs * c.str, 0);
  const cap = n.capital >= 0 ? g.regions[n.capital] : null;
  return `${date(g.turn).padEnd(15)} ${n.alive ? 'alive' : 'FALLEN'}  provinces ${String(heldBy(g, nat).length).padStart(3)}  corps ${String(cs.length).padStart(2)} (${r1(divs)} divs)  treasury ${r1(m.treasury).padStart(7)}  net ${r1(m.net).padStart(6)}/month  ${bloc(n) ?? n.align}  at war with: ${wars(g, nat)}${cap && cap.ctrl !== nat ? `  CAPITAL ${cap.name} held by ${cap.ctrl}` : ''}`;
}

/** The first day's printout for a seat. */
function firstDay(g: Game, me: string): string[] {
  const n = g.nations[me], L: string[] = [];
  const owned = g.regions.filter((r) => r.owner === me), held = heldBy(g, me);
  const cap = n.capital >= 0 ? g.regions[n.capital] : null;
  L.push(`${n.name} (${me}), ${date(g.turn)}; part of the world: ${partName(me)}; government: ${formOf(me)}; stands: ${bloc(n) ?? n.align}; quality ${n.quality}, air ${n.air}, resilience ${n.resilience}, sea lift ${n.sealift}`);
  L.push(`  capital: ${cap ? `${cap.name} (province ${cap.id}, held by ${cap.ctrl})` : 'NONE'}`);
  L.push(`  provinces: holds ${held.length}, owns ${owned.length}${held.length !== owned.length || held.some((r) => r.owner !== me) ? ` (holds of its own ${held.filter((r) => r.owner === me).length}; of others' ${held.filter((r) => r.owner !== me).length}; its own held by others: ${owned.filter((r) => r.ctrl !== me).map((r) => `${r.name} by ${r.ctrl}`).join(', ') || 'none'})` : ''}; at home ${owned.filter((r) => !r.overseas).length}, colonies ${owned.filter((r) => r.overseas).length}; ports ${held.filter((r) => r.port).length}; factories ${held.reduce((s, r) => s + r.fac, 0)}; victory points ${held.reduce((s, r) => s + r.vp, 0)}; oilfields ${held.filter((r) => r.oil).length}`);
  const seas = new Set<number>(); for (const r of held) for (const s of r.seas) seas.add(s);
  L.push(`  seas it stands on: ${[...seas].map((s) => g.seas[s]?.name ?? `?${s}`).join(', ') || 'none (landlocked)'}`);
  const nbs = new Set<string>(); for (const r of held) for (const j of r.nb) if (g.regions[j].ctrl !== me) nbs.add(g.regions[j].ctrl);
  L.push(`  neighbours by land: ${[...nbs].join(', ') || 'none'}`);
  const cut = held.filter((r) => cap && g.capDist[me] && g.capDist[me][r.id] >= 999);
  if (cut.length) L.push(`  no way by land from the capital to: ${cut.map((r) => r.name).join(', ')}`);
  const cs = corpsOf(g, me);
  L.push(`  army: ${cs.length} corps, ${r1(cs.reduce((s, c) => s + c.divs, 0))} divisions on paper, ${r1(cs.reduce((s, c) => s + c.divs * c.str, 0))} at strength`);
  for (const c of cs) L.push(`    ${c.name} [${c.kind}] ${c.divs} div at ${Math.round(c.str * 100)}% in ${g.regions[c.reg].name}${c.town !== undefined ? ' (in the town)' : ''}`);
  const fl = g.fleets.filter((f) => f.nat === me);
  L.push(`  fleets: ${fl.length ? fl.map((f) => `${f.name}${f.admiral ? ` under ${f.admiral.name}` : ' (no admiral named)'} at ${f.port} (${Object.entries(f.ships).filter(([, v]) => v > 0).map(([k, v]) => `${v} ${k}`).join(', ') || 'no ships'}) in ${g.seas[f.zone]?.name ?? '?'}`).join('; ') : 'none'}`);
  const air = (g.airFleets as any[]).filter((a) => a.nat === me);
  L.push(`  air groups: ${air.length ? air.map((a) => `${a.name ?? a.id}`).join('; ') : 'none'}`);
  const m = money(g, me);
  L.push(`  books (as the interface shows them, x${m.scale}): income ${r1(m.gross)} a month, upkeep ${r1(m.upkeep)}, net ${r1(m.net)}, treasury ${r1(m.treasury)}`);
  L.push(`  at war with: ${wars(g, me)}; follows: ${WORLD.follow[me] ?? 'nobody'}; friends: ${friendsOf(me).map((f) => f.nat).join(', ') || 'none'}`);
  const tr = traitsOf(me), el = eliteOf(me);
  L.push(`  ${tr.length === 1 ? 'ability' : tr.length ? 'abilities' : 'ability'}: ${tr.length ? tr.map((t) => t.name).join('; ') : 'none'}${el ? `; formation: ${el.label}` : ''}`);
  const ps = small(me) ? T.paths(g) : [];
  L.push(`  paths: ${ps.length ? ps.map((p: any) => `${p.id}${p.now ? ' (now)' : p.ok ? ' (open)' : ` (no: ${p.why})`}`).join('; ') : 'none offered'}`);
  // The League: the countries it could found one with (league.ts founders: its own part of the world only).
  const fs = small(me) ? founders(g, me) : [];
  const far = fs.filter((f) => partOf(f.nat) !== partOf(me));
  L.push(`  League founders it could ask (${fs.length}, all of ${partName(me)} unless said): ${fs.map((f) => `${f.nat}${f.ok ? ' (willing)' : ''}${partOf(f.nat) !== partOf(me) ? ` [${partName(f.nat)}]` : ''}`).join(', ') || 'none'}; of another part of the world: ${far.length ? far.map((f) => f.nat).join(', ') : 'none'}`);
  const os = offers(g, me, n.capital);
  const can = os.filter((o) => !o.why), poor = os.filter((o) => o.poor);
  L.push(`  arsenal (with the capital picked on the map): ${os.length} offers; can order today ${can.length}; short of money only ${poor.length}; barred for another reason ${os.length - can.length - poor.length}`);
  for (const o of os) L.push(`    ${o.key.padEnd(14)} ${o.label} (${o.size}) cost ${r1(o.cost * m.scale)}, ${o.months} months${o.why ? `  NO: ${o.why}` : '  can order'}`);
  // A capital with no harbour says nothing of the shipyard: the same offers with the first home port picked.
  const quay = cap && !cap.port ? owned.find((r) => r.ctrl === me && r.port && !r.overseas) : undefined;
  if (quay) for (const o of offers(g, me, quay.id).filter((x) => x.key === 'work:yard' || x.key.startsWith('ship:tp'))) L.push(`    with ${quay.name} picked: ${o.key.padEnd(10)} ${o.label} (${o.size}) cost ${r1(o.cost * m.scale)}, ${o.months} months${o.why ? `  NO: ${o.why}` : '  can order'}`);
  return L;
}

/**
 * How the seat's army can leave home, on the day the player is first up: what the interface's own `reach` and
 * `shipTargets` give each corps this month, and how far its own and its allies' ground runs from the capital in
 * the end (terrain that bars tanks or wheels, and room in a province, are not counted in that second figure).
 */
function waysOut(g: Game, me: string): string[] {
  const L: string[] = [], n = g.nations[me];
  L.push(`  ways out of the country on ${date(g.turn)} (sea lift ${n.sealift}, seaSupply ${n.seaSupply}):`);
  const anyPort = g.regions.find((r) => r.port && r.owner !== me && allied(g, me, r.ctrl));
  for (const c of corpsOf(g, me)) {
    const m = T.reach(g, c);
    const all = [...m.keys()].filter((id) => id !== c.reg && !m.get(id)!.attack);
    const abroad = all.filter((id) => g.regions[id].owner !== me);
    const here = g.regions[c.reg];
    const ports = T.shipTargets(g, c);
    const sea = ports.length ? `${ports.length} ports` : `no port${here.port && anyPort ? ` ("${whyNotShip(g, c, anyPort)}")` : here.port ? ' (no friendly port abroad)' : ' (it stands in no port)'}`;
    L.push(`    ${c.name} in ${here.name}: marches to ${all.length} provinces this month, ${abroad.length} outside its own country${abroad.length ? ` (${abroad.slice(0, 3).map((id) => `${g.regions[id].name}, ${g.regions[id].ctrl}`).join('; ')}${abroad.length > 3 ? '; ...' : ''})` : ''}; by sea to ${sea}`);
  }
  if (n.capital < 0) return L;
  const road = (label: string, open: (holder: string) => boolean): void => {
    const dist = new Map<number, number>([[n.capital, 0]]), q = [n.capital];
    const edge = new Map<string, { d: number; at: string }>();
    for (let i = 0; i < q.length; i++) {
      const d = dist.get(q[i])!;
      for (const j of g.regions[q[i]].nb) {
        const r = g.regions[j];
        if (open(r.ctrl)) { if (!dist.has(j)) { dist.set(j, d + 1); q.push(j); } continue; }
        const e = edge.get(r.ctrl);
        if (!e || e.d > d + 1) edge.set(r.ctrl, { d: d + 1, at: r.name });
      }
    }
    const got = [...dist.keys()].map((id) => g.regions[id]);
    const by: Record<string, number> = {};
    for (const r of got) by[r.ctrl] = (by[r.ctrl] ?? 0) + 1;
    const old = got.filter((r) => r.id < WORLD_FIRST).sort((a, b) => dist.get(a.id)! - dist.get(b.id)!);
    const furthest = got.reduce((a, b) => (dist.get(b.id)! > dist.get(a.id)! ? b : a));
    L.push(`    by land in the end, over ${label} from ${g.regions[n.capital].name}: ${got.length} provinces (${Object.entries(by).sort((a, b) => b[1] - a[1]).map(([k, v]) => `${k} ${v}`).join(', ')}); furthest ${furthest.name}, ${dist.get(furthest.id)} provinces off; on the map of Europe ${old.length}${old.length ? ` (nearest ${old[0].name}, ${dist.get(old[0].id)} off)` : ''}`);
    L.push(`      frontiers that road comes to: ${[...edge.entries()].sort((a, b) => a[1].d - b[1].d).map(([k, e]) => `${k}${g.nations[me].atWar.includes(k) ? ' (AT WAR)' : ''} at ${e.at}, ${e.d} off`).join('; ') || 'none'}`);
  };
  road("its own and its allies' ground", (h) => allied(g, me, h));
  // A dominion: the same road over its own ground and its mother country's alone (France's Africa left out).
  const mother = WORLD.follow[me];
  if (mother) road(`its own ground and ${mother}'s alone`, (h) => h === me || h === mother);
  return L;
}

/** One real order, through the function the interface calls (turn.ts orderArsenal): the cheapest corps it can afford, else the cheapest of anything. */
function oneOrder(g: Game, me: string): { line: string; ok: boolean; key?: string; at?: number } {
  const os = offers(g, me, g.nations[me].capital), m = money(g, me);
  const can = os.filter((o) => !o.why).sort((a, b) => a.cost - b.cost);
  const pick = can.find((o) => o.group === 'army') ?? can[0];
  if (pick) {
    const before = g.nations[me].pp;
    const b = T.orderArsenal(g, pick.key, pick.at);
    if (!b) return { line: `ORDER REFUSED: ${pick.key} (${pick.label}) was on offer and orderArsenal returned nothing`, ok: false };
    return { line: `order given: ${pick.label} (${pick.size}) for ${r1(pick.cost * m.scale)}, due in ${pick.months} months${pick.at >= 0 ? ` at ${g.regions[pick.at].name}` : ''}; treasury ${r1(before * m.scale)} -> ${r1(g.nations[me].pp * m.scale)}; orders in hand ${g.nations[me].queue.length}`, ok: true, key: pick.key, at: pick.at };
  }
  const poor = os.filter((o) => o.poor).sort((a, b) => a.cost - b.cost)[0];
  if (!poor) return { line: `no order can be given: nothing on offer is open to it (${os.length} offers, every one barred for a reason other than money)`, ok: false };
  const L = ledger(g, me), short = poor.cost - g.nations[me].pp;
  const wait = L.net > 0 ? Math.ceil(short / L.net) : Infinity;
  return { line: `it can afford nothing today: the cheapest offer is ${poor.label} at ${r1(poor.cost * m.scale)} against a treasury of ${r1(m.treasury)}; at ${r1(m.net)} a month that is ${Number.isFinite(wait) ? `${wait} month(s) away` : 'never (no net income)'}`, ok: false };
}

const real = (ps: string[]): string[] => ps.filter((p) => !p.includes('[note]'));

/** Plays the player's seat: one order, then passing. */
function play(me: string, seed: number, months: number, level: string): boolean {
  const seat = TRIAL_SEATS.find((s) => s.id === me);
  out(`=== ${me}, seed ${seed}, ${months} months, level ${level}; seats files: ${TRIAL_FILES.join(', ') || 'none (placeholder rows only)'}${seat?.placeholder ? '; THIS SEAT IS ON PLACEHOLDER ROWS' : ''}${seat ? '' : '; NOT A SEAT OF THIS BUILD'}`);
  const problems: string[] = [], seen = new Set<string>(), notes: string[] = [];
  const t0 = Date.now();
  let g: Game, up = false, ordered = false, orderLine = '', orderWait = '', saveOk: boolean | null = null, error = '';
  let twin: Game | null = null, twinAt = 0;
  const until = months;   // turn 1 is September 1939
  try {
    g = start(me, 1, seed, level);
    if (!g.nations[me]) throw new Error(`${me} is no country in this game`);
    if (T.beginEve(g)) { T.endEve(g); notes.push('the eve of war was offered and passed'); }
    check(g, 'on the first day', problems, seen);
    for (const l of firstDay(g, me)) out(l);
    up = advance(g, (nat) => check(g, `after ${nat}`, problems, seen));
    out(`  first month reached in ${Math.round((Date.now() - t0) / 1000)} s`);
    out('--- month by month (the line is the day the player is up)');
    let last = Date.now();
    if (up) for (const l of waysOut(g, me)) out(l);
    while (up && g.turn <= until) {
      if (!ordered) {
        const o = oneOrder(g, me);
        // (A game loaded from the half-way save is given the same order, so that the two can be compared to the end.)
        if (o.ok && twin && !T.orderArsenal(twin, o.key!, o.at)) problems.push(`[${date(g.turn)}] the game loaded from the save refused the order the first game took (${o.key})`);
        if (o.ok) { ordered = true; orderLine = `${date(g.turn)}: ${o.line}`; out(`  ${orderLine}`); } else if (!orderWait) { orderWait = `${date(g.turn)}: ${o.line}`; out(`  ${orderWait}`); }
      }
      out(`  ${monthLine(g, me)}  [${Math.round((Date.now() - last) / 1000)} s]`);
      last = Date.now();
      check(g, 'after the player', problems, seen);
      // Half-way: save, load, and play the loaded game on beside this one. Never in September 1939: on today's main
      // a save made in the first month does not play on the same (README, finding 16), with or without the trial.
      if (!twin && g.turn >= Math.max(2, Math.ceil(until / 2))) {
        const text = saveGame(g);
        const back = loadGame(text);
        if (!back) { saveOk = false; problems.push(`[${date(g.turn)}] the game could not be loaded from its own save (${Math.round(text.length / 1024)} KB)`); } else {
          const again = saveGame(back);
          if (!sameSave(again, text)) problems.push(`[${date(g.turn)}] save, load, save again does not give the same save (${differs(text, again)})`);
          twin = back; twinAt = g.turn;
          notes.push(`saved at ${date(g.turn)}: ${Math.round(text.length / 1024)} KB`);
        }
      }
      T.endHuman(g);
      up = advance(g, (nat) => check(g, `after ${nat}`, problems, seen));
      if (twin) {
        T.endHuman(twin);
        const tUp = advance(twin);
        if (tUp !== up || !sameSave(saveGame(twin), saveGame(g))) { if (saveOk !== false) { saveOk = false; problems.push(`[${date(g.turn)}] the game loaded from the save of ${date(twinAt)} no longer plays as the one it was saved from (${differs(saveGame(g), saveGame(twin))})`); } } else if (saveOk === null) saveOk = true;
      }
    }
    if (g.nations[me].alive) out(`  ${monthLine(g, me)}  (end)`);
  } catch (e) { error = (e as Error).stack ?? String(e); g = null as any; }
  const secs = Math.round((Date.now() - t0) / 1000);
  if (error) out(`  EXCEPTION: ${error}`);
  const bad = real(problems);
  for (const p of problems.slice(0, 30)) out(`    ${p}`);
  if (problems.length > 30) out(`    ... and ${problems.length - 30} more`);
  for (const n of notes) out(`    [note] ${n}`);
  const n = g ? g.nations[me] : null;
  const fell = n && !n.alive ? `fell ${n.fellOn ? `${date(n.fellOn.turn)} to ${n.fellOn.to}` : ''}` : '';
  const played = g ? Math.min(g.turn, until + 1) - 1 : 0;
  const pass = !error && !bad.length && ordered && saveOk !== false && played >= Math.min(months, 12) && !!n?.alive;
  const why = [error ? 'an exception' : '', bad.length ? `${bad.length} broken invariant(s)` : '', !ordered ? 'no order could be given' : '', saveOk === false ? 'the save did not play on the same' : '', saveOk === null && !error ? 'the save was not tried' : '', fell, !error && !fell && played < Math.min(months, 12) ? `only ${played} months played` : ''].filter(Boolean).join('; ');
  out(`VERDICT ${me} seed ${seed}: ${pass ? 'PASS' : 'FAIL'}: started ${g || !error ? 'yes' : 'no'}; order ${ordered ? `given (${orderLine.split(':')[0]})` : 'NOT given'}; months played ${played}; save and load ${saveOk === true ? 'plays on the same' : saveOk === false ? 'DIFFERS' : 'not tried'}; invariants broken ${bad.length}; ${n?.alive ? 'still standing' : fell || 'not standing'}${why ? `; why: ${why}` : ''}; ${secs} s`);
  return pass;
}

/** A save with every table's keys in order: the same contents with keys in another order is the same game (the Playtester's rule, lib.ts saveCheck). */
const canon = (text: string): string => JSON.stringify(JSON.parse(text), (_k, x) => (x && typeof x === 'object' && !Array.isArray(x) ? Object.fromEntries(Object.entries(x).sort(([a], [b]) => (a < b ? -1 : a > b ? 1 : 0))) : x));
const sameSave = (a: string, b: string): boolean => a === b || canon(a) === canon(b);

/** Which top-level parts of two saves differ. */
function differs(a: string, b: string): string {
  try {
    const A = JSON.parse(a), B = JSON.parse(b);
    const ks = [...new Set([...Object.keys(A), ...Object.keys(B)])].filter((k) => canon(JSON.stringify(A[k] ?? null)) !== canon(JSON.stringify(B[k] ?? null)));
    return `differs in: ${ks.slice(0, 12).join(', ')}${ks.length > 12 ? ' ...' : ''}`;
  } catch { return 'the saves are not the same text'; }
}

/** Nobody playing: what the computer does with each country. */
function watch(seed: number, months: number, codes: string[]): boolean {
  out(`=== watch, seed ${seed}, ${months} months; seats files: ${TRIAL_FILES.join(', ') || 'none (placeholder rows only)'}; countries: ${codes.join(' ')}`);
  const problems: string[] = [], seen = new Set<string>();
  const t0 = Date.now();
  let error = '';
  let g: Game | null = null;
  const log: Record<string, string[]> = Object.fromEntries(codes.map((c) => [c, []]));
  try {
    g = start(null, 1, seed);
    for (const c of codes) if (!g.nations[c]) throw new Error(`${c} is no country in this game`);
    const was: Record<string, { wars: string; align: string; regs: number; corps: number; away: number; alive: boolean; league: boolean }> = {};
    const snap = (c: string) => { const n = g!.nations[c]; return { wars: wars(g!, c), align: String(bloc(n) ?? n.align), regs: heldBy(g!, c).length, corps: corpsOf(g!, c).length, away: corpsOf(g!, c).filter((x) => g!.regions[x.reg].owner !== c).length, alive: n.alive, league: !!(n as any).league }; };
    for (const c of codes) { was[c] = snap(c); log[c].push(`${date(g.turn)}: ${was[c].align}; provinces ${was[c].regs}; corps ${was[c].corps}; at war with ${was[c].wars}`); }
    while (g.turn <= months) {
      const h0 = g.headlines.length;
      T.runTurnAI(g);
      check(g, 'after the month', problems, seen);
      for (const c of codes) {
        const now = snap(c), w = was[c], ch: string[] = [];
        if (now.alive !== w.alive) ch.push(now.alive ? 'restored' : `FELL${g.nations[c].fellOn ? ` to ${g.nations[c].fellOn!.to}` : ''}`);
        if (now.align !== w.align) ch.push(`now ${now.align}`);
        if (now.wars !== w.wars) ch.push(`at war with ${now.wars}`);
        if (now.regs !== w.regs) ch.push(`provinces ${w.regs} -> ${now.regs}`);
        if (now.corps !== w.corps) ch.push(`corps ${w.corps} -> ${now.corps}`);
        if (now.away !== w.away) ch.push(`corps outside its own country ${w.away} -> ${now.away}`);
        const news = g.headlines.slice(h0).filter((h: any) => h.nat === c || h.text.includes(g!.nations[c].name)).map((h: any) => h.text);
        if (ch.length || news.length) log[c].push(`${date(g.turn - 1)}: ${ch.join('; ')}${news.length ? `${ch.length ? '; ' : ''}news: ${[...new Set(news)].slice(0, 3).join(' | ')}` : ''}`);
        was[c] = now;
      }
    }
  } catch (e) { error = (e as Error).stack ?? String(e); }
  for (const c of codes) {
    out(`--- ${c}${g?.nations[c] ? ` (${g.nations[c].name})` : ''}`);
    for (const l of log[c]) out(`  ${l}`);
    if (g?.nations[c]) out(`  ${monthLine(g, c)}  (end)`);
  }
  if (error) out(`  EXCEPTION: ${error}`);
  const bad = real(problems);
  for (const p of problems.slice(0, 30)) out(`    ${p}`);
  if (problems.length > 30) out(`    ... and ${problems.length - 30} more`);
  const pass = !error && !bad.length;
  out(`VERDICT watch seed ${seed}: ${pass ? 'PASS' : 'FAIL'}: months played ${g ? g.turn - 1 : 0}; invariants broken ${bad.length}${error ? '; an exception' : ''}; fallen among the watched: ${codes.filter((c) => g?.nations[c] && !g.nations[c].alive).join(' ') || 'none'}; ${Math.round((Date.now() - t0) / 1000)} s`);
  return pass;
}

const args = process.argv.slice(2);
if (!args.length || args[0] === 'list') {
  out(`seats of this build (${TRIAL_FILES.join(', ') || 'placeholder rows only'}; ${TRIAL_WARNINGS} warning(s) at build):`);
  for (const s of TRIAL_SEATS) out(`  ${s.id}  from ${s.from}${s.placeholder ? '  (placeholder rows)' : ''}`);
  out(`first-day table: ${Object.keys(TRIAL_FAR.held).length} provinces held by another than their owner; capitals moved: ${Object.entries(TRIAL_FAR.capitals).map(([k, v]) => `${k} to ${v}`).join(', ') || 'none'}; rows dropped: ${TRIAL_FAR.drop.length}; first-day wars: ${TRIAL_FAR.wars.map((w) => w.join('-')).join(', ') || 'none'}; the wider world begins at province ${WORLD_FIRST}`);
  out('usage: node dist/trial/seat-play.mjs SEAT [SEED] [MONTHS] [LEVEL]   |   watch [SEED] [MONTHS] [CODES]   |   list');
  process.exit(0);
}
const seed = Number(args[1] ?? 7), months = Number(args[2] ?? 12);
const ok = args[0] === 'watch' ? watch(seed, months, (args[3] ? args[3].split(',') : TRIAL_SEATS.map((s) => s.id))) : play(args[0].toUpperCase(), seed, months, args[3] ?? 'colonel');
process.exit(ok ? 0 : 1);

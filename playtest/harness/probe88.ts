// Version 82.3: one full grand campaign by hand that uses the features of 2026-11-16 as a player would: bombing raids
// chosen from the raid card's BY DAY / BY NIGHT forecast, railways cut by bombing and torn up on a retreat,
// anti-aircraft guns ordered where the enemy has bombed, artillery corps bought and brought into attacks, reserves
// moved by rail. usage: probe88 SEED [UNTIL=76] [PLAN-JSON] [ME=GER]   (build with build70.sh)
// TC_SAVE=dir TC_SAVE_AT=2,13,30 writes the game as the player's month opens, for the page (localStorage
// 'theater-command-save-v1', then theater.continueGame()).
// Output: one M line a month (the diary), R lines (each raid of the player's: the forecast both ways, the way flown,
// the result), then the tables: FORECAST (the card against what happened), FOERAIDS (who bombed whom, by day and by
// night, against guns and without), CUTS (every cut railway: cause, how long until relaid), GUNS (levels by nation
// each January), TRAINS (long moves, the reasons no train ran), DULL (the longest quiet stretches), SEAT.
import { writeFileSync, mkdirSync } from 'node:fs';
import { campaign, date, T, atWar, allied, saveGame, hostileUnits } from './lib';
import { railCut, cutCause, onNet } from '@tc/campaign/rails';
import { gunsAt } from '@tc/campaign/air';
import { railReach, whyNoTrain } from '@tc/campaign/game';
import { fuelMet } from '@tc/campaign/resources';

const [seedS, untilS, planS, meS] = process.argv.slice(2);
const me = meS || 'GER';
const GER_DECL = [[8, 'DEN'], [9, 'LUX'], [9, 'BEL'], [9, 'NED'], [20, 'YUG'], [22, 'SOV']];
const extra = JSON.parse(planS || '{}');
const opt = { raids: true, tear: true, aa: true, maxRaids: 6, lossMost: 0.12, human: false, aaMost: 99, ...(extra.opt || {}) };
const HUMAN_BUY = ['corps:arm', 'corps:inf', 'corps:mot'];
let savedMonths = 0;
delete extra.opt;
const X = T as any;
const SAVE = process.env.TC_SAVE || '';
const SAVE_AT = new Set((process.env.TC_SAVE_AT || '').split(',').filter(Boolean).map(Number));
if (SAVE) mkdirSync(SAVE, { recursive: true });

interface Raid { turn: number; place: string; holder: string; what: string; km: number; way: string; fS: number; fLossN: number; fLoss: number; altS: number; altLossN: number; altWhy: string | null; guns: number; bombers: number; fighters: number; won: boolean; lostB: number; lostF: number; cut: string; result: string; rail: number; fac: number; gunStop: number }
const raids: Raid[] = [];
const notes: string[] = [];
const note = (g: any, s: string): void => { if (notes.length < 400) notes.push(`${date(g.turn)}: ${s}`); };
const cuts = new Map<number, { on: number; cause: string; holder: string; owner: string }>();
const cutsDone: { name: string; on: number; off: number; cause: string; holder: string; by: string }[] = [];
const gunOrders: { turn: number; name: string; ok: boolean; why: string }[] = [];
const gunsByYear: string[] = [];
const tears: { turn: number; name: string; ok: boolean; why: string }[] = [];
const noTrain: Record<string, number> = {};
let longMoves = 0, railChecks = 0, railChecksOpen = 0;
const bought: Record<string, number> = {};
const artSay: string[] = [];
const unsup: Record<number, [number, number]> = {};
const longByYear: Record<number, number> = {};
let airSeen = 0;
const foe: { turn: number; att: string; def: string; mission: string; place: string; won: boolean; bmr: number; lostB: number; guns: number; cut: boolean; res: string }[] = [];
const armsByYear: string[] = [];
const airByYear: string[] = [];
const deaths: Record<number, string[]> = {};
let headSeen = 0;
const month: Record<number, { raids: string[]; foeOnMe: number; foeNight: number; tears: number; guns: number; long: number; cutsNew: number; mended: number }> = {};
const M = (t: number) => (month[t] ??= { raids: [], foeOnMe: 0, foeNight: 0, tears: 0, guns: 0, long: 0, cutsNew: 0, mended: 0 });
const before = new Map<number, number>();
const pct = (x: number): string => `${Math.round(x * 100)}%`;

function dist(g: any, a: number, b: number, most = 3): number {
  if (a === b) return 0;
  let cur = [a]; const seen = new Set([a]);
  for (let d = 1; d <= most; d++) { const nx: number[] = []; for (const i of cur) for (const j of g.regions[i].nb) { if (j === b) return d; if (!seen.has(j)) { seen.add(j); nx.push(j); } } cur = nx; }
  return most + 1;
}

/** The air reports filed since the last look: every bombing raid in the world, with the guns at the target then. */
function scanAir(g: any): void {
  const all = g.airReports ?? [];
  // (The list may be trimmed by the rules: go by id.)
  for (const r of all) {
    if (r.id <= airSeen) continue;
    if (!['bomb', 'night', 'town', 'line'].includes(r.mission) || r.region < 0) continue;
    const bmr = r.att.reduce((s: number, a: any) => s + a.bmr, 0), lostB = r.att.reduce((s: number, a: any) => s + a.lostBmr, 0);
    foe.push({ turn: r.turn, att: r.attNat, def: r.defNat, mission: r.mission, place: r.place, won: r.won, bmr, lostB, guns: gunsAt(g, r.region), cut: railCut(g, r.region), res: r.defNat === me ? String(r.result ?? '') : '' });
    if (r.defNat === me && r.attNat !== me) { M(r.turn).foeOnMe++; if (r.mission === 'night') M(r.turn).foeNight++; raidedMine.add(r.region); }
  }
  for (const r of all) if (r.id > airSeen) airSeen = r.id;
}
const raidedMine = new Set<number>();

/** Every railway in the world: what is cut now, by what cause, and how long each cut lasted. */
function scanCuts(g: any): void {
  for (const r of g.regions) {
    if (!onNet(r.id)) continue;
    const on = railCut(g, r.id), was = cuts.get(r.id);
    if (on && !was) { cuts.set(r.id, { on: g.turn, cause: String(cutCause(g, r.id) ?? '?'), holder: r.ctrl, owner: r.owner }); if (r.ctrl === me || r.owner === me) M(g.turn).cutsNew++; }
    else if (!on && was) { cutsDone.push({ name: r.name, on: was.on, off: g.turn, cause: was.cause, holder: was.holder, by: r.ctrl }); cuts.delete(r.id); if (r.ctrl === me) M(g.turn).mended++; }
  }
}

/** The player's bombing for the month, picked from the raid card: the target and the way that promise most for the bombers risked. */
function flyRaids(g: any): void {
  if (!opt.raids) return;
  for (let n = 0; n < opt.maxRaids; n++) {
    let targets: any[] = [];
    try { targets = X.raidTargetsFor(g, 'bomb') ?? []; } catch (e) { note(g, `raidTargetsFor threw: ${(e as Error).message}`); return; }
    if (!targets.length) return;
    let best: { t: any; w: any; other: any; dn: any; u: number } | null = null;
    for (const t of targets.slice(0, 12)) {
      let dn: any = null;
      try { dn = X.previewDayNight(g, 'bomb', t.id); } catch (e) { note(g, `previewDayNight threw at ${t.name}: ${(e as Error).message}`); }
      if (!dn) continue;
      for (const [w, other] of [[dn.day, dn.night], [dn.night, dn.day]] as [any, any][]) {
        if (w.why || !w.picked?.length || !w.bombers) continue;
        if (w.bomberLoss > opt.lossMost) continue;
        // What a hit is worth: a factory's output or a cut railway near the front count for more than a miss costs.
        const worth = (w.figures?.rail ?? 0) * 1.0 + (w.figures?.output ?? 0) * 4 + (w.figures?.factory ?? 0) * 1.5;
        const u = worth - 2.5 * w.bomberLoss;
        if (!best || u > best.u) best = { t, w, other, dn, u };
      }
    }
    if (!best || best.u <= 0.02) { if (n === 0 && targets.length) M(g.turn).raids.push(`none worth flying of ${targets.length}`); return; }
    const { t, w, other, dn } = best;
    const cutB = railCut(g, t.id);
    let rep: any = null;
    try { rep = X.orderRaid(g, w.kind, t.id, w.picked); } catch (e) { note(g, `orderRaid threw at ${t.name}: ${(e as Error).message}`); return; }
    if (!rep) { note(g, `ANOMALY the raid on ${t.name} by ${w.label} was on the card (${pct(w.success)}) and could not be flown as picked`); return; }
    const lostB = rep.att.reduce((s: number, a: any) => s + a.lostBmr, 0), lostF = rep.att.reduce((s: number, a: any) => s + a.lostFtr, 0);
    const cutA = railCut(g, t.id);
    const r: Raid = { turn: g.turn, place: t.name, holder: t.holder, what: t.what, km: t.km, way: w.kind === 'night' ? 'night' : 'day', fS: w.success, fLossN: w.bomberLossN, fLoss: w.bomberLoss, altS: other.why ? -1 : other.success, altLossN: other.why ? -1 : other.bomberLossN, altWhy: other.why, guns: dn.guns, bombers: w.bombers, fighters: w.fighters, won: rep.won, lostB, lostF, cut: cutB ? 'was cut' : cutA ? 'CUT' : 'runs', result: String(rep.result).slice(0, 110), rail: w.figures?.rail ?? 0, fac: w.figures?.factory ?? 0, gunStop: w.gunsStop ?? 0 };
    raids.push(r);
    M(g.turn).raids.push(`${t.name} ${r.way} ${pct(w.success)}${rep.won ? ' WON' : ' lost'} -${lostB}${cutA && !cutB ? ' cut' : ''}`);
    if (w.kind === 'night' && lostF > 0) note(g, `ANOMALY a night raid on ${t.name} lost ${lostF} fighters though escorts stay home`);
    if (lostB > Math.max(15, 3 * w.bomberLossN)) note(g, `the raid on ${t.name} by ${r.way} lost ${lostB} bombers against a forecast of about ${w.bomberLossN}`);
    if (rep.mission !== w.kind) note(g, `ANOMALY ordered ${w.kind}, the report says ${rep.mission}`);
  }
}

/** Guns where the enemy has bombed, two orders a month at most; Berlin and the Ruhr as the war opens. */
function orderGuns(g: any): void {
  if (!opt.aa) return;
  { let lv = 0; for (const r of g.regions) if (r.ctrl === me) lv += gunsAt(g, r.id); if (lv + g.nations[me].queue.filter((b: any) => b.what === 'work' && b.item === 'aa').length >= opt.aaMost) { raidedMine.clear(); return; } }
  const want: number[] = [];
  if (g.turn === 2) for (const nm of me === 'GER' ? ['Ruhr', 'Berlin'] : []) { const r = g.byName?.[nm] ?? g.regions.find((x: any) => x.name === nm); if (r) want.push(r.id); }
  for (const id of raidedMine) want.push(id);
  raidedMine.clear();
  let n = 0;
  for (const id of want) {
    const r = g.regions[id];
    if (!r || r.ctrl !== me || n >= 2) continue;
    if (gunsAt(g, id) >= 3) continue;
    let b: any = null, why = '';
    try {
      const off = (X.arsenalOffers(g, id) ?? []).find((o: any) => o.key === 'work:aa');
      why = off ? String(off.why ?? off.note ?? '') : 'no offer';
      b = X.orderArsenal(g, 'work:aa', id);
    } catch (e) { why = (e as Error).message; }
    gunOrders.push({ turn: g.turn, name: r.name, ok: !!b, why: b ? '' : why.slice(0, 90) });
    if (b) { n++; M(g.turn).guns++; }
  }
}

/** A retreating player tears up the line: a province of mine beside the enemy where I am outnumbered two to one, after ground was lost near it. */
function tearUp(g: any, run: any): void {
  if (!opt.tear) return;
  const last = run.rows[run.rows.length - 1];
  if (!last || last.lost === 0) return;
  let n = 0;
  for (const r of g.regions) {
    if (n >= 2) break;
    if (r.ctrl !== me || !onNet(r.id) || railCut(g, r.id)) continue;
    const mine = r.units.filter((u: any) => u.nat === me && u.kind !== 'fort');
    if (!mine.length) continue;
    let foeStr = 0;
    for (const j of r.nb) { const o = g.regions[j]; if (atWar(g, me, o.ctrl)) foeStr += hostileUnits(g, o, me).reduce((s: number, u: any) => s + u.divs * u.str, 0); }
    const myStr = mine.reduce((s: number, u: any) => s + u.divs * u.str, 0);
    if (foeStr < 2 * myStr || foeStr === 0) continue;
    const why = X.whyNotCutRailway(g, r.id);
    const ok = why === null ? X.orderCutRailway(g, r.id) : false;
    tears.push({ turn: g.turn, name: r.name, ok, why: why ?? '' });
    if (ok) { n++; M(g.turn).tears++; if (!railCut(g, r.id)) note(g, `ANOMALY tore up the railway at ${r.name} and it still runs`); }
  }
}

/** Trains: what a rear corps is offered, and the reason where none will take it. */
function trainLook(g: any): void {
  const mine = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort' && T.canOrder(g, c));
  for (const c of mine.slice(0, 40)) {
    railChecks++;
    let n = 0;
    try { n = railReach(g, c).size; } catch { n = -1; }
    if (n > 0) { railChecksOpen++; continue; }
    let w: any = null; try { w = whyNoTrain(g, c); } catch { w = null; }
    const code = w?.code ?? (n < 0 ? 'threw' : 'none given');
    noTrain[code] = (noTrain[code] || 0) + 1;
    if (!w && n === 0 && (noTrain['none given'] || 0) <= 3) note(g, `a corps at ${g.regions[c.reg].name} has no train and no reason is given`);
  }
}

const plan: any = {
  minOdds: 1.4, declare: me === 'GER' ? GER_DECL : [], land: true, guns: 1, useGuns: true, engs: 2, depth: 'theatre', byHand: ['air'],
  ...extra,
  first: (g: any, run: any) => {
    if (SAVE && SAVE_AT.has(g.turn)) writeFileSync(`${SAVE}/save-t${g.turn}.json`, saveGame(g));
    scanAir(g); scanCuts(g);
    { const mine = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort'); unsup[g.turn] = [mine.filter((c: any) => !c.supplied).length, mine.length]; }
    if ([1, 13, 25, 37, 49, 61].includes(g.turn)) { try { const o = (X.arsenalOffers(g) ?? []).find((x: any) => x.key === 'corps:art'); artSay.push(`${date(g.turn)}: ${o ? (o.why ? `cannot be ordered (${String(o.why).slice(0, 90)})` : `on offer at ${o.cost}, ${o.months} months`) : 'not on the list'}`); } catch (e) { artSay.push(`${date(g.turn)}: threw ${(e as Error).message}`); } }
    before.clear(); for (const c of g.corps) if (c.nat === me && !c.dead) before.set(c.id, c.reg);
    if (opt.human) {
      const field = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort');
      const due = ((run as any).gunsDue ?? []).filter((t: number) => t > g.turn).length;
      const need = field.filter((c: any) => c.kind === 'art').length + due < Math.floor((field.length * (plan.guns ?? 0)) / 10);
      let poor = false;
      if (need) { try { const o = (X.arsenalOffers(g, g.nations[me].capital) ?? []).find((x: any) => x.key === 'corps:art'); poor = !!o && !!o.why && /treasury/i.test(String(o.why)); } catch { poor = false; } }
      plan.buy = need && poor ? [] : HUMAN_BUY;
      if (need && poor) savedMonths++;
    }
    orderGuns(g);
    if (g.turn % 3 === 1) trainLook(g);
    flyRaids(g);
    for (const h of g.headlines.slice(headSeen)) { if (/surrender|laid down|lays down|destroyed|wiped out|cut off|pocket|disband|overrun|annihilat|ceases to exist|is no more/i.test(h.text)) (deaths[h.turn] ??= []).push(String(h.text).slice(0, 150)); }
    headSeen = g.headlines.length;
    if (g.turn % 12 === 5) armsByYear.push(`${date(g.turn)}: ` + ['GER', 'UK', 'FRA', 'ITA', 'SOV', 'USA'].filter((n) => g.nations[n]?.alive).map((n) => { const cs = g.corps.filter((c: any) => c.nat === n && !c.dead && c.kind !== 'fort'); return `${n} ${cs.length} corps, ${cs.filter((c: any) => c.kind === 'art').length} art, ${cs.filter((c: any) => c.kind === 'eng').length} eng`; }).join('; '));
    if (g.turn % 12 === 5) airByYear.push(`${date(g.turn)}: ` + ['GER', 'UK', 'FRA', 'ITA', 'SOV', 'USA'].filter((n) => g.nations[n]?.alive).map((n) => { const fs = (g.airFleets ?? []).filter((f: any) => f.nat === n); const land = fs.filter((f: any) => !/carrier|strike wing/i.test(String(f.name))); return `${n} ${land.reduce((s2: number, f: any) => s2 + (f.bmr || 0), 0)} bombers and ${land.reduce((s2: number, f: any) => s2 + (f.ftr || 0), 0)} fighters ashore in ${land.length} groups`; }).join('; '));
    if (g.turn % 12 === 5) gunsByYear.push(`${date(g.turn)}: ` + ['GER', 'UK', 'FRA', 'ITA', 'SOV', 'USA', 'JAP', 'JPN'].filter((n) => g.nations[n]).map((n) => { let lv = 0, at = 0; for (const r of g.regions) if (r.ctrl === n) { const x = gunsAt(g, r.id); if (x) { lv += x; at++; } } return `${n} ${lv} in ${at}`; }).join(', '));
  },
  each: (g: any, run: any) => {
    let long = 0;
    for (const c of g.corps) { if (c.nat !== me || c.dead) continue; const b = before.get(c.id); if (b === undefined || b === c.reg) continue; if (dist(g, b, c.reg, 3) > 3) long++; }
    longMoves += long; M(g.turn).long = long; const y = 1939 + Math.floor((g.turn + 7) / 12); longByYear[y] = (longByYear[y] || 0) + long;
    tearUp(g, run);
  },
};

const UNTIL = Number(untilS || 76);
const run = campaign(me, 1, Number(seedS || 7), plan, { until: UNTIL, level: process.env.TC_LEVEL || 'colonel', onMonth: (run, row) => {
  const g: any = run.g; const m = M(row.turn);
  const art = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind === 'art').length;
  for (const b of row.bought) { const k = String(b).replace(/[0-9]+ ?x? ?/g, '').trim() || String(b); bought[k] = (bought[k] || 0) + 1; }
  const us = unsup[row.turn] ?? [0, 0];
  console.log(`M ${row.date} t${row.turn}: ${row.myCorps} corps (${art} art, ${g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind === 'eng').length} eng; ${us[0]} of ${us[1]} unsupplied; oil ${pct(fuelMet(g, me))}), ${row.myRegions} prov, +${row.gained}/-${row.lost}, attacks ${row.attacksWon}/${row.attacksOrdered} of ${row.attackOptions} on offer, hit ${row.hitByEnemy}; raids [${m.raids.join('; ')}]; bombed ${m.foeOnMe}${m.foeNight ? ` (${m.foeNight} by night)` : ''}; trains ${m.long}; torn up ${m.tears}; guns ordered ${m.guns}; my lines cut +${m.cutsNew}/relaid ${m.mended}; wars ${row.wars}`);
} });
const g: any = run.g;
scanAir(g); scanCuts(g);

for (const r of raids) console.log(`R ${date(r.turn)} ${r.place} (${r.holder}; ${r.what}; ${Math.round(r.km)} km; guns ${r.guns}) flown by ${r.way}: card ${pct(r.fS)}, about ${r.fLossN} of ${r.bombers} bombers lost${r.fighters ? `, ${r.fighters} escorts` : ''}; the other way ${r.altWhy ? `barred (${String(r.altWhy).slice(0, 50)})` : `${pct(r.altS)}, about ${r.altLossN} lost`}; rail ${pct(r.rail)}, factory ${pct(r.fac)}${r.gunStop ? `, guns stop ${pct(r.gunStop)}` : ''} => ${r.won ? 'WON' : 'lost'}, ${r.lostB} bombers${r.lostF ? ` and ${r.lostF} fighters` : ''} lost, railway ${r.cut}. ${r.result}`);

// FORECAST: the card's chance against what happened, by way and by band.
const band = (x: number): string => (x < 0.2 ? 'under 20%' : x < 0.4 ? '20 to 39%' : x < 0.6 ? '40 to 59%' : '60% and over');
for (const way of ['day', 'night']) {
  const rs = raids.filter((r) => r.way === way);
  if (!rs.length) { console.log(`FORECAST ${way}: no raid flown`); continue; }
  const mean = (f: (r: Raid) => number): number => rs.reduce((s, r) => s + f(r), 0) / rs.length;
  console.log(`FORECAST ${way}: ${rs.length} raids; the card's mean chance ${pct(mean((r) => r.fS))}, won ${pct(mean((r) => (r.won ? 1 : 0)))}; bombers the card said would be lost ${Math.round(rs.reduce((s, r) => s + r.fLossN, 0))}, lost ${rs.reduce((s, r) => s + r.lostB, 0)} of ${rs.reduce((s, r) => s + r.bombers, 0)} sent; railway: the card's mean chance ${pct(mean((r) => r.rail))}, cut in ${rs.filter((r) => r.cut === 'CUT').length} of ${rs.filter((r) => r.cut !== 'was cut').length}`);
  const by: Record<string, Raid[]> = {}; for (const r of rs) (by[band(r.fS)] ??= []).push(r);
  for (const k of ['under 20%', '20 to 39%', '40 to 59%', '60% and over']) if (by[k]) console.log(`FORECAST ${way} ${k}: ${by[k].length} raids, won ${by[k].filter((r) => r.won).length}`);
}
const withGuns = raids.filter((r) => r.guns > 0);
console.log(`FORECAST guns: ${withGuns.length} of my ${raids.length} raids met anti-aircraft guns` + (withGuns.length ? `; first ${date(withGuns[0].turn)} at ${withGuns[0].place} (level ${withGuns[0].guns})` : ''));

// FOERAIDS: every bombing raid in the world by who flew it.
const nats = [...new Set(foe.map((f) => f.att))];
for (const a of nats) {
  const fs = foe.filter((f) => f.att === a);
  const k = (m: string) => fs.filter((f) => f.mission === m);
  const line = (m: string): string => { const x = k(m); return x.length ? `${m} ${x.length} (won ${x.filter((f) => f.won).length}, ${x.reduce((s, f) => s + f.lostB, 0)} of ${x.reduce((s, f) => s + f.bmr, 0)} bombers lost)` : ''; };
  const first = (m: string): string => { const x = k(m)[0]; return x ? `first ${m} ${date(x.turn)}` : ''; };
  console.log(`FOERAIDS ${a}: ${['bomb', 'night', 'town', 'line'].map(line).filter(Boolean).join('; ')}; ${[first('bomb'), first('night')].filter(Boolean).join(', ')}; against ${[...new Set(fs.map((f) => f.def))].join(',')}`);
}
const gunned = foe.filter((f) => f.guns > 0 && ['bomb', 'night'].includes(f.mission)), bare = foe.filter((f) => f.guns === 0 && ['bomb', 'night'].includes(f.mission));
const rate = (x: typeof foe): string => (x.length ? `${x.length} raids, won ${pct(x.filter((f) => f.won).length / x.length)}, bombers lost ${pct(x.reduce((s, f) => s + f.lostB, 0) / Math.max(1, x.reduce((s, f) => s + f.bmr, 0)))}` : 'none');
console.log(`FOERAIDS against guns: ${rate(gunned)}; against none: ${rate(bare)}`);
const onMe = foe.filter((f) => f.def === me && f.att !== me);
console.log(`FOERAIDS on ${me}: ${onMe.length} (${onMe.filter((f) => f.mission === 'night').length} by night), won ${onMe.filter((f) => f.won).length}; by year ` + [1939, 1940, 1941, 1942, 1943, 1944, 1945].map((y) => `${y}: ${onMe.filter((f) => 1939 + Math.floor((f.turn + 7) / 12) === y).length}`).join(', '));

// CUTS
const allCuts = [...cutsDone, ...[...cuts.entries()].map(([id, c]) => ({ name: g.regions[id].name, on: c.on, off: 0, cause: c.cause, holder: c.holder, by: '' }))];
const causes = [...new Set(allCuts.map((c) => c.cause))];
for (const k of causes) {
  const cs = allCuts.filter((c) => c.cause === k), done = cs.filter((c) => c.off);
  console.log(`CUTS ${k}: ${cs.length} (${cs.filter((c) => c.holder === me).length} on ground ${me} held); relaid ${done.length}, after ${done.length ? (done.reduce((s, c) => s + c.off - c.on, 0) / done.length).toFixed(1) : '-'} months on average (longest ${done.length ? Math.max(...done.map((c) => c.off - c.on)) : '-'}); still cut at the end ${cs.length - done.length}`);
}
console.log(`CUTS torn up by the player: ${tears.filter((t) => t.ok).length} of ${tears.length} tried` + (tears.length ? `: ${tears.slice(0, 8).map((t) => `${t.name} ${date(t.turn)}${t.ok ? '' : ` REFUSED (${t.why.slice(0, 60)})`}`).join('; ')}` : ''));
console.log(`GUNS ordered by the player: ${gunOrders.filter((o) => o.ok).length} of ${gunOrders.length} tried` + (gunOrders.length ? `: ${gunOrders.slice(0, 10).map((o) => `${o.name} ${date(o.turn)}${o.ok ? '' : ` REFUSED (${o.why})`}`).join('; ')}` : ''));
for (const l of gunsByYear) console.log(`GUNS levels ${l}`);
for (const l of armsByYear) console.log(`ARMS ${l}`);
for (const l of airByYear) console.log(`AIRFORCE ${l}`);
{ const ys: string[] = []; for (const y of [1939, 1940, 1941, 1942, 1943, 1944, 1945]) { const fs = onMe.filter((f) => 1939 + Math.floor((f.turn + 7) / 12) === y); if (!fs.length) continue; let prod = 0, n = 0; for (const f of fs) { const m = /: ([0-9.]+)% of \w+ production/.exec(f.res); if (m) { prod += Number(m[1]); n++; } } const g0 = fs.filter((f) => f.guns === 0), g1 = fs.filter((f) => f.guns > 0); ys.push(`${y}: ${fs.length} raids (${g1.length} on guns), ${prod.toFixed(1)}% of a month's production stopped in ${n}, lines cut ${fs.filter((f) => /railway/i.test(f.res) && /cut/i.test(f.res)).length}`); } console.log(`BOMBED ${me}: ${ys.join('; ')}`); const ex = onMe.filter((f) => f.guns > 0).slice(0, 2).concat(onMe.filter((f) => f.guns === 0).slice(0, 1)); for (const f of ex) console.log(`BOMBED example ${date(f.turn)} ${f.place} (guns ${f.guns}) by ${f.att} ${f.mission}: ${f.res.slice(0, 260)}`); }
{ const big = run.rows.map((r, i) => ({ r, d: i > 0 ? run.rows[i - 1].myCorps - r.myCorps : 0 })).filter((x) => x.d >= 6); for (const x of big.slice(0, 6)) console.log(`LOST ${x.d} corps in the month before ${x.r.date}: ` + ((deaths[x.r.turn - 1] ?? []).concat(deaths[x.r.turn] ?? [])).slice(0, 4).join(' | ')); }
console.log(`TRAINS long moves (over three provinces in a month) ${longMoves}: ` + Object.entries(longByYear).map(([y, n]) => `${y}: ${n}`).join(', ') + `; of ${railChecks} corps looked at, ${railChecksOpen} had a train on offer; no train: ${Object.entries(noTrain).map(([k, n]) => `${k} ${n}`).join(', ') || 'never'}`);
// DULL: months with nothing to attack, nothing gained or lost, not attacked, no raid flown.
let streak = 0, bestS = 0, bestEnd = 0; const quiet: number[] = [];
for (const row of run.rows) { const m = M(row.turn); const q = row.attacksOrdered === 0 && row.gained === 0 && row.lost === 0 && row.hitByEnemy === 0 && !m.raids.some((x) => !x.startsWith('none')); if (q) { streak++; quiet.push(row.turn); if (streak > bestS) { bestS = streak; bestEnd = row.turn; } } else streak = 0; }
console.log(`DULL: ${quiet.length} of ${run.rows.length} months with no attack ordered, no ground changing hands, no attack on the player and no raid flown; the longest run ${bestS} months${bestS ? `, ${date(bestEnd - bestS + 1)} to ${date(bestEnd)}` : ''}`);
const fell = (n: string): string => (g.nations[n].alive ? 'stands' : `fell ${date(g.nations[n].fellOn?.turn ?? 0)}`);
const POW = ['GER', 'ITA', 'JAP', 'JPN', 'UK', 'FRA', 'SOV', 'USA'].filter((n) => g.nations[n]);
console.log(`SEAT ${me} seed ${seedS || 7}: ${g.nations[me].alive ? `stands at ${date(g.turn)}` : `fell ${date(run.fellOn ?? 0)}`}; provinces ${g.regions.filter((r: any) => r.ctrl === me).length}; ` + POW.filter((n) => n !== me).map((n) => `${n} ${fell(n)}`).join(', ') + `; artillery corps at the end ${g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind === 'art').length}`);
console.log(`ARTILLERY the offer: ${artSay.join('; ')}; months spent saving for guns ${savedMonths}`);
console.log(`BOUGHT ${Object.entries(bought).sort((a, b) => b[1] - a[1]).slice(0, 14).map(([k, n]) => `${k} ${n}`).join(', ')}`);
{ const ys: string[] = []; for (const y of [1941, 1942, 1943, 1944]) { const ts = Object.keys(unsup).map(Number).filter((t) => 1939 + Math.floor((t + 7) / 12) === y); if (ts.length) ys.push(`${y}: ${Math.round((100 * ts.reduce((s, t) => s + unsup[t][0], 0)) / Math.max(1, ts.reduce((s, t) => s + unsup[t][1], 0)))}%`); } console.log(`SUPPLY corps unsupplied as the player's month opens: ${ys.join(', ')}`); }
for (const n of notes) console.log(`NOTE ${n}`);
for (const p of run.problems.slice(0, 30)) console.log(`PROBLEM ${p}`);
for (const e of run.errors) console.log(`ERROR ${e.slice(0, 600)}`);

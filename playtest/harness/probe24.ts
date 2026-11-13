// Government from the player's seat (Version 57 on): every directive the player is given and whether it can be done,
// how the cabinet's trust moves, when the cabinet falls, what ministers and decrees do to the budget.
// usage: probe24 ME FROM SEED UNTIL PLAN-JSON [CABINET]
//   PLAN-JSON may be '{"idle":true}'.  CABINET: none (default) | money | men | calm | most | aside | hawks | doves (the last two from Version 60)
//   Version 60: plan options "vow" (promises) and "head" (sack or stand) are the scripted player's; every head the cabinet asks for is printed with the general's record and his last battles.
import { campaign, date, T, ledger, allied, atWar, headDetail } from './lib';
import { townSize, townName } from '@tc/campaign/towns';
import { limitAttackers } from '@tc/campaign/game';
import { besieged, garrisonOf } from '@tc/campaign/siege';
import * as GOVM from '@tc/campaign/government';
import type { Game } from './lib';
const [me, fromS, seedS, untilS, planS, cabinet = 'none'] = process.argv.slice(2);
const plan = JSON.parse(planS || '{"minOdds":1.5}');
const WANT: Record<string, { seats: [string, string][]; tracks: [string, number][] }> = {
  none: { seats: [], tracks: [] }, aside: { seats: [], tracks: [] },
  hawks: { seats: [['staff', 'organiser'], ['arms', 'industrialist'], ['foreign', 'statesman'], ['finance', 'taxman']], tracks: [] },
  doves: { seats: [['staff', 'reformer'], ['arms', 'planner'], ['foreign', 'merchant'], ['finance', 'paymaster']], tracks: [] },
  money: { seats: [['finance', 'taxman'], ['arms', 'planner']], tracks: [['economy', 1], ['economy', 2]] },
  men: { seats: [['staff', 'organiser']], tracks: [['service', 1], ['service', 2]] },
  calm: { seats: [['finance', 'paymaster'], ['foreign', 'statesman']], tracks: [] },
  most: { seats: [['finance', 'taxman'], ['arms', 'planner'], ['foreign', 'statesman'], ['staff', 'organiser']], tracks: [['economy', 1], ['economy', 2], ['service', 1]] },
};
const want = WANT[cabinet];
const logSeen = new Set<string>(); const trustBy: number[] = [], capBy: number[] = []; let atCap = 0, moneyWon = 0, corpsWon = 0;
let doubted = 0;
let lastHead = -1, heads = 0, headsShown = 0; const wingMonths: Record<string, number> = {};
let lastDir = -1, months = 0, low35 = 0, debtMonths = 0, minTrust = 999, sumTrust = 0, falls: string[] = [], sumNet = 0, sumGross = 0, spent = 0;
const issued: Record<string, number> = {}, ended: Record<string, number> = {};
let open: { kind: string; title: string; reward: string } | null = null;

/** Hops from the nearest corps of mine to a province next to the target, walking only over ground my side holds. -1: no way by land. */
function landHops(g: Game, target: number): number {
  const goal = new Set(g.regions[target].nb.filter((j) => allied(g, me, g.regions[j].ctrl)));
  if (!goal.size) return -1;
  const d = new Int16Array(g.regions.length).fill(-1); const q: number[] = [];
  for (const c of g.corps) if (c.nat === me && !c.dead && c.reg >= 0 && d[c.reg] < 0) { d[c.reg] = 0; q.push(c.reg); }
  for (let i = 0; i < q.length; i++) { if (goal.has(q[i])) return d[q[i]]; for (const j of g.regions[q[i]].nb) if (d[j] < 0 && allied(g, me, g.regions[j].ctrl)) { d[j] = d[q[i]] + 1; q.push(j); } }
  return -1;
}

const dirAt: Record<number, string> = {};
let headSeen = -1, headText = '';
const cardBy: Record<number, { first: number | null; best: number | null; months: number; none: number }> = {};
let openId = -1;
const run = campaign(me, Number(fromS), Number(seedS), { ...plan, first: (g) => {
  // What the cabinet saw when it asked: read before the player gives any order this month.
  // The general the cabinet names, with his last battles as they stood when it asked (before the month's own fights).
  const h0 = (T.governmentOf(g) as any)?.head;
  if (h0 && h0.general !== headSeen) { headSeen = h0.general; headText = headDetail(g, me, h0); }
  if (!h0) headSeen = -1;
  const d = T.governmentOf(g)?.directive;
  if (d && d.region !== undefined && d.region >= 0 && (d.kind === 'take' || d.kind === 'relief')) {
    // The attack card's own odds for the town, as the player would see them this month before he moves a corps.
    const r0 = g.regions[d.region];
    const cs = atWar(g, r0.ctrl, me) ? limitAttackers(g, T.candidates(g, r0), r0) : [];
    const look = cs.length ? T.attackOutlook(g, cs, r0) : null;
    const c = (cardBy[d.id] ??= { first: null, best: null, months: 0, none: 0 });
    c.months++;
    if (look) { if (c.first === null && c.months === 1) c.first = look.odds; c.best = Math.max(c.best ?? 0, look.odds); } else c.none++;
  }
  if (!d || d.region === undefined || d.region < 0 || dirAt[d.id] !== undefined) return;
  const r = g.regions[d.region];
  const foes = r.units.filter((u) => !u.dead && atWar(g, u.nat, me)).reduce((s, u) => s + u.divs * u.str, 0) + garrisonOf(r).filter((u) => !u.dead).reduce((s, u) => s + u.divs * u.str, 0);
  const o = (GOVM as any).directiveOdds?.(g, me, r.id);
  dirAt[d.id] = `when asked: held by ${r.ctrl}, ${foes.toFixed(1)} divs in it${besieged(g, r) ? ', under siege' : ''}${typeof o === 'number' ? `, the cabinet's odds ${o > 99 ? 'over 99' : o.toFixed(2)}` : ''}`;
}, each: (g, run) => {
  const v = T.governmentOf(g);
  if (!v || !v.mine) return;
  months++;
  const L = ledger(g, me) as any;
  sumNet += L.net * L.scale; sumGross += L.gross * L.scale;
  if (g.nations[me].pp < 0) debtMonths++;
  if (v.trust < 35) low35++;
  if ((v as any).doubts) doubted++;
  minTrust = Math.min(minTrust, v.trust); sumTrust += v.trust; trustBy.push(v.trust); capBy.push(v.capital); if (v.capital >= v.capitalMost) atCap++;
  // News from the cabinet's own log since last month.
  const log = [...v.log].reverse();
  for (const e of log) {
    const key = `${e.turn}|${e.text}`; if (logSeen.has(key)) continue; logSeen.add(key);
    if (/^Directive (carried out|not carried out|set aside|withdrawn)/.test(e.text)) { const how = e.text.split(':')[0].replace('Directive ', ''); const k = `${open?.kind ?? '?'} ${how}`; ended[k] = (ended[k] ?? 0) + 1; if (how === 'carried out' && open) { const m = /([\d,]+) for the treasury/.exec(open.reward); if (m) moneyWon += Number(m[1].replace(/,/g, '')); if (/corps/.test(open.reward)) corpsWon++; } const cb = cardBy[openId]; console.log(`   ${date(e.turn)}  => ${how}${open ? '' : ' (?)'}${cb ? ` [the card's odds before the player moved: the month asked ${cb.first === null ? 'no attack' : cb.first.toFixed(2)}, best in ${cb.months} months ${cb.best === null ? 'no attack' : cb.best.toFixed(2)}, months with no attack on the card ${cb.none}]` : ''}`); open = null; }
    else if (/cabinet fell/.test(e.text)) { falls.push(date(e.turn)); console.log(`   ${date(e.turn)}  THE CABINET FELL (trust now ${v.trust})`); }
    else if (!/^Directive:/.test(e.text)) console.log(`   ${date(e.turn)}  ${e.text}`);
  }
  const hd = (v as any).head;
  const answered = ((run as any).cab?.heads ?? []) as any[];
  for (; headsShown < answered.length; headsShown++) { heads++; console.log(`${date(answered[headsShown].turn)}  HEAD asked: ${answered[headsShown].detail}; the player's answer: ${answered[headsShown].did}${answered[headsShown].ok ? '' : ' (REFUSED BY THE RULES)'}`); }
  if (hd && hd.general !== lastHead) { lastHead = hd.general; heads++; console.log(`${date(g.turn)}  HEAD asked: ${headSeen === hd.general && headText ? headText : headDetail(g, me, hd) + ' (READ AFTER THE MONTH)'}; the player's answer: none`); }
  if (!hd) lastHead = -1;
  const wg = (v as any).wings; if (wg) { wingMonths[wg.top] = (wingMonths[wg.top] ?? 0) + 1; }
  const d = v.directive;
  if (d && d.id !== lastDir) {
    lastDir = d.id; openId = d.id; issued[d.kind] = (issued[d.kind] ?? 0) + 1; open = { kind: d.kind, title: d.title, reward: d.reward };
    let extra = '';
    if (d.region !== undefined && d.region >= 0) {
      const r = g.regions[d.region];
      const mineNb = r.nb.filter((j) => g.regions[j].ctrl === me).length, corpsNb = r.nb.reduce((s, j) => s + g.regions[j].units.filter((c) => c.nat === me && !c.dead).length, 0);
      const foes = r.units.filter((u) => !u.dead && atWar(g, u.nat, me)).reduce((s, u) => s + u.divs * u.str, 0) + garrisonOf(r).filter((u) => !u.dead).reduce((s, u) => s + u.divs * u.str, 0);
      const beside = g.corps.filter((c) => c.nat === me && !c.dead && c.reg >= 0 && g.regions[c.reg].nb.includes(r.id)).reduce((s, c) => s + c.divs * c.str, 0);
      const cands = limitAttackers(g, T.candidates(g, r), r); const look = cands.length ? T.attackOutlook(g, cands, r) : null;
      const odds = `my strength beside it ${Math.round(beside)} divs; odds now ${look ? look.odds.toFixed(2) : 'none'}`;
      extra = d.kind === 'hold' ? ` [${townSize(g, r)}${r.fort ? ' fort ' + r.fort.lv : ''}; ${besieged(g, r) ? 'under siege' : 'threatened'}; my troops there ${Math.round(r.units.filter((u) => !u.dead && allied(g, u.nat, me)).reduce((s, u) => s + u.divs * u.str, 0) + garrisonOf(r).filter((u) => !u.dead).reduce((s, u) => s + u.divs * u.str, 0))} divs]`
        : ` [${townSize(g, r)}${r.fort ? ' fort ' + r.fort.lv : ''}${r.cap ? ' CAPITAL' : ''} of ${r.ctrl}; my provinces beside it ${mineNb}, my corps beside it ${corpsNb}, land hops for my nearest corps ${landHops(g, r.id)}; defenders ${Math.round(foes)} divs; ${odds}]`;
    } else extra = ` [army now ${Math.floor(g.corps.filter((c) => c.nat === me && !c.dead).reduce((s, c) => s + c.divs, 0))} divs, target ${d.target}; treasury ${Math.round(L.treasury * L.scale)}, net ${Math.round(L.net * L.scale)}]`;
    console.log(`${date(g.turn)}  DIRECTIVE ${d.kind} from ${d.by}: ${d.title}; ${d.left} months; reward ${d.reward}; price if missed ${d.price}${extra}${dirAt[d.id] ? ' | ' + dirAt[d.id] : ''}${(d as any).hard ? ' | strongly held' : ''}${(d as any).again ? ' | asked before' : ''}${(d as any).asidePrice !== undefined ? ` | aside costs ${(d as any).asidePrice}` : ''}`);
    if (cabinet === 'aside') T.orderSetAside(g);
  }
  // The cabinet the plan wants, as capital allows.
  for (const [seat, m] of want.seats) { const s = v.seats.find((x) => x.id === seat)!; if (s.minister.id !== m && !T.whyNotAppointMinister(g, seat, m)) { if (T.orderAppoint(g, seat, m)) spent += 6; } }
  for (const [track, step] of want.tracks) { const t = T.governmentOf(g)!.tracks.find((x) => x.id === track)!; if (t.at === step - 1 && !T.whyNotOrderDecree(g, track, step)) { if (T.orderDecree(g, track, step)) spent += t.steps[step].cost; break; } }
  if ((g.turn - 1) % 6 === 0) {
    const w = T.governmentOf(g)!;
    console.log(`${date(g.turn)}: trust ${w.trust} (${w.trustWord}; settles at ${w.rest}; falls at ${w.fallsAt}, low for ${w.low}), capital ${w.capital} (+${w.capitalMonth}), provinces ${g.regions.filter((r) => r.ctrl === me).length}, divs ${Math.round(g.corps.filter((c) => c.nat === me && !c.dead).reduce((s, c) => s + c.divs, 0))}, net ${Math.round(L.net * L.scale)}, treasury ${Math.round(L.treasury * L.scale)}; ${w.sums.join(', ') || 'caretakers'}; ${(w as any).wings ? `table ${(w as any).wings.word} (lean ${(w as any).wings.lean}); ` : ''}${(w as any).promise ? `promise: ${(w as any).promise.title ?? (w as any).promise.town} (${(w as any).promise.band}, ${(w as any).promise.left} months left); ` : ''} moved: ${w.moved.map((m) => `${m.label} ${m.amount > 0 ? '+' : ''}${m.amount}`).join(', ') || 'nothing'}`);
  }
} }, { until: Number(untilS) });
for (const e of run.errors) console.log('ERROR', String(e).slice(0, 600));
console.log(`TRUST ${trustBy.join(' ')}`);
console.log(`CAPITAL ${capBy.join(' ')}`);
console.log(`TOTAL ${me} seed ${seedS} cabinet ${cabinet}${plan.idle ? ' IDLE' : ''}: ${months} months; directives issued ${JSON.stringify(issued)}; ended ${JSON.stringify(ended)}; cabinet fell ${falls.length} times ${falls.join(', ')}; trust mean ${(sumTrust / Math.max(1, months)).toFixed(0)}, lowest ${minTrust}, months under 35: ${low35}; months the cabinet doubted the player: ${doubted}; heads asked ${heads}; promises kept ${(T.governmentOf(run.g) as any)?.kept ?? '-'}, broken ${(T.governmentOf(run.g) as any)?.broken ?? '-'}; promises made ${JSON.stringify(((run as any).cab?.vows ?? []).reduce((m: any, x: any) => { m[x.band] = (m[x.band] ?? 0) + 1; return m; }, {}))}; months by wing on top ${JSON.stringify(wingMonths)}; months in debt ${debtMonths}; capital spent on the cabinet ${spent}; months with capital at its most ${atCap}; directives paid ${moneyWon} and ${corpsWon} corps; income in all gross ${Math.round(sumGross)} net ${Math.round(sumNet)}`);

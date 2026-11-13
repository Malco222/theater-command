// Government from the player's seat (Version 57 on): every directive the player is given and whether it can be done,
// how the cabinet's trust moves, when the cabinet falls, what ministers and decrees do to the budget.
// usage: probe24 ME FROM SEED UNTIL PLAN-JSON [CABINET]
//   PLAN-JSON may be '{"idle":true}'.  CABINET: none (default) | money | men | calm | most | aside
import { campaign, date, T, ledger, allied, atWar } from './lib';
import { townSize, townName } from '@tc/campaign/towns';
import { besieged, garrisonOf } from '@tc/campaign/siege';
import type { Game } from './lib';
const [me, fromS, seedS, untilS, planS, cabinet = 'none'] = process.argv.slice(2);
const plan = JSON.parse(planS || '{"minOdds":1.5}');
const WANT: Record<string, { seats: [string, string][]; tracks: [string, number][] }> = {
  none: { seats: [], tracks: [] }, aside: { seats: [], tracks: [] },
  money: { seats: [['finance', 'taxman'], ['arms', 'planner']], tracks: [['economy', 1], ['economy', 2]] },
  men: { seats: [['staff', 'organiser']], tracks: [['service', 1], ['service', 2]] },
  calm: { seats: [['finance', 'paymaster'], ['foreign', 'statesman']], tracks: [] },
  most: { seats: [['finance', 'taxman'], ['arms', 'planner'], ['foreign', 'statesman'], ['staff', 'organiser']], tracks: [['economy', 1], ['economy', 2], ['service', 1]] },
};
const want = WANT[cabinet];
let lastDir = -1, logSeen = 0, months = 0, low35 = 0, debtMonths = 0, minTrust = 999, sumTrust = 0, falls: string[] = [], sumNet = 0, sumGross = 0, spent = 0;
const issued: Record<string, number> = {}, ended: Record<string, number> = {};
let open: { kind: string; title: string } | null = null;

/** Hops from the nearest corps of mine to a province next to the target, walking only over ground my side holds. -1: no way by land. */
function landHops(g: Game, target: number): number {
  const goal = new Set(g.regions[target].nb.filter((j) => allied(g, me, g.regions[j].ctrl)));
  if (!goal.size) return -1;
  const d = new Int16Array(g.regions.length).fill(-1); const q: number[] = [];
  for (const c of g.corps) if (c.nat === me && !c.dead && c.reg >= 0 && d[c.reg] < 0) { d[c.reg] = 0; q.push(c.reg); }
  for (let i = 0; i < q.length; i++) { if (goal.has(q[i])) return d[q[i]]; for (const j of g.regions[q[i]].nb) if (d[j] < 0 && allied(g, me, g.regions[j].ctrl)) { d[j] = d[q[i]] + 1; q.push(j); } }
  return -1;
}

const run = campaign(me, Number(fromS), Number(seedS), { ...plan, each: (g) => {
  const v = T.governmentOf(g);
  if (!v || !v.mine) return;
  months++;
  const L = ledger(g, me) as any;
  sumNet += L.net * L.scale; sumGross += L.gross * L.scale;
  if (g.nations[me].pp < 0) debtMonths++;
  if (v.trust < 35) low35++;
  minTrust = Math.min(minTrust, v.trust); sumTrust += v.trust;
  // News from the cabinet's own log since last month.
  const log = [...v.log].reverse();
  for (const e of log.slice(logSeen)) {
    if (/^Directive (carried out|not carried out|set aside|withdrawn)/.test(e.text)) { const how = e.text.split(':')[0].replace('Directive ', ''); const k = `${open?.kind ?? '?'} ${how}`; ended[k] = (ended[k] ?? 0) + 1; console.log(`   ${date(e.turn)}  => ${how}${open ? '' : ' (?)'}`); open = null; }
    else if (/cabinet fell/.test(e.text)) { falls.push(date(e.turn)); console.log(`   ${date(e.turn)}  THE CABINET FELL (trust now ${v.trust})`); }
    else if (!/^Directive:/.test(e.text)) console.log(`   ${date(e.turn)}  ${e.text}`);
  }
  logSeen = log.length;
  const d = v.directive;
  if (d && d.id !== lastDir) {
    lastDir = d.id; issued[d.kind] = (issued[d.kind] ?? 0) + 1; open = { kind: d.kind, title: d.title };
    let extra = '';
    if (d.region !== undefined && d.region >= 0) {
      const r = g.regions[d.region];
      const mineNb = r.nb.filter((j) => g.regions[j].ctrl === me).length, corpsNb = r.nb.reduce((s, j) => s + g.regions[j].units.filter((c) => c.nat === me && !c.dead).length, 0);
      const foes = r.units.filter((u) => !u.dead && atWar(g, u.nat, me)).reduce((s, u) => s + u.divs * u.str, 0) + garrisonOf(r).filter((u) => !u.dead).reduce((s, u) => s + u.divs * u.str, 0);
      const beside = g.corps.filter((c) => c.nat === me && !c.dead && c.reg >= 0 && g.regions[c.reg].nb.includes(r.id)).reduce((s, c) => s + c.divs * c.str, 0);
      const odds = `my strength beside it ${Math.round(beside)} divs`;
      extra = d.kind === 'hold' ? ` [${townSize(g, r)}${r.fort ? ' fort ' + r.fort.lv : ''}; ${besieged(g, r) ? 'under siege' : 'threatened'}; my troops there ${Math.round(r.units.filter((u) => !u.dead && allied(g, u.nat, me)).reduce((s, u) => s + u.divs * u.str, 0) + garrisonOf(r).filter((u) => !u.dead).reduce((s, u) => s + u.divs * u.str, 0))} divs]`
        : ` [${townSize(g, r)}${r.fort ? ' fort ' + r.fort.lv : ''}${r.cap ? ' CAPITAL' : ''} of ${r.ctrl}; my provinces beside it ${mineNb}, my corps beside it ${corpsNb}, land hops for my nearest corps ${landHops(g, r.id)}; defenders ${Math.round(foes)} divs; ${odds}]`;
    } else extra = ` [army now ${Math.floor(g.corps.filter((c) => c.nat === me && !c.dead).reduce((s, c) => s + c.divs, 0))} divs, target ${d.target}; treasury ${Math.round(L.treasury * L.scale)}, net ${Math.round(L.net * L.scale)}]`;
    console.log(`${date(g.turn)}  DIRECTIVE ${d.kind} from ${d.by}: ${d.title}; ${d.left} months; reward ${d.reward}; price if missed ${d.price}${extra}`);
    if (cabinet === 'aside') T.orderSetAside(g);
  }
  // The cabinet the plan wants, as capital allows.
  for (const [seat, m] of want.seats) { const s = v.seats.find((x) => x.id === seat)!; if (s.minister.id !== m && !T.whyNotAppointMinister(g, seat, m)) { if (T.orderAppoint(g, seat, m)) spent += 6; } }
  for (const [track, step] of want.tracks) { const t = T.governmentOf(g)!.tracks.find((x) => x.id === track)!; if (t.at === step - 1 && !T.whyNotOrderDecree(g, track, step)) { if (T.orderDecree(g, track, step)) spent += t.steps[step].cost; break; } }
  if ((g.turn - 1) % 6 === 0) {
    const w = T.governmentOf(g)!;
    console.log(`${date(g.turn)}: trust ${w.trust} (${w.trustWord}; settles at ${w.rest}; falls at ${w.fallsAt}, low for ${w.low}), capital ${w.capital} (+${w.capitalMonth}), provinces ${g.regions.filter((r) => r.ctrl === me).length}, divs ${Math.round(g.corps.filter((c) => c.nat === me && !c.dead).reduce((s, c) => s + c.divs, 0))}, net ${Math.round(L.net * L.scale)}, treasury ${Math.round(L.treasury * L.scale)}; ${w.sums.join(', ') || 'caretakers'}; moved: ${w.moved.map((m) => `${m.label} ${m.amount > 0 ? '+' : ''}${m.amount}`).join(', ') || 'nothing'}`);
  }
} }, { until: Number(untilS) });
for (const e of run.errors) console.log('ERROR', String(e).slice(0, 600));
console.log(`TOTAL ${me} seed ${seedS} cabinet ${cabinet}${plan.idle ? ' IDLE' : ''}: ${months} months; directives issued ${JSON.stringify(issued)}; ended ${JSON.stringify(ended)}; cabinet fell ${falls.length} times ${falls.join(', ')}; trust mean ${(sumTrust / Math.max(1, months)).toFixed(0)}, lowest ${minTrust}, months under 35: ${low35}; months in debt ${debtMonths}; capital spent on the cabinet ${spent}; income in all gross ${Math.round(sumGross)} net ${Math.round(sumNet)}`);

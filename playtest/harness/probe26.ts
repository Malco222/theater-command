// Version 57: what each minister and decree is worth in money at one moment of one war, read from the ledger.
// usage: probe26 ME FROM SEED AT-TURN PLAN-JSON
// At that month the probe gives the player all the political capital the levers need (a test cheat, on this run only),
// then pulls them one at a time and prints income, upkeep, what is left, and the price of an infantry and an armoured corps.
import { campaign, date, T, ledger } from './lib';
import { corpsCost } from '@tc/campaign/economy';
const [me, fromS, seedS, atS, planS] = process.argv.slice(2);
const at = Number(atS);
let done = false;
const row = (g: any, label: string, base?: any): any => {
  const L = ledger(g, me) as any, s = L.scale;
  const cab = L.changes.find((c: any) => c.key === 'cabinet');
  const r = { gross: Math.round(L.gross * s), upkeep: Math.round(L.upkeepTotal * s), net: Math.round(L.net * s), inf: Math.round(corpsCost(g, me, 'inf') * s), arm: Math.round(corpsCost(g, me, 'arm') * s), cab: cab ? Math.round(cab.amount * s) : 0 };
  const pct = (a: number, b: number): string => (b ? `${a >= b ? '+' : ''}${((a / b - 1) * 100).toFixed(1)}%` : '');
  console.log(`${label.padEnd(46)} income ${String(r.gross).padStart(6)} ${base ? pct(r.gross, base.gross).padStart(7) : '       '}  upkeep ${r.upkeep}  left ${String(r.net).padStart(6)} ${base ? pct(r.net, base.net).padStart(7) : '       '}  ledger line "cabinet" ${r.cab}  infantry corps ${r.inf}  armoured corps ${r.arm}`);
  return r;
};
campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS), each: (g: any) => {
  if (g.turn !== at || done) return;
  done = true;
  const n = g.nations[me];
  console.log(`${me} ${date(g.turn)}: political capital ${n.pc}, treasury ${Math.round(n.pp)}`);
  const base = row(g, 'caretakers, no decree');
  const pull = (label: string, f: () => unknown): void => { n.pc = 30; const why = f(); row(g, `${label}${why === true ? '' : ` (REFUSED: ${why})`}`, base); };
  const ap = (seat: string, m: string) => () => T.whyNotAppointMinister(g, seat, m) ?? T.orderAppoint(g, seat, m);
  const de = (track: string, step: number) => () => T.whyNotOrderDecree(g, track, step) ?? T.orderDecree(g, track, step);
  pull('+ the taxman', ap('finance', 'taxman'));
  pull('+ the planner', ap('arms', 'planner'));
  pull('+ war economy', de('economy', 1));
  pull('+ total mobilisation', de('economy', 2));
  pull('+ the statesman', ap('foreign', 'statesman'));
  pull('+ the organiser', ap('staff', 'organiser'));
  pull('+ general conscription', de('service', 1));
  pull('+ the last levy', de('service', 2));
  const v = T.governmentOf(g)!;
  console.log(`  the screen sums it up as: ${v.sums.join(' | ')}`);
  console.log(`  trust ${v.trust}, settles at ${v.rest}, falls at ${v.fallsAt}; capital a month ${v.capitalMonth}`);
  process.exit(0);
} }, { until: at + 1, quiet: true });

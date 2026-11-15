// Version 71: the west as the British player by hand finds it. usage: probe61 SEED [UNTIL=58]
// One WEST line a war: at January 1942, January 1943, June 1943, December 1943 and May 1944, the German corps standing
// in France and the Low Countries, who holds those provinces, where the player's corps that could sail are standing
// (home ports, Gibraltar, the Mediterranean), and the lowest landing risk on offer that month with its odds.
import { campaign, date } from './lib';
const [seedS, untilS] = process.argv.slice(2);
const LOW = new Set(['FRA', 'BEL', 'NED', 'LUX']);
const plan: any = { minOdds: 1.5, land: true, buy: ['corps:arm', 'corps:inf', 'air:ftr', 'ship:dd', 'work:factory'] };
const at = new Set([29, 41, 46, 52, 57]);
const out: string[] = [];
let landed: string[] = [];
const run = campaign('UK', 1, Number(seedS), { ...plan, each: (g: any) => {
  if (!at.has(g.turn)) return;
  const regs = g.regions.filter((r: any) => LOW.has(r.owner) && !r.overseas && r.name !== 'Corsica');
  const by: Record<string, number> = {};
  for (const r of regs) by[r.ctrl] = (by[r.ctrl] ?? 0) + 1;
  const ger = g.corps.filter((c: any) => c.nat === 'GER' && !c.dead && c.kind !== 'fort' && regs.includes(g.regions[c.reg])).length;
  const gerAll = g.corps.filter((c: any) => c.nat === 'GER' && !c.dead && c.kind !== 'fort').length;
  const mine = g.corps.filter((c: any) => c.nat === 'UK' && !c.dead && c.kind !== 'fort');
  const home = mine.filter((c: any) => g.regions[c.reg].owner === 'UK' && !g.regions[c.reg].overseas).length;
  const homePort = mine.filter((c: any) => g.regions[c.reg].owner === 'UK' && !g.regions[c.reg].overseas && g.regions[c.reg].port).length;
  out.push(`${date(g.turn)}: German corps there ${ger} of ${gerAll}, held by ${Object.entries(by).map(([k, v]) => `${k} ${v}`).join(' ')}, British corps ${mine.length} (at home ${home}, in a home port ${homePort}), Germany ${g.nations.GER.alive ? 'stands' : 'fallen'}`);
} } as any, { until: Number(untilS || 58) });
const log: string[] = (run as any).landLog ?? [];
for (const r of run.rows) for (const n of r.notes) if (/landing/.test(n) && r.turn >= 29) landed.push(`${r.date} ${n.replace(/, [0-9.]+ lost at sea/, '')}`);
console.log(`WEST UK seed ${seedS}: ${out.join(' | ')} | landings from 1942: ${landed.join('; ') || 'none'} | landing log lines ${log.length}`);

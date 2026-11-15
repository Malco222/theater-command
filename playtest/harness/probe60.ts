// Version 71: why the British player by hand gets ashore later. usage: probe60 SEED [UNTIL=76]
// One line a month from January 1942: the player's corps standing in the west, German corps in the west's coast
// provinces and in the whole west, the landings the player ordered and how they went, and why none was made.
import { campaign, date } from './lib';
const [seedS, untilS] = process.argv.slice(2);
const WEST = new Set(['FRA', 'BEL', 'NED', 'LUX', 'GER', 'DEN']);
const plan: any = { minOdds: 1.5, land: true, buy: ['corps:arm', 'corps:inf', 'air:ftr', 'ship:dd', 'work:factory'] };
const lines: string[] = [];
let logAt = 0;
const run = campaign('UK', 1, Number(seedS), { ...plan, each: (g: any) => {
  if (g.turn < 29) return;
  const inWest = (c: any) => !c.dead && c.kind !== 'fort' && WEST.has(g.regions[c.reg].owner) && !g.regions[c.reg].overseas;
  const mine = g.corps.filter((c: any) => c.nat === 'UK' && inWest(c) && g.regions[c.reg].owner !== 'UK').length;
  const ger = g.corps.filter((c: any) => c.nat === 'GER' && inWest(c));
  const coast = ger.filter((c: any) => g.regions[c.reg].coast || g.regions[c.reg].port).length;
  const fra = ger.filter((c: any) => ['FRA', 'BEL', 'NED', 'LUX'].includes(g.regions[c.reg].owner)).length;
  const log: string[] = (run0 as any)?.landLog ?? [];
  const fresh = log.slice(logAt).filter((l) => l.startsWith(date(g.turn))); logAt = log.length;
  const best = fresh.map((l) => l.split(': ').slice(1).join(': ')).slice(0, 3).join(' ; ');
  lines.push(`${date(g.turn)}: British corps in the west ${mine}; German corps in France and the Low Countries ${fra}, in the whole west ${ger.length}; GER ${g.nations.GER.alive ? 'stands' : 'fallen'}; landing log: ${best || '-'}`);
} } as any, { until: Number(untilS || 76) });
var run0: any = run;
for (const r of run.rows) for (const n of r.notes) if (/landing/.test(n)) console.log(`NOTE ${r.date}: ${n}`);
console.log(lines.join('\n'));
const log: string[] = (run as any).landLog ?? [];
const why: Record<string, number> = {};
for (const l of log) { const w = l.split(': ').slice(1).join(': ').replace(/[0-9.]+/g, '#'); why[w] = (why[w] ?? 0) + 1; }
for (const l of log) if (/^(June 1943|February 1944|March 1944)/.test(l)) console.log('LOG ' + l);
console.log(`LANDLOG ${log.length} lines; sealift at the end ${run.g.nations.UK.sealift}`);
console.log('LANDLOG by reason: ' + Object.entries(why).sort((a, b) => b[1] - a[1]).slice(0, 12).map(([k, v]) => `${v}x ${k}`).join(' | '));

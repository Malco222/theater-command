// Version 57: writes saves for the page at moments of the government worth looking at.
// usage: probe25 ME FROM SEED UNTIL PLAN-JSON OUT-PREFIX
// Saves: OUT-start.json (the player's first month), OUT-directive.json (the first month a directive stands),
// OUT-doubt.json (the first month the cabinet's trust is under 35), OUT-crisis.json (the first month at or under the line it falls at),
// OUT-late.json (a directive with two months or fewer left), OUT-rich.json (the first month with 30 political capital).
import { writeFileSync } from 'node:fs';
import { campaign, date, T, saveGame } from './lib';
const [me, fromS, seedS, untilS, planS, out] = process.argv.slice(2);
const got = new Set<string>();
const keep = (g: any, k: string, why: string): void => { if (got.has(k)) return; got.add(k); writeFileSync(`${out}-${k}.json`, saveGame(g)); console.log(`${date(g.turn)}: saved ${k} (${why})`); };
const run = campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS), each: (g: any) => {
  const v = T.governmentOf(g);
  if (!v?.mine) return;
  keep(g, 'start', `trust ${v.trust}, capital ${v.capital}`);
  if (v.directive) keep(g, 'directive', `${v.directive.title}, ${v.directive.left} months left`);
  if (v.directive && v.directive.left <= 2) keep(g, 'late', `${v.directive.title}, ${v.directive.left} left`);
  if (v.trust < 35) keep(g, 'doubt', `trust ${v.trust}`);
  if (v.low > 0 || v.trust <= v.fallsAt) keep(g, 'crisis', `trust ${v.trust}, low ${v.low}, falls at ${v.fallsAt}`);
  if (v.capital >= v.capitalMost) keep(g, 'rich', `capital ${v.capital}`);
} }, { until: Number(untilS), quiet: true });
for (const e of (run as any).errors ?? []) console.log('ERROR', e);
console.log('saved:', [...got].join(', '));

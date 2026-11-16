// Version 81 and later: an American PLAYER with a computer-led enemy next door (the rules builder's "worth your eye":
// a hostile Panama). usage: probe86 SEED [UNTIL=76] [FOE=PAN] [T=13] [PLAN-JSON]   (build with build70.sh)
// The player plays the United States by the plan of the seat set (by hand with the stage plan, or {"depth":"field"});
// the FOE turns to the Axis and declares war on the United States in month T. One FOEP line a war: when the foe
// declared, who held the Canal Zone and when it changed hands, the foe's corps on American ground, the first month an
// American corps stood on the foe's soil, when the foe fell, battles between the two, Germany's fall, and the news
// lines that named the foe or the Canal.
import { campaign, date } from './lib';
import { declareWar } from '@tc/campaign/game';
const [seedS, untilS = '76', foe = 'PAN', tS = '13', planS] = process.argv.slice(2);
const T0 = Number(tS);
const plan = { minOdds: 1.5, land: true, stage: true, buy: ['corps:arm', 'corps:inf', 'air:ftr', 'ship:dd', 'work:factory'], ...JSON.parse(planS || '{}') };
if (plan.depth) { delete (plan as any).stage; delete (plan as any).buy; }
let declared = 0, onFoe = 0, foeOnUs = 0, seen = 0, canalWas = '', fights = 0, gerFell = 0, foeFell = 0;
const canal: string[] = [], news: string[] = [], seenRep = new Set<number>();
const look = (g: any): void => {
  const F = g.nations[foe];
  if (!declared && g.turn === T0 && F?.alive) { F.align = 'axis'; declareWar(g, foe, 'USA'); declared = g.turn; }
  const cz = g.byName['Canal Zone'];
  if (cz && cz.ctrl !== canalWas) { canal.push(`${cz.ctrl} from ${date(g.turn)}`); canalWas = cz.ctrl; }
  if (declared) {
    if (!onFoe && g.regions.some((r: any) => r.owner === foe && r.units.some((u: any) => u.nat === 'USA' && u.kind !== 'fort'))) onFoe = g.turn;
    if (!foeOnUs && g.regions.some((r: any) => r.owner === 'USA' && r.units.some((u: any) => u.nat === foe && u.kind !== 'fort'))) foeOnUs = g.turn;
    if (!foeFell && F && !F.alive) foeFell = F.fellOn?.turn ?? g.turn;
  }
  if (!gerFell && !g.nations.GER.alive) gerFell = g.nations.GER.fellOn?.turn ?? g.turn;
  for (const r of g.reports ?? []) { if (seenRep.has(r.id)) continue; seenRep.add(r.id); const s = JSON.stringify(r); if (s.includes(`"${foe}"`) && s.includes('"USA"')) fights++; }
  for (const h of g.headlines.slice(seen)) if (declared && /Panama|Canal/.test(h.text) && news.length < 12) news.push(`${date(h.turn)}: ${h.text.slice(0, 170)}`);
  seen = g.headlines.length;
};
const run = campaign('USA', 1, Number(seedS), { ...plan, first: look, each: look } as any, { until: Number(untilS), quiet: true } as any);
const g: any = run.g; look(g);
const F = g.nations[foe];
console.log(`FOEP seed ${seedS} ${plan.depth ? 'at ' + plan.depth : 'by hand'}: ${foe} declared ${declared ? date(declared) : 'never'} | Canal Zone: ${canal.join(', ')} | ${foe} corps on American ground ${foeOnUs ? 'from ' + date(foeOnUs) : 'never'} | first American corps on its soil ${onFoe ? date(onFoe) + ` (${onFoe - declared} months)` : 'never'} | ${foe} ${F.alive ? 'stands at the end' : `fell ${date(foeFell)} (${foeFell - declared} months, to ${F.fellOn?.to})`} | battles between them ${fights} | Germany ${gerFell ? 'fell ' + date(gerFell) : 'stands'} | errors ${run.errors.length}`);
for (const n of news) console.log(`   NEWS ${n}`);

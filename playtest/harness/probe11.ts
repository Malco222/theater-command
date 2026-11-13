// Computer-only wars: every country that falls, to whom, and whether that makes sense; and how assaults on towns go.
// usage: probe11 SEEDS UNTIL
import { start, date, T, atWar, allied } from './lib';
import { besieged, garrisonOf, guardOf } from '@tc/campaign/siege';
const seeds = (process.argv[2] ?? '1,2,3,4,5,6').split(',').map(Number);
const until = Number(process.argv[3] ?? 88);
let odd = 0, falls = 0;
const A = { n: 0, took: 0, attMen: 0, attLost: 0, defMen: 0, defLost: 0, odds: [] as number[], hasty: 0 };
const F = { n: 0, won: 0, attMen: 0, attLost: 0, defMen: 0, defLost: 0 };
const hungerFalls: string[] = [];
for (const seed of seeds) {
  const g = start(null, 1, seed);
  let p0 = g.passings.length, r0 = g.reports.length, h0 = g.headlines.length;
  const invested = new Map<number, number>();
  while (g.turn <= until) {
    const t = g.turn;
    const before = g.regions.map((r) => ({ ctrl: r.ctrl, nats: [...new Set(r.units.filter((u) => !u.dead).map((u) => u.nat))] }));
    T.runTurnAI(g);
    for (const r of g.regions) { if (besieged(g, r)) { if (!invested.has(r.id)) invested.set(r.id, t); } else invested.delete(r.id); }
    for (const rep of g.reports.slice(r0) as any[]) {
      const am = rep.attackers.reduce((s: number, u: any) => s + u.men, 0), dm = rep.defenders.reduce((s: number, u: any) => s + u.men, 0);
      if (rep.town) { A.n++; if (rep.taken) A.took++; A.attMen += am; A.attLost += rep.attLost; A.defMen += dm; A.defLost += rep.defLost; A.odds.push(rep.odds); }
      else if (!rep.sally) { F.n++; if (rep.taken || rep.siege) F.won++; F.attMen += am; F.attLost += rep.attLost; F.defMen += dm; F.defLost += rep.defLost; }
    }
    r0 = g.reports.length;
    for (const h of g.headlines.slice(h0) as any[]) if (/starv|hunger|opens its gates|gives in|out of food|stores are gone/i.test(h.text)) hungerFalls.push(`seed ${seed} ${date(h.turn ?? t)}: ${h.text.slice(0, 150)}`);
    h0 = g.headlines.length;
    for (const p of g.passings.slice(p0) as any[]) {
      if (p.kind !== 'fell') continue;
      falls++;
      const loser = g.nations[p.nat], cap = g.regions[p.capital];
      const home = g.regions.filter((r) => r.owner === p.nat && !r.overseas);
      const b = before;
      const capHolderBefore = cap ? b[cap.id].ctrl : '-', capFieldBefore = cap ? b[cap.id].nats.filter((n) => n !== p.nat).join('+') : '';
      const byHadTroops = home.filter((r) => b[r.id].nats.includes(p.by) || b[r.id].ctrl === p.by).length;
      const near = home.some((r) => r.nb.some((j) => b[j].nats.includes(p.by) || b[j].ctrl === p.by));
      const strange = p.how === 'beaten' && byHadTroops === 0 && !near;
      const others: Record<string, number> = {};
      for (const r of home) for (const n of b[r.id].nats) if (n !== p.nat && atWar(g, n, p.nat) || (n !== p.nat && !g.nations[p.nat].alive && !allied(g, n, p.nat))) others[n] = (others[n] ?? 0) + 1;
      const line = `seed ${seed} ${date(p.turn)}: ${loser.name} fell to ${p.by} (${p.how}); capital ${cap?.name ?? '-'} was held by ${capHolderBefore}${capFieldBefore ? ' with ' + capFieldBefore + ' round it' : ''}; ${p.by} held or stood on ${byHadTroops} of ${home.length} home provinces${near ? '' : ', and was not next to any'}; others on its home ground: ${Object.entries(others).map(([k, v]) => `${k} ${v}`).join(', ') || 'none'}`;
      const wrongVictor = p.how === 'beaten' && cap && capHolderBefore !== p.nat && capHolderBefore !== p.by && !allied(g, capHolderBefore, p.by);
      const beaten = p.how === 'beaten';
      const bigger = beaten ? Object.entries(others).filter(([k, v]) => k !== p.by && v > byHadTroops + 2) : [];
      if (strange || wrongVictor || bigger.length) { odd++; console.log('ODD   ' + line); }
      else if (['GER', 'SOV', 'ITA', 'FRA', 'UK'].includes(p.nat)) console.log('      ' + line);
    }
    p0 = g.passings.length;
  }
}
const med = (a: number[]): number => { const s = [...a].sort((x, y) => x - y); return s.length ? s[Math.floor(s.length / 2)] : 0; };
console.log(`\n${falls} falls, ${odd} odd`);
console.log(`assaults on a town: ${A.n}, took it ${A.took} (${Math.round(100 * A.took / Math.max(1, A.n))}%); median odds ${med(A.odds).toFixed(2)}, a quarter under ${[...A.odds].sort((x, y) => x - y)[Math.floor(A.odds.length / 4)]?.toFixed(2)}; attackers lost ${(100 * A.attLost / Math.max(1, A.attMen)).toFixed(1)}% of those engaged, defenders ${(100 * A.defLost / Math.max(1, A.defMen)).toFixed(1)}%`);
console.log(`battles in the field: ${F.n}, won ${F.won} (${Math.round(100 * F.won / Math.max(1, F.n))}%); attackers lost ${(100 * F.attLost / Math.max(1, F.attMen)).toFixed(1)}% of those engaged, defenders ${(100 * F.defLost / Math.max(1, F.defMen)).toFixed(1)}%`);
console.log(`headlines about hunger or gates: ${hungerFalls.length}`); for (const h of hungerFalls.slice(0, 12)) console.log('   ' + h);

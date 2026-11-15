// Version 75: where the computer's American air groups stand in a war with nobody playing. One line a war: at each
// June, and the month Germany falls, the groups at home and across the ocean (and on whose ground).
// node out75b/probe72.mjs SEEDS(a,b,c) [UNTIL=80] [NAT=USA]     (build with build70.sh)
import { start, date, T } from './lib';
const [seedsS = '17', untilS = '80', nat = 'USA'] = process.argv.slice(2);
for (const seed of seedsS.split(',').map(Number)) {
  const g: any = start(null, 1, seed, 'colonel', 3);
  const at: string[] = []; let gerFell = 0, mostOver = 0;
  const where = (): string => {
    const A = (g.airFleets ?? g.air ?? []).filter((f: any) => f.nat === nat && !f.dead);
    const st: Record<string, number> = {};
    for (const f of A) { const r = g.regions[f.reg ?? f.base ?? f.at]; const k = !r ? '?' : r.owner === nat ? 'home' : r.owner; st[k] = (st[k] ?? 0) + 1; }
    const over = A.length - (st.home ?? 0); if (over > mostOver) mostOver = over;
    return `${A.length} groups (${Object.entries(st).sort((a, b) => b[1] - a[1]).map(([k, v]) => `${k} ${v}`).join(', ') || 'none'})`;
  };
  while (g.turn <= Number(untilS)) {
    const t = g.turn;
    T.runTurnAI(g);
    const w = where();
    if ([34, 46, 58].includes(t)) at.push(`${date(t)} ${w}`);
    if (!gerFell && g.nations.GER && !g.nations.GER.alive) { gerFell = t; at.push(`when Germany fell (${date(t)}) ${w}`); break; }
  }
  console.log(`AIR ${nat} seed ${seed}: ${at.join(' | ')} | the most groups across the ocean in any month ${mostOver}`);
}

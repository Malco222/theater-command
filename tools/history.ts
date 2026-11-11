// Plays the war with every nation on the computer and prints what happened.
// Usage: npm run history -- [seed] [turns]   or   npm run history -- sweep [count]

import { createGame, armySize, collapseScore } from '../src/campaign/game';
import { runTurnAI } from '../src/campaign/turn';
import { turnToDate } from '../src/core/ticket';

const args = process.argv.slice(2);

function play(seed: number, turns: number, verbose: boolean) {
  const g = createGame(seed, null);
  let seen = 0;
  for (let t = 1; t <= turns; t++) {
    runTurnAI(g);
    if (verbose) {
      console.log(`\n== ${turnToDate(t).label} ==`);
      for (const h of g.headlines.slice(seen)) console.log(`  wk${h.week} ${h.text}`);
      seen = g.headlines.length;
      const reps = g.reports.filter((r) => r.turn === t);
      const by: Record<string, number> = {};
      for (const r of reps) by[`${r.attNat}>${r.defNat} ${r.taken ? 'won' : 'lost'}`] = (by[`${r.attNat}>${r.defNat} ${r.taken ? 'won' : 'lost'}`] ?? 0) + 1;
      if (reps.length) console.log('  battles:', JSON.stringify(by));
      const line = ['GER', 'POL', 'FRA', 'UK', 'BEL', 'NED'].map((n) => `${n} ${g.nations[n].alive ? armySize(g, n).toFixed(0) + 'div/' + collapseScore(g, n).toFixed(2) : 'fallen'}`).join('  ');
      console.log('  ' + line);
    }
  }
  return g;
}

const weeksAfter = (g: ReturnType<typeof play>, nat: string, start: number): number | null => {
  const f = g.nations[nat].fellOn;
  return f ? (f.turn - start) * 4 + f.week : null;
};

if (args[0] === 'sweep') {
  const n = Number(args[1] ?? 20);
  const rows: string[] = [];
  const fr: number[] = [], pl: number[] = [];
  for (let s = 1; s <= n; s++) {
    const g = play(s, 14, false);
    const p = weeksAfter(g, 'POL', 1), f = weeksAfter(g, 'FRA', 9);
    if (p !== null) pl.push(p);
    if (f !== null) fr.push(f);
    rows.push(`seed ${s}: Poland ${p ?? '-'} wk, Belgium ${weeksAfter(g, 'BEL', 9) ?? '-'}, Netherlands ${weeksAfter(g, 'NED', 9) ?? '-'}, France ${f ?? '-'} wk, UK corps left ${g.corps.filter((c) => c.nat === 'UK').length}, Italy at war ${g.nations.ITA.atWar.join('/') || 'no'}`);
  }
  console.log(rows.join('\n'));
  const med = (a: number[]) => a.sort((x, y) => x - y)[Math.floor(a.length / 2)];
  console.log(`\nPoland fell in ${pl.length}/${n}, median ${med(pl)} weeks. France fell in ${fr.length}/${n}, median ${med(fr)} weeks (min ${Math.min(...fr)}, max ${Math.max(...fr)}).`);
} else {
  const g = play(Number(args[0] ?? 1), Number(args[1] ?? 12), true);
  console.log('\nFell:', Object.values(g.nations).filter((n) => n.fellOn).map((n) => `${n.name} ${turnToDate(n.fellOn!.turn).label} wk${n.fellOn!.week}`).join('; '));
}

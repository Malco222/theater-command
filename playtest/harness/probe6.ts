import { start, advance, date, T, collapseScore, judgeGoals } from './lib';
import { playMonth } from './lib';
const plan = { minOdds: 1.4, declare: [[10, 'FRA'], [10, 'UK'], [14, 'GRE']] as [number, string][], land: true };
const g = start('ITA', 1, 7);
const run: any = { g, me: 'ITA', from: 1, seed: 7, level: 'colonel', rows: [], problems: [], fellOn: null, errors: [] };
const st = () => `alive ${g.nations.ITA.alive} human ${g.human} prov ${g.regions.filter((r) => r.ctrl === 'ITA').length} corps ${g.corps.filter((c) => c.nat === 'ITA').length} collapse ${collapseScore(g, 'ITA').toFixed(2)} humanUp ${T.humanUp(g)} lost ${T.playerLost(g)}`;
let up = advance(g);
while (up && g.turn <= 76) {
  const row: any = { bought: [], notes: [], attacksOrdered: 0, attacksWon: 0, moves: 0 };
  if (g.turn >= 68) console.log(`${date(g.turn)} player's month opens: ${st()}`);
  playMonth(run, plan, row);
  judgeGoals(g, 'ITA');
  T.endHuman(g);
  if (g.turn >= 68) console.log(`   after endHuman: ${st()}`);
  up = advance(g, (nat) => { if (g.turn >= 68 && (!g.nations.ITA.alive || g.regions.filter((r) => r.ctrl === 'ITA').length === 0)) console.log(`   after ${nat} (${date(g.turn)}): ${st()}`); });
}
console.log('end', date(g.turn), st());
console.log(g.headlines.filter((h) => h.turn >= 67).filter((h) => /Ital|armistice|Rome/.test(h.text)).map((h) => `${date(h.turn)} wk${h.week}: [${h.kind}] ${h.text}`).join('\n'));

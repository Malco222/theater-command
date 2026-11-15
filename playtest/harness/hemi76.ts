// Version 76, the hemisphere set: a war with nobody playing, read each January for what the countries of the
// Americas earn. The design director's question: a neutral selling oil, beef or copper to both sides must not
// out-earn a country at war. One line a country a year: at war or at peace, income a month (and how much of it is
// trade), its trading partners, its treasury, its field corps and how many are away from home.
// node out/hemi76.mjs SEED [UNTIL]
import { income } from '@tc/campaign/economy';
import { atWar, createGame } from '@tc/campaign/game';
import { startOpen } from '@tc/campaign/staff';
import { dealsOf, tradeIncome } from '@tc/campaign/trade';
import { runTurnAI } from '@tc/campaign/turn';
import { turnToDate } from '@tc/core/ticket';

const [seedS = '17', untilS = '77'] = process.argv.slice(2);
const seed = Number(seedS);
const g: any = startOpen(createGame(seed, null));
const AMERICAS = ['USA', 'CAN', 'MEX', 'CUB', 'BRA', 'ARG', 'CHL', 'PER', 'COL', 'VEN', 'GUA', 'ELS', 'HON', 'NIC', 'COS', 'PAN', 'HAI', 'DOM', 'ECU', 'BOL', 'PAR', 'URU'];
const OTHERS = ['UK', 'GER', 'SOV', 'ITA', 'FRA', 'SWE', 'ESP', 'TUR', 'GRE', 'ROM', 'HUN', 'POR', 'SWI', 'IRL'];
const read = (): void => {
  for (const id of [...AMERICAS, ...OTHERS]) {
    const n = g.nations[id];
    if (!n) continue;
    const field = g.corps.filter((c: any) => c.nat === id && !c.dead && c.kind !== 'fort' && c.kind !== 'eng');
    const away = field.filter((c: any) => g.regions[c.reg].owner !== id).length;
    const deals = dealsOf(g, id).map((d: any) => (d.a === id ? d.b : d.a));
    const both = deals.some((p: string) => atWar(g, p, 'GER') || g.nations[p]?.align === 'allies') && deals.some((p: string) => g.nations[p]?.align === 'axis');
    console.log(`H seed ${seed} ${turnToDate(g.turn).label} | ${id} ${n.alive ? (n.atWar.length ? 'WAR' : 'peace') : 'fallen'} ${n.align} | income ${income(g, id).toFixed(1)} trade ${tradeIncome(g, id).toFixed(1)} | partners ${deals.join(',') || '-'}${both ? ' BOTH-SIDES' : ''} | treasury ${Math.round(n.pp)} | corps ${field.length} away ${away} | provinces ${g.regions.filter((r: any) => r.ctrl === id).length}`);
  }
};
read();
let fell = 0;
while (g.turn <= Number(untilS)) {
  runTurnAI(g);
  if ((g.turn - 5) % 12 === 0) read();
  if (!g.nations.GER.alive && !fell) { fell = g.turn; }
  if (fell && g.turn >= fell + 1) break;
}
read();
const changed = g.regions.filter((r: any) => r.id >= 479 && r.ctrl !== r.owner).map((r: any) => `${r.name} ${r.owner}>${r.ctrl}`);
console.log(`E seed ${seed}: ended ${turnToDate(g.turn).label}; Germany ${g.nations.GER.alive ? 'stands' : 'fallen'}; Latin ground not in its owner's hands: ${changed.join(', ') || 'none'}`);

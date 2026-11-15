// Version 75: the American goals card against a player who declares war on Germany in September 1939 and ships one
// corps to a French port. Prints the goals panel's checks and its state each month for MONTHS months.
// usage: probe70 SEED [MONTHS=6] [PORT=Bordeaux]     (build with build70.sh)
import { start, advance, date, T, judgeGoals, goalState } from './lib';
import { goalChecks, GOALS } from '@tc/campaign/goals';
const [seedS = '25', monthsS = '6', portName = 'Bordeaux'] = process.argv.slice(2);
const g: any = start('USA', 1, Number(seedS));
let up = advance(g);
const say = (when: string): void => {
  for (const key of ['short', 'long'] as const) {
    const goal = (GOALS as any).USA[key];
    console.log(`${when} ${key} "${goal.title}": ${goalState(g, key)} | ${goalChecks(g, 'USA', goal).map((c: any) => `${c.done ? 'yes' : 'no'} ${c.label}`).join('; ')}`);
  }
};
say(`${date(g.turn)}, before any order:`);
console.log(`declare on Germany: ${T.orderDeclare(g, 'GER')}; at war with ${g.nations.USA.atWar.join(',')}; side ${g.nations.USA.align}`);
const c = g.corps.find((x: any) => x.nat === 'USA' && g.regions[x.reg].port && x.kind !== 'fort');
const port = g.byName[portName];
const can = T.shipTargets(g, c).some((r: any) => r.id === port.id);
console.log(`ship ${c.name} from ${g.regions[c.reg].name} to ${port.name} (${port.ctrl}): offered ${can}; ordered ${can ? T.orderShip(g, c, port.id) : false}; it now stands in ${g.regions[c.reg].name}`);
judgeGoals(g, 'USA'); say(`${date(g.turn)}, after the orders:`);
for (let m = 0; m < Number(monthsS) && up; m++) {
  T.endHuman(g); up = advance(g);
  say(`${date(g.turn)}:`);
}
console.log(`the corps stands in ${g.regions[c.reg].name} (${g.regions[c.reg].ctrl}); headlines on goals: ${g.headlines.filter((h: any) => /goal|campaign won|Second front|second front/i.test(h.text)).map((h: any) => `${date(h.turn)}: ${h.text}`).join(' | ') || 'none'}`);

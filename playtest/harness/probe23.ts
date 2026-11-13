// What the places outside towns not yet taken pay the player (Version 56 on): each month, the prize line of the
// ledger beside the whole income, and where the money came from.   usage: probe23 ME FROM SEED UNTIL PLAN-JSON
import { campaign, date, ledger } from './lib';
import { prizeIncome, prizeUser, hasPrize, prizeInside, PRIZE_KINDS } from '@tc/campaign/prizes';
import { besieged, fieldCtrl } from '@tc/campaign/siege';
const [me, fromS, seedS, untilS, planS] = process.argv.slice(2);
let months = 0, paid = 0, sum = 0, net = 0, most = 0, held = 0, working = 0;
const where: Record<string, number> = {};
campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS), each: (g) => {
  months++;
  const L = ledger(g, me) as any;
  const p = prizeIncome(g, me);
  const money = p.total * L.scale;
  net += L.net * L.scale; sum += money; if (money > 0) paid++; most = Math.max(most, money);
  for (const r of g.regions) {
    if (r.ctrl === me || !besieged(g, r) || fieldCtrl(g, r) !== me) continue;
    const got = PRIZE_KINDS.filter((k) => hasPrize(g, r, k) && !prizeInside(g, r, k));
    if (!got.length) continue;
    held++;
    if (got.some((k) => prizeUser(g, r, k) === me)) { working++; where[r.name] = (where[r.name] ?? 0) + 1; }
  }
  if (money > 0) console.log(`${date(g.turn)}: prize income ${Math.round(money)} of net ${Math.round(L.net * L.scale)} (works ${p.works}, oil ${p.oil}, harbour ${p.harbour}; ${p.count} places)`);
} }, { until: Number(untilS) });
console.log(`TOTAL ${months} months; months with any prize income ${paid}; prize income in all ${Math.round(sum)} against net income in all ${Math.round(net)} (${(sum / net * 100).toFixed(2)}%); most in a month ${Math.round(most)}`);
console.log(`town-months with places held outside an untaken town ${held}; of those with a place already working for the player ${working}; where it worked: ${JSON.stringify(where)}`);

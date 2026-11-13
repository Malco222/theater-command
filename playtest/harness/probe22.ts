// Finds the first month the player's army holds places outside a town it has not yet taken, and writes a save of it
// for the page (Version 56 on).  usage: probe22 ME FROM SEED UNTIL PLAN-JSON OUT.json [MIN-PLACES]
import { writeFileSync } from 'node:fs';
import { campaign, date } from './lib';
import { saveGame } from '@tc/campaign/save';
import { hasPrize, prizeInside, prizeUser, prizeOut, PRIZE_KINDS } from '@tc/campaign/prizes';
import { besieged, fieldCtrl } from '@tc/campaign/siege';
const [me, fromS, seedS, untilS, planS, out, minS] = process.argv.slice(2);
const min = Number(minS ?? 2);
let n = 0;
campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS), each: (g) => {
  for (const r of g.regions) {
    if (r.ctrl === me || !besieged(g, r) || fieldCtrl(g, r) !== me) continue;
    const got = PRIZE_KINDS.filter((k) => hasPrize(g, r, k) && !prizeInside(g, r, k));
    if (got.filter((k) => k !== 'airfield').length < min) continue;
    console.log(`${date(g.turn)}: ${r.name} (${r.ctrl}) places outside ${got.join('+')}; in use by ${got.map((k) => prizeUser(g, r, k) ?? 'nobody').join(',')}; out ${got.map((k) => prizeOut(g, r, k)).join(',')}`);
    if (n++ === 0 || got.some((k) => prizeOut(g, r, k) > 0)) { writeFileSync(out.replace('.json', `-${n}.json`), saveGame(g)); console.log('  saved', n, r.id); }
    if (n >= 6) process.exit(0);
  }
} }, { until: Number(untilS) });

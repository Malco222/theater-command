// Plays Germany (seed 7) without ever storming a town, and writes a browser save in the first month the player's
// army stands round a held town, so the page's siege panel and assault card can be looked at.
import { campaign, date, atWar } from './lib';
import { saveGame } from '@tc/campaign/save';
import { townHeld } from '@tc/campaign/siege';
import { writeFileSync } from 'node:fs';
let done = false;
campaign('GER', 1, 7, { minOdds: 1.4, noStorm: true, each: (g) => {
  if (done) return;
  const r = g.regions.find((x) => atWar(g, 'GER', x.ctrl) && x.units.some((u) => u.nat === 'GER') && townHeld(g, x) && (x.garr ?? []).length > 0) ;
  if (r) { done = true; writeFileSync(process.argv[2] ?? 'siege.json', saveGame(g)); console.log(`saved in ${date(g.turn)}: ${r.name} (id ${r.id}) held by ${r.ctrl}, garrison ${(r.garr ?? []).map((u) => u.name).join(', ')}, my corps round it ${r.units.filter((u) => u.nat === 'GER').map((u) => u.name).join(', ')}`); }
} } as any, { until: 14 });
if (!done) console.log('no siege of a garrisoned town by turn 14');

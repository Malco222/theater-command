// Plays Italy (seed 7) to the month before it falls of hunger and writes the save, so the browser can show what the player sees.
import { campaign } from './lib';
import { saveGame } from '@tc/campaign/save';
import { writeFileSync } from 'node:fs';
const at = Number(process.argv[2] ?? 69);
let done = false;
campaign('ITA', 1, 7, { minOdds: 1.4, declare: [[10, 'FRA'], [10, 'UK'], [14, 'GRE']], land: true, each: (g) => { if (g.turn === at && !done) { done = true; writeFileSync(process.argv[3] ?? 'ita-fall.json', saveGame(g)); console.log('saved at turn', g.turn); } } }, { until: at });

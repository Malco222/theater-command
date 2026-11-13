// A browser save looked at from the rules: which places a province has and who holds them (Version 56 on).
// usage: probe21 SAVE.json REGION-NAME
import { readFileSync } from 'node:fs';
import { loadGame } from '@tc/campaign/save';
import { prizeRules, hasPrize, prizesAt, PRIZE_KINDS } from '@tc/campaign/prizes';
import { besieged } from '@tc/campaign/siege';
const [file, name] = process.argv.slice(2);
const g = loadGame(readFileSync(file, 'utf8'))!;
console.log('loaded', !!g, 'turn', g?.turn, 'human', g?.human, 'open', g?.flags.open, 'prizeRules', prizeRules(g));
const r = g.regions.find((x) => x.name === name)!;
console.log(r.name, 'ctrl', r.ctrl, 'besieged', besieged(g, r), 'fac', r.fac, 'port', r.port, 'oil', r.oil);
for (const k of PRIZE_KINDS) console.log(' ', k, hasPrize(g, r, k));
console.log(JSON.stringify(prizesAt(g, r.id), null, 1));
const sieges = g.regions.filter((x) => besieged(g, x));
console.log('besieged now:', sieges.map((x) => `${x.name}(${x.ctrl}) places ${PRIZE_KINDS.filter((k) => hasPrize(g, x, k)).join('+') || 'none'}`).join('; '));

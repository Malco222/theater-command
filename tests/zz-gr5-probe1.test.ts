import { it } from 'vitest';
import { writeFileSync } from 'node:fs';
import { createGame } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { runTurnAI } from '../src/campaign/turn';

const OUT = '/tmp/claude-0/-home-user/be1a4137-fd5c-5bcb-8530-6094418c2077/scratchpad';
const log = (s: string) => process.stderr.write(s + '\n');

it('plays the campaign and keeps saves', () => {
  const g = createGame(3, null);
  const t0 = Date.now();
  while (g.turn < 48 && Date.now() - t0 < 170_000) {
    if ([9, 10, 47].includes(g.turn)) writeFileSync(`${OUT}/save-${g.turn}.json`, saveGame(g));
    runTurnAI(g);
    log(`turn ${g.turn} at ${((Date.now() - t0) / 1000).toFixed(1)} s`);
  }
}, 200_000);

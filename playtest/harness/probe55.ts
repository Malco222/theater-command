// Version 68: writes a save for the page after the player's orders of a month and before "End month", at each turn
// asked for. For showing on the page what a month the rules crash in looks like to the player (ui32.cjs).
// usage: probe55 ME FROM SEED UNTIL PLAN-JSON OUT-PREFIX TURN[,TURN...]
import { writeFileSync } from 'node:fs';
import { campaign, date, saveGame } from './lib';
const [me, fromS, seedS, untilS, planS, out, turnsS = '32'] = process.argv.slice(2);
const want = new Set(turnsS.split(',').map(Number));
const run = campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS),
  each: (g: any) => {
    if (!want.has(g.turn)) return;
    writeFileSync(`${out}-${g.turn}.json`, saveGame(g));
    console.log(`${date(g.turn)} (turn ${g.turn}): saved ${out}-${g.turn}.json after the player's orders`);
  },
} as any, Number(untilS));
console.log(`played to ${date(run.g.turn)}${run.fell ? ` (fell ${run.fell})` : ''}`);

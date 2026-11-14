// Version 66: writes a save for the page as the player's month opens, at each turn asked for, with a line on the
// treasury, the men, and what the Staff tab says of the building desk. For looking at a long war's later years on
// the page (ui30.cjs).
// usage: probe51 ME FROM SEED UNTIL PLAN-JSON OUT-PREFIX TURN[,TURN...]
import { writeFileSync } from 'node:fs';
import { campaign, date, saveGame, T } from './lib';
const [me, fromS, seedS, untilS, planS, out, turnsS = '41'] = process.argv.slice(2);
const want = new Set(turnsS.split(',').map(Number));
const run = campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS),
  first: (g: any) => {
    if (!want.has(g.turn)) return;
    const v = (T as any).commandOf?.(g);
    const n = g.nations[me];
    writeFileSync(`${out}-${g.turn}.json`, saveGame(g));
    console.log(`${date(g.turn)} (turn ${g.turn}): saved ${out}-${g.turn}.json; ${v ? v.name : 'no depth'}; treasury ${Math.round(n.pp * 100)}; the staff may still spend ${v ? Math.round(v.left * 100) : '-'}; building desk: ${v ? (v.desks.find((d: any) => d.id === 'build')?.state ?? '') : ''}`);
  },
} as any, Number(untilS));
console.log(`played to ${date(run.g.turn)}${run.fell ? ` (fell ${run.fell})` : ''}`);

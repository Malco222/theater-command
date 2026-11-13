// Does saving change the war? Plays the same war with and without a save, load, save check in the middle and compares the ends.
// usage: probe16 ME FROM SEED UNTIL PLAN-JSON [SAVE-TURNS]
import { campaign, date, saveGame } from './lib';
import { createHash } from 'node:crypto';
const [me, fromS, seedS, untilS, planS, turnsS] = process.argv.slice(2);
const plan = JSON.parse(planS);
const turns = (turnsS ?? '4,40').split(',').map(Number);
const sig = (g: any) => createHash('md5').update(JSON.stringify(g.regions.map((r: any) => [r.ctrl, r.units.length, (r.garr ?? []).length])) + g.corps.filter((c: any) => !c.dead).length).digest('hex').slice(0, 10);
const trace = (checkSave?: number[]) => {
  const marks: string[] = [];
  const run = campaign(me, Number(fromS), Number(seedS), { ...plan }, { until: Number(untilS), checkSave, onMonth: (r: any, row: any) => { marks.push(`${row.turn}:${sig(r.g)}`); } } as any);
  return { marks, end: sig(run.g), text: saveGame(run.g).length };
};
const a = trace(undefined), b = trace(turns), c = trace(undefined);
const first = (x: string[], y: string[]) => { for (let i = 0; i < Math.min(x.length, y.length); i++) if (x[i] !== y[i]) return x[i].split(':')[0]; return null; };
console.log(`no save twice: ${a.end === c.end ? 'the same war' : 'DIFFERENT wars, first differing at turn ' + first(a.marks, c.marks)}`);
console.log(`with a save check at turns ${turns.join(', ')}: ${a.end === b.end ? 'the same war' : 'a DIFFERENT war, first differing at turn ' + first(a.marks, b.marks) + ' (' + date(Number(first(a.marks, b.marks))) + ')'}`);

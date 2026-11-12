import { createGame } from '/home/user/theater-command/src/campaign/game';
import { runTurnAI } from '/home/user/theater-command/src/campaign/turn';
import { hashString } from '/home/user/theater-command/src/core/rng';
const from = +process.argv[2], to = +process.argv[3];
const marks = (process.argv[4] ?? '28,39,46').split(',').map(Number);
const skip = new Set((process.argv[5] ?? '').split(',').filter(Boolean));
const state = (g: any): string => {
  const cs = g.corps.filter((c: any) => !skip.has(c.nat)).map((c: any) => `${c.nat}|${c.name}|${c.reg}|${c.str.toFixed(4)}|${c.org.toFixed(4)}|${c.xp.toFixed(3)}`).join(';');
  const rs = g.regions.slice(0, 373).map((r: any) => r.ctrl).join('');
  const ns = Object.values(g.nations).filter((n: any) => !skip.has(n.id)).map((n: any) => `${n.id}${n.alive ? 1 : 0}${n.pp.toFixed(3)}${n.atWar.filter((x: string) => !skip.has(x)).join('')}`).join(';');
  return [cs, rs, ns, g.reports.length].join('#');
};
for (let seed = from; seed <= to; seed++) {
  const g = createGame(seed, null);
  const out: string[] = [];
  const last = Math.max(...marks);
  for (let t = 1; t < last; t++) {
    runTurnAI(g);
    if (marks.includes(g.turn)) out.push(`T${g.turn}:${hashString(state(g)).toString(16)}`);
  }
  console.log(`seed ${seed} ${out.join(' ')}`);
}

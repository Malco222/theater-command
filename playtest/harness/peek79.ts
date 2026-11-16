// Who takes a computer-led Axis Brazil's ground (probe80's bra40 mode)? usage: peek79 SEED UNTIL [NAT=BRA]
import { start, date, T } from './lib';
import { declareWar } from '@tc/campaign/game';
const [seedS = '17', untilS = '70', nat = 'BRA'] = process.argv.slice(2);
const g: any = start(null, 1, Number(seedS), 'colonel', 3); let last = '';
while (g.turn <= Number(untilS)) {
  const t = g.turn;
  if (t === 13) { g.nations[nat].align = 'axis'; declareWar(g, nat, 'USA'); }
  const home = g.regions.filter((r: any) => r.owner === nat);
  const held: Record<string, number> = {}; for (const r of home) held[r.ctrl] = (held[r.ctrl] ?? 0) + 1;
  const on: Record<string, number> = {}; for (const c of g.corps) if (!c.dead && c.nat !== nat && g.regions[c.reg]?.owner === nat) on[c.nat] = (on[c.nat] ?? 0) + 1;
  const line = `provinces held ${JSON.stringify(held)}; foreign corps on its soil ${JSON.stringify(on)}; at war with ${g.nations[nat].atWar.filter((x: string) => g.nations[x]?.alive).join(' ')}; ${g.nations[nat].alive ? 'stands' : 'FALLEN'}`;
  if (line !== last) console.log(`${nat} seed ${seedS} ${date(t)}: ${line}`); last = line;
  if (!g.nations[nat].alive || !g.nations.GER.alive) break;
  (T as any).runTurnAI(g);
}

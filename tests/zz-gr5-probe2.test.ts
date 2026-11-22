import { it } from 'vitest';
import { readFileSync } from 'node:fs';
import { loadGame } from '../src/campaign/save';
import { beginHuman, candidates, current, stepAI } from '../src/campaign/turn';
import { hostileUnits } from '../src/campaign/game';
import { landTicket } from '../src/battle/land';
import { LandBattle } from '../src/battle/land/sim';
import type { Game } from '../src/campaign/types';

const OUT = '/tmp/claude-0/-home-user/be1a4137-fd5c-5bcb-8530-6094418c2077/scratchpad';
const log = (s: string) => process.stderr.write(s + '\n');
const FOOT = new Set(['officer', 'rifle', 'mg', 'at', 'sapper', 'holder', 'fo', 'leader', 'loader', 'crew']);

function at(turn: number, nat: string): Game {
  const g = loadGame(readFileSync(`${OUT}/save-${turn}.json`, 'utf8'))!;
  for (let k = 0; current(g) !== nat && k < 60; k++) stepAI(g);
  g.human = nat;
  beginHuman(g);
  return g;
}

function tally(label: string, b: LandBattle, opts: Parameters<LandBattle['drillOn']>[0]) {
  const d = b.drillOn(opts);
  const per: Record<string, { men: number; tanks: number; lorries: number; guns: number }> = {};
  for (const blk of d.view() as { id: number; men: { r: string }[] }[]) {
    const u = b.unit(blk.id)!;
    const s = (per[u.side] ??= { men: 0, tanks: 0, lorries: 0, guns: 0 });
    for (const m of blk.men) {
      if (FOOT.has(m.r)) s.men++;
      else if (m.r === 'hmg' || m.r === 'mortar') s.men += 3;
      else if (m.r === 'tank') s.tanks++;
      else if (m.r === 'lorry') s.lorries++;
      else if (m.r === 'gun') s.guns++;
    }
  }
  log(`${label} N=${d.scale}: att ${JSON.stringify(per.att)} def ${JSON.stringify(per.def)}`);
}

function report(name: string, t: ReturnType<typeof landTicket>) {
  if (!t) { log(`${name}: no ticket`); return; }
  const b = new LandBattle(t);
  for (const side of ['att', 'def'] as const) {
    const us = b.units.filter((u) => u.side === side);
    const men = us.reduce((s, u) => s + u.men0, 0);
    const kinds: Record<string, number> = {};
    for (const u of us) kinds[u.kind + (u.team ?? '')] = (kinds[u.kind + (u.team ?? '')] ?? 0) + 1;
    const armMen = us.filter((u) => u.kind === 'arm').reduce((s, u) => s + u.men0, 0);
    log(`${name} ${side} ${t[side].nat}: blocks ${us.length} ${JSON.stringify(kinds)} men0 ${men} (arm ${armMen}) figures0 ${us.reduce((s, u) => s + u.figures0, 0)} forces ${t[side].forces.map((f) => `${f.name}:${f.kind}x${f.divs}@${f.str.toFixed(2)}=${f.men}`).join('; ')}`);
  }
  log(`${name} scale() = ${b.scale()}`);
  tally(`${name} keys only`, new LandBattle(t), {});
  tally(`${name} sections`, new LandBattle(t), { sections: true });
  tally(`${name} platoons (unscaled)`, new LandBattle(t), { sections: true, platoons: true });
  tally(`${name} platoons scale 45`, new LandBattle(t), { sections: true, platoons: true, scale: 45 });
  tally(`${name} platoons scale 20`, new LandBattle(t), { sections: true, platoons: true, scale: 20 });
  tally(`${name} platoons scale true`, new LandBattle(t), { sections: true, platoons: true, scale: true });
  // Per block at 45: cap and drawn
  const b45 = new LandBattle(t), d45 = b45.drillOn({ sections: true, platoons: true, scale: 45 });
  const whole = new LandBattle(t).drillOn({ sections: true, platoons: true });
  const rows: string[] = [];
  for (const blk of d45.view() as { id: number; men: { r: string }[] }[]) {
    const u = b45.unit(blk.id)!;
    const cnt = (x: { men: { r: string }[] }) => x.men.reduce((s, m) => s + (FOOT.has(m.r) ? 1 : m.r === 'hmg' || m.r === 'mortar' ? 3 : 0), 0);
    const w = (whole.view() as { id: number; men: { r: string }[] }[]).find((v) => v.id === blk.id)!;
    rows.push(`${u.side} ${u.nat} ${u.kind}${u.team ?? ''} men0=${u.men0} fig0=${u.figures0} cap45=${Math.floor(u.men0 / 45)} whole=${cnt(w)} at45=${cnt(blk)} owed=${Math.floor(u.men0 / 45) - cnt(blk)}`);
  }
  const agg = new Map<string, number>();
  for (const r of rows) agg.set(r, (agg.get(r) ?? 0) + 1);
  for (const [r, n] of agg) log(`  ${n} x ${r}`);
}

it('Sedan and Kursk', () => {
  for (const turn of [9, 10]) {
    const g = at(turn, 'GER');
    const r = g.regions.find((x) => x.name === 'Sedan')!;
    log(`turn ${turn}: Sedan owner ${r.owner}, hostile ${hostileUnits(g, r, 'GER').length}, candidates ${candidates(g, r).length}`);
    const cs = candidates(g, r);
    if (cs.length) report(`Sedan t${turn}`, landTicket(g, cs, r));
  }
  const g = at(47, 'GER');
  const r = g.regions.find((x) => x.name === 'Kursk')!;
  log(`turn 47: Kursk owner ${r.owner}, hostile ${hostileUnits(g, r, 'GER').length}, candidates ${candidates(g, r).length}`);
  const cs = candidates(g, r);
  if (cs.length) report('Kursk t47', landTicket(g, cs, r));
}, 170_000);

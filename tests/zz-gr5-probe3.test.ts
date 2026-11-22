import { it } from 'vitest';
import { readFileSync } from 'node:fs';
import { loadGame } from '../src/campaign/save';
import { beginHuman, candidates, current, stepAI } from '../src/campaign/turn';
import { hostileUnits } from '../src/campaign/game';
import { landTicket } from '../src/battle/land';
import { LandBattle } from '../src/battle/land/sim';

const OUT = '/tmp/claude-0/-home-user/be1a4137-fd5c-5bcb-8530-6094418c2077/scratchpad';
const log = (s: string) => process.stderr.write(s + '\n');
const FOOT = new Set(['officer', 'rifle', 'mg', 'at', 'sapper', 'holder', 'fo', 'leader', 'loader', 'crew']);

it('Sedan', () => {
  const g0 = loadGame(readFileSync(`${OUT}/save-9.json`, 'utf8'))!;
  for (let k = 0; current(g0) !== 'GER' && k < 60; k++) stepAI(g0);
  const n0 = g0.reports.length;
  stepAI(g0);
  const reps = g0.reports.slice(n0).filter((x) => x.regionName === 'Sedan');
  for (const rp of reps) log(`report Sedan turn ${rp.turn}: att ${rp.attackers.map((a: any) => `${a.name}`).join(', ')} | def ${rp.defenders.map((a: any) => a.name).join(', ')}`);
  if (!reps.length) { log('no Sedan report'); return; }
  const names = new Set(reps[0].attackers.map((a: any) => a.name));
  const g = loadGame(readFileSync(`${OUT}/save-9.json`, 'utf8'))!;
  for (let k = 0; current(g) !== 'GER' && k < 60; k++) stepAI(g);
  g.human = 'GER'; beginHuman(g);
  const r = g.regions.find((x) => x.name === 'Sedan')!, ard = g.regions.find((x) => x.name === 'Ardennes')!;
  const cs = g.corps.filter((c) => c.nat === 'GER' && !c.dead && names.has(c.name));
  for (const c of cs) { c.reg = ard.id; c.ap = Math.max(c.ap, 1); }
  // Ardennes is Belgian (neutral at turn 9): hand it to Germany so the roads are its.
  ard.owner = 'GER';
  log(`corps moved: ${cs.map((c) => c.name).join(', ')}; candidates ${candidates(g, r).length}`);
  const t = landTicket(g, candidates(g, r), r);
  if (!t) { log('no ticket'); return; }
  const tally = (label: string, opts: any) => {
    const b = new LandBattle(t), d = b.drillOn(opts);
    const per: any = {};
    for (const blk of d.view() as any[]) {
      const u = b.unit(blk.id)!; const s = (per[u.side] ??= { men: 0, tanks: 0, lorries: 0, guns: 0 });
      for (const m of blk.men) { if (FOOT.has(m.r)) s.men++; else if (m.r === 'hmg' || m.r === 'mortar') s.men += 3; else if (m.r === 'tank') s.tanks++; else if (m.r === 'lorry') s.lorries++; else if (m.r === 'gun') s.guns++; }
    }
    log(`${label} N=${d.scale}: att ${JSON.stringify(per.att)} def ${JSON.stringify(per.def)}`);
  };
  const b = new LandBattle(t);
  for (const side of ['att', 'def'] as const) {
    const us = b.units.filter((u) => u.side === side);
    const kinds: Record<string, number> = {};
    for (const u of us) { const key = u.kind + (u.team ?? '') + (u.short.includes('Regiment') ? '-regiment' : ''); kinds[key] = (kinds[key] ?? 0) + 1; }
    log(`Sedan ${side} ${t[side].nat}: blocks ${us.length} ${JSON.stringify(kinds)} men0 ${us.reduce((s, u) => s + u.men0, 0)} forces ${t[side].forces.map((f) => `${f.name}:${f.kind}x${f.divs}@${f.str.toFixed(2)}=${f.men}`).join('; ')}`);
  }
  log(`scale() = ${b.scale()}`);
  tally('platoons unscaled', { sections: true, platoons: true });
  tally('platoons scale true', { sections: true, platoons: true, scale: true });
});

import { it } from 'vitest';
import { createGame, men } from '../src/campaign/game';
import { beginHuman, candidates, current, humanUp, runTurnAI, stepAI } from '../src/campaign/turn';
import { takeOver } from '../src/campaign/stories';
import { turnToDate } from '../src/core/ticket';
import { LandBattle, landTicket, goingIn } from '../src/battle/land';
import type { Game } from '../src/campaign/types';

const say = (s: string) => process.stderr.write(s + '\n');
const SEEDS = (process.env.SEEDS ?? '3').split(',').map(Number);

it('kursk probe', () => {
  for (const seed of SEEDS) {
    let t0 = performance.now();
    const g: Game = createGame(seed, null);
    while (g.turn < 47) runTurnAI(g);
    g.human = 'GER';
    takeOver(g);
    for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
    beginHuman(g);
    say(`seed ${seed}: reached turn ${g.turn} (${turnToDate(g.turn).label}), current ${current(g)}, ${((performance.now() - t0) / 1000).toFixed(1)} s`);
    const r = g.byName['Kursk'];
    say(`Kursk ctrl ${r.ctrl} owner ${r.owner} fort ${JSON.stringify(r.fort)} nb ${r.nb.map((j) => `${g.regions[j].name}(${g.regions[j].ctrl})`).join(', ')}`);
    say(`  defenders: ${r.units.map((u) => `${u.nat} ${u.name} ${u.kind} divs ${u.divs} str ${u.str.toFixed(2)} men ${men(u)}`).join('; ')}`);
    const cs = candidates(g, r);
    say(`  candidates (${cs.length}): ${cs.map((c) => `${c.name}@${g.regions[c.reg].name} ${c.kind} ${c.divs}d ${men(c)}`).join('; ')}`);
    const going = goingIn(g, cs, r);
    say(`  goingIn (${going.length}): ${going.map((c) => c.name).join(', ')}`);
    t0 = performance.now();
    const t = landTicket(g, cs, r);
    if (!t) { say('  no ticket'); continue; }
    const tTicket = performance.now() - t0;
    const sum = (s: 'att' | 'def') => t[s].forces.reduce((a, f) => a + f.men, 0);
    say(`  ticket: ${t.date} turn ${t.turn} seed ${t.seed} ${t.title} odds ${t.reckoning.odds.toFixed(3)} att ${t.att.nat} ${sum('att')} v def ${t.def.nat} ${sum('def')} (${tTicket.toFixed(0)} ms)`);
    say(`  att forces: ${t.att.forces.map((f) => `${f.name} ${f.kind} ${f.divs}d ${f.men}`).join('; ')}`);
    say(`  def forces: ${t.def.forces.map((f) => `${f.name} ${f.kind} ${f.divs}d ${f.men}`).join('; ')}`);
    say(`  place ${JSON.stringify(t.place)}`);
    t0 = performance.now();
    const b = new LandBattle(t);
    const tNew = performance.now() - t0;
    const per = (s: 'att' | 'def') => b.units.filter((u) => u.side === s);
    const m0 = (s: 'att' | 'def') => per(s).reduce((a, u) => a + u.men0, 0);
    say(`  blocks att ${per('att').length} (men0 ${m0('att')}) def ${per('def').length} (men0 ${m0('def')}); scale ${b.scale()} raw ${((m0('att') + m0('def')) / 8500).toFixed(2)}; new LandBattle ${tNew.toFixed(0)} ms`);
    t0 = performance.now();
    for (let i = 0; i < 200 && !b.over; i++) b.step();
    say(`  200 steps ${(performance.now() - t0).toFixed(0)} ms, t=${b.t.toFixed(1)}`);
  }
}, 600_000);

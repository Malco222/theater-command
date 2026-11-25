// Scratch (not committed): strength-83's staged story, measured seed by seed on this tree.
import { appendFileSync } from 'node:fs';
import { it } from 'vitest';
import { landingWatch } from '../../src/campaign/ai';
import { reckon } from '../../src/campaign/ashore';
import { atWar, capture, createGame } from '../../src/campaign/game';
import { setStop } from '../../src/campaign/history';
import { startOpen, strengthWatch } from '../../src/campaign/staff';
import { runTurnAI } from '../../src/campaign/turn';
import { type Corps, type Region } from '../../src/campaign/types';

const SEEDS = (process.env.S83_SEEDS ?? '46').split(',').map(Number);
const OUT = process.env.S83_OUT ?? '/tmp/claude-0/r2merge/s83.jsonl';
it.each(SEEDS)('seed %i', (seed) => {
  const CUT = 0.25, ISLE = 'Corsica';
  const g = startOpen(createGame(seed, null));
  setStop(g, 3, true);
  const landings: { turn: number; nat: string; to: Region; corps: number }[] = [];
  const refused = new Set<number>();
  type Refusal = { at: Region; ask: number; chance: number; force: Corps[] };
  const seen: { first: Refusal | null } = { first: null };
  let after = -1;
  const isles: Record<number, string[]> = {};
  landingWatch.tell = (e) => { if (!e.wave) landings.push({ turn: g.turn, nat: e.nat, to: e.to, corps: e.force.length }); };
  strengthWatch.tell = (e) => {
    if (e.nat === 'USA' && g.turn >= 57 && e.force.length >= 3 && e.at.owner === 'FRA' && e.at.nb.length > 0 && e.R.chance < e.ask) {
      refused.add(g.turn);
      seen.first ??= { at: e.at, ask: e.ask, chance: e.R.chance, force: [...e.force] };
    }
  };
  const open = (f: { ask: number; force: Corps[] }): string[] => g.regions.filter((r) => r.nb.length === 0 && r.port && atWar(g, 'USA', r.ctrl) && (reckon(g, 'USA', r, f.force)?.chance ?? 0) >= f.ask).map((r) => r.name);
  let setupOk = true;
  try {
    while (g.turn <= 60 && g.nations.GER.alive) {
      if (g.turn === 56) {
        capture(g, g.byName[ISLE], 'GER');
        if (g.byName[ISLE].ctrl !== 'GER' || !atWar(g, 'USA', 'GER')) setupOk = false;
      }
      runTurnAI(g);
      const first = seen.first;
      if (first && after < 0) {
        for (const c of g.corps) if (!c.dead && c.nat === 'GER' && c.kind !== 'fort' && c.kind !== 'eng') c.str *= CUT;
        after = reckon(g, 'USA', first.at, first.force)!.chance;
      }
      if (first) isles[g.turn] = open(first);
    }
  } finally { landingWatch.tell = null; strengthWatch.tell = null; }
  const f = seen.first;
  const usa = landings.filter((l) => l.nat === 'USA' && l.turn >= 57);
  const checks = {
    setup: setupOk,
    refused: !!f,
    isleEveryMonth: !!f && Object.keys(isles).length > 0 && Object.values(isles).every((n) => n.length > 0),
    refusedBeforeLanding: [...refused].some((t) => usa.some((l) => l.turn > t)),
    noIsland: usa.filter((l) => l.to.nb.length === 0).length === 0,
    franceLanding: usa.some((l) => l.to.owner === 'FRA' && !l.to.overseas),
  };
  const pass = Object.values(checks).every(Boolean);
  appendFileSync(OUT, JSON.stringify({ seed, pass, checks, first: f ? `${f.at.name} m${[...refused][0]} ${f.chance.toFixed(2)}->${after.toFixed(2)} ask ${f.ask}` : null, usa: usa.map((l) => `m${l.turn} ${l.to.name} x${l.corps}`), gerAlive: g.nations.GER.alive, turn: g.turn }) + '\n');
}, 600000);

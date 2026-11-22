// The air battle drawn to one scale (the production lead's scale plan of 22 November, items AR1, AR2 and AR4; Brian,
// 22 November 04:07Z: "the bombers get destroyed a lot. is that accurate or is it a miscalibration"). Looks only:
//  - AR1: a model falls only for its share of the losses. A group's losses stand for round(lost / per) models, so falls
//    x per = losses; at one model an aircraft every loss is a fall, as before. Every aircraft lost is still told.
//  - AR2: a group is drawn as boxes of at most SKY.look.box models in trail, never one ever wider line abreast.
//  - AR4: the 3D game's budget (SKY.look.all) rises; the browser page keeps its own (SKY.look.page).
// None of it moves a count, the bombs or the verdict: the same battle with any budget ends the same.
import { describe, expect, it } from 'vitest';
import { planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import { runOut } from '../src/battle/core/clock';
import type { FieldEvent } from '../src/battle/core/types';
import { SKY } from '../src/battle/air/book';
import { AirSim, airBattle } from '../src/battle/air/sim';
import { airTicket } from '../src/battle/air/ticket';

const WARS: Record<string, string[]> = { GER: ['UK', 'FRA'], UK: ['GER'], FRA: ['GER'] };

function raid(nat: string, kind: RaidKind, name: string) {
  const g: Game = startOpen(createGame(7, nat));
  for (const w of WARS[nat]) setWar(g, nat, w);
  const t = raidTargets(g, kind, nat).find((x) => planRaid(g, kind, x.id, undefined, nat).place === name);
  if (!t) throw new Error(`no ${kind} raid on ${name} for ${nat}`);
  const day = planRaid(g, kind, t.id, undefined, nat);
  const ids = [...day.picked, ...day.seats.filter((s) => !s.why && !s.picked && s.kind === 'ftr').map((s) => s.id)];
  const { ticket, why } = airTicket(g, kind, t.id, ids, nat);
  if (!ticket) throw new Error(why ?? 'no ticket');
  return ticket;
}

function fight(tk: ReturnType<typeof raid>, seed: number, look?: number): AirSim {
  const s = airBattle({ ...tk, seed }, { show: true, ...(look ? { look } : {}) });
  s.ai.att = true; s.ai.def = true;
  runOut(s);
  return s;
}

/** A battle's history with what is drawn left out (which models fall, which straggler is shown, a survived hit's slot, the boxes). */
const counted = (h: FieldEvent[]) => h.map((e) => {
  const { models: _m, model: _s, slot: _k, boxes: _b, ...rest } = (e.extra ?? {}) as Record<string, unknown>;
  return { t: e.t, what: e.what, unit: e.unit, side: e.side, text: e.text, cue: e.cue, extra: rest };
});

const CASES: [string, RaidKind, string][] = [['GER', 'bomb', 'Alsace'], ['UK', 'bomb', 'Bremen'], ['FRA', 'bomb', 'Saar']];

describe('a model falls only for its share of the losses (AR1)', () => {
  it('falls x per = losses, every group: a group keeps a model while it has an aircraft, and none once it has none', () => {
    let many = 0, falls = 0, lostAll = 0;
    for (const [nat, kind, name] of CASES) {
      const tk = raid(nat, kind, name);
      for (const look of [12, 30]) for (let k = 0; k < 3; k++) {
        const s = fight(tk, tk.seed + k, look);
        const lost = s.history().filter((e) => e.what === 'air:lost');
        for (const u of s.units) {
          const mine = lost.filter((e) => e.unit === u.id);
          const models = mine.reduce((t, e) => t + (e.extra as { models: number }).models, 0);
          // Every aircraft lost is told; the models that fell are the ones the group no longer shows.
          expect(mine.length).toBe(u.n0 - u.n);
          if (u.state !== 'gone' || u.n === 0) expect(models).toBe(u.figures0 - u.figures);
          if (u.n > 0 && u.state !== 'gone') expect(u.figures).toBeGreaterThanOrEqual(1);
          if (u.n === 0) expect(u.figures).toBe(0);
          // At the model's scale, within one model (a straggler shown falling out may come a loss early or late).
          if (u.n > 0) expect(Math.abs(models - (u.n0 - u.n) / u.per)).toBeLessThanOrEqual(1.5);
          if (u.per > 1) many++;
          falls += models; lostAll += u.n0 - u.n;
        }
        // Each loss brings down no model, one, or at the very last whatever is left.
        for (const e of lost) expect((e.extra as { models: number }).models).toBeGreaterThanOrEqual(0);
        for (const e of s.history().filter((x) => x.what === 'air:straggle')) expect([0, 1]).toContain((e.extra as { model: number }).model);
      }
    }
    expect(many).toBeGreaterThan(0);
    // Far fewer falls than aircraft lost once a model stands for several.
    expect(falls).toBeLessThan(lostAll * 0.6);
  });

  it('at one model an aircraft every loss is one fall and every straggler is shown, as before', () => {
    const tk = raid('GER', 'bomb', 'Alsace');
    const s = fight(tk, tk.seed, 100_000);
    for (const u of s.units) { expect(u.per).toBe(1); expect(u.figures).toBe(u.state === 'gone' && u.n > 0 ? 0 : u.n); }
    const lost = s.history().filter((e) => e.what === 'air:lost');
    expect(lost.length).toBeGreaterThan(0);
    for (const e of lost) expect((e.extra as { models: number }).models).toBe(1);
    for (const e of s.history().filter((x) => x.what === 'air:straggle')) expect((e.extra as { model: number }).model).toBe(1);
  });
});

describe('drawing never moves a count (ruling 217; AR4: the same result whatever the budget)', () => {
  it('the same battle drawn at 12, 60, 150 and 400 models: the same losses, bombs, verdict and dispatches', () => {
    for (const [nat, kind, name] of CASES) {
      const tk = raid(nat, kind, name);
      for (let k = 0; k < 10; k++) {
        const runs = [12, 60, SKY.look.all, 400].map((look) => fight(tk, tk.seed + k, look));
        const [a, ...rest] = runs;
        for (const b of rest) {
          expect(b.outcome()).toEqual(a.outcome());
          expect(b.over!.lines).toEqual(a.over!.lines);
          expect(b.over!.level).toBe(a.over!.level);
          expect(counted(b.history())).toEqual(counted(a.history()));
        }
      }
    }
  });
});

describe('the budgets, and the boxes (AR2, AR4)', () => {
  it('the 3D game draws up to SKY.look.all models at the start, the browser page its own SKY.look.page', () => {
    expect(SKY.look.all).toBeGreaterThan(SKY.look.page);
    const tk = raid('UK', 'bomb', 'Bremen');
    const sent = tk.att.forces.concat(tk.def.forces).filter((f) => tk.place.starts[f.corps]).reduce((n, f) => n + Math.max(0, f.men), 0);
    const three = new AirSim(tk), page = new AirSim(tk, { look: SKY.look.page });
    expect(three.units[0].per).toBe(Math.max(1, Math.ceil(sent / SKY.look.all)));
    expect(page.units[0].per).toBe(Math.max(1, Math.ceil(sent / SKY.look.page)));
  });

  it('a group is drawn one box wide at most; its models fly in as many boxes as they need', () => {
    // The production lead's boxes of 18 to 24 models, for bombers and fighters alike.
    for (const k of ['ftr', 'bmr'] as const) { expect(SKY.look.box[k]).toBeGreaterThanOrEqual(18); expect(SKY.look.box[k]).toBeLessThanOrEqual(24); }
    for (const [nat, kind, name] of CASES) {
      const tk = raid(nat, kind, name);
      const s = airBattle(tk, { show: true, look: 100_000 });
      s.ai.att = true; s.ai.def = true;
      for (let k = 0; k < 400 && !s.over; k++) {
        s.step();
        for (const u of s.view().units) {
          const box = SKY.look.box[u.air], rows = Math.min(SKY.look.rows[u.air], box);
          expect(u.w).toBeLessThanOrEqual(Math.ceil(box / rows) * SKY.look.gap[u.air]);
          expect(u.d).toBeLessThanOrEqual(rows * SKY.look.gap[u.air]);
          const boxes = (u.extra as { boxes?: number } | undefined)?.boxes;
          if (boxes !== undefined) expect(boxes * box).toBeGreaterThanOrEqual(u.figures);
        }
      }
    }
  });
});

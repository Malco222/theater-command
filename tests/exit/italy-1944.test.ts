// Anzio, Cassino, Rome and the Gothic Line, run against the campaign rules with every nation on the historical
// plan and nobody at the controls. In January 1944 the Fifth Army lands a corps at Anzio, behind the Gustav Line,
// and its first attack on Cassino fails. In February the Germans try to throw the beachhead into the sea and
// cannot; the second and third battles for Cassino fail in February and March. In April the Eighth Army comes
// across the mountains and the air forces win the sky over the line. In May, Operation Diadem breaks it. Rome is
// free in June, given up without a battle. The Germans fall back to Tuscany and the Marche, lose both in August,
// and stand on the Gothic Line, which holds against the autumn battles to the end of the year. In August a third
// of the Fifth Army leaves for the south of France.
//
// The plan only sets intentions. Every region still has to be won under the rules the player
// uses, so the bands below are loose on purpose and are checked across twenty sets of dice.
import { describe, expect, it } from 'vitest';
import { createGame, men } from '../../src/campaign/game';
import { runTurnAI } from '../../src/campaign/turn';
import type { Game } from '../../src/campaign/types';

const DEC_43 = 52, JAN_44 = 53, FEB_44 = 54, MAR_44 = 55, APR_44 = 56, MAY_44 = 57, JUNE_44 = 58, JULY_44 = 59, AUG_44 = 60, SEPT_44 = 61, OCT_44 = 62, DEC_44 = 64;
/** The ground the campaign of 1944 is fought over. */
const GROUND = ['Apulia', 'Naples', 'Cassino', 'Anzio', 'Abruzzo', 'Rome', 'Marche', 'Tuscany', 'Emilia', 'Liguria'];
const GUSTAV = ['Cassino', 'Abruzzo'], GOTHIC = ['Emilia', 'Liguria'];
const ALLIES = ['UK', 'USA'];

interface Fight { turn: number; place: string; by: string; pre: number; odds: number; taken: boolean; attLost: number; defLost: number }
interface Month {
  /** Who holds each place as the month ends. */
  held: Record<string, string>;
  /** Field corps and divisions on the ground of the campaign, by nation. */
  force: Record<string, { corps: number; divs: number; men: number }>;
  /** German divisions by place. */
  at: Record<string, number>;
  /** The level of the works at each place. */
  works: Record<string, number>;
}

interface Run {
  g: Game;
  months: Record<number, Month>;
  fights: Fight[];
  /** The month each piece of news came, or 0. */
  news: { landed: number; dugIn: number; gustav: number; rome: number; gothic: number; left: number; luftwaffe: number; allied: number; german: number };
  /** The months in which the Allied air forces won the sky over Cassino. */
  sky: number[];
  /** What the armies were worth as June 1944 opened. */
  worth: { usa: number; ger: number; gerAir: number; usaAir: number; ukAir: number };
}

function history(seed: number): Run {
  const g = createGame(seed, null);
  const ids = new Set(GROUND.map((nm) => g.byName[nm].id));
  const run: Run = {
    g, months: {}, fights: [], sky: [], worth: { usa: 0, ger: 0, gerAir: 0, usaAir: 0, ukAir: 0 },
    news: { landed: 0, dugIn: 0, gustav: 0, rome: 0, gothic: 0, left: 0, luftwaffe: 0, allied: 0, german: 0 },
  };
  let seen = 0, flown = 0;
  for (let t = 1; t <= DEC_44; t++) {
    if (t === JUNE_44) run.worth = { usa: g.nations.USA.quality, ger: g.nations.GER.quality, gerAir: g.nations.GER.air, usaAir: g.nations.USA.air, ukAir: g.nations.UK.air };
    runTurnAI(g);
    if (t >= DEC_43) {
      for (const r of g.reports.slice(seen)) if (ids.has(r.region)) run.fights.push({ turn: t, place: r.regionName, by: r.attNat, pre: r.aPower / r.dPower, odds: r.odds, taken: r.taken, attLost: r.attLost, defLost: r.defLost });
      for (const a of g.airReports.slice(flown)) if (a.mission === 'superiority' && a.place === 'Cassino' && ALLIES.includes(a.attNat) && a.sky) run.sky.push(t);
      const here = g.corps.filter((c) => !c.dead && c.kind !== 'fort' && ids.has(c.reg));
      const force: Month['force'] = {};
      for (const nat of ['UK', 'USA', 'GER']) {
        const cs = here.filter((c) => c.nat === nat);
        force[nat] = { corps: cs.length, divs: cs.reduce((s, c) => s + c.divs, 0), men: cs.reduce((s, c) => s + men(c), 0) };
      }
      const at: Record<string, number> = {};
      for (const c of here) if (c.nat === 'GER') at[g.regions[c.reg].name] = (at[g.regions[c.reg].name] ?? 0) + c.divs;
      run.months[t] = {
        held: Object.fromEntries(GROUND.map((nm) => [nm, g.byName[nm].ctrl])),
        force, at,
        works: Object.fromEntries(GROUND.map((nm) => [nm, g.byName[nm].fort?.lv ?? 0])),
      };
    }
    seen = g.reports.length; flown = g.airReports.length;
  }
  const told = (re: RegExp): number => g.headlines.find((h) => h.turn >= JAN_44 && re.test(h.text))?.turn ?? 0;
  run.news = {
    landed: told(/land at Anzio from the sea/), dugIn: told(/beachhead at Anzio is dug in/), gothic: told(/building the Gothic Line/), left: told(/lent to the campaign in Italy leave for other fronts/),
    gustav: told(/The Gustav Line is broken/), rome: told(/Rome is free/),
    luftwaffe: g.flags['event:luftwaffe-1944'] ?? 0, allied: g.flags['event:allied-armies-1944'] ?? 0, german: g.flags['event:german-army-1944'] ?? 0,
  };
  return run;
}

const SEEDS = Array.from({ length: 20 }, (_, i) => i + 1);
const runs = SEEDS.map(history);
const count = (ok: (r: Run) => boolean): number => runs.filter(ok).length;
const ours = (r: Run, t: number, ...places: string[]): boolean => places.every((nm) => ALLIES.includes(r.months[t].held[nm]));
const theirs = (r: Run, t: number, ...places: string[]): boolean => places.every((nm) => r.months[t].held[nm] === 'GER');
const battles = (r: Run, place: string, from: number, to = from): Fight[] => r.fights.filter((f) => f.place === place && f.turn >= from && f.turn <= to);
const allied = (r: Run, t: number): { corps: number; divs: number } => ({ corps: r.months[t].force.UK.corps + r.months[t].force.USA.corps, divs: r.months[t].force.UK.divs + r.months[t].force.USA.divs });
/** Men each side lost in the battles of the year. */
const lost = (r: Run): { allies: number; germans: number } => {
  const out = { allies: 0, germans: 0 };
  for (const f of r.fights) { if (f.turn < JAN_44) continue; const a = ALLIES.includes(f.by); out.allies += a ? f.attLost : f.defLost; out.germans += a ? f.defLost : f.attLost; }
  return out;
};

describe('exit test: Italy in 1944 on the historical plan', () => {
  it('the year opens on the line of 1943: the Germans hold the mountains, Rome and the coast behind them', () => {
    expect(count((r) => theirs(r, DEC_43, 'Cassino', 'Abruzzo', 'Anzio', 'Rome', 'Tuscany', 'Emilia') && ours(r, DEC_43, 'Naples', 'Apulia'))).toBeGreaterThanOrEqual(18);
    expect(count((r) => GUSTAV.every((nm) => r.months[DEC_43].works[nm] === 2))).toBeGreaterThanOrEqual(18);
  });

  it('Shingle: a corps of the Fifth Army lands at Anzio in January against nobody, and digs in', () => {
    expect(count((r) => ours(r, JAN_44, 'Anzio') && r.news.landed === JAN_44)).toBeGreaterThanOrEqual(18);
    expect(count((r) => battles(r, 'Anzio', JAN_44).length === 0)).toBeGreaterThanOrEqual(18);
    expect(count((r) => r.news.dugIn === JAN_44 && r.months[JAN_44].works['Anzio'] === 2)).toBeGreaterThanOrEqual(18);
  });

  it('the first three battles for Cassino fail, one a month, and cost the attacker far more than the defender', () => {
    for (const r of runs) for (const f of battles(r, 'Cassino', JAN_44, APR_44)) expect(f.taken).toBe(false);
    expect(count((r) => [JAN_44, FEB_44, MAR_44].every((t) => battles(r, 'Cassino', t).length === 1))).toBeGreaterThanOrEqual(17);
    expect(count((r) => battles(r, 'Cassino', JAN_44, MAR_44).every((f) => f.odds < 0.8 && f.attLost > 2 * f.defLost))).toBeGreaterThanOrEqual(17);
    expect(count((r) => theirs(r, APR_44, 'Cassino', 'Abruzzo', 'Rome'))).toBeGreaterThanOrEqual(19);
  });

  it('Fischfang: the German counter-attack on the beachhead in February is beaten off, and they do not try again', () => {
    expect(count((r) => { const f = battles(r, 'Anzio', FEB_44); return f.length === 1 && f[0].by === 'GER' && !f[0].taken && f[0].pre >= 0.6 && f[0].pre < 1.1; })).toBeGreaterThanOrEqual(17);
    expect(count((r) => battles(r, 'Anzio', MAR_44, DEC_44).length === 0)).toBeGreaterThanOrEqual(18);
    expect(count((r) => [FEB_44, MAR_44, APR_44].every((t) => ours(r, t, 'Anzio')))).toBeGreaterThanOrEqual(19);
  });

  it('April: no battle is fought, the Eighth Army comes west, and both sides build up', () => {
    expect(count((r) => r.fights.filter((f) => f.turn === APR_44).length === 0)).toBeGreaterThanOrEqual(18);
    expect(count((r) => allied(r, APR_44).divs >= 20 && allied(r, APR_44).corps >= 6)).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.months[APR_44].force.GER.divs >= 14 && (r.months[APR_44].at['Cassino'] ?? 0) >= 5)).toBeGreaterThanOrEqual(17);
  });

  it('the air forces win the sky over the Gustav Line before the offensive opens', () => {
    expect(count((r) => r.sky.some((t) => t === APR_44 || t === MAY_44))).toBeGreaterThanOrEqual(17);
  });

  it('the armies of 1944: the Luftwaffe leaves the front in March, and by May the Americans have learned their trade', () => {
    expect(count((r) => r.news.luftwaffe === MAR_44 && r.worth.gerAir === 45)).toBeGreaterThanOrEqual(19);
    expect(count((r) => r.news.allied === MAY_44 && r.worth.usa === 1 && r.worth.usaAir === 140 && r.worth.ukAir === 90)).toBeGreaterThanOrEqual(19);
    expect(count((r) => r.news.german === MAY_44 && r.worth.ger === 1.15)).toBeGreaterThanOrEqual(19);
  });

  it('Diadem: the Gustav Line is broken at Cassino in May, in a battle that could have gone either way', () => {
    expect(count((r) => ours(r, MAY_44, 'Cassino') && r.news.gustav === MAY_44)).toBeGreaterThanOrEqual(17);
    expect(count((r) => { const won = battles(r, 'Cassino', MAY_44).filter((f) => f.taken); return won.length === 1 && won[0].pre >= 1.1 && won[0].pre <= 1.8; })).toBeGreaterThanOrEqual(17);
    // It takes more than one assault: the line is worn down before it gives.
    expect(count((r) => battles(r, 'Cassino', MAY_44).length >= 2)).toBeGreaterThanOrEqual(17);
  });

  it('Rome is free in June, given up without a battle, and the Germans fall back to Tuscany and the Marche', () => {
    expect(count((r) => ours(r, JUNE_44, 'Rome', 'Abruzzo', 'Cassino', 'Anzio') && r.news.rome === JUNE_44)).toBeGreaterThanOrEqual(17);
    expect(count((r) => battles(r, 'Rome', JAN_44, DEC_44).length === 0)).toBeGreaterThanOrEqual(18);
    expect(count((r) => ['Cassino', 'Anzio', 'Abruzzo', 'Rome'].every((nm) => !r.months[JUNE_44].at[nm]) && (r.months[JUNE_44].at['Tuscany'] ?? 0) >= 6 && (r.months[JUNE_44].at['Marche'] ?? 0) >= 4)).toBeGreaterThanOrEqual(17);
    // Rome does not fall before the Gustav Line does.
    for (const r of runs) for (const t of [JAN_44, FEB_44, MAR_44, APR_44]) expect(r.months[t].held['Rome']).toBe('GER');
  });

  it('the Gothic Line is dug across the northern Apennines in July', () => {
    expect(count((r) => r.news.gothic === JULY_44 && r.months[JULY_44].works['Emilia'] === 3 && r.months[JULY_44].works['Liguria'] === 2)).toBeGreaterThanOrEqual(18);
  });

  it('Florence and Ancona: Tuscany and the Marche are Allied by the end of August', () => {
    expect(count((r) => ours(r, AUG_44, 'Tuscany', 'Marche'))).toBeGreaterThanOrEqual(17);
    expect(count((r) => ours(r, AUG_44, 'Tuscany'))).toBeGreaterThanOrEqual(19);
    // Neither falls before Rome.
    for (const r of runs) expect(ours(r, MAY_44, 'Tuscany') || ours(r, MAY_44, 'Marche')).toBe(false);
  });

  it('Dragoon takes its share: a third of the Fifth Army leaves Italy in August', () => {
    expect(count((r) => r.news.left === AUG_44)).toBeGreaterThanOrEqual(18);
    expect(count((r) => r.months[JULY_44].force.USA.divs - r.months[AUG_44].force.USA.divs >= 7)).toBeGreaterThanOrEqual(17);
  });

  it('the autumn battles on the Gothic Line fail: one by the Eighth Army in September, one by the Fifth in October', () => {
    for (const r of runs) for (const f of battles(r, 'Emilia', JAN_44, DEC_44)) expect(f.taken).toBe(false);
    expect(count((r) => { const f = battles(r, 'Emilia', SEPT_44); return f.length === 1 && f[0].by === 'UK' && f[0].odds < 0.8; })).toBeGreaterThanOrEqual(17);
    expect(count((r) => { const f = battles(r, 'Emilia', OCT_44); return f.length === 1 && f[0].by === 'USA' && f[0].odds < 0.8; })).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.fights.filter((f) => f.turn > OCT_44).length === 0)).toBeGreaterThanOrEqual(18);
  });

  it('the year ends with the Germans on the Gothic Line and two Allied armies in front of it', () => {
    expect(count((r) => GOTHIC.every((nm) => r.months[DEC_44].held[nm] === 'GER'))).toBeGreaterThanOrEqual(19);
    expect(count((r) => ours(r, DEC_44, 'Rome', 'Tuscany', 'Marche', 'Naples'))).toBeGreaterThanOrEqual(17);
    expect(count((r) => allied(r, DEC_44).corps >= 6 && allied(r, DEC_44).divs >= 17)).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.months[DEC_44].force.GER.corps >= 8 && r.months[DEC_44].force.GER.divs >= 16)).toBeGreaterThanOrEqual(17);
    expect(count((r) => GOTHIC.reduce((s, nm) => s + (r.months[DEC_44].at[nm] ?? 0), 0) >= 14)).toBeGreaterThanOrEqual(17);
  });

  it('the year costs what it cost: the attacker pays more than the defender, and nobody loses an army', () => {
    expect(count((r) => { const l = lost(r); return l.allies >= 140000 && l.allies <= 280000; })).toBeGreaterThanOrEqual(17);
    expect(count((r) => { const l = lost(r); return l.germans >= 80000 && l.germans <= 170000; })).toBeGreaterThanOrEqual(17);
    expect(count((r) => lost(r).allies > lost(r).germans)).toBeGreaterThanOrEqual(18);
  });
});

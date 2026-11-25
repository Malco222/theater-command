// Ruling 107(d): where a bombing raid is ordered, BY DAY and BY NIGHT stand side by side, each a whole small forecast:
// the chance, the bombers lost and what a hit does, in the counts and odds the raid card gives (the art director's
// brief of 16 November). One shared function, `dayOrNight`, weighs the same groups against the same target both ways,
// so the browser's raid card and the 3D game's say the same thing. Display only: it changes nothing in the game, and
// no rule of the night raid is touched here.
import { describe, expect, it } from 'vitest';
import { gunsAt } from '../src/campaign/air';
import { dayOrNight, planRaid, type RaidKind } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { previewDayNight, previewRaid, raidTargetsFor } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const pct = (x: number): string => `${Math.round(x * 100)}%`;
const num = (n: number): string => Math.round(n).toLocaleString('en-US');
/** A German campaign at war with Britain and France too, so both kinds of bombing raid have targets. */
function west(seed = 7): Game {
  const g = startOpen(createGame(seed, 'GER'));
  setWar(g, 'GER', 'UK');
  setWar(g, 'GER', 'FRA');
  return g;
}
/** Targets a day raid can be flown against this month, those with industry first. */
function targets(g: Game, most = 8): number[] {
  const all = raidTargetsFor(g, 'bomb').filter((t) => !previewRaid(g, 'bomb', t.id).why).map((t) => t.id);
  return [...all.filter((t) => g.regions[t].fac > 0), ...all.filter((t) => g.regions[t].fac <= 0)].slice(0, most);
}
/** The staff's bombers for a target, and every fighter group that could go with them by day. */
function withEscorts(g: Game, t: number): number[] {
  const day = planRaid(g, 'bomb', t);
  return [...day.picked, ...day.seats.filter((s) => !s.why && !s.picked && s.kind === 'ftr').map((s) => s.id)];
}

describe('a bombing raid by day and by night, side by side (ruling 107(d))', () => {
  it('is offered for the two bombing raids and for no other kind', () => {
    const g = west();
    const t = targets(g)[0];
    expect(dayOrNight(g, 'bomb', t)).not.toBeNull();
    expect(dayOrNight(g, 'night', t)).not.toBeNull();
    for (const kind of ['ground', 'line', 'town', 'superiority', 'strike', 'fleet', 'harbour', 'recon'] as RaidKind[]) expect(dayOrNight(g, kind, t), kind).toBeNull();
  });

  it('gives each way the very numbers the raid card gives for it', () => {
    const g = west();
    const ts = targets(g, 60);
    expect(ts.length).toBeGreaterThan(2);
    let works = 0, yards = 0;
    for (const t of ts) {
      const ask = withEscorts(g, t);
      const day = planRaid(g, 'bomb', t, ask), night = planRaid(g, 'night', t, ask);
      const dn = dayOrNight(g, 'bomb', t, ask)!;
      expect(dn.chosen).toBe('bomb');
      expect(dn.day.label).toBe('BY DAY');
      expect(dn.night.label).toBe('BY NIGHT');
      expect(dn.day.foot).toBe('Escorts can fly with them.');
      expect(dn.night.foot).toBe('No escorts. Fewer lost, fewer hits.');
      for (const [w, p] of [[dn.day, day], [dn.night, night]] as const) {
        expect(w.why).toBe(p.why);
        expect(w.picked).toEqual(p.picked);
        expect(w.bombers).toBe(p.bombers);
        expect(w.fighters).toBe(p.fighters);
        expect(w.success).toBe(p.success);
        expect(w.verdict).toBe(p.verdict);
        expect(w.bomberLoss).toBe(p.bomberLoss);
        expect(w.bomberLossN).toBe(p.bomberLossN);
        expect(w.fighterLossN).toBe(p.fighterLossN);
        expect(w.figures).toEqual(p.figures);
        if (p.why) continue;
        // The share and the aircraft, as the card prints them.
        expect(w.lost).toBe(`${pct(p.bomberLoss)} · about ${num(p.bomberLossN)}`);
        expect(w.fightersLost).toBe(p.fighters > 0 ? `${pct(p.fighterLossN / p.fighters)} · about ${num(p.fighterLossN)}` : '');
        expect(w.prose).toBe(p.effects[0]);
        // What a hit does: the card's three figures as lines, in the card's order, and never more than three.
        const want: string[] = [];
        if (p.figures.output !== undefined) want.push(`${p.figures.output <= 0 ? '0%' : p.figures.output < 0.005 ? '<1%' : `−${pct(p.figures.output)}`} output, for a month`);
        if (p.figures.rail) want.push(`${pct(p.figures.rail)} chance the railway is cut`);
        if (p.figures.factory) want.push(`${pct(p.figures.factory)} chance a factory is wrecked`);
        if (!want.length) want.push(' No industry there, and too few bombers to cut the railway.');
        // (A line the other way has and this way lacks is printed too, dim, at 0%; then the "nothing there" line gives way to it.)
        const dim = w.hits.filter((h) => h.dim);
        const other = w === dn.day ? dn.night : dn.day;
        for (const h of dim) { expect(h.n).toBe('0%'); expect(other.hits.some((x) => x.words === h.words && !x.dim), h.words).toBe(true); }
        expect(w.hits.filter((h) => !h.dim).map((h) => `${h.n} ${h.words}`)).toEqual(dim.length ? want.filter((x) => !x.startsWith(' No industry')) : want);
        expect(w.hits.length).toBeLessThanOrEqual(3);
        if (!other.why) expect(w.hits.map((h) => h.words)).toEqual(other.hits.map((h) => h.words));
        if (g.regions[t].fac > 0) { works++; expect(p.effects[0]).toContain(`about ${pct(p.figures.output!)} of`); } else { yards++; expect(p.figures.output).toBeUndefined(); }
      }
    }
    expect(works).toBeGreaterThan(0);
    expect(yards).toBeGreaterThan(0);
  });

  it('flies the same bombers both ways: by day with their escort, by night alone, and a change of mind drops nobody', () => {
    const g = west();
    let escorted = 0;
    for (const t of targets(g, 60)) {
      const ask = withEscorts(g, t);
      const dn = dayOrNight(g, 'bomb', t, ask)!;
      if (dn.day.why || dn.night.why) continue;
      expect(dn.night.fighters).toBe(0);
      expect(dn.night.fightersLost).toBe('');
      expect(dn.night.bombers).toBe(dn.day.bombers);
      expect(dn.bombers).toBe(dn.day.bombers);
      for (const id of dn.night.picked) expect(dn.day.picked).toContain(id);
      // The fighters picked stay at home by night; every group that flies by day is either going by night or staying home.
      expect([...dn.night.picked, ...dn.home].sort((a, b) => a - b)).toEqual([...dn.day.picked].sort((a, b) => a - b));
      for (const id of dn.home) expect(g.airFleets.find((f) => f.id === id)!.bmr).toBe(0);
      if (dn.day.fighters > 0) { escorted++; expect(dn.home.length).toBeGreaterThan(0); expect(dn.day.fightersLost).not.toBe(''); }
      // Asked from the night side with the same groups, the two ways are the same two ways.
      const back = dayOrNight(g, 'night', t, ask)!;
      expect(back.chosen).toBe('night');
      expect({ ...back, chosen: 'bomb' }).toEqual(dn);
      // A fighter group that could go by day and was not asked for is offered by day only.
      const bombersOnly = dayOrNight(g, 'night', t, dn.night.picked)!;
      for (const id of dn.home) if (!bombersOnly.day.picked.includes(id)) expect(bombersOnly.barred).toContain(id);
    }
    expect(escorted).toBeGreaterThan(0);
  });

  it('names what is common to both, and by night fewer bombs find the target', () => {
    const g = west();
    let seen = 0;
    for (const t of targets(g)) {
      const dn = dayOrNight(g, 'bomb', t)!;
      if (dn.day.why || dn.night.why) continue;
      const day = planRaid(g, 'bomb', t);
      expect(dn.bombers).toBe(day.bombers);
      expect(dn.foes).toBe(day.meets.reduce((n, x) => n + x.ftr, 0));
      if (g.regions[t].fac <= 0 || dn.day.bombers !== dn.night.bombers) continue;
      seen++;
      expect(dn.night.figures.output!).toBeLessThan(dn.day.figures.output! + 1e-9);
    }
    expect(seen).toBeGreaterThan(0);
  });

  it('keeps a way that cannot be flown, with the rules\' own reason in place of its figures', () => {
    const g = west();
    const t = targets(g)[0];
    const none = dayOrNight(g, 'night', t, [])!;
    for (const w of [none.day, none.night]) {
      expect(w.why).toBe('Pick at least one air group.');
      expect(w.lost).toBe('');
      expect(w.fightersLost).toBe('');
      expect(w.hits).toEqual([]);
      expect(w.prose).toBe('');
    }
    // Fighters alone: by day they have no bombers to escort, by night they do not fly at all.
    const ftr = planRaid(g, 'bomb', t).seats.filter((s) => s.kind === 'ftr').map((s) => s.id);
    expect(ftr.length).toBeGreaterThan(0);
    const alone = dayOrNight(g, 'bomb', t, ftr)!;
    expect(alone.day.why).not.toBeNull();
    expect(alone.night.why).not.toBeNull();
  });

  it('shows the defences on the ground: the anti-aircraft guns, and the bombers the flak should bring down', () => {
    // The player's ask of 16 November: "also show defenses meaning anticipated flak and AA guns". The guns were always
    // in the forecast's numbers; now the card names them, and sets apart the bombers that fall to them.
    const g = west();
    let seen = 0, works = 0;
    for (const t of targets(g, 20)) {
      const dn = dayOrNight(g, 'bomb', t)!;
      if (dn.day.why || dn.night.why) continue;
      seen++;
      expect(dn.guns).toBe(gunsAt(g, t));
      expect(dn.gunsLabel).toBe(dn.guns ? `Level ${dn.guns} anti-aircraft guns` : 'Only the 60 light guns every target has.');
      for (const [w, p] of [[dn.day, planRaid(g, 'bomb', t)], [dn.night, planRaid(g, 'night', t)]] as const) {
        expect(w.gunLossN).toBe(p.guns.lossN);
        expect(w.gunLoss).toBe(p.guns.loss);
        expect(w.gunsStop).toBe(p.guns.stop);
        expect(w.toGuns).toBe(`about ${num(p.guns.lossN)}`);
        expect(w.gunsWords).toBe(p.guns.level ? 'of them to flak and guns' : 'of them to light flak');
        expect(w.stopNote).toBe(p.guns.stop > 0 ? `after the guns stop ${Math.round(p.guns.stop * 100)}% of the bombing` : '');
        // A part of the bombers lost, never more: the rest fall to fighters.
        expect(w.gunLossN).toBeLessThanOrEqual(w.bomberLossN);
        expect(w.gunLoss).toBeLessThanOrEqual(w.bomberLoss + 1e-12);
        expect(w.gunLoss).toBeGreaterThan(0);
      }
      // Flak finds fewer of them in the dark.
      expect(dn.night.gunLoss).toBeLessThan(dn.day.gunLoss);
      // Guns built round the target bring down more, and stop part of the bombing; with none, only the flak any target puts up.
      const key = `wk:aa:${t}`, was = g.flags[key];
      g.flags[key] = 0;
      const none = dayOrNight(g, 'bomb', t)!;
      g.flags[key] = 2;
      const armed = dayOrNight(g, 'bomb', t)!;
      if (was === undefined) delete g.flags[key]; else g.flags[key] = was;
      expect(none.guns).toBe(0);
      expect(none.gunsLabel).toBe('Only the 60 light guns every target has.');
      expect(none.day.gunsStop).toBe(0);
      expect(none.day.gunsWords).toBe('of them to light flak');
      expect(none.gunsTail).toBe('');
      expect(none.day.stopNote).toBe('');
      expect(armed.guns).toBe(2);
      expect(armed.gunsLabel).toBe('Level 2 anti-aircraft guns');
      expect(armed.day.gunsStop).toBeCloseTo(0.3, 9);
      expect(armed.day.gunsWords).toBe('of them to flak and guns');
      expect(armed.gunsTail).toBe('');
      expect(armed.night.stopNote).toBe('after the guns stop 30% of the bombing');
      expect(armed.night.gunsStop).toBeCloseTo(0.3, 9);
      expect(armed.day.gunLoss).toBeGreaterThan(none.day.gunLoss);
      expect(armed.day.bomberLoss).toBeGreaterThan(none.day.bomberLoss);
      if (g.regions[t].fac > 0) { works++; expect(armed.day.figures.output!).toBeLessThan(none.day.figures.output!); }
    }
    expect(seen).toBeGreaterThan(2);
    expect(works).toBeGreaterThan(0);
    // A way that cannot be flown has no figures for the guns either.
    const idle = dayOrNight(g, 'bomb', targets(g)[0], [])!;
    expect(idle.day.toGuns).toBe('');
    expect(idle.day.gunsWords).toBe('');
    expect(idle.day.gunLossN).toBe(0);
  });

  it('changes nothing, and is plain data both builds can read', () => {
    const g = west();
    const before = saveGame(g);
    for (const t of targets(g, 4)) {
      const dn = previewDayNight(g, 'bomb', t)!;
      expect(JSON.parse(JSON.stringify(dn))).toEqual(dn);
      expect(previewDayNight(g, 'bomb', t)).toEqual(dayOrNight(g, 'bomb', t, undefined, g.human));
    }
    expect(saveGame(g)).toBe(before);
  });
});

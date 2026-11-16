// Ruling 107(d): where a bombing raid is ordered, BY DAY and BY NIGHT stand side by side, each with the bombers likely
// lost and the damage expected, in the counts and odds the raid card gives. One shared function, `dayOrNight`, weighs
// the same bombers against the same target both ways, so the browser's raid card and the 3D game's say the same thing.
// Display only: it changes nothing in the game, and no rule of the night raid is touched here.
import { describe, expect, it } from 'vitest';
import { dayOrNight, planRaid, type RaidKind } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { previewDayNight, previewRaid, raidTargetsFor } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const pct = (x: number): string => `${Math.round(x * 100)}%`;
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
    const ts = targets(g);
    expect(ts.length).toBeGreaterThan(2);
    for (const t of ts) {
      const day = planRaid(g, 'bomb', t), night = planRaid(g, 'night', t, day.picked);
      const dn = dayOrNight(g, 'bomb', t, day.picked)!;
      expect(dn.chosen).toBe('bomb');
      expect(dn.day.label).toBe('BY DAY');
      expect(dn.night.label).toBe('BY NIGHT');
      for (const [w, p] of [[dn.day, day], [dn.night, night]] as const) {
        expect(w.why).toBe(p.why);
        expect(w.picked).toEqual(p.picked);
        expect(w.bombers).toBe(p.bombers);
        expect(w.bomberLoss).toBe(p.bomberLoss);
        expect(w.bomberLossN).toBe(p.bomberLossN);
        expect(w.figures).toEqual(p.figures);
        expect(w.success).toBe(p.success);
        if (p.why) continue;
        // Counts and odds, as the card prints them.
        expect(w.lost).toEqual({ n: `${p.bomberLossN.toLocaleString('en-US')} of ${p.bombers.toLocaleString('en-US')}`, label: 'Bombers likely lost', sub: `${pct(p.bomberLoss)} of them` });
        if (g.regions[t].fac > 0) {
          expect(w.harm.n).toBe(pct(p.figures.output!));
          expect(w.harm.label).toBe(`${g.regions[t].name}'s output stopped`);
          expect(w.harm.sub).toContain(`${(p.figures.national! * 100).toFixed(1)}% of ${g.nations[g.regions[t].ctrl].adj} production`);
          // The card's own sentence carries the same two numbers.
          expect(p.effects[0]).toContain(`about ${w.harm.n} of`);
        } else {
          expect(p.figures.output).toBeUndefined();
          expect(w.harm.sub).toContain('No industry at');
        }
      }
    }
  });

  it('flies the same bombers both ways: by day with their escort, by night alone', () => {
    const g = west();
    let withEscort = 0;
    for (const t of targets(g, 60)) {
      const day = planRaid(g, 'bomb', t);
      // Every fighter group that could go is asked for as well.
      const ask = [...day.picked, ...day.seats.filter((s) => !s.why && s.kind === 'ftr').map((s) => s.id)];
      const dn = dayOrNight(g, 'bomb', t, ask)!;
      if (dn.day.why || dn.night.why) continue;
      expect(dn.night.fighters).toBe(0);
      expect(dn.night.bombers).toBe(dn.day.bombers);
      expect(dn.night.with).toBe('The bombers go alone: no escort flies by night.');
      for (const id of dn.night.picked) expect(dn.day.picked).toContain(id);
      if (dn.day.fighters > 0) { withEscort++; expect(dn.day.with).toBe(`${dn.day.fighters.toLocaleString('en-US')} fighters go as escort.`); }
      else expect(dn.day.with).toBe('No escort goes with them.');
      // Asked from the night side with the same groups, the two ways are the same two ways.
      const back = dayOrNight(g, 'night', t, ask)!;
      expect(back.chosen).toBe('night');
      expect(back.day).toEqual(dn.day);
      expect(back.night).toEqual(dn.night);
    }
    expect(withEscort).toBeGreaterThan(0);
  });

  it('says in one sentence what the choice is, and by night fewer bombs find the target', () => {
    const g = west();
    let seen = 0;
    for (const t of targets(g)) {
      const dn = dayOrNight(g, 'bomb', t)!;
      expect(dn.note).toBe('By night the fighters and the guns find fewer of the bombers, and the bombers find less of the target.');
      if (dn.day.why || dn.night.why || g.regions[t].fac <= 0 || dn.day.bombers !== dn.night.bombers) continue;
      seen++;
      expect(dn.night.figures.output!).toBeLessThan(dn.day.figures.output! + 1e-9);
    }
    expect(seen).toBeGreaterThan(0);
  });

  it('says why a way cannot be flown, and still shows the other', () => {
    const g = west();
    const t = targets(g)[0];
    const day = planRaid(g, 'bomb', t);
    // Fighters alone: by day they have no bombers to escort, by night they do not fly at all.
    const ftr = day.seats.filter((s) => !s.why && s.kind === 'ftr').map((s) => s.id);
    if (ftr.length) {
      const dn = dayOrNight(g, 'bomb', t, ftr)!;
      expect(dn.day.why).not.toBeNull();
      expect(dn.night.why).not.toBeNull();
      expect(dn.day.lost.n).toBe('');
      expect(dn.day.harm.n).toBe('');
    }
    // Nobody picked.
    const none = dayOrNight(g, 'night', t, [])!;
    expect(none.day.why).toBe('Pick at least one air group.');
    expect(none.night.why).toBe('Pick at least one air group.');
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

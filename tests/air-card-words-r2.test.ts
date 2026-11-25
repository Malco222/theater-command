// The pilots' and doctrine card words under Units decide (release 2; the UI director's ruling of 24 Nov 2026 18:10Z on
// the air thread's draft, air-battles/dogfight/card-words-pilots-1124.md). One ladder of grade words, the army's own; the
// grade leads, the hours follow where the school is read; the refit note only in the month new pilots took the grade
// down a step; the schools and doctrine first on the player's own group page; the pilots' training on the raid card only
// where both sides' hours are read. "Not yet read" and raw figures never reach a card. Off, every card is as it was.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { assessAir, deckBred, joinPilots, openPilots, pilotWords, schoolWords } from '../src/campaign/air';
import { armsOf } from '../src/campaign/units';
import { raidCardLines } from '../src/campaign/fighters';
import { groupRow, raidTargets, planRaid } from '../src/campaign/airwar';
import { groupLines } from '../src/campaign/airpages';
import { turnToDate } from '../src/core/ticket';
import type { AirFleet, Game } from '../src/campaign/types';

function open(on: boolean, seed = 1, human: string | null = null): Game {
  const g = createGame(seed, human);
  if (on) g.flags.unitsDecide = 1;
  return startOpen(g);
}
const land = (g: Game, nat: string): AirFleet[] => g.airFleets.filter((f) => f.nat === nat && !deckBred(f) && f.ftr + f.bmr > 0);
const LADDER = ['Green', 'Trained', 'Seasoned', 'Veteran', 'Elite'];
const NO_RAW = /not yet read|\b0\.\d|\bfxp\b|\bk\b/;

describe('off: every card as it was', () => {
  const g = open(false, 1, 'GER');
  it('no pilots, refit or schools words on any group', () => {
    for (const f of g.airFleets) {
      const r = groupRow(g, f, f.nat);
      expect([r.pilots, r.joined, r.schools], f.name).toEqual(['', '', '']);
      expect(groupLines(r).join(' ')).not.toMatch(/pilots/);
    }
  });
});

describe('on: the group card', () => {
  const g = open(true, 1, 'GER');

  it('the grade leads, the hours follow where the school is read; unread hours leave the clause out', () => {
    const ger = land(g, 'GER')[0], uk = land(g, 'UK')[0];
    const a = pilotWords(g, ger).pilots, b = pilotWords(g, uk).pilots;
    expect(a).toMatch(new RegExp(`^(${LADDER.join('|')}) pilots, 250 hours' training\\.$`));
    expect(b).toMatch(new RegExp(`^(${LADDER.join('|')}) pilots\\.$`));
    for (const s of [a, b]) expect(s).not.toMatch(NO_RAW);
    // Its own sentence after Readiness, on the group's line.
    const line = groupLines(groupRow(g, ger, 'GER'))[0];
    expect(line).toMatch(/Readiness \d+%\. \w+ pilots, 250 hours' training\.$/);
  });

  it('the refit note only in the month new pilots took the grade down a step, and never after', () => {
    const h = open(true, 1, 'GER');
    const f = land(h, 'GER').find((x) => x.ftr >= 20)!;
    f.fxp = 0.45; // seasoned
    delete f.joined;
    const T = f.ftr + f.bmr, n1 = Math.max(1, Math.floor(T * 0.05)), n2 = Math.ceil(T * 0.2);
    joinPilots(h, f, n1, 'ftr', 'school');
    // A few new against many: still seasoned, so no note.
    expect(pilotWords(h, f).pilots).toMatch(/^Seasoned/);
    expect(pilotWords(h, f).joined).toBe('');
    f.ftr += n1;
    joinPilots(h, f, n2, 'ftr', 'school');
    f.ftr += n2;
    const w = pilotWords(h, f);
    expect(f.fxp!).toBeLessThan(0.4);
    expect(w.pilots).toMatch(/^Trained pilots/);
    expect(w.joined).toBe(`${n1 + n2} new pilots joined this month.`);
    expect(groupLines(groupRow(h, f, 'GER'))[0]).toMatch(new RegExp(`training\\. ${n1 + n2} new pilots joined this month\\.$`));
    h.turn++;
    expect(pilotWords(h, f).joined).toBe('');
  });

  it('the schools and doctrine first on the player\'s own group page only; unread doctrine left out; a change training in says when', () => {
    const h = open(true, 1, 'GER');
    const f = land(h, 'GER')[0], A = armsOf(h, 'GER')!.air;
    expect(A.fighter).toBeNull();
    expect(schoolWords(h, f)).toBe('Your flying schools give 250 hours before combat.');
    expect(groupRow(h, f, 'GER').schools).toBe('Your flying schools give 250 hours before combat.');
    expect(groupRow(h, f, 'UK').schools).toBe('');
    A.fighter = { id: 'turn', next: 'dive-zoom', ready: h.turn + 6, by: 'chosen' };
    expect(schoolWords(h, f)).toBe(`Your flying schools give 250 hours before combat. Fighter doctrine: turning fight; dive and climb ready in ${turnToDate(h.turn + 6).label}.`);
    h.turn += 6;
    expect(schoolWords(h, f)).toBe('Your flying schools give 250 hours before combat. Fighter doctrine: dive and climb.');
    // A school not read: only the doctrine, or nothing.
    const uk = land(h, 'UK')[0];
    expect(schoolWords(h, uk)).toBe('');
    armsOf(h, 'UK')!.air.fighter = { id: 'dive-zoom', by: 'chosen' };
    expect(schoolWords(h, uk)).toBe('Fighter doctrine: dive and climb.');
  });

  it('a carrier wing\'s page names the carrier school', () => {
    const h = open(true, 1, 'UK');
    armsOf(h, 'UK')!.air.deck = 500;
    openPilots(h);
    const w = h.airFleets.find((x) => x.nat === 'UK' && deckBred(x))!;
    expect(schoolWords(h, w)).toBe('Your carrier school gives 500 hours before combat.');
  });
});

describe('on: the raid card', () => {
  it('no pilots\' line until both sides\' hours are read; then ours against theirs, beside the types line', () => {
    const g = open(true);
    for (const t of raidTargets(g, 'superiority', 'GER')) {
      const ids = planRaid(g, 'superiority', t.id, undefined, 'GER').picked ?? [];
      const fs = g.airFleets.filter((x) => ids.includes(x.id));
      if (!fs.length) continue;
      const a1 = assessAir(g, fs, 'superiority', t.id);
      const nats = new Set(a1.def.map((x) => x.f.nat));
      if (a1.why || a1.Epow <= 0 || a1.Dpow <= 0 || nats.size !== 1 || a1.def.some((x) => deckBred(x.f))) continue;
      expect(a1.pilotsLine).toBeUndefined();
      expect(raidCardLines(g, a1, 'superiority')?.pilots).toBeUndefined();
      const A = armsOf(g, [...nats][0])!.air;
      A.school = 350; A.pool = 350;
      openPilots(g);
      const a2 = assessAir(g, fs, 'superiority', t.id);
      expect(a2.pilotsLine).toBe("Pilots' training: ours 250 hours, theirs 350.");
      const card = raidCardLines(g, a2, 'superiority')!;
      expect(card.types).toMatch(/^Fighters at/);
      expect(card.pilots).toBe(a2.pilotsLine);
      return;
    }
    throw new Error('no sweep with one nation\'s land groups defending');
  });

  it('off: no pilots\' line', () => {
    const g = open(false);
    for (const t of raidTargets(g, 'superiority', 'GER').slice(0, 10)) {
      const ids = planRaid(g, 'superiority', t.id, undefined, 'GER').picked ?? [];
      const fs = g.airFleets.filter((x) => ids.includes(x.id));
      if (fs.length) expect(assessAir(g, fs, 'superiority', t.id).pilotsLine).toBeUndefined();
    }
  });
});

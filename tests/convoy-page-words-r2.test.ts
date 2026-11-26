// The convoy practice page's after card: what a campaign would make of the night (release 2; the UI director's W5b and
// final lines W5c and W5d of 26 November 2026, on the design director's 12:04Z ruling: the month is said in its share as
// booked, never split, with the night's own ships among it). The staff's nights here are laid by hand, so that each
// branch the words take is reached on one practice ticket: no ship lost (with none, one, some or all of the staff's
// nights also losing none), ships lost untied and tied, the better, worse and ordinary places, and cards that book. The
// words are pinned as the UI director wrote them.
import { describe, expect, it } from 'vitest';
import { countKeys, type StaffNights } from '../src/battle/convoy/calibrate';
import { CONVOY_PRACTICE, convoyPracticeTicket } from '../src/battle/convoy/practice';
import type { ConvoyOverExtra, ConvoyTicket } from '../src/battle/convoy/types';
import type { Side } from '../src/battle/core/types';
import { campaignWords } from '../src/battle/ui/convoy-words';

const practice = convoyPracticeTicket(CONVOY_PRACTICE[0], 1234, 'def');
/** The same night as a campaign would hand it: every fleet that brings anything expects some losses, so the card books them. */
const booking: ConvoyTicket = (() => {
  const t = JSON.parse(JSON.stringify(practice)) as ConvoyTicket;
  for (const f of t.place.fleets) if (f.n > 0) f.expect = 1.4;
  return t;
})();
/** Thirty staff nights: the merchants sunk as given, every boat and escort count 0 or 1 by turns (or 'each' in all). */
function staff(t: ConvoyTicket, mv: number[], each?: number): StaffNights {
  const counts: Record<string, number[]> = {};
  for (const k of countKeys(t)) counts[k] = k === 'mv' ? mv : mv.map((_, i) => each ?? i % 2);
  return { n: mv.length, counts, profile: {}, tallies: {} };
}
/** The night as a campaign would hand it with only one side's fleets expecting losses, each 'expect'. */
function bookingOf(side: Side | 'both', expect: number): ConvoyTicket {
  const t = JSON.parse(JSON.stringify(practice)) as ConvoyTicket;
  for (const f of t.place.fleets) f.expect = f.n > 0 && (side === 'both' || f.side === side) ? expect : 0;
  return t;
}
const night = (mv: number): ConvoyOverExtra => ({ counts: { mv, ss: {}, dd: {} }, handed: false, steered: { att: 0, def: 0 } } as unknown as ConvoyOverExtra);
const nights = (spec: [number, number][]): number[] => spec.flatMap(([v, k]) => new Array(k).fill(v));
const SHARE = '\\d+% of the tonnage a convoy usually loses';

describe('the after card, in practice (W5c)', () => {
  for (const side of ['att', 'def'] as Side[]) {
    it(`a night that lost no ship says the share and that all of it was lost elsewhere on the route (${side})`, () => {
      const none = campaignWords(practice, night(0), staff(practice, nights([[1, 15], [2, 15]])), 'def', side);
      expect(none).toMatch(new RegExp(`^The convoy lost no ships, fewer than in any of the 30 times the staff fought this night\\. In a campaign this night would set the month at ${SHARE}, all of it lost elsewhere on the route\\. A practice night books nothing\\.$`));
      const some = campaignWords(practice, night(0), staff(practice, nights([[0, 21], [1, 9]])), 'def', side);
      expect(some).toMatch(new RegExp(`^The convoy lost no ships, as in 21 of the 30 times the staff fought this night\\. In a campaign a draw among the tied nights would set the month at ${SHARE}, all of it lost elsewhere on the route\\. A practice night books nothing\\.$`));
      const all = campaignWords(practice, night(0), staff(practice, nights([[0, 30]])), 'def', side);
      expect(all).toMatch(new RegExp(`^The convoy lost no ships, as in every one of the 30 times the staff fought this night\\. In a campaign a draw among the tied nights would set the month at ${SHARE}, all of it lost elsewhere on the route\\. A practice night books nothing\\.$`));
      // (one staff night tied: this night and that one make two, W5d 4)
      const one = campaignWords(practice, night(0), staff(practice, nights([[0, 1], [1, 29]])), 'def', side);
      expect(one).toMatch(new RegExp(`^The convoy lost no ships, as in one of the 30 times the staff fought this night\\. In a campaign a draw among the tied nights would set the month at ${SHARE}, all of it lost elsewhere on the route\\.`));
      for (const c of [none, some, all, one]) expect(c).not.toMatch(/better than|those nights|among them/);
    });

    it(`a night that lost ships and drew no lot counts them among the month's share (${side})`, () => {
      const three = campaignWords(practice, night(3), staff(practice, nights([[1, 15], [5, 15]])), 'att', side);
      expect(three).toMatch(new RegExp(`^In a campaign this night would count as [^:]+: the month would book ${SHARE}, with this night's three ships among it\\. A practice night books nothing\\.$`));
      expect(campaignWords(practice, night(1), staff(practice, nights([[0, 15], [5, 15]])), 'att', side)).toMatch(new RegExp(`: the month would book ${SHARE}, with this night's ship among it\\. A practice night books nothing\\.$`));
      expect(campaignWords(practice, night(12), staff(practice, nights([[1, 15], [20, 15]])), 'att', side)).toMatch(new RegExp(`: the month would book ${SHARE}, with this night's 12 ships among it\\. A practice night books nothing\\.$`));
    });

    it(`a night that favours the side that lost it says whose night it was, and who won it (${side})`, () => {
      // (nine ships sunk where every staff night sank one: the pack's way; the escorts won it all the same)
      const pack = campaignWords(practice, night(9), staff(practice, nights([[1, 30]])), 'def', side);
      const forPack = side === 'att' ? 'one of the better nights the staff could have had for the pack, by the ships it sank'
        : 'one of the worse nights the staff could have had for the escorts, by the ships they lost';
      expect(pack).toMatch(new RegExp(`^In a campaign this night would count as ${forPack}, though the escorts won it: the month would book ${SHARE}, with this night's nine ships among it\\. A practice night books nothing\\.$`));
      // (one ship sunk where every staff night sank five: the escorts' way; the pack won it all the same)
      const escorts = campaignWords(practice, night(1), staff(practice, nights([[5, 30]])), 'att', side);
      const forEscorts = side === 'att' ? 'one of the worse nights the staff could have had for the pack, by the ships it sank'
        : 'one of the better nights the staff could have had for the escorts, by the ships they lost';
      expect(escorts).toMatch(new RegExp(`^In a campaign this night would count as ${forEscorts}, though the pack won it: the month would book ${SHARE}, with this night's ship among it\\. A practice night books nothing\\.$`));
      // (an ordinary night names no winner)
      const ordinary = campaignWords(practice, night(3), staff(practice, nights([[1, 15], [5, 15]])), 'att', side);
      const who = side === 'att' ? 'the pack, by the ships it sank' : 'the escorts, by the ships they lost';
      expect(ordinary).toMatch(new RegExp(`^In a campaign this night would count as an ordinary night for ${who}, about as the staff would have fought it: the month would book ${SHARE}, with this night's three ships among it\\.`));
    });

    it(`a tied night's line stands as built (${side})`, () => {
      const tied = campaignWords(practice, night(3), staff(practice, nights([[3, 5], [1, 10], [6, 15]])), 'att', side);
      const who = side === 'att' ? 'the pack sank' : 'the escorts lost';
      expect(tied).toMatch(new RegExp(`: ${who} as many ships as in five of the 30 times the staff fought this night, and a draw among the tied nights set the month's share at ${SHARE}\\. A practice night books nothing\\.$`));
    });
  }

  it('says no rest it cannot book: no other nights, and no "it" left pointing at the tonnage', () => {
    const cards = [
      campaignWords(practice, night(0), staff(practice, nights([[1, 30]])), 'def', 'def'),
      campaignWords(practice, night(3), staff(practice, nights([[1, 15], [5, 15]])), 'att', 'att'),
      campaignWords(booking, night(3), staff(booking, nights([[1, 15], [5, 15]])), 'att', 'att'),
      campaignWords(booking, night(0), staff(booking, nights([[1, 30]])), 'def', 'def', true),
    ];
    for (const c of cards) {
      expect(c).not.toMatch(/other nights|and the rest/);
      expect(c).not.toContain(' Its ');
    }
  });
});

describe('the after card, for a night a campaign books (W5c (e))', () => {
  it('a card that books says the night\'s own boats and escorts, as it would book them', () => {
    const c = campaignWords(booking, night(3), staff(booking, nights([[1, 15], [5, 15]])), 'att', 'att');
    expect(c.endsWith(' The boats and escorts lost this night, placed the same way by their own counts, would book one boat lost (the dice allow one or two) and one escort lost (the dice allow one or two).')).toBe(true);
    expect(c).not.toContain('A practice night books nothing');
  });

  it('says a nought in words, and names only the side that books (W5d 5)', () => {
    // (each fleet expects 0.6: the dice allow none or one; every staff night lost one of each, this night none: none booked)
    const both = campaignWords(bookingOf('both', 0.6), night(3), staff(bookingOf('both', 0.6), nights([[1, 15], [5, 15]]), 1), 'att', 'att');
    expect(both.endsWith(' The boats and escorts lost this night, placed the same way by their own counts, would book no boats lost (the dice allow none or one) and no escorts lost (the dice allow none or one).')).toBe(true);
    const escorts = campaignWords(bookingOf('def', 0.6), night(3), staff(bookingOf('def', 0.6), nights([[1, 15], [5, 15]]), 1), 'att', 'def', true);
    expect(escorts.endsWith(' The escorts lost this night, placed the same way by their own counts, book no escorts lost (the dice allow none or one).')).toBe(true);
    const boats = campaignWords(bookingOf('att', 1.4), night(3), staff(bookingOf('att', 1.4), nights([[1, 15], [5, 15]])), 'att', 'att');
    expect(boats.endsWith(' The boats lost this night, placed the same way by their own counts, would book one boat lost (the dice allow one or two).')).toBe(true);
    for (const c of [both, escorts, boats]) expect(c).not.toMatch(/\b0\b/);
  });

  for (const side of ['att', 'def'] as Side[]) {
    it(`in a campaign: no "would", no practice sentence (${side})`, () => {
      const books = / The boats and escorts lost this night, placed the same way by their own counts, book one boat lost \(the dice allow one or two\) and one escort lost \(the dice allow one or two\)\.$/;
      const three = campaignWords(booking, night(3), staff(booking, nights([[1, 15], [5, 15]])), 'att', side, true);
      const who = side === 'att' ? 'the pack, by the ships it sank' : 'the escorts, by the ships they lost';
      expect(three).toMatch(new RegExp(`^This night counts as an ordinary night for ${who}, about as the staff would have fought it: the month books ${SHARE}, with this night's three ships among it\\.`));
      expect(three).toMatch(books);
      const clean = campaignWords(booking, night(0), staff(booking, nights([[1, 30]])), 'def', side, true);
      expect(clean).toMatch(new RegExp(`^The convoy lost no ships, fewer than in any of the 30 times the staff fought this night\\. This night set the month at ${SHARE}, all of it lost elsewhere on the route\\.`));
      expect(clean).toMatch(books);
      const drawn = campaignWords(booking, night(0), staff(booking, nights([[0, 21], [1, 9]])), 'def', side, true);
      expect(drawn).toMatch(new RegExp(`\\. A draw among the tied nights set the month at ${SHARE}, all of it lost elsewhere on the route\\.`));
      // (the booking's own mood: 'about as the staff would have fought it' is the staff's, and stays)
      for (const c of [three, clean, drawn]) {
        expect(c).not.toMatch(/would (set|book|count)|practice|In a campaign/);
      }
      // (a campaign night whose card books nothing says nothing of it)
      expect(campaignWords(practice, night(0), staff(practice, nights([[1, 30]])), 'def', side, true)).toMatch(new RegExp(`all of it lost elsewhere on the route\\.$`));
    });
  }
});

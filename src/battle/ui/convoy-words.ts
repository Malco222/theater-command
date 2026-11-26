// The convoy night's after card in words: what a campaign would make of the night (release 2; the UI director's W5,
// W5b and final lines W5c and W5d, 26 November 2026, on the design director's 12:04Z ruling). Kept apart from the practice page,
// which boots itself when it loads, so that the screen that offers a night in a campaign (217(5)) and the tests can say
// the same card without the page; it reads no page state.
import { CONVOY_BATTLE } from '../../campaign/navy';
import { againstStaff, foughtOf, ranksOf, type StaffNights } from '../convoy/calibrate';
import { countWords, sharesOf } from '../convoy/sim';
import type { ConvoyOverExtra, ConvoyTicket } from '../convoy/types';
import type { Side } from '../core/types';

/** A count and its noun, singular or plural, one to ten in words (the UI director, round 5): 'one torpedo', 'three torpedoes', '12 hits'. */
export const nof = (k: number, one: string, many: string): string => `${countWords(k)} ${k === 1 ? one : many}`;
/** And-joined: 'a', 'a and b', 'a, b and c'. */
export const andOf = (xs: string[]): string => xs.length > 1 ? `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}` : xs[0] ?? '';

/** The night's place among the staff's nights by the ships sunk, 0 to 1 as the pack reads it: the booked swing's own (calibrate.ts foughtOf on the night's ranks). */
function placeOf(t: ConvoyTicket, x: ConvoyOverExtra, staff: StaffNights): number {
  const { low, high } = CONVOY_BATTLE.swing;
  return (foughtOf(t, ranksOf(t, x.counts, staff)).swing - low) / Math.max(1e-6, high - low);
}

/**
 * The side the night favours by the ships sunk (the campaign line's better and worse nights): the pack's where its place
 * is 0.6 or more and it sank more ships than at least half the staff's nights did, the escorts' where its place is 0.4
 * or less and it sank fewer than at least half did, else neither. A tie drawn by lot may place a night high among the
 * staff's nights it ties, but a night that sank no more ships than most of them is never one of the better ones for the
 * pack, nor one that sank no fewer one of the better ones for the escorts.
 */
export function leanOf(t: ConvoyTicket, x: ConvoyOverExtra, staff: StaffNights): Side | null {
  const place = placeOf(t, x, staff), a = againstStaff(t, x.counts, staff), half = a.n / 2;
  return place >= 0.6 && a.mv.less >= half ? 'att' : place <= 0.4 && a.mv.more >= half ? 'def' : null;
}

/**
 * What a campaign would make of the night, in plain words: where the night falls among the staff's nights by the ships
 * sunk, which is what sets the tonnage a month would book (calibrate.ts foughtOf on the night's ranks, unchanged), and
 * the share it would book. Where that place favours the side the headline did not name the winner, it says who won it
 * after all ('..., though the escorts won it'), so the two never read as one against the other (the playtester's m5).
 * A practice night books nothing and is said so, in two sentences in place of the booking's rules (the UI director,
 * round 7 W5, the playtester's oldbugs-1): ties drawn by lot say that a draw set the share. The share is said where the
 * night lost no ship too, and where it fell: all of it lost elsewhere on the route (W5b: the design director's 06:32Z
 * ruling (a), the campaign keeps the lot; firsttimer-8's share over a convoy that lost nothing is so read as the
 * month's, not the night's; 'elsewhere', for the booked tons include raiders that are not boats: W5c (f)); where the
 * night lost ships and drew no lot, the month's share is said bare, as in round 7: a share of tonnage does not hold the
 * night's ships by any rule, so nothing says they lie in it (the design director, 13:16Z). The UI director's final
 * lines, W5c (c), (d), (e), 26 November 12:34Z, and W5d, 13:03Z. A night fought in a campaign ('inCampaign', set by the screen that offers it, none yet: 217(5)) says
 * what the month books, with no 'would' and no practice sentence; a card that books says the boats and escorts it
 * books, the night's own, so that no 'it' is left pointing at the tonnage.
 */
export function campaignWords(t: ConvoyTicket, x: ConvoyOverExtra, staff: StaffNights, winner: Side, side: Side, inCampaign = false): string {
  const a = againstStaff(t, x.counts, staff), n = Math.max(1, a.n);
  const ranks = ranksOf(t, x.counts, staff), fought = foughtOf(t, ranks);
  const { low, high } = CONVOY_BATTLE.swing;
  // (never printed as a bound it did not reach (the UI director, round 5, 5): sharesOf)
  const pc = sharesOf(fought.swing, high, low);
  // Boats and escorts lost: booked at the lower or the higher of the two whole counts around what the month expects, by
  // the same ranks. A practice card expects none: it books nothing at all.
  const booked = (sd: Side): string => {
    const fs = t.place.fleets.filter((f) => f.side === sd && f.n > 0);
    if (!fs.some((f) => f.expect > 0)) return '';
    const lo = fs.reduce((k, f) => k + Math.floor(f.expect), 0), hi = fs.reduce((k, f) => k + Math.ceil(f.expect), 0);
    const got = fs.reduce((k, f) => k + ((sd === 'att' ? fought.ss[f.id] : fought.dd[f.id]) ?? 0), 0);
    const what = sd === 'att' ? ['boat', 'boats'] : ['escort', 'escorts'];
    // (a nought in words: 'no escorts lost (the dice allow none or one)', W5d 5)
    const bound = (k: number): string => k === 0 ? 'none' : countWords(k);
    return `${got === 0 ? `no ${what[1]}` : nof(got, what[0], what[1])} lost${lo === hi ? '' : ` (the dice allow ${bound(lo)} or ${bound(hi)})`}`;
  };
  const boats = booked('att'), escorts = booked('def'), bits = [boats, escorts].filter(Boolean);
  // (the frame names only the side that books: W5d 5)
  const whose = boats && escorts ? 'The boats and escorts' : boats ? 'The boats' : 'The escorts';
  const books = bits.length ? ` ${whose} lost this night, placed the same way by their own counts, ${inCampaign ? 'book' : 'would book'} ${andOf(bits)}.`
    : inCampaign ? '' : ' A practice night books nothing.';
  // (no ship lost: how often the staff's nights lost none, and the share a draw among them sets, all of it lost
  // elsewhere on the route: W5b, W5c (c))
  if (x.counts.mv === 0) {
    const k = a.mv.same;
    const month = `${pc.share}% of the tonnage a convoy usually loses, all of it lost elsewhere on the route`;
    // (a draw among the tied nights, however many: this night and one more make two, W5d 4)
    const set = k === 0 ? (inCampaign ? `This night set the month at ${month}` : `In a campaign this night would set the month at ${month}`)
      : inCampaign ? `A draw among the tied nights set the month at ${month}` : `In a campaign a draw among the tied nights would set the month at ${month}`;
    // (fewer than in any, on either side's card: W5d 1)
    return `The convoy lost no ships, ${k === 0 ? `fewer than in any of the ${countWords(n)} times the staff fought this night` : `as in ${k === n ? 'every one' : countWords(k)} of the ${countWords(n)} times the staff fought this night`}. ${set}.${books}`;
  }
  // (the place is the booked swing's own, held to the counts: never one of the better nights for a side whose count most of the staff's nights matched or beat)
  const lean = leanOf(t, x, staff);
  const who = side === 'att' ? 'the pack, by the ships it sank' : 'the escorts, by the ships they lost';
  const place = lean === side ? `one of the better nights the staff could have had for ${who}` : lean ? `one of the worse nights the staff could have had for ${who}` : `an ordinary night for ${who}, about as the staff would have fought it`;
  // (the side the place favours did not win the night: said so, in the same sentence)
  const though = lean && lean !== winner ? `, though ${winner === 'att' ? 'the pack' : 'the escorts'} won it` : '';
  const share = `${pc.share}% of the tonnage a convoy usually loses`;
  // (tied: a lot settles where the night falls among the staff's nights it ties, and the share says where it fell; untied:
  // the month's share, bare (the design director, 13:16Z))
  const tons = a.mv.same > 0
    ? `${side === 'att' ? 'the pack sank' : 'the escorts lost'} as many ships as in ${a.mv.same === n ? 'every one' : countWords(a.mv.same)} of the ${countWords(n)} times the staff fought this night, and a draw among the tied nights set the month's share at ${share}`
    : `the month ${inCampaign ? 'books' : 'would book'} ${share}`;
  return `${inCampaign ? 'This night counts as' : 'In a campaign this night would count as'} ${place}${though}: ${tons}.${books}`;
}

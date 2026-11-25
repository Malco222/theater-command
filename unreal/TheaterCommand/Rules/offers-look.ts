// Formations offered out of the player's own corps, as the 3D game draws them (release 2, Japan and the Pacific; the
// design director's ruling B8 of 25 November 2026, design-director/pacific-japan-1125.md).
//
// A dated row that would take corps of the player's off the map is an offer, never a silent removal (the rules:
// src/campaign/game.ts `rowOffers`). "Smooth play first: the month's offers come as one list on the month card, take
// all or pick, never one card each." Nothing here decides anything: it reads the rules' list, puts it in plain words
// ready to draw, and hands the player's answer to the rules. With the whole world off there is never an offer, and
// TC.offers() answers a list of none.
//
//   TC.offers()                   -> { count, head, note, can, why, rows: [{ id, name, what, where, region, since, takes: [{ corps, name, where, region, men, size }], line, can, why }] }
//                                    `id` is what the two calls below take. `where` is the place it would form this month ('' where nowhere),
//                                    `takes` the corps of his it would strike off (name, where each stands, its men), `line` the whole of it in
//                                    one sentence, `can` whether it can be taken now and `why` the reason where it cannot.
//   TC.offerTake(ids | 'all')     -> { ok, taken: [id], left: [{ id, name, why }], said, offers, view }: he takes those offers (all the open ones for 'all');
//                                    one that cannot form this month stays open, with the reason
//   TC.offerDecline(ids | 'all')  -> { ok, declined: [id], said, offers, view }: those formations never come, and his corps stay as they are
//
// The same list rides on the month card (campaign-bridge.ts, the card's `offers`), so the card can show the count and
// the rows without a second call. campaign-bridge.ts adds the calls to TC with Object.assign(TC, offerCalls(game, view)).

import { formationOffers, orderDeclineFormation, orderTakeFormation, whyNotAnswerOffers, type RowOffer } from '../../../src/campaign/turn';
import { turnToDate } from '../../../src/core/ticket';
import { KIND, type Game } from '../../../src/campaign/types';

/** One of his corps an offer would take, in words. */
export interface OfferTakesLook { corps: number; name: string; where: string; region: number; men: number; size: string }
/** One offer, ready to draw. */
export interface OfferLook {
  id: number;
  /** What would form: the formation's name, and its arm and size in words. */
  name: string; what: string;
  /** Where it would form this month ('' and -1 where there is nowhere), and the month the offer was made. */
  where: string; region: number; since: string;
  takes: OfferTakesLook[];
  /** The whole offer in one sentence. */
  line: string;
  /** Whether he can take it now, and the reason where he cannot ('' where he can). */
  can: boolean; why: string;
}
/** The month's one list: how many are open, its heading and the line under it, and whether he can answer now. */
export interface OffersLook { count: number; head: string; note: string; can: boolean; why: string; rows: OfferLook[] }

const num = (n: number): string => Math.round(n).toLocaleString('en-US');
const listed = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);

function rowLook(s: Game, o: RowOffer, shut: string | null): OfferLook {
  const place = (id: number): string => s.regions[id]?.name ?? '';
  const takes = o.takes.map((t) => ({ corps: t.id, name: t.name, where: place(t.region), region: t.region, men: t.men, size: `${num(t.men)} men` }));
  const where = o.region >= 0 ? place(o.region) : '';
  const what = `${KIND[o.kind]?.label ?? 'Corps'}, ${num(o.men)} men`;
  const forms = o.joins ? `Joins ${o.joins}${where ? ` at ${where}` : ''}` : where ? `Forms at ${where}` : 'Has nowhere to form this month';
  const taking = takes.length ? `, taking ${listed(takes.map((t) => `${t.name} at ${t.where} (${t.size})`))}` : '';
  const why = o.why ?? shut ?? '';
  return { id: o.id, name: o.name, what, where, region: o.region, since: `Offered in ${turnToDate(o.since).label}`, takes, line: `${forms} (${what.toLowerCase()})${taking}.`, can: !why, why };
}

/** The player's open offers in plain words. A list of none where there is no player, no offer, or the whole world is off. */
export function offersLook(s: Game): OffersLook {
  let open: RowOffer[] = [];
  try { open = formationOffers(s); } catch { open = []; }
  const shut = open.length ? whyNotAnswerOffers(s) : null;
  const n = open.length;
  return {
    count: n,
    head: n ? `${n} formation${n === 1 ? '' : 's'} offered` : '',
    note: n ? `${n === 1 ? 'It would be formed' : 'Each would be formed'} out of corps you have, as the record formed ${n === 1 ? 'it' : 'them'}. Take ${n === 1 ? 'it or decline' : 'all, take some, or decline'}: an offer stands until you answer.` : '',
    can: n > 0 && !shut, why: shut ?? '',
    rows: open.map((o) => rowLook(s, o, shut)),
  };
}

/** The ids a call names: a list, one number, or 'all' for every open offer. */
function asked(s: Game, ids: unknown): number[] {
  if (ids === 'all') return formationOffers(s).map((o) => o.id);
  const list = Array.isArray(ids) ? ids : [ids];
  return [...new Set(list.map((x) => Number(x)).filter((x) => Number.isInteger(x) && x >= 0))];
}

/** The calls the 3D game makes: the list, and his answer to it. */
export function offerCalls(game: () => Game, view: () => unknown) {
  return {
    offers: (): OffersLook => offersLook(game()),
    offerTake: (ids: unknown) => {
      const s = game();
      const names = new Map(formationOffers(s).map((o) => [o.id, o.name]));
      const taken: number[] = [], left: { id: number; name: string; why: string }[] = [];
      for (const id of asked(s, ids)) {
        const why = orderTakeFormation(s, id);
        if (why === null) taken.push(id); else left.push({ id, name: names.get(id) ?? '', why });
      }
      const said = [
        taken.length ? `${taken.length === 1 ? `${names.get(taken[0])} is formed` : `${taken.length} formations are formed: ${listed(taken.map((id) => names.get(id) ?? ''))}`}.` : '',
        ...left.map((x) => (x.name ? `${x.name} is not formed: ${x.why}` : x.why)),
      ].filter(Boolean).join(' ');
      return { ok: taken.length > 0 && !left.length, taken, left, said, offers: offersLook(s), view: view() };
    },
    offerDecline: (ids: unknown) => {
      const s = game();
      const names = new Map(formationOffers(s).map((o) => [o.id, o.name]));
      const declined: number[] = [];
      for (const id of asked(s, ids)) if (orderDeclineFormation(s, id)) declined.push(id);
      const said = declined.length ? `${declined.length === 1 ? `${names.get(declined[0])} is not formed` : `${declined.length} formations are not formed`}: your corps stay as they are.` : '';
      return { ok: declined.length > 0, declined, said, offers: offersLook(s), view: view() };
    },
  };
}

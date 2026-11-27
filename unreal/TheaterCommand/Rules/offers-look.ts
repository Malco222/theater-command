// Formations offered out of the player's own corps, as the 3D game draws them (release 2, Japan and the Pacific; the
// design director's ruling B8 of 25 November 2026, design-director/pacific-japan-1125.md).
//
// A dated row that would take corps of the player's off the map is an offer, never a silent removal (the rules:
// src/campaign/game.ts `rowOffers`). "Smooth play first: the month's offers come as one list on the month card, take
// all or pick, never one card each." Nothing here decides anything: it reads the rules' list, puts it in plain words
// ready to draw, and hands the player's answer to the rules. With the whole world off there is never an offer, and
// TC.offers() answers a list of none.
//
//   TC.offers()                   -> { count, head, note, can, why, rows: [{ id, name, what, where, region, since, takes: [{ corps, name, where, region, men, size }], line, after, can, why, loan }] }
//                                    `id` is what the two calls below take. `where` is the place it would form this month ('' where nowhere),
//                                    `takes` the corps of his it would strike off (name, where each stands, its men), `line` the whole of it in
//                                    one sentence, `can` whether it can be taken now and `why` the reason where it cannot. `after` is a sentence
//                                    for the formations he gives up by declining it, those made out of it ('' where there is none).
//                                    `loan` is true for the end of a loan: a regiment only lent to a campaign, whose time there is up and whose
//                                    division awaits it. It is in the same list because it is the same case: the regiment leaves only if he lets
//                                    it go. Its `name`, `what` and `where` are the lent regiment's own, `takes` is that regiment, and `line` says
//                                    what it was lent for and what each answer does.
//   TC.offerTake(ids | 'all')     -> { ok, taken: [id], left: [{ id, name, why }], said, offers, view }: he takes those offers (all the open ones for 'all');
//                                    one that cannot form this month stays open, with the reason. A loan's end taken: the regiment is let go.
//   TC.offerDecline(ids | 'all')  -> { ok, declined: [id], said, offers, view }: those formations never come, and his corps stay as they are.
//                                    A loan's end declined: the regiment stays under his command for good.
//
// The same list rides on the month card (campaign-bridge.ts, the card's `offers`), so the card can show the count and
// the rows without a second call. campaign-bridge.ts adds the calls to TC with Object.assign(TC, offerCalls(game, view)).

import { formationOffers, orderDeclineFormation, orderTakeFormation, whyNotAnswerOffers, type RowOffer } from '../../../src/campaign/turn';
import { loansEnding, onOrAt, placeName, rowsAwaitingWar, theName } from '../../../src/campaign/game';
import { heldLoanAside } from '../../../src/campaign/arrivalwaits';
import { menWords } from '../../../src/campaign/manpower';
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
  /** What declining it also gives up: the formations made out of it, in a sentence ('' where there is none). */
  after: string;
  /** Whether he can take it now, and the reason where he cannot ('' where he can). */
  can: boolean; why: string;
  /** True for the end of a loan (the lent regiment goes home if he takes it, stays his if he declines); false for a formation offered. */
  loan: boolean;
}
/** The month's one list: how many are open, its heading and the line under it, and whether he can answer now. */
export interface OffersLook { count: number; head: string; note: string; can: boolean; why: string; rows: OfferLook[] }

const num = (n: number): string => Math.round(n).toLocaleString('en-US');
const listed = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);

function rowLook(s: Game, o: RowOffer, shut: string | null): OfferLook {
  const place = (id: number): string => s.regions[id]?.name ?? '';
  const takes = o.takes.map((t) => ({ corps: t.id, name: t.name, where: place(t.region), region: t.region, men: t.men, size: `${num(t.men)} men` }));
  const where = o.region >= 0 ? place(o.region) : '';
  // (The kind in the division words, "Armoured", not "Armor": the UI director's marks, section 2.)
  const what = `${kindWord(o.kind)}, ${num(o.men)} men`;
  const forms = o.joins ? `Joins ${o.joins}${where ? ` at ${where}` : ''}` : where ? `Forms at ${where}` : 'Has nowhere to form this month';
  const taking = takes.length ? `, taking ${listed(takes.map((t) => `${t.name} at ${t.where} (${t.size})`))}` : '';
  const why = o.why ?? shut ?? '';
  // (The end of a loan says its own sentence: what it was lent for, and what letting it go and keeping it do.)
  const line = o.loan ?? `${forms} (${what.toLowerCase()})${taking}.`;
  const since = `${o.loan === null ? 'Offered in' : 'Its loan ended in'} ${turnToDate(o.since).label}`;
  // (The marks, section 2.3: "which would later be formed out of it"; "which is made out of it" read as though it already existed.)
  const after = o.then.length ? `Declining it also gives up ${listed(o.then)}, which would later be formed out of it.` : '';
  return { id: o.id, name: o.name, what, where, region: o.region, since, takes, line, after, can: !why, why, loan: o.loan !== null };
}

// (The words of the head, the note and the offers page's rows are the UI director's marks of 25 November 2026,
// 13:39Z, pacific-japan/wip/offers-list-UI-MARKS.md, word for word; {braces} there are the figures computed here.)

/** "10 formations offered", "10 formations offered, 1 loan ending", "1 loan ending" (the marks, section 5). */
export function headOf(forms: number, loans: number): string {
  const a = forms ? `${forms} formation${forms === 1 ? '' : 's'} offered` : '', b = loans ? `${loans} loan${loans === 1 ? '' : 's'} ending` : '';
  return [a, b].filter(Boolean).join(', ');
}

/**
 * The note under the head (the marks, section 5): one formation, more than one, and the line added with loans. (With
 * loans alone it is the loan's line only: the marks word no other, and the formations' lines would not be true.)
 */
export function noteOf(forms: number, loans: number): string {
  const a = forms === 1 ? 'It would be formed out of corps you have, as history formed it. It waits for your answer until its last month, or until a corps it needs is gone.'
    : forms > 1 ? 'Each would be formed out of corps you have, as history formed it. Each waits for your answer until its last month, or until a corps it needs is gone.' : '';
  return [a, loans ? "A loan's end: send the regiment home into its division, or keep it for good." : ''].filter(Boolean).join(' ');
}

/** A month in words: "October 1939". */
const monthOf = (t: number): string => turnToDate(t).label;
/** The kinds' words as a division is named (the forces page's, campaign-bridge.ts): "Armoured", not "Armor". */
export const DIV_WORD: Record<string, string> = { Armor: 'armoured', Engineers: 'engineer', Fortress: 'fortress' };
const kindWord = (kind: string): string => { const w = DIV_WORD[KIND[kind as keyof typeof KIND]?.label ?? ''] ?? KIND[kind as keyof typeof KIND]?.label ?? 'Corps'; return w.charAt(0).toUpperCase() + w.slice(1); };
/** "the 2nd Tank Division": a division's name with its article. */
const theDivision = (name: string): string => (/^the /i.test(name) ? name : `the ${name}`);

/** One line of a row on the offers page, and how it is set: '' plain, 'muted', or 'amber' (the marks: amber once, for an offer in its last month). */
export interface OfferLine { text: string; tone: '' | 'muted' | 'amber' }

/**
 * An offer's lines on the offers page, in the marks' order (section 2): what forms and where; one line for each corps
 * it takes, and where the men beyond the formation's go as the rule does it; what declining also gives up; when it was
 * offered and until when; and, where FORM IT is shut, the reason. A loan's end: its three lines.
 */
export function offerLines(s: Game, o: RowOffer): OfferLine[] {
  const place = (id: number): string => s.regions[id]?.name ?? '';
  const out: OfferLine[] = [];
  if (o.lent) {
    // (The row's title is the regiment's own name, so its first line does not say it again: the UI director's second
    // marks of 15:23Z, 25 November 2026, pacific-japan/wip/offers-page-ui-marks-2.md, section 3.)
    out.push({ text: `Lent to ${o.lent.to} until ${monthOf(o.since)}.`, tone: '' });
    // (A division that never came or is gone takes nobody back: the rule's own case, `comeHome`.)
    out.push({ text: o.lent.goes === 'lost' ? `Sent home, it leaves the map: ${theDivision(o.lent.division)} is no longer on it. Kept, it stays yours for good.` : `Sent home, it goes back into ${theDivision(o.lent.division)}. Kept, it stays yours for good.`, tone: '' });
  } else {
    const r = o.region >= 0 ? s.regions[o.region] : null;
    // (The design director's island test: a place with no land neighbour is "on", every other "at": game.ts `onOrAt`,
    // which the no-room line reads too.)
    const at = r ? `${onOrAt(r)} ${r.name}` : '';
    // (A row that joins a standing corps forms no corps of its own: it joins it where it stands.)
    const where = o.joins ? `, to join your ${o.joins}${at ? ` ${at}` : ''}` : at ? `, to form ${at}` : '';
    out.push({ text: `${kindWord(o.kind)}, ${menWords(o.men)} men${where}.`, tone: '' });
    // (Sent on by sea from a full island of the table, the rule's own answer (`moved`, 'sea'): why the place moved, in
    // the UI director's words of 18:04Z, 27 November 2026, the row's second line, plain, in every month it stands so.
    // Both names from the rule's readers: the full place as the islands' sentences name it, the harbour as the line
    // above prints it. Not for a row sent on by land: her list, its own question.)
    if (o.moved && o.moved.by === 'sea') out.push({ text: `${placeName(s.regions[o.moved.from].name, true)} has no room for it. ${s.regions[o.moved.to].name} is your nearest harbour with room.`, tone: '' });
    for (const t of o.takes) out.push({ text: `Takes your ${t.name} at ${place(t.region)}, ${menWords(t.men)} men.`, tone: '' });
    // (Every figure is menWords'. Where the corps taken and the formation read the same in round figures, 14,142 and
    // 13,500 both "14,000", no line is added: the player sees no gap to read as men thrown away.)
    const took = o.takes.reduce((n, t) => n + t.men, 0), over = menWords(took) === menWords(o.men) ? 0 : took - o.men;
    // (The rule's own: a dated row strikes the corps it takes off whole and forms at its own size, so the rest are lost
    // with them; nothing goes back to the depots. A row of the formations list takes only its own strength, and the
    // rest stay in the last corps it draws on. Both sentences stand by the UI director's second marks, section 7.)
    if (over > 0 && o.takes.length) out.push({ text: o.rest === 'stay' ? `The other ${menWords(over)} stay in your ${o.takes[o.takes.length - 1].name}.` : `The other ${menWords(over)} are lost with the old corps.`, tone: '' });
    if (o.then.length) out.push({ text: `Declining it also gives up ${listed(o.then)}, which would later be formed out of it.`, tone: '' });
  }
  const last = o.last === null ? `Offered in ${monthOf(o.since)}.` : o.last <= s.turn ? `Offered in ${monthOf(o.since)}. This is its last month.` : `Offered in ${monthOf(o.since)}, open until ${monthOf(o.last)}.`;
  out.push({ text: last, tone: o.last !== null && o.last <= s.turn ? 'amber' : 'muted' });
  if (o.why && !o.lent) out.push({ text: o.why, tone: 'muted' });
  return out;
}

/**
 * The corps the offers taken in this order strike off the map whole, as the rule does it, each once, with the men it
 * holds when it goes: every corps a dated row takes (`rest` 'lost'), and of a row of the formations list (`rest`
 * 'stay') only those it empties, drawing on them in their order and on what earlier rows of the list left in them
 * (Oahu's 25th Infantry Division takes the 298th whole and part of the Hawaiian Division, and the 24th then empties
 * the division). A corps that gives only part of its men stays on the map. None for a loan's end: the regiment goes
 * into its division.
 */
function struckWhole(go: RowOffer[]): { id: number; men: number }[] {
  const left = new Map<number, number>(), out: { id: number; men: number }[] = [];
  for (const o of go) {
    if (o.lent) continue;
    let got = 0;
    for (const t of o.takes) {
      const has = left.get(t.id) ?? t.men;
      if (has <= 0) continue;
      const part = o.rest === 'stay' ? Math.min(has, Math.max(o.men - got, 0)) : has;
      got += part;
      left.set(t.id, has - part);
      if (part >= has) out.push({ id: t.id, men: has });
    }
  }
  return out;
}

/**
 * The question ACCEPT ALL asks on the head row (the marks, section 4), or null where nothing can be answered. `rows`
 * are the offers that would be answered. (The marks' sentence is for many: with one corps or one formation it is
 * said in the singular, "1 of your corps leaves the map", "1 formation forms"; only the corps struck off whole are
 * counted as leaving, with their men (`struckWhole`); and a loan's end is its own clause, the regiments going home,
 * not a corps leaving the map. The singular and the loans' clause stand by the UI director's second marks, section 4.)
 */
export function askForm(s: Game, open: RowOffer[]): string | null {
  const go = open.filter((o) => !o.why), passed = open.filter((o) => o.why && !o.lent);
  if (!go.length) return null;
  const struck = struckWhole(go), corps = struck.length, men = struck.reduce((n, t) => n + t.men, 0);
  const forms = go.filter((o) => !o.lent).length, loans = go.length - forms;
  const said = [
    corps ? `${corps} of your corps ${corps === 1 ? 'leaves' : 'leave'} the map, ${menWords(men)} men` : '',
    forms ? `${forms} formation${forms === 1 ? '' : 's'} form${forms === 1 ? 's' : ''}` : '',
    loans ? `${loans} lent regiment${loans === 1 ? '' : 's'} ${loans === 1 ? 'goes' : 'go'} home` : '',
  ].filter(Boolean);
  const all = said.length <= 1 ? said.join('') : `${said.slice(0, -1).join(', ')}, and ${said[said.length - 1]}`;
  return `Form all ${go.length}? ${all.charAt(0).toUpperCase()}${all.slice(1)}. It cannot be undone.${passed.length ? ` ${passed.length} cannot be formed this month and ${passed.length === 1 ? 'stays' : 'stay'} open.` : ''}`;
}

/** The question DECLINE ALL asks on the head row (the marks, section 4). */
export function askDecline(open: RowOffer[]): string {
  const loans = open.some((o) => o.lent);
  // (Two sentences, not three clauses: the second marks, section 4.)
  return `Decline all ${open.length}? None of them will be offered again. You keep the corps they would take${loans ? ', and the regiments lent to you' : ''}.`;
}

/**
 * The month card's row for the offers (the marks, section 1): its text, whether it is amber (an offer in its last
 * month), and the act that opens the page. Empty where nothing is offered. (A list of loans' ends alone is counted in
 * the head's own words: the marks word no card row for it.)
 */
export function cardRowOf(s: Game, open: RowOffer[]): { text: string; amber: boolean; act: string } {
  if (!open.length) return { text: '', amber: false, act: '' };
  const forms = open.filter((o) => !o.lent).length, loans = open.length - forms;
  const due = open.filter((o) => o.last !== null && o.last <= s.turn).length;
  // (The count in the head's own words (`headOf`), so that a loan's end waiting for his answer is counted on the card
  // too, beside the formations: "6 formations offered, 1 loan ending: they wait for your answer." And more than one in
  // its last month is "in their last month". All five forms stand by the UI director's second marks, section 2.)
  const said = headOf(forms, loans);
  const text = due ? `${said}, ${due} in ${due === 1 ? 'its' : 'their'} last month.` : open.length === 1 ? `${said}: it waits for your answer.` : `${said}: they wait for your answer.`;
  return { text, amber: due > 0, act: 'open:offers' };
}

/**
 * The two lines the design director asked for that the marks do not word (ruling of 13:39Z, 25 November 2026, dated
 * rows, points 2 and 7), each muted on the offers page, in the words of the UI director's second marks, section 6.
 *   loans   his corps lent to a campaign that leave by themselves when the loan ends, with when (`loansEnding`).
 *   waiting his dated rows that wait for their war to begin, by ruling B7, with what they wait for (`rowsAwaitingWar`).
 */
export function asideLines(s: Game): { loans: string[]; waiting: string[] } {
  const who = (nat: string): string => (s.nations[nat] ? theName(s.nations[nat]) : nat);
  // (A loan the contact rule holds at the front past its month: the month gives way to the UI director's words, B6 of
  // 16:07Z, 27 November 2026, arrivalwaits.ts `heldLoanAside`. Under the switch 'fromWait' only: off, none is held.)
  const held = (i: number): string | null => heldLoanAside(s, i);
  const loans = loansEnding(s).map((l) => (l.until !== null
    ? held(l.id) ? `${l.name} leaves your command by itself: ${held(l.id)}` : `${l.name} leaves your command by itself in ${monthOf(l.until)}, when its loan ends.`
    : l.takes ? `${l.name} leaves your command by itself once ${l.takes} is yours, when its loan ends.`
      : `${l.name} leaves your command by itself once ${listed(l.holds ?? [])} ${(l.holds ?? []).length === 1 ? 'is' : 'are'} lost, when its loan ends.`));
  // (A row that will be an offer when its war begins says so; "forms" only where it forms by itself with no answer
  // from him: the second marks, section 6.)
  const waiting = rowsAwaitingWar(s).map((w) => {
    const once = w.pair ? `once ${who(w.pair[0])} and ${who(w.pair[1])} are at war` : w.with === '*' ? 'once you are at war' : `once you are at war with ${who(w.with)}`;
    return w.offer ? `${w.name} is offered ${once}.` : `${w.name} waits for its war: it forms ${once}.`;
  });
  return { loans, waiting };
}

/** The player's open offers in plain words. A list of none where there is no player, no offer, or the whole world is off. */
export function offersLook(s: Game): OffersLook {
  let open: RowOffer[] = [];
  try { open = formationOffers(s); } catch { open = []; }
  const shut = open.length ? whyNotAnswerOffers(s) : null;
  const n = open.length, loans = open.filter((o) => o.loan !== null).length;
  return {
    count: n,
    head: n ? headOf(n - loans, loans) : '',
    note: n ? noteOf(n - loans, loans) : '',
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
      const was = formationOffers(s);
      const names = new Map(was.map((o) => [o.id, o.name]));
      const lent = new Set(was.filter((o) => o.loan !== null).map((o) => o.id));
      const taken: number[] = [], left: { id: number; name: string; why: string }[] = [];
      for (const id of asked(s, ids)) {
        const why = orderTakeFormation(s, id);
        if (why === null) taken.push(id); else left.push({ id, name: names.get(id) ?? '', why });
      }
      const formed = taken.filter((id) => !lent.has(id)), gone = taken.filter((id) => lent.has(id));
      // (A reason is a sentence of its own, with its capital: it follows a full stop, never a colon.)
      const said = [
        formed.length ? `${formed.length === 1 ? `${names.get(formed[0])} is formed` : `${formed.length} formations are formed: ${listed(formed.map((id) => names.get(id) ?? ''))}`}.` : '',
        // (A loan's end is answered in the words of its buttons, SEND IT HOME and KEEP IT: the UI director's marks, section 3.)
        gone.length ? `${listed(gone.map((id) => names.get(id) ?? ''))} ${gone.length === 1 ? 'is' : 'are'} sent home.` : '',
        // (A loan refused "is not sent home": "stays" read like the answer he chose. The second marks, section 5.)
        ...left.map((x) => (x.name ? `${x.name} ${lent.has(x.id) ? 'is not sent home' : 'is not formed'}. ${x.why}` : x.why)),
      ].filter(Boolean).join(' ');
      return { ok: taken.length > 0 && !left.length, taken, left, said, offers: offersLook(s), view: view() };
    },
    offerDecline: (ids: unknown) => {
      const s = game();
      const was = formationOffers(s);
      const names = new Map(was.map((o) => [o.id, o.name]));
      const lent = new Set(was.filter((o) => o.loan !== null).map((o) => o.id));
      const declined: number[] = [];
      for (const id of asked(s, ids)) if (orderDeclineFormation(s, id)) declined.push(id);
      const unformed = declined.filter((id) => !lent.has(id)), kept = declined.filter((id) => lent.has(id));
      const said = [
        // ("Is not formed" is the refusal's word; a decline is his own answer and says so: the second marks, section 5.)
        unformed.length ? `${unformed.length === 1 ? `${names.get(unformed[0])} is declined` : `${unformed.length} offers are declined`}: your corps stay as they are.` : '',
        kept.length ? `${listed(kept.map((id) => names.get(id) ?? ''))} ${kept.length === 1 ? 'stays' : 'stay'} yours for good.` : '',
      ].filter(Boolean).join(' ');
      return { ok: declined.length > 0, declined, said, offers: offersLook(s), view: view() };
    },
  };
}

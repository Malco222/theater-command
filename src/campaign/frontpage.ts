/**
 * The war correspondent's front page (release 2; Brian's pick of 21 November 2026). Each month the player's own
 * country's newspaper prints the war as its readers would have had it: a masthead and a date, a banner when a war is
 * declared or a country falls, the lead story with its headline, sub-headlines, the correspondent's byline and a
 * photograph, two or three other stories, the war elsewhere in brief, a letter from the front, and the month in
 * figures. Every story is drawn from the month card (monthcard.ts), so from what happened in this game.
 *
 * The paper speaks in its country's voice: a free press prints bad news plainly under the censor's eye, a state
 * press prints what its government allows (a defeat becomes "our line shortened", its own losses go unprinted). The
 * month card beside it keeps the plain truth.
 *
 * Display only, like the card: nothing reads it, no number changes, no die is drawn (any choice of words is made by
 * a hash of the paper and the month). On the months that change the war (a war declared, a country fallen, a capital,
 * a ring, a fleet action, a great battle) it is a special edition, and the interfaces open it before the month card.
 *
 * Words and papers are in data/campaign/frontpage.json, whose `_about` block explains each part.
 */
import data from '../../data/campaign/frontpage.json';
import { turnToDate } from '../core/ticket';
import { hashString } from '../core/rng';
import type { CardPicture, CardReport, CardRow, CardTone, MonthCard } from './monthcard';
import type { Game } from './types';

interface Paper { name: string; city: string; motto: string; price: string; issue: number; voice: 'free' | 'state'; lang?: string }
interface FrontData {
  papers: Record<string, Paper>;
  correspondents: Record<string, string>;
  heads: [pattern: string, good: string, badFree: string, badState: string][];
  close: Record<'good' | 'bad' | 'plain', Record<'free' | 'state', string[]>>;
  captions: Record<string, Record<'good' | 'bad' | 'plain', string>>;
  soften: Record<'land' | 'sea' | 'air' | 'shore' | 'ground', string>;
  weeks: string[];
  extra: { men: number };
  letters: { head: string; intro: string };
}
export const FRONT = data as unknown as FrontData;

const GREAT = ['GER', 'UK', 'FRA', 'SOV', 'USA', 'ITA', 'JAP'];
const MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
const DAYS = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];

/** One story on the page. The lead carries a byline, decks, a photograph and paragraphs; the others a short body. */
export interface FrontStory {
  /** Small capitals over the headline: where the story comes from ('THE FRONT', 'AT SEA', 'IN THE AIR', 'WAR'). */
  kicker: string;
  /** The headline, in capitals. */
  head: string;
  /** Lines under the headline in smaller type: the lead has two, the others none or one. */
  decks: string[];
  /** 'ATHENS, July 4 —' : the place and the day the story was filed, set before its first paragraph. */
  dateline: string;
  body: string[];
  /** Good, bad or plain for the reader's side: the interfaces may rule the story's edge by it. */
  tone: 'good' | 'bad' | 'plain';
  /** The nations in it (the reader's first), for flags. */
  flags: string[];
  /** Where it happened, a province or a sea, and the report it opens: as the month card's row. */
  region?: number;
  sea?: number;
  report?: CardReport;
}

export interface FrontPage {
  turn: number;
  /** The reader's country, whose paper it is. */
  viewer: string;
  /** The masthead, where it is printed, the line under it, the price, and 'No. 27,512'. */
  paper: string;
  city: string;
  motto: string;
  price: string;
  issue: string;
  /** 'Translated from the German', or ''. */
  translated: string;
  /** The day it is printed: the first of the month after the month it reports ('Saturday, June 1, 1940'). */
  date: string;
  /** 'SPECIAL EDITION' on a great month, else 'LATE EDITION'. */
  edition: string;
  /** True on a month that changes the war: the interfaces open the page before the month card. */
  extra: boolean;
  /** The free press or the state press. */
  voice: 'free' | 'state';
  /** A headline across the whole page in the largest type (a war declared, a country fallen or freed), or null. */
  banner: { head: string; sub: string; flags: string[] } | null;
  lead: FrontStory | null;
  /** What the lead's photograph shows (the month card's picture kinds), and the words under it. */
  picture: CardPicture | null;
  caption: string;
  /** 'From our war correspondent, Arthur Pell'. */
  byline: string;
  stories: FrontStory[];
  /** THE WAR ELSEWHERE: a line each. */
  briefs: string[];
  /** A letter from the front, when one of his units wrote one this month. */
  letter: { head: string; intro: string; text: string; by: string } | null;
  /** THE MONTH IN FIGURES: the month card's boxes. */
  figures: { label: string; n: string; sub: string; tone: CardTone }[];
  /** A small line at the foot: 'Passed by the censor.' or 'Approved for publication.' */
  censor: string;
  /** When nothing was fought: the quiet month's line, printed as the lead's place. */
  quiet: string;
}

const cap = (s: string): string => s.charAt(0).toUpperCase() + s.slice(1);
const stop = (s: string): string => (/[.!?…]$/.test(s.trim()) ? s.trim() : `${s.trim()}.`);
const num = (n: number): string => Math.round(n).toLocaleString('en-US');

/** The paper's words for what the card says to the player: "your ships" are "our ships" in his country's paper. */
export function ourWords(s: string): string {
  return s
    .replace(/\byours\b/g, 'ours').replace(/\bYours\b/g, 'Ours')
    .replace(/\byour\b/g, 'our').replace(/\bYour\b/g, 'Our')
    .replace(/\byou have\b/g, 'we have').replace(/\bYou have\b/g, 'We have')
    .replace(/\byou\b/g, 'us').replace(/\bYou\b/g, 'We');
}

/** A nation's paper, with its capital filled in where the row is the default. */
export function paperOf(g: Game, nat: string): Paper {
  const p = FRONT.papers[nat];
  if (p) return p;
  const d = FRONT.papers._default;
  const n = g.nations[nat];
  const capital = n && n.capital >= 0 ? g.regions[n.capital]?.name ?? n.name : n?.name ?? nat;
  return { ...d, name: d.name.replace('{capital}', capital), city: d.city.replace('{capital}', capital) };
}

const COUNT = ['NO', 'ONE', 'TWO', 'THREE', 'FOUR', 'FIVE', 'SIX', 'SEVEN', 'EIGHT', 'NINE', 'TEN'];
/** A headline never opens on a figure under eleven: '1 PROVINCE OCCUPIED' is 'ONE PROVINCE OCCUPIED'. */
const counted = (h: string): string => h.replace(/^(\d+)\b/, (d) => COUNT[+d] ?? d);

const toneOf = (t: CardTone): 'good' | 'bad' | 'plain' => (t === 'good' ? 'good' : t === 'bad' || t === 'warn' ? 'bad' : 'plain');

/** The paper's headline for a line of the card: the voice's own words where the data has a pattern, else the card's, in capitals. */
export function paperHead(head: string, tone: 'good' | 'bad' | 'plain', voice: 'free' | 'state', foe = ''): string {
  const said = head.trim().replace(/[.:]$/, '');
  for (const [pat, good, badFree, badState] of FRONT.heads) {
    const m = new RegExp(pat).exec(said);
    if (!m) continue;
    const words = tone === 'good' ? good : tone === 'bad' ? (voice === 'state' && badState ? badState : badFree || good) : (badFree || good);
    if (!words) break;
    // (A fleet struck or beaten by the reader's side is named with its country: "the Italian 1st Fleet".)
    const named = (s: string): string => (foe && tone === 'good' && /^the /i.test(s) && !s.includes(foe) ? s.replace(/^the /i, `the ${foe} `) : s);
    return counted(words.replace(/\{(\d)\}/g, (_, k) => named(m[+k] ?? '')).replace(/\s+/g, ' ').trim().toUpperCase());
  }
  return counted(said.toUpperCase());
}

/** Where a story was filed: its place in capitals and a day of the month, from the week it is dated. */
function dateline(place: string, week: number, turn: number): string {
  const d = turnToDate(turn);
  const day = [4, 11, 18, 25][Math.max(0, Math.min(3, (week || 1) - 1))];
  return `${(place || 'THE FRONT').toUpperCase()}, ${MONTHS[d.month]} ${day} —`;
}

const kickerOf = (r: CardRow): string => {
  if (r.cls === 2) return r.kind === 'WAR' ? 'WAR' : r.kind === 'FALLEN' ? 'CAPITULATION' : r.kind === 'FREED' ? 'LIBERATION' : 'THE NEWS';
  if (r.cls === 5) return 'HONOURS';
  if (r.arm === 'sea') return 'AT SEA';
  if (r.arm === 'air') return 'IN THE AIR';
  return 'THE FRONT';
};

/** "7,278 of ours, 64,169 of the enemy's": the figures in the paper's voice (a state paper never prints its own losses on a bad day). */
function lossesLine(r: CardRow, voice: 'free' | 'state', tone: 'good' | 'bad' | 'plain'): string {
  const f = r.facts.length ? r.facts : r.figures;
  const ours = f.find((x) => /men lost|^lost$/.test(x.label));
  const theirs = f.find((x) => /of theirs$|^theirs$/.test(x.label));
  if (r.arm === 'land' || (!r.arm && (ours || theirs))) {
    if (!ours && !theirs) return '';
    if (voice === 'state') {
      // A state paper prints its own losses only when they are the smaller, and the enemy's only when they are worth boasting of.
      const n = (x?: { n: string }): number => Number((x?.n ?? '0').replace(/[^0-9]/g, '')) || 0;
      const fewer = !!ours && n(ours) < n(theirs);
      if (theirs && n(theirs) >= n(ours)) return `The enemy lost ${theirs.n} men${fewer ? `; our own losses were ${ours!.n}` : ''}.`;
      return tone === 'bad' ? 'The enemy paid dearly for every yard.' : theirs ? `The enemy lost ${theirs.n} men.` : '';
    }
    if (ours && theirs) return `Our losses are given as ${ours.n} men; the enemy lost ${theirs.n}.`;
    return ours ? `Our losses are given as ${ours.n} men.` : `The enemy lost ${theirs!.n} men.`;
  }
  // At sea and in the air the figures are things counted: ships sunk, aircraft lost.
  const said = f.filter((x) => x.n && x.n !== '0').map((x) => `${x.n} ${x.label}`);
  if (!said.length) return '';
  if (voice === 'state' && tone === 'bad') return said.filter((x) => !/\blost\b/.test(x)).length ? `${cap(said.filter((x) => !/\blost\b/.test(x)).join(', '))}.` : '';
  return `${cap(said.join(', '))}.`;
}

/** The unit that led the way, from the battle report: the reader's side's largest corps. */
function ledBy(g: Game, r: CardRow, viewer: string): string | null {
  if (!r.report || r.report.type !== 'land') return null;
  const rep = g.reports.find((x) => x.id === r.report!.id);
  if (!rep) return null;
  const ours = rep.attNat === viewer || (rep.attackers[0] && rep.attackers[0].nat === viewer) ? rep.attackers : rep.defenders;
  const best = [...ours].filter((u) => !u.guard).sort((a, b) => b.men - a.men)[0];
  return best ? best.name : null;
}

const pickOf = <T,>(xs: T[], key: string): T => xs[hashString(key) % xs.length];

/** A story from a row of the card. */
const FIRST_WORDS = /^(The|A|An|Their|Our|Enemy|One|Two|Three|Four|Five|Six|Seven|Eight|Nine|Ten|No|Escorts)\b/;

const SMALL = ['no', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine'];
/** A paper's style: a figure under ten before a word is spelled out ('2 corps' is 'two corps', '3 months' 'three
 *  months'), a capital when it opens a sentence. Names and numbers in names stay ('No. 1 Group', '1st Fleet', '0.1%'). */
export function spelled(s: string): string {
  return s.replace(/(^|[\s(])([0-9])(?= [a-z])/g, (_m, pre: string, d: string, at: number) => {
    const before = s.slice(0, at).trimEnd();
    if (/\bNo\.$/.test(before)) return pre + d;
    const w = SMALL[+d];
    return pre + (before === '' || /[.!?:—]$/.test(before) ? w.charAt(0).toUpperCase() + w.slice(1) : w);
  });
}
const spelledStory = (st: FrontStory): FrontStory => ({ ...st, decks: st.decks.map(spelled), body: st.body.map(spelled) });
const firstSentence = (s: string): string => { const m = /^(.+?[.!?])(\s|$)/.exec(s); return m ? m[1] : s; };

/** The card's sentence as the paper prints it: the reader's side is "our", the enemy is named, and "its factories" are the place's. */
function paperWords(s: string, place: string, tone: 'good' | 'bad' | 'plain'): string {
  let out = ourWords(s).replace(/^Their (bombers|aircraft|fighters|ships|troops)\b/, 'Enemy $1');
  // (The card's second line under a U-boat sunk: a paper says who made the kill.)
  out = out.replace(/^Sunk by escorts of the (.+?)\.?$/, 'Escorts of the $1 made the kill.');
  if (place && !/fleet|force|squadron/i.test(place)) out = out.replace(/\b[Ii]ts (factories|defences|harbour|rail yards|works)\b/g, (_m, what) => `the ${what} of ${place}`).replace(/^the /, 'The ');
  if (tone === 'bad') out = out.replace(/\bto them\b/g, 'to the enemy');
  return out;
}

function storyOf(g: Game, r: CardRow, viewer: string, voice: 'free' | 'state', lead: boolean, key: string, month: number): FrontStory {
  const tone = toneOf(r.tone);
  const foe = r.theirs && r.theirs !== viewer ? g.nations[r.theirs]?.adj ?? '' : '';
  const head = paperHead(r.head || r.line, tone, voice, foe);
  // Where the story is filed: the sea for a story at sea, else the place.
  const where = r.sea !== undefined ? g.seas[r.sea]?.name ?? r.place : r.place;
  // A state paper does not print the card's account of a defeat: it prints its own.
  const hush = voice === 'state' && tone === 'bad';
  const arm = r.report?.type === 'shore' ? 'shore' : r.arm ?? 'land';
  const line = hush ? FRONT.soften[r.kind === 'GROUND' ? 'ground' : arm].replace('{place}', where) : paperWords(stop(r.line || r.head), where, tone);
  const follows = hush || !r.follows ? '' : paperWords(stop(r.follows), where, tone);
  const losses = lossesLine(r, voice, tone);
  const body: string[] = [];
  if (lead) {
    const led = ledBy(g, r, viewer);
    const when = FRONT.weeks[Math.max(0, Math.min(3, (r.week || 1) - 1))].replace('{month}', MONTHS[turnToDate(month).month]);
    // ("In the first days of May six corps took the province"; a name keeps its capital after a comma: "In the first days of May, Warsaw falls".)
    const opening = FIRST_WORDS.test(line) ? ` ${line.charAt(0).toLowerCase()}${line.slice(1)}` : `, ${line}`;
    body.push(`${when}${opening}${follows && follows !== line ? ` ${follows}` : ''}`);
    if (losses) body.push(losses);
    if (led) body.push(tone === 'bad' ? `${led} was among the troops in the fighting.` : `${led} led the way.`);
    const n = g.nations[viewer];
    const capital = n && n.capital >= 0 ? g.regions[n.capital]?.name ?? n.name : n?.name ?? viewer;
    body.push(pickOf(FRONT.close[tone][voice], `${key}:close`).replace('{capital}', capital));
  } else {
    body.push([line, follows && follows !== line ? firstSentence(follows) : '', losses].filter(Boolean).join(' '));
  }
  const decks: string[] = [];
  if (lead) {
    // On land the deck says what was done and what it cost; at sea and in the air what was sunk or shot down comes first.
    const what = line.replace(/\.$/, ''), then = follows.replace(/\.$/, ''), cost = losses.replace(/\.$/, '');
    const pair = arm === 'land' ? [what, cost] : then ? [then, what] : [what, cost];
    for (const d of pair) if (d && !decks.includes(d) && d.toUpperCase() !== head) decks.push(d);
  }
  const flags = [r.ours, r.theirs].filter((x): x is string => !!x);
  return {
    kicker: kickerOf(r), head, decks, dateline: dateline(where, r.week, month), body, tone, flags,
    ...(r.region !== undefined ? { region: r.region } : {}), ...(r.sea !== undefined ? { sea: r.sea } : {}), ...(r.report ? { report: r.report } : {}),
  };
}

/** The biggest number among a row's figures and facts that counts men, or 0. */
function menIn(r: CardRow): number {
  let most = 0;
  for (const f of [...r.facts, ...r.figures]) {
    if (!/lost|theirs/.test(f.label)) continue;
    const v = Number(f.n.replace(/[^0-9]/g, ''));
    if (Number.isFinite(v) && /men|^lost$|theirs/.test(f.label)) most = Math.max(most, v);
  }
  return most;
}

/** True on a month that changes the war for the reader: a special edition. */
export function isExtra(g: Game, card: MonthCard): boolean {
  const viewer = card.viewer;
  for (const b of card.bands) {
    if (b.word !== 'WAR' && b.word !== 'FALLEN' && b.word !== 'FREED') continue;
    if (b.nats.includes(viewer) || b.nats.some((n) => GREAT.includes(n))) return true;
  }
  const L = card.lead;
  if (!L) return false;
  if (L.kind === 'THE CAPITAL' || L.kind === 'FLEET ACTION' || /ring closes/i.test(L.head)) return true;
  if (L.region !== undefined && Object.values(g.nations).some((n) => n.capital === L.region)) return true;
  return menIn(L) >= FRONT.extra.men;
}

/**
 * The front page of the reader's paper for the month the card reports. `g` is the game as the card was made (the
 * month after the month reported); the card is monthCard's.
 */
export function frontPage(g: Game, card: MonthCard): FrontPage {
  const viewer = card.viewer;
  const p = paperOf(g, viewer);
  const voice = p.voice;
  const key = `${p.name}:${card.turn}`;
  // The paper is printed on the first day of the month after the one it reports.
  const t = turnToDate(card.turn + 1);
  const day = new Date(Date.UTC(t.year, t.month, 1)).getUTCDay();
  const date = `${DAYS[day]}, ${MONTHS[t.month]} 1, ${t.year}`;
  const issue = `No. ${num(p.issue + Math.round((Date.UTC(t.year, t.month, 1) - Date.UTC(1939, 8, 1)) / 86400000))}`;
  const extra = isExtra(g, card);

  // The banner: the first band of war, a fall or a freeing. A second such band becomes a story.
  const news = card.bands.filter((b) => b.word === 'WAR' || b.word === 'FALLEN' || b.word === 'FREED' || b.word === 'NEWS');
  const first = news[0];
  const banner = first ? {
    head: paperHead(first.head, first.good ? 'good' : first.nats.includes(viewer) && first.word === 'FALLEN' ? 'bad' : 'plain', voice),
    // (Under the banner, a band's second line where it has one; its text where that says more than the headline.)
    sub: first.sub ? ourWords(stop(first.sub)) : first.text.split(/(?<=\.)\s+/).length === 1 && first.text.replace(/\.$/, '') !== first.head ? ourWords(stop(first.text)) : '',
    flags: first.nats,
  } : null;

  const lead = card.lead ? storyOf(g, card.lead, viewer, voice, true, key, card.turn) : null;
  const rows = card.rows.filter((r) => r !== card.lead && r.cls !== 5);
  const stories: FrontStory[] = [];
  for (const b of news.slice(1)) {
    stories.push({ kicker: b.word === 'WAR' ? 'WAR' : b.word === 'FALLEN' ? 'CAPITULATION' : b.word === 'FREED' ? 'LIBERATION' : 'THE NEWS', head: paperHead(b.head, b.good ? 'good' : 'plain', voice), decks: [], dateline: '', body: [ourWords(stop(b.text))], tone: b.good ? 'good' : 'plain', flags: b.nats });
  }
  // The other stories: the heaviest rows after the lead, no two of the same headline, at most three on the page.
  const seen = new Set<string>(lead ? [lead.head] : []);
  for (const r of rows) {
    if (stories.length >= 3) break;
    const s = storyOf(g, r, viewer, voice, false, `${key}:${stories.length}`, card.turn);
    if (seen.has(s.head)) continue;
    seen.add(s.head);
    stories.push(s);
  }
  // The war elsewhere, and honours, as one-line briefs.
  const briefs: string[] = [];
  const brief = (s: string): void => { if (s && !briefs.includes(s)) briefs.push(s); };
  for (const r of card.rows) if (r.cls === 5) brief(ourWords(stop(r.head)));
  for (const e of card.elsewhere) brief(stop(e.text));
  for (const r of rows) if (!seen.has(paperHead(r.head, toneOf(r.tone), voice, r.theirs ? g.nations[r.theirs]?.adj ?? '' : ''))) brief(ourWords(stop(r.head)));
  const letter = card.letters[0] ? { head: FRONT.letters.head, intro: FRONT.letters.intro, text: card.letters[0].text, by: card.letters[0].by } : null;
  const picture = card.lead?.picture ?? null;
  const tone = card.lead ? toneOf(card.lead.tone) : 'plain';
  const caption = picture ? (FRONT.captions[picture]?.[tone] ?? '').replace('{place}', card.lead!.place || '') : '';
  const man = FRONT.correspondents[viewer] ?? FRONT.correspondents._default;
  return {
    turn: card.turn, viewer, paper: p.name, city: p.city, motto: p.motto, price: p.price, issue,
    translated: p.lang ? `Translated from the ${p.lang}` : '', date, edition: extra ? 'SPECIAL EDITION' : 'LATE EDITION', extra, voice,
    banner, lead: lead && spelledStory(lead), picture, caption,
    byline: man === FRONT.correspondents._default ? 'From our own correspondent' : `From our war correspondent, ${man}`,
    stories: stories.map(spelledStory), briefs: briefs.slice(0, 6).map(spelled), letter,
    figures: card.boxes.map((b) => ({ label: b.label, n: b.figure, sub: b.sub, tone: b.tone })),
    censor: voice === 'state' ? 'Approved for publication.' : 'Passed by the censor.',
    quiet: card.lead ? '' : ourWords(card.quiet || 'A quiet month on every front.'),
  };
}

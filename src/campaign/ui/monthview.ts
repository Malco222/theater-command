// The month's report on one card, as the page draws it (Version 82). The content and its order come from
// monthCard (../monthcard.ts); the look is the art director's mock-up (art-director/turn-report), class for class.
// Display only.
import { cardHead, type CardFigure, type CardRow, type CardTone, type MonthCard } from '../monthcard';
import { ORDER_SIGN, ORDER_STAR } from './ordersigns';

const PENNANT = 'M2.4 1.2H4.2V15.2H2.4ZM4.9 2H14.6L11.9 5.3L14.6 8.6H4.9Z';
const PIN = 'M8 .8A5.2 5.2 0 0 1 13.2 6C13.2 9.6 8 15.4 8 15.4S2.8 9.6 2.8 6A5.2 5.2 0 0 1 8 .8ZM8 3.9A2.1 2.1 0 1 0 8 8.1 2.1 2.1 0 0 0 8 3.9Z';
const DIM = '#a89e8e', BRASS = '#d8aa62', PALE = '#f4eeda';

/** What the card needs of the page: text made safe, a nation's flag at a width, and its name. */
export interface MonthViewHelp {
  esc: (s: string) => string;
  flag: (nat: string, w: number) => string;
  name: (nat: string) => string;
  /** True once the player has asked for every row. */
  all: boolean;
  /** True where a player's orders follow: the main button reads GIVE ORDERS. */
  orders: boolean;
  /** True where the card has a place on the map to show again: TOUR THE MAP is offered. */
  tour: boolean;
}

/** One of the order signs at a height, or the star; nothing for a key the page has no sign for. */
export function monthSign(key: string, h: number, fill: string): string {
  if (key === 'star') return `<svg width="${h}" height="${h}" viewBox="-4 -4 8 8" aria-hidden="true"><path fill="${fill}" d="${ORDER_STAR}"/></svg>`;
  const d = ORDER_SIGN[key];
  return d ? `<svg width="${(h * 1.5).toFixed(1)}" height="${h}" viewBox="0 0 24 16" aria-hidden="true"><path fill="${fill}" d="${d}"/></svg>` : '';
}

const glyph = (d: string, h: number, fill: string): string => `<svg width="${h}" height="${h}" viewBox="0 0 16 16" aria-hidden="true"><path fill="${fill}" fill-rule="evenodd" d="${d}"/></svg>`;
const toneClass = (t: CardTone): string => `t-${t}`;
const brass = (word: string): boolean => word === 'WAR' || word === 'FALLEN' || word === 'FREED';
const REPORT_ACT: Record<string, string> = { land: 'report', sea: 'seareport', air: 'airreport', shore: 'shorereport' };

export function monthCardHtml(card: MonthCard, o: MonthViewHelp): string {
  const { esc } = o;
  const fl = (nat: string | null, w: number): string => (nat ? `<span class="mc-flag">${o.flag(nat, w)}</span>` : '');
  const nums = (fs: CardFigure[]): string => fs.map((f) => `<span><b>${esc(f.n)}</b>${esc(f.label)}</span>`).join('');
  const versus = (r: CardRow, w: number): string => (r.ours && r.theirs ? `${fl(r.ours, w)}<i class="mc-ag">against</i>${fl(r.theirs, w)}` : fl(r.ours ?? r.theirs, w));
  const act = (r: CardRow): string => (r.report ? ` data-act="${REPORT_ACT[r.report.type]}" data-id="${r.report.id}" title="Read the battle report" role="button" tabindex="0"` : '');
  // (A province has a place on the map and a sea has one on the chart: either way the row carries the pin.)
  const where = (r: { region?: number; sea?: number }): string => (r.region !== undefined ? `data-act="showmap" data-id="${r.region}"` : r.sea !== undefined ? `data-act="showsea" data-id="${r.sea}"` : '');
  const pin = (r: CardRow): string => (where(r) ? `<button class="mc-go" ${where(r)} title="Show on map" aria-label="Show on map">${glyph(PIN, 17, BRASS)}</button>` : '<span class="mc-go none"></span>');

  // (The card that opens a campaign is headed by the moment, 'As July 1943 opens', the two small words dimmer: ruling 123(b).)
  const H = cardHead(card);
  const head = `<div class="mc-head"><div><div class="mc-eyebrow kick">${card.opening ? 'Dispatches · before your first month' : 'Dispatches · the month in one look'}</div><h1${card.opening ? ' class="eve"' : ''}>${H.before ? `<span>${esc(H.before)}</span> ` : ''}${esc(H.month)}${H.after ? ` <span>${esc(H.after)}</span>` : ''}</h1></div>
    <div class="mc-btns">${o.tour ? '<button class="mc-alt" data-act="watchagain" title="The map visits the places of the month, one by one, and comes back to this report">TOUR THE MAP</button>' : ''}<button class="mc-fly" data-act="close" title="Enter">${o.orders ? 'GIVE ORDERS' : 'CONTINUE'} ▸</button></div></div>`;

  // (A band with a second line, the country in danger, is read whole: the line stands under the headline in full.)
  // Red is the dated warning's alone (ruling 160): a band of war, a fall or a freeing is brass on the dark ground, as the
  // news is, and a freeing his own side did keeps its green.
  const bands = card.bands.map((b) => `<div class="mc-banner${b.good ? ' good' : ''}${b.word === 'NEWS' ? ' news' : ''}${brass(b.word) && !b.good ? ' brass' : ''}${b.spent ? ' spent' : ''}${b.sub ? ' tall' : ''}"><span class="k">${esc(b.word)}</span>${b.sub ? `<span class="mc-bflags">${b.nats.map((n) => fl(n, 38)).join('')}</span>` : b.nats.map((n) => fl(n, 38)).join('')}<b title="${esc(b.text)}">${esc(b.head)}</b><small${b.note ? ` title="${esc(b.text)}"` : ''}>${esc(b.note ?? b.when)}</small>${b.sub ? `<span class="mc-bsub">${esc(b.sub)}</span>` : ''}${b.region !== undefined ? `<button class="mc-alt" data-act="${b.alarm ? 'homealarm' : 'showmap'}" data-id="${b.region}">SHOW ON MAP</button>` : ''}</div>`).join('');

  const tileSign: Record<string, string> = { provinces: glyph(PENNANT, 13, DIM), men: monthSign('inf', 11, DIM), sea: monthSign('hull', 11, DIM), air: monthSign('ftr', 11, DIM) };
  const tally = card.boxes.length ? `<div class="mc-tally" style="--n:${Math.max(4, card.boxes.length)}">${card.boxes.map((b) => `<div class="mc-tile ${toneClass(b.tone)}"><u>${tileSign[b.key] ?? ''}${esc(b.label)}</u><b>${esc(b.figure)}</b><small>${esc(b.sub)}</small></div>`).join('')}</div>` : '';

  const row = (r: CardRow): string => {
    const pic = r.picture ? `<i class="tpic k-${r.picture}"></i>` : `<span class="mc-nopic">${r.cls === 2 ? fl(r.ours, 34) : ''}</span>`;
    const disc = r.cls === 2 ? '' : `<span class="mc-disc">${monthSign(r.sign || 'star', r.sign === 'star' || !r.sign ? 16 : 15, PALE)}</span>`;
    // (The word that goes with the tag, the other side's flag and its name: "WON from [flag] Mexico".)
    const flags = r.cls === 2 ? fl(r.theirs, 30) : r.join && r.theirs ? `<i class="mc-ag">${esc(r.join.word)}</i>${fl(r.theirs, 30)}<span class="mc-cn">${esc(r.join.name)}</span>` : versus(r, 30);
    // (An item that found no band keeps its band's colour: red for a warning, brass for the rest, ruling 160.)
    return `<div class="mc-row ${toneClass(r.tone)}${r.cls === 2 ? (r.tag === 'WARNING' ? ' red' : ' brass') : ''}${r.report ? ' opens' : ''}"${act(r)}>${pic}${disc || '<span></span>'}
      <div class="mc-rtx"><div class="r1"><b class="pl${r.place.length > 16 ? ' long' : ''}">${esc(r.place)}</b>${r.tag ? `<span class="mc-chip">${esc(r.tag)}</span>` : ''}<span class="vs">${flags}</span></div><div class="r2" title="${esc(r.line)}">${esc(r.line)}</div></div>
      <div class="mc-nums">${nums(r.figures)}</div>${pin(r)}</div>`;
  };

  const L = card.lead;
  // The month a campaign's stake falls due (ruling 118(f); the art director's last-month-lead): the warning band grown
  // into the lead's frame. No picture, the country's flag large, the stamp on a cream plate, one short line under the
  // headline, the sentence beneath, two figures in the band's red, and SHOW ON MAP alone.
  const stake = !L?.stake ? '' : `<div class="mc-leadcol"><div class="mc-lab">The lead story</div>
    <div class="mc-photo stake"><span class="mc-kind">${esc(L.kind)} · ${esc(L.when.toUpperCase())}</span>${L.tag ? `<span class="mc-stamp plate">${esc(L.tag)}</span>` : ''}<span class="mc-bigflag">${L.ours ? o.flag(L.ours, 114) : ''}</span>
      <div class="mc-cap"><h2>${esc(L.head)}</h2><span>${esc(L.line)}</span></div></div>
    ${L.follows ? `<p class="mc-so stake">${esc(L.follows)}</p>` : ''}${L.facts.length ? `<div class="mc-facts stake" style="--n:${L.facts.length}">${L.facts.map((f) => `<div><b>${esc(f.n)}</b>${esc(f.label)}</div>`).join('')}</div>` : ''}
    <div class="mc-leadb">${where(L) ? `<button class="mc-alt" ${where(L)}>SHOW ON MAP</button>` : ''}</div></div>`;
  const lead = !L ? '' : stake ? stake : `<div class="mc-leadcol"><div class="mc-lab">The lead story</div>
    <div class="mc-photo${L.report ? ' opens' : ''}"${act(L)}><i class="tpic k-${L.picture}"></i><span class="mc-kind">${esc(L.kind)} · ${esc(L.when.toUpperCase())}</span>${L.tag ? `<span class="mc-stamp ${toneClass(L.tone)}">${esc(L.tag)}</span>` : ''}
      <div class="mc-cap"><h2>${esc(L.head)}</h2><span>${L.ours ? `${fl(L.ours, 30)} ${esc(o.name(L.ours))}` : ''}${L.ours && L.theirs ? ' <i class="mc-ag">against</i> ' : ''}${L.theirs ? `${fl(L.theirs, 30)} ${esc(o.name(L.theirs))}` : ''}</span></div></div>
    ${L.follows ? `<p class="mc-so">${esc(L.follows)}</p>` : ''}${L.facts.length ? `<div class="mc-facts" style="--n:${L.facts.length}">${L.facts.map((f) => `<div><b>${esc(f.n)}</b>${esc(f.label)}</div>`).join('')}</div>` : ''}
    <div class="mc-leadb">${where(L) ? `<button class="mc-alt" ${where(L)}>SHOW ON MAP</button>` : ''}${L.report ? `<button class="mc-alt" data-act="${REPORT_ACT[L.report.type]}" data-id="${L.report.id}">READ THE BATTLE REPORT</button>` : ''}</div></div>`;

  const shown = o.all ? card.rows : card.rows.slice(0, card.show);
  const fold = card.rows.length > shown.length ? `<button class="mc-all" data-act="monthall">${esc(card.count || `ALL ${card.rows.length + (L ? 1 : 0)} REPORTS`)} ▸</button>` : '';
  const more = card.foot || fold ? `<div class="mc-more"><span>${esc(card.foot)}</span>${fold}</div>` : '';
  // A letter from the front is read where the rows leave it room; the rest are in the Newsreel.
  const room = o.all ? 0 : L ? (shown.length <= 3 ? 1 : 0) : shown.length <= 3 ? 2 : shown.length <= 5 ? 1 : 0;
  const read = card.letters.slice(0, room);
  const letters = read.length ? `<div class="mc-letters"><div class="mc-lab">${read.length > 1 ? 'Letters from the front' : 'A letter from the front'}</div>${read.map((l) => `<blockquote><p>${esc(l.text)}</p><cite>${esc(l.by)}</cite></blockquote>`).join('')}</div>` : '';
  // (Where the lead story was the month's only fighting, no head stands over an empty column: the second Playtester's
  // 84.2.6 find. A letter keeps its own.)
  const fronts = `<div class="mc-rt">${!shown.length && !card.quiet ? '' : `<div class="mc-lab">${card.lead || shown.some((r) => r.cls === 3 || r.cls === 4) ? 'Your fronts · the land first, then sea and air' : 'Your fronts'}</div>`}
    ${card.quiet ? `<p class="mc-quiet">${esc(card.quiet)}</p>` : ''}${shown.length ? `<div class="mc-rows">${shown.map(row).join('')}</div>` : ''}${more}${letters}</div>`;

  const it = (sign: string, body: string, cls = ''): string => `<div class="mc-it${cls}">${sign}${body}</div>`;
  // Each box holds three lines beside a lead story and five on a quieter card; what is over is counted, not lost.
  const lines = L ? 3 : 5;
  // (What was delivered, and where: a city that finds no room beside it goes whole to a second line, in grey.)
  const goods = (h: MonthCard['home'][number]): string => it(`<span class="sg">${monthSign(h.sign, 15, h.next ? DIM : BRASS)}</span>`, `${h.bare ? '' : `<b>${h.count}</b>`}<span class="gt">${esc(h.text)}${h.place ? ` <small>${esc(h.place)}</small>` : ''}</span>`, h.next ? ' gd next' : h.bare ? ' gd bare' : ' gd');
  // (What is ready first, then research, then what next month brings, then the other news of home. Deliveries
  // that find no line are counted in one; news that finds none is in the Newsreel.)
  const homeAll: { html: string; ready: number; next: number }[] = [
    // (The line when men run out comes first, with its door: ruling 133(d)(1).)
    ...(card.menOut ? [{ html: it('', `<span>${esc(card.menOut)} <button class="tb" data-act="openarsenal" title="The arsenal: what money alone can buy">OPEN THE ARSENAL</button></span>`, ' also'), ready: 0, next: 0 }] : []),
    ...card.home.filter((h) => !h.next).map((h) => ({ html: goods(h), ready: h.count, next: 0 })),
    ...card.research.map((t) => ({ html: it(`<span class="sg">${monthSign('star', 13, BRASS)}</span>`, `<span>Research complete: ${esc(t)}</span>`), ready: 0, next: 0 })),
    ...card.home.filter((h) => h.next).map((h) => ({ html: goods(h), ready: 0, next: h.count })),
    // (In a box where no line has a sign, the lines start at the box's left edge.)
    ...card.also.map((t, i) => ({ html: it(card.home.length + card.research.length ? '<span class="sg"></span>' : '', `<span title="${esc(card.alsoFull[i] ?? t)}">${esc(t)}</span>`, ' also'), ready: 0, next: 0 })),
  ];
  let homeOver = 0;
  let home = homeAll.slice(0, lines).map((x) => x.html);
  if (homeAll.length > lines) {
    const hid = homeAll.slice(lines - 1);
    const ready = hid.reduce((n, x) => n + x.ready, 0), next = hid.reduce((n, x) => n + x.next, 0);
    const said = [ready ? `${ready} more ready` : '', next ? `${next} due next month` : ''].filter(Boolean).join(' · ');
    if (said) home = [...homeAll.slice(0, lines - 1).map((x) => x.html), `<div class="mc-it over"><span>${said}</span></div>`];
    homeOver = (said ? hid : homeAll.slice(lines)).filter((x) => !x.ready && !x.next).length;
  }
  // (A desk's first line carries its button, and the first lines of the desks come first.)
  const firsts = card.staff.filter((s, i) => card.staff.findIndex((x) => x.desk === s.desk) === i);
  const staffAll = [...firsts, ...card.staff.filter((s) => !firsts.includes(s))];
  const staff = staffAll.slice(0, lines).map((s) => it(`<span class="sg">${monthSign(s.sign, s.sign === 'star' ? 13 : 15, BRASS)}</span>`, `<span title="${esc(s.text)}">${esc(s.short)}</span>${!firsts.includes(s) ? '' : s.mayTake ? `<button class="tb" data-act="desk" data-desk="${esc(s.desk)}" data-on="0" title="What your staff did this month stands. From next month this desk is yours again.">TAKE THE DESK BACK</button>` : '<small class="mc-took">yours again from next month</small>'}`, ' st'));
  const els = card.elsewhere.slice(0, lines).map((e) => it(fl(e.nat, 26), `<span title="${esc(e.full ?? e.text)}">${esc(e.text)}</span>`, ' el'));
  const over = homeOver + Math.max(0, staffAll.length - lines) + Math.max(0, card.elsewhere.length - lines);
  const none = (t: string): string => `<div class="mc-it none"><span>${t}</span></div>`;
  const low = `<div class="mc-low">
    <div class="mc-box"><div class="mc-lab">At home</div>${home.join('') || none('Nothing was delivered this month.')}</div>
    <div class="mc-box"><div class="mc-lab">What your staff did · it stands</div>${staff.join('') || none('Your staff ran no desk this month.')}</div>
    <div class="mc-box"><div class="mc-lab">Elsewhere in the war</div>${els.join('') || none('No news from the other fronts.')}</div></div>`;

  const rest = card.more + over + (card.letters.length - read.length);
  const foot = `<div class="mc-foot"><span>R opens this report again at any time.</span><span>${rest ? `${rest} more ${rest === 1 ? 'line' : 'lines'} of news ${rest === 1 ? 'is' : 'are'} in the Newsreel.` : 'Every figure comes from the month\'s own battle reports.'}</span></div>`;

  // THE SKY (weather, ruling 143(b)): after the rows of news and before AT HOME; plain, never a warning.
  const sk = card.sky;
  const sky = !sk ? '' : `<div class="mc-box mc-sky"><div class="mc-lab">The sky</div>${sk.lines.map((l) => it('', `<span>${esc(l.text)}${l.forecast.map((f) => `<br><small style="opacity:.75">${esc(f)}</small>`).join('')}</span>`)).join('')}${sk.report ? it('', `<span><small>${esc(sk.report)}</small></span>`) : ''}${sk.tip ? it('', `<span><small><b>Tip.</b> ${esc(sk.tip)}</small></span>`) : ''}</div>`;
  // (City defences, the switch on: a long label goes on to a second line and is never cut. Off, as shipped.)
  return `<div class="mc${card.bands.length > 1 ? ' two' : ''}${card.wrap ? ' wrap' : ''}">${head}${bands}${tally}<div class="mc-main${L ? '' : ' solo'}">${lead}${fronts}</div>${sky}${low}${foot}</div>`;
}

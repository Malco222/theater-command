// probe91: the red band before the player's own country falls (rulings 117(d) and 133(a), Version 83.1 on).
//   node out831/probe91.mjs ME 1 SEED UNTIL PLAN-JSON     (TC_LEVEL sets the level, default colonel)
// A seat played by hand (the lib's plan). Each month, as it opens, the month card is made as the page makes it and
// its warning of the country in danger is read: a FALL band ("X can fall next month"), a REACH band (once), or the
// LAST MONTH lead. One DANGER line a war: what the card said the month the country fell and the months before.
import { campaign, date } from './lib';
import { monthCard, monthMark, markFor } from '@tc/campaign/monthcard';
import { IN_DANGER, reading } from '@tc/campaign/danger';

const [me, fromS, seedS, untilS, planS] = process.argv.slice(2);
const plan = JSON.parse(planS || '{}');
type Said = { turn: number; level: 'fall' | 'reach' | 'last' | 'stake'; head: string; sub: string; note: string };
const said: (Said | null)[] = []; const turns: number[] = [];
let mark: any = null; const errs: string[] = []; const detail: string[] = [];
const read = (g: any, opening: boolean): void => {
  // (A country beaten before its month opens is handed no card: the lib still calls once, and that call is not a card.)
  if (!g.nations[me].alive) return;
  let s: Said | null = null;
  try {
    const card: any = monthCard(g, mark ?? markFor(g, me), me, opening ? { opening: true } : {});
    const b = (card.bands ?? []).find((x: any) => x.title === IN_DANGER);
    if (b) s = { turn: g.turn, level: /can fall next month/.test(b.head) ? 'fall' : 'reach', head: b.head, sub: b.sub ?? '', note: b.note ?? '' };
    else if (card.lead?.tag === 'LAST MONTH') s = { turn: g.turn, level: card.lead.kind === 'THE CAPITAL' ? 'last' : 'stake', head: card.lead.line ?? card.lead.text ?? card.lead.kind ?? '', sub: card.lead.sentence ?? '', note: '' };
  } catch (e: any) { errs.push(`${date(g.turn)}: ${e.message}`); }
  said.push(s); turns.push(g.turn);
  // TC_DETAIL=1: the reading itself each month (the seat, the reckoning against its limit, what can end it).
  if (process.env.TC_DETAIL) try {
    const r: any = reading(g, me);
    detail.push(r ? `${date(g.turn)}: ${s ? s.level.toUpperCase() : '-'} seat ${r.seat} by ${r.by ?? '-'} score ${r.score.toFixed(3)} of ${r.limit} (capital ${r.parts.capital.toFixed(2)}, home ${r.parts.home.toFixed(2)}, army ${r.parts.army.toFixed(2)}, more ${r.parts.more.toFixed(3)}) canFall ${r.canFall} how ${r.how} sure ${r.sure} last ${r.lastMonth} within ${r.within} twoThirds ${r.twoThirds} lost ${r.lost}/${r.of} open ${g.flags.open}` : `${date(g.turn)}: no reading`);
  } catch (e: any) { detail.push(`${date(g.turn)}: ${e.message}`); }
  try { mark = monthMark(g, me); } catch (e: any) { errs.push(`mark ${date(g.turn)}: ${e.message}`); }
};
const run = campaign(me, Number(fromS), Number(seedS), { ...plan, first: (g: any) => { read(g, said.length === 0); } }, {
  until: Number(untilS), level: process.env.TC_LEVEL || 'colonel', quiet: true,
});
const g: any = run.g;
const alive = g.nations[me].alive;
// The card the player read as his last month opened is the last one recorded.
const last = said[said.length - 1], lastTurn = turns[turns.length - 1];
const lv = (s: Said | null) => (s ? s.level.toUpperCase() : '-');
const tail = said.slice(-8).map((s, i) => `${date(turns[turns.length - Math.min(8, said.length) + i]).replace(/(\w{3})\w* /, '$1 ')} ${lv(s)}`).join(', ');
const falls = said.map((s, i) => (s && s.level === 'fall' ? turns[i] : 0)).filter(Boolean);
const reach = said.map((s, i) => (s && s.level === 'reach' ? turns[i] : 0)).filter(Boolean);
const lasts = said.map((s, i) => (s && s.level === 'last' ? turns[i] : 0)).filter(Boolean);
// A run of warnings unbroken up to the end: how many months of notice the fall had.
let notice = 0; for (let i = said.length - 1; i >= 0 && said[i] && said[i]!.level !== 'reach'; i--) notice++;
if (!alive) {
  const fellT = (run as any).fellOn ?? g.turn;
  console.log(`DANGER ${me} seed ${seedS}: FELL ${date(fellT)} | the card as its last month (${date(lastTurn)}) opened: ${last ? `${lv(last)} "${last.head}"` : 'NOTHING'} | months of unbroken notice ${notice} | the last eight cards: ${tail} | fall bands in ${falls.length} months${falls.length ? ` (first ${date(falls[0])})` : ''}, of them not in the run up to the end ${falls.length - Math.max(0, notice - lasts.filter((t) => t > lastTurn - notice).length)} | reach said ${reach.length ? reach.map((t) => date(t)).join(', ') : 'never'} | last month lead ${lasts.length ? lasts.map((t) => date(t)).join(', ') : 'never'}`);
  if (last) console.log(`   WORDS seed ${seedS}: ${last.head}. ${last.sub} [${last.note}]`);
} else {
  console.log(`DANGER ${me} seed ${seedS}: STANDS to ${date(g.turn)} | fall bands in ${falls.length} months${falls.length ? ` (${falls.slice(0, 6).map((t) => date(t)).join(', ')}${falls.length > 6 ? ', ...' : ''})` : ''} | reach said ${reach.length ? reach.map((t) => date(t)).join(', ') : 'never'} | last month lead ${lasts.length ? lasts.map((t) => date(t)).join(', ') : 'never'}`);
}
if (process.env.TC_DETAIL) { for (const d of detail.slice(-Number(process.env.TC_DETAIL) - 1)) console.log(`   READ ${d}`); const f = g.nations[me].fellOn; if (f) console.log(`   HOW it fell: ${f.how} to ${f.to}, turn ${date(f.turn)} week ${f.week}; capital held by ${g.regions[g.nations[me].capital]?.ctrl}`); }
if (errs.length) console.log(`   ERR seed ${seedS}: ${errs.slice(0, 3).join(' ;; ')} (${errs.length})`);
if (run.errors.length) console.log(`   RUNERR seed ${seedS}: ${run.errors[0].slice(0, 300)}`);

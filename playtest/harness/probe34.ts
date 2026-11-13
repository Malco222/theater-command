// Version 59: writes saves for the page at moments worth looking at.
// usage: probe34 ME FROM SEED UNTIL PLAN-JSON OUT-PREFIX [hawks|doves]
// Version 60: OUT-head.json (the cabinet asks for a general), OUT-stood.json (a general stood by, with plan option "head":"stand"), OUT-canvow.json (a promise can be made),
// OUT-vow.json (a promise stands, with plan option "vow"), OUT-hawks.json and OUT-doves.json (that wing has the table; the last argument seats it), OUT-spare.json (an order to spare the army).
// Saves: OUT-thinning.json, OUT-short.json, OUT-spent.json (the first month the reserve of men reads so), OUT-hard.json (the first
// directive marked strongly held), OUT-again.json (a town asked for before), OUT-trade.json (the first directive to sign a trade agreement),
// OUT-hold.json (the first hold order), OUT-doubt.json (the first month the cabinet doubts the player), OUT-directive.json (the first directive of any kind).
import { writeFileSync } from 'node:fs';
import { campaign, date, T, saveGame } from './lib';
const [me, fromS, seedS, untilS, planS, out, wing] = process.argv.slice(2);
const SEATS: Record<string, [string, string][]> = { hawks: [['staff', 'organiser'], ['arms', 'industrialist'], ['foreign', 'statesman'], ['finance', 'taxman']], doves: [['staff', 'reformer'], ['arms', 'planner'], ['foreign', 'merchant'], ['finance', 'paymaster']] };
const got = new Set<string>();
const keep = (g: any, k: string, why: string): void => { if (got.has(k)) return; got.add(k); writeFileSync(`${out}-${k}.json`, saveGame(g)); console.log(`${date(g.turn)}: saved ${k} (${why})`); };
const run = campaign(me, Number(fromS), Number(seedS), { ...JSON.parse(planS), each: (g: any) => {
  const m = (T as any).manpowerOf?.(g, me);
  if (m) { if (m.word === 'Thinning') keep(g, 'thinning', m.line); if (m.word === 'Short') keep(g, 'short', m.line); if (m.word === 'Spent') keep(g, 'spent', m.line); }
  const v = T.governmentOf(g) as any; if (!v || !v.mine) return;
  const d = v.directive;
  if (d) { keep(g, 'directive', `${d.kind}: ${d.title}`); if (d.hard) keep(g, 'hard', d.title); if (d.again) keep(g, 'again', d.title); if (d.kind === 'trade') keep(g, 'trade', d.title); if (d.kind === 'hold') keep(g, 'hold', d.title); }
  if (v.doubts) keep(g, 'doubt', `trust ${v.trust}: ${v.withheld}`);
  if (d && d.kind === 'spare') keep(g, 'spare', d.title);
  if (v.head) keep(g, 'head', `${v.head.rank} ${v.head.name}: ${v.head.why}`);
  if (v.stoodBy) keep(g, 'stood', `${v.stoodBy.name}, watched ${v.stoodBy.left} more months`);
  if (v.promise) keep(g, 'vow', `${v.promise.title} (${v.promise.band})`); else if (v.promiseWhy === null && g.turn > 6) keep(g, 'canvow', 'a promise can be made');
  if (v.wings && v.wings.top !== 'balanced') keep(g, v.wings.top, v.wings.text);
  for (const [seat, m] of SEATS[wing ?? ''] ?? []) { const s = v.seats.find((x: any) => x.id === seat); if (s && s.minister.id !== m && !T.whyNotAppointMinister(g, seat, m)) T.orderAppoint(g, seat, m); }
} }, { until: Number(untilS), quiet: true });
for (const e of run.errors) console.log('ERROR', String(e).slice(0, 600));
console.log(`saved: ${[...got].join(', ') || 'nothing'}; played to ${date(run.g.turn)}${run.fellOn ? ` (fell ${run.fellOn})` : ''}`);

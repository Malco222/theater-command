// probe95: the desert of 1942 from an Axis seat: does the British offensive arrive as a blow the player could see
// coming, or out of a clear sky? (the design director's rulings 170 and 171, Version 84 on)
//   node out84b/probe95.mjs ME 1 SEED UNTIL PLAN-JSON     (TC_FROM=30 TC_TO=44: the months read, February 1942 to April 1943)
// A seat is played by the lib's plan. As each month in the window opens: the Axis corps in the desert and the line
// (Axis ground next to British troops) with the odds the enemy could bring against it (threatOdds, the page's own
// reading); the British corps in the desert, the artillery among them, and how many the player has identified; the
// month card as the page makes it (any band or row about the desert) and the month's news lines that name it.
// Then every British attack on Axis corps in the desert that month. One DESERT line a month, one BLOW line a war.
import { writeFileSync, mkdirSync } from 'node:fs';
import { saveGame, campaign, date } from './lib';
import { monthCard, monthMark, markFor } from '@tc/campaign/monthcard';
import { threatOdds } from '@tc/campaign/government';
import { corpsKnown } from '@tc/campaign/intel';
import { allied, atWar } from '@tc/campaign/game';
const [me, fromS, seedS, untilS, planS] = process.argv.slice(2);
const plan = JSON.parse(planS || '{}');
const FROM = Number(process.env.TC_FROM || 30), TO = Number(process.env.TC_TO || 44);
let mark: any = null, seenReports = 0, seenHeads = 0;
const months: any[] = []; const errs: string[] = [];
const side = (g: any, nat: string): boolean => nat === me || allied(g, me, nat);
// The desert theatre: every province of desert and the provinces next to one (Alexandria, Cairo, Tripoli and Tunis are not desert ground).
let theatre: Set<number> | null = null;
const isDesert = (r: any): boolean => !!theatre && theatre.has(r.id);
const first = (g: any): void => {
  if (!theatre) { theatre = new Set<number>(); for (const r of g.regions) if (r.terrain === 'desert') { theatre.add(r.id); for (const j of r.nb) theatre.add(j); } }
  if (!g.nations[me].alive) return;
  if (process.env.TC_SAVE && (process.env.TC_SAVE_AT || '').split(',').map(Number).includes(g.turn)) { mkdirSync(process.env.TC_SAVE, { recursive: true }); writeFileSync(`${process.env.TC_SAVE}/save-t${g.turn}.json`, saveGame(g)); }
  // the battles of the month just ended: British blows on Axis corps in the desert
  const blows: any[] = [];
  for (const rep of g.reports.slice(seenReports)) {
    const r = g.regions[rep.region];
    if (!r || !isDesert(r) || !atWar(g, rep.attNat, me) || !(side(g, rep.defNat) || rep.defenders.some((u: any) => side(g, u.nat)))) continue;
    blows.push({ t: rep.turn, where: rep.regionName, by: rep.attNat, odds: rep.aPower / Math.max(1e-9, rep.dPower), guns: rep.attackers.filter((u: any) => u.kind === 'art').length, n: rep.attackers.length, taken: !!rep.taken, mine: rep.defNat === me || rep.defenders.some((u: any) => u.nat === me), said: rep.paid?.def ?? '' });
  }
  seenReports = g.reports.length;
  if (months.length) months[months.length - 1].blows = blows.filter((b) => b.t === months[months.length - 1].t);
  const news = g.headlines.slice(seenHeads).filter((h: any) => (!h.nat || h.nat === me) && /Alexandria|Egypt|Libya|Cyrenaica|Tripoli|Tobruk|Benghazi|Alamein|Matruh|desert|Eighth Army|Royal Artillery/i.test(h.text)).map((h: any) => `[${h.kind}] ${h.text}`);
  seenHeads = g.headlines.length;
  let cardLines: string[] = [];
  try {
    const card: any = monthCard(g, mark ?? markFor(g, me), me, {});
    const names = new Set(g.regions.filter(isDesert).map((r: any) => r.name));
    const hit = (s: string): boolean => !!s && (/desert|Egypt|Libya|Africa|Eighth Army|artillery/i.test(s) || [...names].some((n) => s.includes(n as string)));
    for (const b of card.bands ?? []) if (hit(`${b.title ?? ''} ${b.head} ${b.text}`)) cardLines.push(`BAND ${b.word}${b.title ? ` ${b.title}` : ''}: ${b.head}`);
    if (card.lead && hit(`${card.lead.place} ${card.lead.line}`)) cardLines.push(`LEAD ${card.lead.tag} ${card.lead.place}: ${card.lead.line}`);
    for (const r of card.rows ?? []) if (hit(`${r.place} ${r.line}`)) cardLines.push(`ROW ${r.tag} ${r.place}: ${r.line}`);
    for (const e of card.elsewhere ?? []) if (hit(JSON.stringify(e))) cardLines.push(`ELSEWHERE ${(e.line ?? e.text ?? JSON.stringify(e)).slice(0, 160)}`);
  } catch (e: any) { errs.push(`${date(g.turn)}: ${e.message}`); }
  try { mark = monthMark(g, me); } catch (e: any) { errs.push(`mark: ${e.message}`); }
  if (g.turn < FROM || g.turn > TO) return;
  const axis: any[] = [], brit: any[] = [];
  for (const r of g.regions) if (isDesert(r)) for (const c of r.units) { if (c.dead || c.kind === 'fort') continue; if (side(g, c.nat)) axis.push({ c, r }); else if (atWar(g, c.nat, me)) brit.push({ c, r }); }
  const known = (c: any): boolean => { try { return corpsKnown(g, c, me).level !== 'none'; } catch { return false; } };
  const guns = brit.filter((x) => x.c.kind === 'art');
  const line: { name: string; odds: number; by: string; who: string }[] = [];
  for (const r of g.regions) {
    if (!isDesert(r) || !side(g, r.ctrl) || !r.units.some((c: any) => !c.dead && side(g, c.nat))) continue;
    if (!r.nb.some((j: number) => g.regions[j].units.some((c: any) => !c.dead && atWar(g, c.nat, me)))) continue;
    let th: any = null; try { th = threatOdds(g, me, r.id); } catch { th = null; }
    line.push({ name: r.name, odds: th ? th.odds : 0, by: th ? th.by : '-', who: [...new Set(r.units.filter((c: any) => !c.dead).map((c: any) => c.nat))].join('+') });
  }
  const holds = ['Tripoli', 'Benghazi', 'Tobruk', 'Mersa Matruh', 'Alexandria', 'Cairo'].map((n) => { const r = g.byName?.[n] ?? g.regions.find((x: any) => x.name === n); return r ? `${n} ${r.ctrl}` : ''; }).filter(Boolean).join(', ');
  months.push({ holds, t: g.turn, axis: axis.length, mine: axis.filter((x) => x.c.nat === me).length, brit: brit.length, britKnown: brit.filter((x) => known(x.c)).length, guns: guns.map((x) => `${x.r.name}${known(x.c) ? ' (identified)' : ' (not identified)'}`), line, news, cardLines, blows: [] });
};
const run = campaign(me, Number(fromS), Number(seedS), { ...plan, first }, { until: Number(untilS), level: process.env.TC_LEVEL || 'colonel', quiet: true });
for (const m of months) {
  console.log(`DESERT ${me} seed ${seedS} ${date(m.t)}: ${m.holds} | Axis corps ${m.axis} (mine ${m.mine}), British ${m.brit} (identified ${m.britKnown}) | their guns: ${m.guns.join(', ') || 'none'} | line: ${m.line.map((l: any) => `${l.name} [${l.who}] threat ${l.odds.toFixed(2)}`).join('; ') || 'no line in touch'} | card: ${m.cardLines.slice(0, 4).join(' // ') || 'nothing of the desert'} | news: ${m.news.slice(0, 3).join(' // ') || 'none'} | blows this month: ${m.blows.map((b: any) => `${b.where} by ${b.by}, ${b.n} corps (${b.guns} artillery), odds ${b.odds.toFixed(2)}, ${b.taken ? 'LOST' : 'held'}`).join('; ') || 'none'}`.slice(0, 1500));
}
// the first blow with guns in it that took ground from a line, and what stood on the cards before it
const withGuns = months.find((m) => m.blows.some((b: any) => b.guns > 0));
const firstLost = months.find((m) => m.blows.some((b: any) => b.taken));
const sayBefore = (m: any): string => {
  if (!m) return 'never in the window';
  const i = months.indexOf(m), prev = i > 0 ? months[i - 1] : null;
  const top = Math.max(0, ...m.line.map((l: any) => l.odds));
  return `${date(m.t)}; as that month opened: the highest threat on the line ${top.toFixed(2)}, their guns ${m.guns.length ? m.guns.join(', ') : 'none in the desert'}, card ${m.cardLines.length ? `"${m.cardLines[0]}"` : 'nothing of the desert'}, news ${m.news.length}; the month before: threat ${prev ? Math.max(0, ...prev.line.map((l: any) => l.odds)).toFixed(2) : '-'}, guns ${prev ? prev.guns.length : '-'}`;
};
console.log(`BLOW ${me} seed ${seedS}: months read ${months.length}; months with Axis corps in the desert ${months.filter((m) => m.axis > 0).length}, with a line in touch ${months.filter((m) => m.line.length).length} | first British blow with artillery: ${sayBefore(withGuns)} | first ground lost in the desert: ${sayBefore(firstLost)} | their guns ever identified before they fired: ${months.some((m) => m.guns.some((s: string) => s.includes('(identified)')) && (!withGuns || m.t <= withGuns.t)) ? 'yes' : 'no'}`);
if (errs.length) console.log(`   ERR: ${errs.slice(0, 3).join(' ;; ')} (${errs.length})`);
if (run.errors.length) console.log(`   RUNERR: ${run.errors[0].slice(0, 300)}`);

// Version 63: can a won war be finished? The end of an enemy great power from the player's seat.
// usage: probe41 ME FROM SEED UNTIL PLAN-JSON [FOE]
// Plays the plan. For the foe (Germany unless named) it notes the month its ground first fell to ten provinces, to
// three and to one, the month it gave in, and every month from the first of those in which the player fought no
// battle against it. It prints the pocket headlines (stores spent, armies laying down their arms) and, at the end,
// the foe's provinces with the troops in them if it still stands.
import { campaign, date } from './lib';
const [me, fromS, seedS, untilS, planS, foe = 'GER'] = process.argv.slice(2);
const plan = JSON.parse(planS || '{"minOdds":1.4}');
const marks: Record<string, number> = {};
let seenRep = 0, seenHl = 0, idle = 0, longestIdle = 0, lastTurn = 0, gone = 0;
const pocket: string[] = [];
const run = campaign(me, Number(fromS), Number(seedS), { ...plan, each: (g: any) => {
  lastTurn = g.turn;
  const f = g.nations[foe];
  const held = g.regions.filter((r: any) => r.ctrl === foe).length;
  for (const [k, n] of [['ten', 10], ['three', 3], ['one', 1]] as [string, number][]) if (held <= n && marks[k] === undefined && f.atWar.includes(me)) marks[k] = g.turn;
  if (!f.alive && !gone) gone = g.turn;
  let fought = 0;
  for (const r of g.reports) { if (r.id <= seenRep) continue; seenRep = r.id; if ((r.attNat === me && r.defNat === foe) || (r.attNat === foe && r.defNat === me)) fought++; }
  if (marks.ten !== undefined && f.alive) { if (fought) idle = 0; else { idle++; longestIdle = Math.max(longestIdle, idle); } }
  for (const h of g.headlines.slice(seenHl)) if (/stores of .* are spent|lays down its arms|are cut off from the rest|lives on the stores|goes hungry/.test(h.text) && pocket.length < 40) pocket.push(`${date(g.turn)}: ${h.text.slice(0, 200)}`);
  seenHl = g.headlines.length;
} }, { until: Number(untilS), quiet: true });
const g: any = run.g, f = g.nations[foe];
for (const e of run.errors) console.log('ERROR', String(e).slice(0, 600));
for (const p of pocket) console.log('   ' + p);
const left = g.regions.filter((r: any) => r.ctrl === foe);
if (f.alive && left.length <= 12) for (const r of left) console.log(`   STILL HELD ${r.name}: ${r.units.filter((u: any) => u.nat === foe && !u.dead).length} corps in the field, ${(r.garr ?? []).length} in the town; beside it ${r.nb.map((j: number) => g.regions[j]).filter((x: any) => x.ctrl === me || x.units.some((u: any) => u.nat === me)).reduce((s: number, x: any) => s + x.units.filter((u: any) => u.nat === me && !u.dead).length, 0)} of my corps`);
const d = (t?: number): string => (t === undefined ? 'never' : date(t));
console.log(`TOTAL ${me} seed ${seedS} against ${foe}: played to ${date(lastTurn)}${run.fellOn ? ` (fell ${run.fellOn})` : ''}; ${foe} ${f.alive ? `still stands with ${left.length} provinces` : `gave in ${d(gone)}`}; down to ten provinces ${d(marks.ten)}, three ${d(marks.three)}, one ${d(marks.one)}; months from ten provinces to the end ${marks.ten !== undefined ? (gone || lastTurn) - marks.ten : '-'}; longest run of months with no battle against it after that ${longestIdle}; at war with me: ${f.atWar.includes(me)}`);

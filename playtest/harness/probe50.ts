// Version 66: the staff's month, to read. usage: probe50 ME FROM SEED UNTIL PLAN-JSON
// Plays the scripted player's war at the plan's depth and prints, as each of his months opens: the treasury, what is
// left of the staff's allowance and what building took of it, the men in the pool, the army, what the Staff tab says
// of each staffed desk (only when it changes), and every line the staff put in the news. At the end: the treasury
// year by year, and how often each wording came up.
import { campaign, date, T } from './lib';
const [me, fromS, seedS, untilS, planS] = process.argv.slice(2);
const X = T as any;
const plan = JSON.parse(planS);
let seenH = 0;
const lastState: Record<string, string> = {};
const lines: { turn: number; desk: string; text: string }[] = [];
const rows: { turn: number; tre: number; spent: number; left: number; men: string; corps: number; queue: number; noMen: boolean }[] = [];
const run = campaign(me, Number(fromS), Number(seedS), { ...plan,
  first: (g: any) => {
    const v = X.commandOf?.(g);
    if (!v) return;
    const n = g.nations[me], mp = X.manpowerOf?.(g, me);
    const tre = Math.round(n.pp * 100), spent = Math.round((v.spent ?? 0) * 100), left = Math.round((v.left ?? 0) * 100);
    const corps = g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort').length;
    const build = v.desks.find((d: any) => d.id === 'build');
    const noMen = !!build?.staff && /no men to spare/.test(build.state);
    rows.push({ turn: g.turn, tre, spent, left, men: mp ? `${mp.word ?? ''} ${Math.round(mp.share * 100)}%` : '-', corps, queue: n.queue.length, noMen });
    console.log(`\n${date(g.turn)}  [${v.name}, allowance ${v.cap}%]  treasury ${tre}, the staff spent ${spent} on building and has ${left} left; men ${mp ? `${mp.word ?? ''} (${Math.round(mp.share * 100)}% of the pool, ${Math.round(mp.divs)} divisions' worth)` : '-'}; ${corps} corps in the field, ${n.queue.length} orders in hand`);
    for (const d of v.desks) if (d.staff && lastState[d.id] !== d.state) { console.log(`   Staff tab, ${d.name}: ${d.state}`); lastState[d.id] = d.state; }
    for (let i = seenH; i < g.headlines.length; i++) { const h = g.headlines[i]; if (h.desk && h.nat === me) { lines.push({ turn: h.turn, desk: h.desk, text: h.text }); console.log(`   NEWS ${h.desk}: ${h.text}`); } }
    seenH = g.headlines.length;
  },
} as any, { until: Number(untilS), quiet: true } as any);
const g = run.g as any;
console.log(`\n==== ${me} seed ${seedS}, played to ${date(Math.min(g.turn, Number(untilS)))}${run.fellOn ? ` (fell ${run.fellOn})` : ''}`);
const yr = (t: number): number => 1939 + Math.floor((t + 7) / 12);
const years = [...new Set(rows.map((r) => yr(r.turn)))];
for (const y of years) { const R = rows.filter((r) => yr(r.turn) === y); console.log(`   ${y}: treasury at the year's first month ${R[0].tre}, highest ${Math.max(...R.map((r) => r.tre))}; the staff spent ${R.reduce((s, r) => s + r.spent, 0)} on building; months with no men to spare ${R.filter((r) => r.noMen).length} of ${R.length}; corps ${R[0].corps} to ${R[R.length - 1].corps}; men ${R[0].men} to ${R[R.length - 1].men}`); }
const shape = (t: string): string => t.replace(/[0-9][0-9,.]*/g, '#');
for (const desk of [...new Set(lines.map((l) => l.desk))]) {
  const L = lines.filter((l) => l.desk === desk);
  const shapes: Record<string, number> = {};
  for (const l of L) shapes[shape(l.text)] = (shapes[shape(l.text)] ?? 0) + 1;
  let same = 0; for (let i = 1; i < L.length; i++) if (L[i].text === L[i - 1].text) same++;
  console.log(`   ${desk}: ${L.length} lines in ${rows.length} months, ${Object.keys(shapes).length} wordings once the numbers are out, ${same} word for word the same as the month before; longest ${Math.max(...L.map((l) => l.text.length))} letters`);
  for (const [t, n] of Object.entries(shapes).sort((a, b) => b[1] - a[1]).slice(0, 4)) console.log(`      ${n} times: ${t.length > 200 ? t.slice(0, 197) + '...' : t}`);
}
console.log(`QUIETDESK ${me} seed ${seedS}: months ${rows.length}; months with no staff line at all ${rows.filter((r) => !lines.some((l) => l.turn === r.turn)).length}; errors ${run.errors.length}; problems ${run.problems.filter((p: string) => !p.startsWith('[note]')).length}`);

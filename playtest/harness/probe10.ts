// A player who never gives an order: who falls, to whom, and where the victor's troops were. usage: probe10 ME SEED UNTIL
import { campaign, date, summary } from './lib';
const [me, seedS, untilS] = process.argv.slice(2);
let h0 = 0;
const run = campaign(me, 1, Number(seedS), { idle: true, each: (g) => {
  const hs = g.headlines.slice(h0).map((h: any) => typeof h === 'string' ? h : (h.text ?? h.t ?? h.line ?? h.head ?? JSON.stringify(h))); h0 = g.headlines.length;
  for (const t of hs) if (/capitulates|is in .* hands|asks for terms|liberated/.test(t)) {
    console.log(`${date(g.turn)}: ${t}`);
    const m = /^(.*) capitulates to (.*)\.$/.exec(t);
    if (m) { const loser = Object.values(g.nations).find((n) => n.name === m[1]); const win = Object.values(g.nations).find((n) => n.name === m[2]); if (loser && win) { const home = g.regions.filter((r) => r.owner === loser.id && !r.overseas); const with_ = home.filter((r) => r.units.some((u) => u.nat === win.id && !u.dead)).length; const holds = home.filter((r) => r.ctrl === win.id).length; const others = [...new Set(home.flatMap((r) => r.units.filter((u) => !u.dead && u.nat !== loser.id).map((u) => u.nat)))]; console.log(`     ${win.name} now holds ${holds} of ${loser.name}'s ${home.length} home provinces and has troops in ${with_} of them; foreign troops there: ${others.join(', ') || 'none'}`); } }
  }
} }, { until: Number(untilS) });
console.log(summary(run));
const g = run.g; const L = run.rows[run.rows.length - 1];
console.log(`last month ${L?.date}: treasury ${L?.treasury}, attacked in ${run.rows.filter((r) => r.hitByEnemy).length} of ${run.rows.length} months`);
for (const h of g.headlines as any[]) if (h.turn >= 84 && /capitulates|is in .* hands|Berlin|Paris/.test(h.text)) console.log(`${date(h.turn)}: ${h.text}`.slice(0, 260));
console.log('British corps on the Continent at the end:', g.corps.filter((c) => c.nat === 'UK' && !c.dead && !g.regions[c.reg].overseas && g.regions[c.reg].owner !== 'UK').map((c) => `${c.name} at ${g.regions[c.reg].name}`).join(', ') || 'none');
console.log('Home provinces of Germany by holder:', JSON.stringify(g.regions.filter((r) => r.owner === 'GER').reduce((m: any, r) => { m[r.ctrl] = (m[r.ctrl] ?? 0) + 1; return m; }, {})), ' of France:', JSON.stringify(g.regions.filter((r) => r.owner === 'FRA' && !r.overseas).reduce((m: any, r) => { m[r.ctrl] = (m[r.ctrl] ?? 0) + 1; return m; }, {})));

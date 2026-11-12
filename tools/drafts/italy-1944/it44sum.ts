import { createGame, men } from '/home/user/theater-command/src/campaign/game';
import { runTurnAI } from '/home/user/theater-command/src/campaign/turn';
const from = +process.argv[2], to = +process.argv[3], T1 = +(process.argv[4] ?? 58), detail = process.argv[5] === 'detail', T0 = +(process.argv[6] ?? 52);
const P = ['Malta', 'Sicily', 'Calabria', 'Apulia', 'Naples', 'Cassino', 'Anzio', 'Abruzzo', 'Rome', 'Marche', 'Tuscany', 'Emilia', 'Liguria'];
const short = (n: string): string => n.replace(/^(Fifth|Eighth|Seventh|14th) Army: /, '').replace(/ \(.*$/, '').replace(/ Corps$/, '');
for (let seed = from; seed <= to; seed++) {
  const g = createGame(seed, null) as any;
  const ids = new Set(P.map((n) => g.byName[n].id));
  const out: string[] = [];
  let seen = 0, read = 0;
  for (let t = 1; t <= T1; t++) {
    runTurnAI(g);
    const reps = (g.reports as any[]).slice(seen); seen = g.reports.length;
    const news = (g.headlines as any[]).slice(read); read = g.headlines.length;
    if (t < T0) continue;
    const f = reps.filter((r) => ids.has(r.region)).map((r) => `${r.attNat}>${r.regionName.slice(0, 4)}${r.taken ? '!' : 'x'}${r.odds.toFixed(2)}(${r.attackers.length}v${r.defenders.length} -${Math.round(r.attLost / 1000)}/-${Math.round(r.defLost / 1000)}${r.prisoners ? ' pr' + Math.round(r.prisoners / 1000) : ''})`).join(' ');
    const ctl = P.slice(3).map((n) => `${n.slice(0, 3)}:${g.byName[n].ctrl[0]}`).join(' ');
    out.push(`T${t} ${ctl} | ${f}`);
    if (detail) {
      for (const n of P) { const r = g.byName[n]; if (r.units.length) out.push(`      ${n}[${r.ctrl}${r.fort ? ' f' + r.fort.lv : ''}] ` + r.units.map((u: any) => `${u.nat}:${short(u.name)}:${u.kind}${u.divs}${u.hold ? 'h' : ''}@${u.str.toFixed(2)}/${u.org.toFixed(2)}${u.supplied ? '' : '!CUT'}`).join(', ')); }
      for (const h of news) if (/land|Anzio|Cassino|Rome|Gustav|Gothic|leave for|broken up|Eisenhower|thrown back|beachhead|Florence|lifted off/.test(h.text)) out.push(`      * ${h.text.slice(0, 170)}`);
    }
  }
  const tot = (nat: string): string => { const cs = (g.corps as any[]).filter((c) => c.nat === nat && !c.dead && ids.has(c.reg) && c.kind !== 'fort'); return `${nat} ${cs.length}c ${cs.reduce((s, c) => s + c.divs, 0)}d ${Math.round(cs.reduce((s, c) => s + men(c), 0) / 1000)}k`; };
  console.log(`seed ${seed}:\n  ` + out.join('\n  ') + `\n  end: ${tot('UK')} ${tot('USA')} ${tot('GER')}`);
}

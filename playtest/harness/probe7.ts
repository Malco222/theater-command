// Britain goes for Berlin in September 1939: every troopship to the German North Sea coast while the Wehrmacht is in Poland.
import { start, advance, date, T, collapseScore, judgeGoals, hostileUnits, atWar, allied, armySize } from './lib';
import { limitAttackers } from '@tc/campaign/game';
for (const seed of [7, 11, 23, 31]) {
  const g = start('UK', 1, seed);
  const me = 'UK';
  let up = advance(g);
  const log: string[] = [];
  while (up && g.turn <= 8 && g.nations.GER.alive) {
    // 1. corps in Germany: attack/march toward Berlin
    const berlin = g.byName['Berlin'];
    const dist = new Int16Array(g.regions.length).fill(-1); dist[berlin.id] = 0; const q = [berlin.id];
    for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (dist[j] < 0) { dist[j] = dist[q[i]] + 1; q.push(j); }
    for (let pass = 0; pass < 4; pass++) {
      for (const c of g.corps.filter((c) => c.nat === me && T.canOrder(g, c) && c.ap > 0 && g.regions[c.reg].owner === 'GER')) {
        const m = T.reach(g, c);
        let best = -1, bd = dist[c.reg];
        for (const [id, info] of m) if (!info.attack && dist[id] >= 0 && dist[id] < bd) { bd = dist[id]; best = id; }
        if (best >= 0) T.orderMove(g, c, best);
        for (const [id, info] of m) if (info.attack && dist[id] < dist[c.reg]) {
          const r = g.regions[id]; const cands = limitAttackers(g, T.candidates(g, r), r); const out = cands.length ? T.attackOutlook(g, cands, r) : null;
          if (out && out.odds >= 1.2) { const rep = T.orderAttack(g, cands, r); if (rep) log.push(`${date(g.turn)}: attacked ${r.name} odds ${rep.odds.toFixed(2)} ${rep.taken ? 'TAKEN' : 'failed'}`); }
        }
      }
    }
    // 2. every corps at a home port lands as near Berlin as it can
    const ports = g.regions.filter((r) => r.port && r.ctrl === me && r.owner === me);
    let landed = 0;
    for (const p of ports) {
      const cs = p.units.filter((u) => u.nat === me && T.canOrder(g, u) && u.ap > 0 && u.kind !== 'fort' && u.kind !== 'eng');
      for (const c of cs) {
        if (g.regions[g.nations.UK.capital].units.filter((u) => u.nat === me && u.kind !== 'fort').length <= 1 && c.reg === g.nations.UK.capital) continue;
        const targets = T.landTargets(g, [c]).filter((r) => r.owner === 'GER').sort((a, b) => dist[a.id] - dist[b.id]);
        for (const r of targets) {
          const pl = T.previewLanding(g, [c], r.id);
          if (pl.why || pl.risk > 0.4 || (pl.assess && pl.assess.odds < 1.3)) continue;
          const res = T.orderLand(g, [c], r.id);
          if (res.ok) { landed++; log.push(`${date(g.turn)}: ${c.name} lands at ${r.name} (${dist[r.id]} from Berlin) ${res.taken ? 'ashore' : 'thrown back'} risk ${pl.risk.toFixed(2)}${pl.assess ? ' odds ' + pl.assess.odds.toFixed(2) : ' undefended'}`); break; }
          else { log.push(`${date(g.turn)}: landing at ${r.name} refused: ${res.why}`); }
        }
      }
    }
    // rail the rest of the army to London/Kent ports for next month
    for (const c of g.corps.filter((c) => c.nat === me && T.canOrder(g, c) && c.ap >= 4 && g.regions[c.reg].owner === me && !g.regions[c.reg].port && !g.regions[c.reg].overseas)) {
      const port = g.regions.filter((r) => r.port && r.ctrl === me && r.owner === me && !r.overseas).sort((a, b) => Math.hypot(a.x - berlin.x, a.y - berlin.y) - Math.hypot(b.x - berlin.x, b.y - berlin.y)).find((r) => T.orderRail(g, c, r.id));
    }
    judgeGoals(g, me); T.endHuman(g);
    up = advance(g);
    log.push(`   -> ${date(g.turn)}: Berlin ${berlin.ctrl}, Hamburg ${g.byName['Hamburg'].ctrl}, GER alive ${g.nations.GER.alive} collapse ${collapseScore(g, 'GER').toFixed(2)}/${g.nations.GER.resilience}, UK corps in Germany ${g.corps.filter((c) => c.nat === me && g.regions[c.reg].owner === 'GER').length}, UK corps total ${g.corps.filter((c) => c.nat === me).length}, Poland alive ${g.nations.POL.alive}`);
  }
  console.log(`=== seed ${seed} ===\n` + log.join('\n'));
  console.log(g.headlines.filter((h) => h.kind === 'fall').map((h) => `${date(h.turn)}: ${h.text}`).join('\n'));
}

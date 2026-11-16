// Version 82 and later: where the Allied armies stand once Germany has fallen and a player's Italy still stands
// (ruling 111's ask). usage: probe87 SEED [UNTIL=76] [PLAN-JSON] (an Italian player by hand, the plan of probe85;
// build with build70.sh)
// Three STAND blocks a war: the month Germany falls, twelve months later, and the last month played. Each gives, for
// Britain, America, the Soviet Union and the other Allies together: whether it is at war with Italy, its living
// enemies, its field corps, and where they stand (on Italy's home ground, beside it, in Germany, Austria, France,
// at home, elsewhere by the ground's first owner). A corps is never at sea in these rules: a landing is made within
// the month. Then who holds each province that borders Italy's home ground and whose corps stand there.
import { campaign, date } from './lib';
const [seedS, untilS, planS] = process.argv.slice(2);
const plan = { minOdds: 1.4, declare: [[10, 'FRA'], [10, 'UK'], [14, 'GRE']], land: true, ...JSON.parse(planS || '{}') };
const AUSTRIA = ['Vienna', 'Upper Austria', 'Lower Austria', 'Tyrol', 'Styria', 'Carinthia', 'Salzburg'];
const UNTIL = Number(untilS || 76);
let gerFell = 0; const done: Record<string, boolean> = {};
const warFrom: Record<string, number> = {}, warTo: Record<string, number> = {};
const out: string[] = [];
const stand = (g: any, label: string): void => {
  const N = g.nations, I = N.ITA;
  const home = g.regions.filter((r: any) => r.owner === 'ITA' && !r.overseas);
  const homeIds = new Set(home.map((r: any) => r.id));
  const beside = new Set<number>(); for (const r of home) for (const n of r.nb) if (!homeIds.has(n)) beside.add(n);
  const byId: Record<number, any> = {}; for (const r of g.regions) byId[r.id] = r;
  const allies = Object.keys(N).filter((id) => N[id].align === 'allies' && N[id].alive);
  out.push(`STAND ${label}, ${date(g.turn)}: Italy ${I.alive ? `stands, ${g.corps.filter((c: any) => c.nat === 'ITA' && !c.dead && c.kind !== 'fort').length} field corps, ${home.filter((r: any) => r.ctrl === 'ITA').length} of ${home.length} home provinces` : 'is out'}; Germany ${N.GER.alive ? 'stands' : `fell ${date(N.GER.fellOn.turn)}`}; Japan ${N.JAP ? (N.JAP.alive ? `stands (${N.JAP.atWar.filter((x: string) => ['UK', 'USA', 'SOV'].includes(x)).join(', ') || 'at war with none of the three'})` : 'is out') : '-'}`);
  const groups: [string, string[]][] = [['Britain', ['UK']], ['America', ['USA']], ['Soviet Union', ['SOV']], ['other Allies', allies.filter((id) => !['UK', 'USA', 'SOV'].includes(id))]];
  for (const [name, ids] of groups) {
    const cs = g.corps.filter((c: any) => ids.includes(c.nat) && !c.dead && c.kind !== 'fort' && byId[c.reg]);
    const b: Record<string, number> = {}; const add = (k: string): void => { b[k] = (b[k] || 0) + 1; };
    for (const c of cs) {
      const r = byId[c.reg];
      if (homeIds.has(r.id)) add("ON ITALY'S HOME GROUND");
      else if (beside.has(r.id)) add(`BESIDE ITALY (${r.name})`);
      else if (AUSTRIA.includes(r.name)) add('Austria');
      else if (r.owner === 'GER') add('Germany');
      else if (r.owner === 'FRA' && !r.overseas) add('France');
      else if (r.owner === c.nat && !r.overseas) add('at home');
      else if (r.owner === 'ITA') add("Italy's overseas ground");
      else add(`${N[r.owner]?.name || r.owner}${r.overseas ? ' (overseas)' : ''}`);
    }
    const war = ids.length === 1 ? (N[ids[0]].alive ? (N[ids[0]].atWar.includes('ITA') ? 'AT WAR with Italy' : 'NOT at war with Italy') : 'out of the war') : `at war with Italy: ${ids.filter((id) => N[id].atWar.includes('ITA')).length} of ${ids.length}`;
    const foes = ids.length === 1 && N[ids[0]].alive ? `; living enemies: ${N[ids[0]].atWar.filter((x: string) => N[x]?.alive).join(', ') || 'none'}` : '';
    out.push(`   ${name}: ${war}${foes}; ${cs.length} field corps: ${Object.entries(b).sort((x, y) => y[1] - x[1]).map(([k, v]) => `${k} ${v}`).join(', ') || '-'}`);
  }
  const edge = [...beside].map((id) => byId[id]).filter(Boolean).map((r: any) => {
    const who: Record<string, number> = {}; for (const u of r.units) if (u.kind !== 'fort') who[u.nat] = (who[u.nat] || 0) + 1;
    return `${r.name} (${r.ctrl}${Object.keys(who).length ? `: ${Object.entries(who).map(([k, v]) => `${k} ${v}`).join(' ')}` : ', empty'})`;
  });
  out.push(`   the ground beside Italy's home: ${edge.join('; ')}`);
  const foesOfItaly = new Set(Object.keys(N).filter((id) => N[id].alive && N[id].atWar.includes('ITA')));
  out.push(`   at war with Italy now: ${[...foesOfItaly].map((id) => `${id} ${g.corps.filter((c: any) => c.nat === id && !c.dead && c.kind !== 'fort').length}`).join(', ') || 'nobody'} (field corps each)`);
  const touch: Record<string, string> = {};
  for (const r of g.regions) if (r.ctrl === 'ITA') for (const n of r.nb) { const m = byId[n]; if (m && foesOfItaly.has(m.ctrl) && !touch[m.name]) touch[m.name] = `${m.name} (${m.ctrl}, ${m.units.filter((u: any) => foesOfItaly.has(u.nat) && u.kind !== 'fort').length} corps) against ${r.name} (${r.units.filter((u: any) => u.nat === 'ITA' && u.kind !== 'fort').length} Italian)`; }
  out.push(`   where an enemy of Italy holds ground next to Italian-held ground: ${Object.values(touch).join('; ') || 'NOWHERE'}`);
  const V = N.VIC; const vic = g.regions.filter((r: any) => r.ctrl === 'VIC');
  out.push(`   Vichy: ${V ? `${V.alive ? 'stands' : 'is out'}, side ${V.align || 'none'}, at war with ${V.atWar.filter((x: string) => N[x]?.alive).join(', ') || 'nobody'}, holds ${vic.length} provinces (${vic.filter((r: any) => !r.overseas).map((r: any) => r.name).slice(0, 14).join(', ')})` : 'no such nation'}`);
  const rim = g.regions.filter((r: any) => r.ctrl === 'ITA' && r.owner !== 'ITA' && !r.overseas);
  out.push(`   Italian-held ground in Europe outside Italy: ${rim.map((r: any) => `${r.name} (${r.units.filter((u: any) => u.kind !== 'fort').length} corps; beside it ${r.nb.map((n: number) => byId[n]).filter((m: any) => m && m.ctrl !== 'ITA').map((m: any) => `${m.name} ${m.ctrl}`).join(', ')})`).join('; ') || 'none'}`);
  const lost = home.filter((r: any) => r.ctrl !== 'ITA').map((r: any) => `${r.name} (${r.ctrl}, ${r.units.filter((u: any) => u.kind !== 'fort').length} corps)`);
  out.push(`   Italy's home provinces lost: ${lost.join('; ') || 'none'}`);
};
const look = (g: any): void => {
  for (const id of ['UK', 'USA', 'SOV', 'FRA']) if (g.nations[id]?.atWar.includes('ITA')) { if (!warFrom[id]) warFrom[id] = g.turn; warTo[id] = g.turn; }
  if (!gerFell && !g.nations.GER.alive) { gerFell = g.turn; stand(g, 'the month after Germany falls'); }
  if (gerFell && g.turn === gerFell + 12 && !done.y) { done.y = true; stand(g, 'twelve months later'); }
};
const run = campaign('ITA', 1, Number(seedS), { ...plan, first: look, each: look } as any, { until: UNTIL, quiet: true } as any);
const g: any = run.g; look(g); stand(g, 'the last month played');
console.log(`ALLIES seed ${seedS}: Germany ${g.nations.GER.fellOn ? `fell ${date(g.nations.GER.fellOn.turn)}` : 'stands'}; at war with Italy: ${['UK', 'USA', 'SOV', 'FRA'].map((id) => `${id} ${warFrom[id] ? `${date(warFrom[id])} to ${date(warTo[id])}` : 'never'}`).join(', ')}; Italy ${g.nations.ITA.alive ? 'stands' : `out ${date(g.nations.ITA.fellOn.turn)} (${g.nations.ITA.fellOn.how})`} at ${date(g.turn)}`);
for (const l of out) console.log(l);

// Version 76: where a Latin player at war can ship a corps (ruling 32: to an ally's port). The player takes the
// Allies' path (or the Axis, MODE axis), declares war on Germany (or on Britain), and in month M the ports the rules
// offer each corps standing in a home port are listed by country, with the reason the first British port is refused.
// usage: peek77 ME SEED MONTH [allies|axis]
import { campaign, date, T, atWar } from './lib';
const [me, seedS, mS = '4', mode = 'allies'] = process.argv.slice(2);
const X = T as any;
const plan: any = { idle: true, each: (g: any) => {
  const n = g.nations[me];
  if (g.turn === 1) console.log(`path ${mode}: ${X.orderPath(g, mode)}`);
  const foe = mode === 'axis' ? 'UK' : 'GER';
  if (g.turn >= 2 && !atWar(g, me, foe)) console.log(`${date(g.turn)}: declare on ${foe}: offered ${X.declarable(g).includes(foe)}, ordered ${X.orderDeclare(g, foe)}`);
  if (g.turn !== Number(mS)) return;
  console.log(`== ${me} ${date(g.turn)}: at war with ${n.atWar.join(' ')}; sealift ${n.sealift}; lift left ${JSON.stringify(X.liftOf?.(g) ?? null).slice(0, 120)}`);
  for (const c of g.corps.filter((c: any) => c.nat === me && !c.dead && c.kind !== 'fort')) {
    const r = g.regions[c.reg];
    const ts = X.shipTargets(g, c) as any[];
    const by: Record<string, string[]> = {};
    for (const t of ts) (by[t.ctrl] ??= []).push(t.name);
    console.log(`CORPS ${c.name} at ${r.name}${r.port ? ' (port)' : ' (no port)'} moves ${c.ap}/${c.apMax}: ${ts.length} ports offered: ${Object.entries(by).map(([k, v]) => `${k} ${v.length}${k !== me ? ` (${v.slice(0, 4).join(', ')})` : ''}`).join('; ') || 'none'}`);
    const ports = g.regions.filter((x: any) => x.port && x.ctrl === (mode === 'axis' ? 'GER' : 'UK'));
    const home = ports.filter((x: any) => x.owner === x.ctrl).slice(0, 2), far = ports.filter((x: any) => x.owner !== x.ctrl || x.overseas).slice(0, 3);
    if (r.port) for (const p of [...home, ...far]) console.log(`   to ${p.name}: ${X.whyNotShipTo?.(g, c, p) ?? (ts.includes(p) ? 'offered' : 'not offered (no reason function on the page)')}`);
    const lt = X.landTargets(g, [c]) as any[];
    console.log(`   enemy coasts in landing reach from here: ${lt.map((t: any) => `${t.name} (${t.ctrl})`).slice(0, 8).join(', ') || 'none'}`);
  }
} };
campaign(me, 1, Number(seedS), plan, { until: Number(mS) });

// Where a save differs after save, load, save again. usage: probe14 ME FROM SEED LEVEL TURN PLAN-JSON
import { campaign, date, saveGame, loadGame } from './lib';
const [me, fromS, seedS, level, turnS, planS] = process.argv.slice(2);
const plan = JSON.parse(planS);
let done = false;
campaign(me, Number(fromS), Number(seedS), { ...plan, each: (g: any) => {
  if (g.turn !== Number(turnS) || done) return;
  done = true;
  const text = saveGame(g), back = loadGame(text)!, again = saveGame(back);
  const a = JSON.parse(text), b = JSON.parse(again);
  console.log(date(g.turn), 'same save:', text === again, 'dirty before', a.dirty, 'after', b.dirty);
  const keys = Object.keys(a).filter((k) => JSON.stringify(a[k]) !== JSON.stringify(b[k]));
  console.log('  differing keys:', keys.join(', ') || 'none', '| key order same:', JSON.stringify(Object.keys(a)) === JSON.stringify(Object.keys(b)));
  for (const k of keys) if (k !== 'supply') console.log(`  ${k}: ${JSON.stringify(a[k]).slice(0, 300)}  THEN  ${JSON.stringify(b[k]).slice(0, 300)}`);
  console.log('  supply keys:', Object.keys(a.supply ?? {}).join(','), 'THEN', Object.keys(b.supply ?? {}).join(','));
  for (const k of new Set([...Object.keys(a.supply ?? {}), ...Object.keys(b.supply ?? {})])) {
    const x: string = a.supply?.[k] ?? '', y: string = b.supply?.[k] ?? '';
    if (x === y) continue;
    const at: string[] = [];
    for (let i = 0; i < Math.max(x.length, y.length); i++) if (x[i] !== y[i]) at.push(`${g.regions[i]?.name ?? i} (held by ${g.regions[i]?.ctrl}, ${g.regions[i]?.units.filter((u: any) => !u.dead).map((u: any) => u.nat).join('/') || 'empty'}${g.regions[i]?.garr?.length ? ', garrison ' + g.regions[i].garr.map((u: any) => u.nat).join('/') : ''}): ${x[i] ?? '-'} then ${y[i] ?? '-'}`);
    console.log(`  supply of ${k}: length ${x.length} then ${y.length}; differs at ${at.length}: ${at.slice(0, 12).join('; ')}`);
  }
} } as any, { until: Number(turnS), level } as any);

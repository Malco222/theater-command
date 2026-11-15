// Version 74: where the player's army can sail from where it stands, read from a save. usage: probe64 SAVE.json
// For every port where a corps of the player's stands: the friendly ports it can be shipped to (by country), the enemy
// coasts it can land on, and what the landing card would say of the three nearest (loss at sea, the sea behind the beach).
import { readFileSync } from 'node:fs';
import { T, loadGame, date } from './lib';
const [file] = process.argv.slice(2);
const raw = readFileSync(file, 'utf8');
const g = loadGame(raw.startsWith('"') ? JSON.parse(raw) : raw) as any;
const X = T as any;
const me: string = g.human; const n = g.nations[me];
console.log(`PORTS ${me} ${date(g.turn)}: at war with ${n.atWar.join(', ') || 'nobody'}; sea lift ${n.sealift}; ${JSON.stringify(X.liftView?.(g) ?? {})}`);
const ports = new Map<number, any[]>();
for (const c of g.corps) if (c.nat === me && g.regions[c.reg].port) (ports.get(c.reg) ?? ports.set(c.reg, []).get(c.reg)!).push(c);
if (!ports.size) console.log('  no corps of the player stands in a port');
for (const [reg, cs] of ports) {
  const r = g.regions[reg];
  const c = cs.find((u: any) => T.canOrder(g, u)) ?? cs[0];
  let ship: any[] = []; try { ship = T.shipTargets(g, c); } catch (e) { console.log(`  shipTargets threw: ${(e as Error).message}`); }
  const by: Record<string, string[]> = {};
  for (const p of ship) (by[`${p.owner}${p.ctrl !== p.owner ? ` held by ${p.ctrl}` : ''}`] ??= []).push(p.name);
  console.log(`  ${r.name} (${cs.length} corps, ${c.ap} of ${c.apMax} moves): can ship to ${ship.length} ports: ${Object.entries(by).map(([k, v]) => `${k}: ${v.slice(0, 8).join(', ')}${v.length > 8 ? ` and ${v.length - 8} more` : ''}`).join(' | ') || 'none'}`);
  let lands: any[] = []; try { lands = T.landTargets(g, [c]); } catch (e) { console.log(`  landTargets threw: ${(e as Error).message}`); }
  console.log(`    can land on ${lands.length} enemy coasts: ${lands.slice(0, 14).map((p: any) => `${p.name} (${p.ctrl})`).join(', ')}`);
  for (const p of lands.slice(0, 3)) { try { const pl = T.previewLanding(g, [c], p.id) as any; console.log(`    landing card for ${p.name}: ${pl.why ? `refused: ${pl.why}` : `lost at sea ${Math.round((pl.loss ?? pl.risk ?? 0) * 100)}%, the sea behind ${pl.hold ? (pl.hold.open ? 'stays open' : 'can be closed') : '?'}${pl.hold ? ` (enemy ${Math.round(pl.hold.foe)}, own ${Math.round(pl.hold.own?.total ?? 0)}, need ${Math.round(pl.hold.need)}; kept ${JSON.stringify(pl.hold.kept ?? null).slice(0, 160)}; shy ${JSON.stringify(pl.hold.shy ?? null).slice(0, 160)})` : ''}`}`); } catch (e) { console.log(`    previewLanding threw: ${(e as Error).message}`); } }
}
try { const cr = X.crossings?.(g) ?? []; console.log(`  crossings on the navy desk: ${cr.length}: ${cr.slice(0, 6).map((x: any) => `${x.from?.name ?? g.regions[x.from]?.name} > ${x.to?.name ?? g.regions[x.to]?.name}${x.bar ? " [barred: " + x.barred + "]" : ""} (${Math.round((x.account?.loss ?? x.loss ?? 0) * 100)}% lost; the sea behind ${x.account?.hold ? (x.account.hold.open ? 'stays open' : 'can be closed') : '?'})`).join('; ')}`); } catch (e) { console.log(`  crossings threw: ${(e as Error).message}`); }

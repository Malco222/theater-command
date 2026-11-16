// Version 79: when is Germany's home ground first entered (the rules' own test for the lapse of France's date:
// an enemy holds a German home province or stands on one), by whom, and for how many months before May 1944.
// usage: [TC_DIAL=3] probe82 SEEDS(a,b,c) [UNTIL=70]     One ENTERED line a war.
import { start, date, T, atWar } from './lib';
const [seedsS = '17', untilS = '70'] = process.argv.slice(2);
for (const seed of seedsS.split(',').map(Number)) {
  const g: any = start(null, 1, seed, 'colonel', Number(process.env.TC_DIAL ?? 3));
  const first: Record<string, number> = {}; let any = 0, months = 0, east = 0, gerFell = 0, lastQuiet = 0;
  while (g.turn <= Number(untilS) && g.nations.GER.alive) {
    const t = g.turn; const who = new Set<string>();
    for (const r of g.regions) { if (r.owner !== 'GER' || r.overseas) continue; if (atWar(g, 'GER', r.ctrl)) who.add(r.ctrl); for (const u of r.units ?? []) if (!u.dead && atWar(g, 'GER', u.nat)) who.add(u.nat); }
    for (const w of who) if (!first[w]) first[w] = t;
    if (who.size) { if (!any) any = t; if (t < 57) months++; } else lastQuiet = t;
    if (!east && atWar(g, 'GER', 'SOV')) east = t;
    (T as any).runTurnAI(g);
    if (!g.nations.GER.alive) gerFell = g.turn;
  }
  console.log(`ENTERED seed ${seed}: the east opens ${east ? date(east) : 'never'}; Germany's home ground first entered ${any ? date(any) : 'never'}; by ${Object.entries(first).sort((a, b) => a[1] - b[1]).map(([k, v]) => `${k} ${date(v)}`).join(', ') || 'nobody'}; entered for good from ${date(lastQuiet + 1)}; months entered before May 1944: ${months}; Germany ${gerFell ? `fell ${date(gerFell)}` : 'stands'}`);
}

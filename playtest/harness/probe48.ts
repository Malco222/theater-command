// Version 65: what a probe by a husk of a corps does. usage: probe48 SAVE.json SAVE.txt
// Reads the probe row of the save's attack as it stands, then with the probing corps cut to a half and a quarter of its men.
import { readFileSync } from 'fs';
import { loadGame, T } from './lib';
const [file, note] = process.argv.slice(2);
const info = JSON.parse(readFileSync(note, 'utf8').split('\n')[0]);
for (const cut of [1, 0.5, 0.25, 0.1]) {
  const g: any = loadGame(readFileSync(file, 'utf8'));
  const r = g.regions[info.target];
  const cands = info.attackers.map((id: number) => g.corps.find((c: any) => c.id === id)).filter(Boolean);
  for (const c of g.corps) if (c.nat === g.human) c.str *= cut;
  const s = (T as any).softenOptions(g, cands, r);
  const p = s?.probe;
  console.log(`every corps of the player's at ${Math.round(cut * 100)}% of its men: the card names ${p?.name} (corps ${p?.corps}), weight ${p?.weight?.toFixed?.(2)}, works off ${p?.wear}, men it would lose ${p?.men}, full weight at ${p?.full} men, why not: ${p?.why}`);
}

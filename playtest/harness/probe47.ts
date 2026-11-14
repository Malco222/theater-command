// Version 65: does a corps that stands the month on empty fieldworks take them? usage: probe47 SAVE.json REGION-ID [MONTHS]
// Loads a browser save, prints the corps in the province and the works standing empty there, ends the player's month
// without an order, lets the computer play, and prints them again as the next month opens.
import { readFileSync } from 'node:fs';
import { loadGame, advance, T, date } from './lib';
const [file, regS, monthsS = '1', setS] = process.argv.slice(2);
const g: any = loadGame(readFileSync(file, 'utf8'));
const show = (label: string): void => {
  const r = g.regions[Number(regS)];
  const left = (T as any).fieldworksLeft ? (T as any).fieldworksLeft(g, r.id) : null;
  console.log(`${label} ${date(g.turn)}, ${r.name}: ${r.units.filter((u: any) => !u.dead).map((u: any) => `${u.name} ${u.kind} ${u.dug ? `works ${Math.round((u.dugFor ?? 1) * 10) / 10}` : 'no works'}${u.relief ? ' (relief)' : ''}`).join('; ')} | standing empty: ${left ? `${left.months} months for ${left.corps} corps` : 'none'}`);
};
if (setS !== undefined) g.flags[`fw:${regS}`] = Number(setS);
show('before');
for (let i = 0; i < Number(monthsS); i++) { T.endHuman(g); advance(g); show('after '); }

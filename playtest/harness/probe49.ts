// Version 65: every staff's character by seed. usage: probe49 SEEDS [ME]
import { start } from './lib';
import * as STAFF from '@tc/campaign/staff';
const [seedsS, me = 'SOV'] = process.argv.slice(2);
const [a, b] = seedsS.split('-').map(Number);
for (let s = a; s <= (b ?? a); s++) {
  const g: any = (start as any)(me, 1, s);
  console.log(`seed ${s}: ${['GER', 'ITA', 'SOV', 'UK', 'FRA', 'USA'].map((n) => `${n} ${(STAFF as any).characterOf(g, n)}`).join(', ')}`);
}

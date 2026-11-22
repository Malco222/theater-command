import { it } from 'vitest';
import { LandBattle } from '../src/battle/land/sim';
import { sampleTicket, withTeams } from '../src/battle/land';

const log = (s: string) => process.stderr.write(s + '\n');
const FOOT = new Set(['officer', 'rifle', 'mg', 'at', 'sapper', 'holder', 'fo', 'leader', 'loader', 'crew']);
it('teams', () => {
  const t = withTeams(sampleTicket('plains', 1.3, { seed: 4, nat: { att: 'GER', def: 'SOV' } }));
  const b = new LandBattle(t), d = b.drillOn({ sections: true, platoons: true });
  for (const blk of d.view() as any[]) {
    const u = b.unit(blk.id)!;
    if (!u.team) continue;
    const n = blk.men.reduce((s: number, m: any) => s + (FOOT.has(m.r) ? 1 : m.r === 'hmg' || m.r === 'mortar' ? 3 : 0), 0);
    log(`${u.side} ${u.nat} team ${u.team} men0 ${u.men0} fig0 ${u.figures0} str0 ${u.str0} drawn ${n} roles ${[...new Set(blk.men.map((m: any) => m.r))].join(',')}`);
  }
});

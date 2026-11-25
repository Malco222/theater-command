// Release 1.6: which machines each nation flies on a raid fought as an air battle, now that the modeller's planes are in
// (the Wellington, D.520, LeO 451, Pe-2 and the rebuilt SM.79, 23 Nov). The raid ticket names each nation's fighter
// and bomber from its designers' year (doctrine.ts fighterOf and bomberOf), and the 3D game draws them by those names
// (CampaignAircraft.cpp MainFighterOf and MainBomberOf). Only the drawing changes: the raid card never reads them, and
// the field's losses are drawn the same whatever the bombers are (sim.ts straggle: "the same draws whatever the engines").
import { describe, expect, it } from 'vitest';
import { raidTargets } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { airBattle } from '../src/battle/air/sim';
import { bomberGuns } from '../src/battle/air/guns';
import { airTicket, raidGroups } from '../src/battle/air/ticket';

/** The first raid a nation can fly on its foe in the given month (turn 1 is September 1939), as a ticket. */
function firstRaid(nat: string, foe: string, turn: number) {
  const g = startOpen(createGame(7, nat));
  setWar(g, nat, foe);
  g.turn = turn;
  for (const t of raidTargets(g, 'bomb', nat)) {
    const ids = raidGroups(g, 'bomb', t.id, nat);
    const ticket = ids.length ? airTicket(g, 'bomb', t.id, ids, nat).ticket : null;
    if (ticket) return { g, target: t.id, ids, ticket };
  }
  throw new Error(`no raid for ${nat} on ${foe} at turn ${turn}`);
}

describe('the machines each nation flies on a raid (release 1.6)', () => {
  const cases: [string, string, number, string, { ftr: string; bmr: string }][] = [
    ['the RAF, August 1940', 'UK', 12, 'GER', { ftr: 'Spitfire', bmr: 'Wellington' }],
    ['France, May 1940', 'FRA', 9, 'GER', { ftr: 'D520', bmr: 'LeO451' }],
    ['the Soviet Union, July 1941', 'SOV', 23, 'GER', { ftr: 'Fighter:SOV', bmr: 'Pe2' }],
    ['Italy, May 1942', 'ITA', 33, 'FRA', { ftr: 'Fighter:ITA', bmr: 'SM79' }],
  ];
  for (const [what, nat, turn, foe, want] of cases)
    it(what, () => {
      const { ticket } = firstRaid(nat, foe, turn);
      expect(ticket.place.models![nat]).toEqual(want);
    });

  it('the bombers drawn do not change what the field loses: an SM.79 raid fought as the plain Italian bomber loses the same', () => {
    const { ticket } = firstRaid('ITA', 'FRA', 10);
    const run = (plain: boolean) => {
      const tk = structuredClone(ticket);
      if (plain) { tk.place.models!.ITA.bmr = 'Bomber:ITA'; tk.place.guns!.ITA.bmr = bomberGuns('Bomber:ITA', 'ITA', tk.place.when!); }
      const s = airBattle(tk);
      while (!s.over) s.step();
      return { models: s.units.filter((u) => u.air === 'bmr').map((u) => u.model), lost: s.outcome().groups };
    };
    const sm79 = run(false), plain = run(true);
    expect(sm79.models.every((m) => m === 'SM79') && plain.models.every((m) => m === 'Bomber:ITA')).toBe(true);
    expect(sm79.lost).toEqual(plain.lost);
  });
});

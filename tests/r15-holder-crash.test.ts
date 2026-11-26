// Release 1.5: a cavalry block drawn as sections remounts after its section's key horse holder has fallen, and the
// drill must not throw. GER 1939 grand (start 1, seed 1): corps 2, 3, 8, 9 and 12 onto Poznan (region 38), the ticket's
// own seed, staffs both sides and the 1.5 card's rows at their start. The Polish cavalry block's key holder falls at
// 156.3 s while his section's holders still hold the horses; at 157.8 s its troopers ran for a horse and the drill threw
// (board's afoot(m, undefined): the men's side looked for the key holder alone). The design director's find through the
// general's benchmark, 23 November; mended by Drill.busOf (a standing holder of his number, the key man first).
import { describe, expect, it } from 'vitest';
import '../unreal/TheaterCommand/Rules/campaign-bridge';
import { liveTicket } from '../src/battle/land/live';

const TC = (globalThis as unknown as { TC: Record<string, (...a: unknown[]) => any> }).TC;
type Look = { ticket: Record<string, any> | null; live?: Record<string, any>; why?: string } | null;

const ROWS = { me: null, setup: false, platoons: true, staff: true, radio: true, hide: true, armour: true, names: true, relief: true, rider: true, cutters: true,
  smoke: false, bocage: false, phones: true, bearers: false, patrol: true };

describe('release 1.5: cavalry remounting after its key horse holder fell', () => {
  it('Poznan 1939 fights on past 157.8 s without a throw', () => {
    TC.start({ human: 'GER', from: 1, view: 'grand', seed: 1 });
    const look = TC.battleLook([2, 3, 8, 9, 12], 38) as Look;
    expect(look?.ticket).toBeTruthy();
    const L: any = liveTicket(structuredClone(look!.ticket) as never, { ...(look!.live ?? {}), ...ROWS } as never);
    // (the Polish cavalry is in it, drawn as sections: the case that threw)
    for (let guard = 0; !L.sim.over && L.sim.t < 170 && guard < 100000; guard++) L.tick(0.5, 1000);
    expect(L.sim.over || L.sim.t >= 170).toBeTruthy();
  }, 300_000);
});

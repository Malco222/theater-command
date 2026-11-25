// Japan's own tank and field gun in a campaign battle (release 2, looks only; the Japan and Pacific thread's finding of
// 25 November 2026: in a 3D ground battle Japan fought in the generic light tank and a German howitzer, because the
// stand-in picker had no Japanese model to reach for). With the switch on, a Japanese armoured block is drawn as the
// Type 97 medium tank and a Japanese battery as the 75 mm field gun, both of which the fight's own table already names
// for her (src/battle/land/shells.ts). Off, the stand-ins are what they were. The fight never reads a stand-in, so the
// battle is the same either way.
import { describe, expect, it } from 'vitest';
import { liveTicket, standInModel } from '../src/battle/land/live';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import type { FieldTicket } from '../src/battle/core/types';

/** A set piece's ticket with one side's army swapped for another's (a campaign battle: no piece is passed to live). */
function ticket(key: string, side: 'att' | 'def', nat: string, date?: string): FieldTicket<never> {
  const t = ticketOf(PIECES.find((x) => x.key === key)!, 11, 'att');
  const army = { ...t[side], nat, adj: nat, forces: t[side].forces.map((f) => ({ ...f, nat })) };
  return { ...t, [side]: army, ...(date ? { date } : {}) } as unknown as FieldTicket<never>;
}

interface Block { nat: string; kind: string; team?: string; model?: string | null; coat?: string }

function models(t: FieldTicket<never>, japan: boolean): Block[] {
  const L = liveTicket(t as never, { me: 'att', japan });
  return (L.header() as unknown as { blocks: Block[] }).blocks;
}

describe("Japan's own tank and field gun (looks only)", () => {
  it('names the Type 97 and the 75 mm gun for Japan, and nothing else, when it is on', () => {
    expect(standInModel('arm', 'JAP', 1942, 0, true)).toBe('Type97');
    expect(standInModel('art', 'JAP', 1942, 0, true)).toBe('Gun75');
    // her blocks before the Type 97 was built keep the light tank
    expect(standInModel('arm', 'JAP', 1937, 0, true)).toBe('LightTank');
    // every other army is drawn as before, switch or no switch
    for (const nat of ['GER', 'SOV', 'USA', 'UK', 'ITA', 'FIN']) {
      for (const kind of ['arm', 'art']) {
        expect(standInModel(kind, nat, 1943, 1, true)).toBe(standInModel(kind, nat, 1943, 1, false));
      }
    }
  });

  it('is off unless asked: Japan is drawn as she was', () => {
    expect(standInModel('arm', 'JAP', 1942, 0)).toBe('LightTank');
    expect(standInModel('art', 'JAP', 1942, 0)).toBe('leFH18');
  });

  it("draws a Japanese army's blocks with her own models in a campaign battle", () => {
    const t = ticket('normandy-1944', 'def', 'JAP');
    const off = models(t, false), on = models(t, true);
    const arm = (b: Block[]): Block[] => b.filter((x) => x.nat === 'JAP' && x.kind === 'arm');
    const art = (b: Block[]): Block[] => b.filter((x) => x.nat === 'JAP' && x.kind === 'art' && !x.team);
    expect(arm(off).length).toBeGreaterThan(0);
    expect(art(off).length).toBeGreaterThan(0);
    expect(new Set(arm(off).map((x) => x.model))).toEqual(new Set(['LightTank']));
    expect(new Set(art(off).map((x) => x.model))).toEqual(new Set(['leFH18']));
    expect(new Set(arm(on).map((x) => x.model))).toEqual(new Set(['Type97']));
    expect(new Set(art(on).map((x) => x.model))).toEqual(new Set(['Gun75']));
    // the other side is untouched
    const theirs = (b: Block[]): (string | null | undefined)[] => b.filter((x) => x.nat !== 'JAP').map((x) => x.model ?? null);
    expect(theirs(on)).toEqual(theirs(off));
  });

  it("marks every Japanese block for her own coat, and no one else's, only when it is on", () => {
    // (the gun, the lorry and the light tank are models she shares with other armies; the 3D game keeps a coat named
    // "japan" for them, and puts it on where a block says so)
    const t = ticket('normandy-1944', 'def', 'JAP');
    const off = models(t, false), on = models(t, true);
    expect(off.some((b) => b.coat !== undefined)).toBe(false);
    const hers = on.filter((b) => b.nat === 'JAP');
    expect(hers.length).toBeGreaterThan(0);
    expect(new Set(hers.map((b) => b.coat))).toEqual(new Set(['japan']));
    expect(on.filter((b) => b.nat !== 'JAP').some((b) => b.coat !== undefined)).toBe(false);
    // and nothing else about any block moves but its model and that word
    const rest = (b: Block[]): string => JSON.stringify(b.map((x) => ({ ...x, model: undefined, coat: undefined })));
    expect(rest(on)).toBe(rest(off));
  });

  it('is looks only: the battle fought is the same', () => {
    const t = ticket('normandy-1944', 'def', 'JAP');
    const fight = (japan: boolean): string => {
      const L = liveTicket(t as never, { me: 'att', japan });
      let over = '';
      // every step's whole delta, kept end to end: the men's places live in `at` and leaving the list in `out`, so a
      // hand-picked few fields would let a move through unseen (the pre-check, 25 November).
      for (let s = 10; s <= 60; s += 10) over += JSON.stringify(L.to(s, 400).men);
      return over;
    };
    expect(fight(true)).toBe(fight(false));
  });

  it('leaves a set piece as the historian drew it', () => {
    const t = ticketOf(PIECES.find((x) => x.key === 'normandy-1944')!, 11, 'att');
    const L = liveTicket(t as never, { me: 'att', japan: true }, 'normandy-1944');
    const blocks = (L.header() as unknown as { blocks: Block[] }).blocks;
    expect(blocks.some((b) => b.model === 'Type97' || b.model === 'Gun75')).toBe(false);
    expect(blocks.some((b) => b.coat !== undefined)).toBe(false);
  });
});

// Island ground in a campaign battle (release 2, looks only; "a card, then an island battle", the Japan and Pacific
// thread's batch 3 of 25 November 2026). The campaign dresses a ticket with the 3D look its ground wants ("island" or
// "jungle") and the place's own latitude and longitude; with the land battle's "island" option on, the header hands
// them to the 3D game as field.look and sky.latLon. Off (the default), or on a ticket that carries neither, the header
// is byte for byte what it was. The fight never reads either key, so the battle is the same either way.
import { describe, expect, it } from 'vitest';
import { liveTicket } from '../src/battle/land/live';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import type { FieldTicket } from '../src/battle/core/types';

type Ticket = FieldTicket<never>;
type Header = { sky: { latLon?: [number, number]; hour?: number }; field: { look?: string } };

/** A set piece's ticket fought as a campaign battle (no piece is passed to live), with the campaign's dress added. */
function ticket(dress: { look?: string; latLon?: [number, number] } = {}): Ticket {
  const t = ticketOf(PIECES.find((x) => x.key === 'normandy-1944')!, 11, 'att');
  return { ...t, ...dress } as unknown as Ticket;
}

function header(t: Ticket, opts: Record<string, unknown> = {}): Header {
  return liveTicket(t as never, { me: 'att', ...opts }).header() as unknown as Header;
}

const TARAWA: [number, number] = [1.35, 172.93];

describe('island ground (looks only)', () => {
  it('is off unless asked: a dressed ticket gives the header an undressed one gives, byte for byte', () => {
    const plain = JSON.stringify(header(ticket()));
    const dressed = ticket({ look: 'island', latLon: TARAWA });
    expect(JSON.stringify(header(dressed))).toBe(plain);
    expect(JSON.stringify(header(dressed, { island: false }))).toBe(plain);
    // (anything but true is off: the bridge passes island: o.island === true)
    expect(JSON.stringify(header(dressed, { island: undefined }))).toBe(plain);
  });

  it('on, hands the look and the place to the 3D game and changes nothing else', () => {
    const h = header(ticket({ look: 'island', latLon: TARAWA }), { island: true });
    expect(h.field.look).toBe('island');
    expect(h.sky.latLon).toEqual(TARAWA);
    const bare = JSON.parse(JSON.stringify(h)) as Header;
    delete bare.field.look;
    delete bare.sky.latLon;
    expect(JSON.stringify(bare)).toBe(JSON.stringify(header(ticket())));
  });

  it('passes "jungle" as it passes "island", and a place with no look word keeps its place', () => {
    expect(header(ticket({ look: 'jungle', latLon: [-9.43, 160.05] }), { island: true }).field.look).toBe('jungle');
    const attu = header(ticket({ latLon: [52.9, 172.9] }), { island: true });
    expect(attu.field.look).toBeUndefined();
    expect(attu.sky.latLon).toEqual([52.9, 172.9]);
  });

  it('on, a ticket the campaign has not dressed is as it was', () => {
    expect(JSON.stringify(header(ticket(), { island: true }))).toBe(JSON.stringify(header(ticket())));
  });

  it('keeps a sky given outright with a place of its own', () => {
    const sky = { cover: 0.3, base: 1200, kind: 'cumulus', latLon: [21.47, -158.06], hour: 9 };
    const h = header(ticket({ look: 'island', latLon: TARAWA }), { island: true, sky });
    expect(h.sky.latLon).toEqual([21.47, -158.06]);
    expect(h.field.look).toBe('island');
  });

  it('leaves a set piece as the historian drew it', () => {
    const t = ticket({ look: 'island', latLon: TARAWA });
    const piece = (island: boolean): string => JSON.stringify(liveTicket(t as never, { me: 'att', island }, 'normandy-1944').header());
    expect(piece(true)).toBe(piece(false));
    expect((JSON.parse(piece(true)) as Header).field.look).toBeUndefined();
  });

  it('is looks only: the battle fought is the same', () => {
    const t = ticket({ look: 'island', latLon: TARAWA });
    const fight = (island: boolean): string => {
      const L = liveTicket(t as never, { me: 'att', island });
      let over = '';
      for (let s = 10; s <= 40; s += 10) over += JSON.stringify(L.to(s, 400).men);
      return over;
    };
    expect(fight(true)).toBe(fight(false));
  });
});

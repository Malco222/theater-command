// The far side's airfields on the record (far-fields.json; ruling 234, the design director's notes of 04:47Z and
// 04:58Z of 21 November 2026 on rider b645 (2); the historian's far-chart-airfields.md). The record's war in the south
// was a chain of airfields, each hop made to take a field for the next. The chart seeded fields only at capitals,
// factory towns and the bases of air rows, so Japan's fighters had nowhere to stand in Malaya or the Indies and the
// sky over Singapore and Palembang stayed the defender's (seed 22 at stop 3: Japan 0 to 40 aircraft in reach of
// Palembang against about 150, every month to the sixth after the strike). The table is read behind the far switch
// alone, whatever the air campaigns' switch says, and only ever raises a field.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { AIRWAR, airfieldsHold, fieldClass, hasAirfield } from '../src/campaign/air';
import { acOn, farFieldRows, recordField } from '../src/campaign/aircampaigns';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import airCampaigns from '../data/campaign/aircampaigns.json';
import farFields from '../data/campaign/far-fields.json';

const ROWS = (farFields as unknown as { rows: { region: string; from: number; class: number; why: string }[] }).rows;
const of = (region: string): [number, number][] => ROWS.filter((f) => f.region === region).map((f) => [f.from, f.class]);

describe('the far side\'s airfields on the record', () => {
  const g = startOpen(createGame(7, null));

  it('every row names a province of the chart, a class the air war knows, a month and its source; the draft rows are gone', () => {
    expect((farFields as any)._draft).toBeUndefined();
    expect(ROWS.length).toBeGreaterThan(80);
    expect(farFieldRows()).toHaveLength(ROWS.length);
    for (const f of ROWS) {
      expect(g.byName[f.region], f.region).toBeDefined();
      expect([1, 2, 3], f.region).toContain(f.class);
      expect(f.from, f.region).toBeGreaterThanOrEqual(1);
      expect(f.why.length, f.region).toBeGreaterThan(5);
    }
  });

  it('a row only ever raises a field: above the chart\'s own first-day field, and above the province\'s row before it', () => {
    const fresh = startOpen(createGame(7, null));
    for (const region of new Set(ROWS.map((f) => f.region))) {
      const mine = of(region);
      const seeded = fresh.flags[`af:${fresh.byName[region].id}`] ?? 0;
      expect(mine[0][1], region).toBeGreaterThan(seeded);
      for (let i = 1; i < mine.length; i++) {
        expect(mine[i][0], region).toBeGreaterThan(mine[i - 1][0]);
        expect(mine[i][1], region).toBeGreaterThan(mine[i - 1][1]);
      }
    }
  });

  it("the historian's rows: Malaya's and the Indies' chain, the fields that rose, and the fields built in the war", () => {
    expect(of('Singora')).toEqual([[1, 1], [28, 2]]);
    for (const nm of ['Kota Bharu', 'Penang', 'Kuantan', 'Palembang', 'Ambon', 'Makassar', 'Toungoo', 'Akyab', 'Hainan', 'Hanoi']) expect(of(nm), nm).toEqual([[1, 2]]);
    for (const nm of ['Ipoh', 'Kuala Lumpur', 'Johore', 'Mergui', 'Moulmein', 'Mandalay', 'Lashio', 'Kuching', 'Jolo', 'Menado', 'Tarakan', 'Koepang', 'Medan', 'Legaspi', 'Aparri']) expect(of(nm), nm).toEqual([[1, 1]]);
    expect([of('Myitkyina'), of('Davao'), of('Lae'), of('Truk')]).toEqual([[[1, 1], [57, 2]], [[1, 1], [58, 3]], [[1, 1], [49, 3]], [[1, 1], [28, 2], [49, 3]]]);
    expect([of('Palau'), of('Cam Ranh'), of('Hong Kong')]).toEqual([[[28, 2]], [[23, 1]], [[41, 2]]]);
    expect([of('Guadalcanal'), of('New Georgia'), of('Milne Bay'), of('Buna')]).toEqual([[[36, 1], [39, 2], [44, 3]], [[40, 1]], [[35, 1]], [[35, 1], [40, 2], [46, 3]]]);
    // Read from the map: the bombers' field of Balikpapan's row lies in Samarinda's province, Malang's in Soerabaja's.
    expect([of('Balikpapan'), of('Samarinda'), of('Soerabaja'), of('Semarang'), of('Miri'), of('Brunei')]).toEqual([[[1, 1]], [[1, 2]], [[1, 2], [29, 3]], [[1, 2]], [[28, 1]], [[41, 1]]]);
    // (The historian's check of the laying, section 6: Singosari was a Dutch medium-bomber base until the B-17s came; Majuro and Yunnanyi laid in the provinces that hold them.)
    expect([of('Eastern Marshalls'), of('Tali')]).toEqual([[[25, 1], [54, 2]], [[33, 2]]]);
    // (Chihkiang, the historian's section 7: laid at Changteh, at the mouth of the valley it lies up.)
    expect(of('Changteh')).toEqual([[50, 2]]);
  });

  it('the three corrections that would make a first-day field smaller are not laid: the table is read as at least', () => {
    for (const nm of ['Del Monte', 'Rabaul', 'Port Moresby', 'Lingayen']) {
      expect(of(nm), nm).toEqual([]);
      expect(fieldClass(g, g.byName[nm].id), nm).toBe(3);
    }
  });

  it('a province of the table has its field from its month, with the air campaigns\' switch off', () => {
    expect(acOn(g)).toBe(false);
    for (const f of ROWS) {
      const r = g.byName[f.region];
      const was = g.turn;
      g.turn = Math.max(f.from, was);
      expect(recordField(g, r.id), f.region).toBeGreaterThanOrEqual(f.class);
      expect(fieldClass(g, r.id), f.region).toBeGreaterThanOrEqual(f.class);
      expect(hasAirfield(g, r.id), f.region).toBe(true);
      expect(airfieldsHold(g, r.id), f.region).toBeGreaterThanOrEqual(AIRWAR.fields.sizes[f.class]);
      // (A field opened in the war is not there before its month.)
      if (f.from > 1 && !ROWS.some((x) => x.region === f.region && x.from < f.from)) { g.turn = f.from - 1; expect(recordField(g, r.id), f.region).toBe(0); }
      g.turn = was;
    }
  });

  it('the field is the ground\'s, not a flag\'s: whoever holds the province has it', () => {
    const f = ROWS[0], r = g.byName[f.region];
    const ctrl = r.ctrl;
    r.ctrl = 'JAP';
    expect(fieldClass(g, r.id)).toBeGreaterThanOrEqual(f.class);
    r.ctrl = ctrl;
  });

  it('the Mediterranean\'s record fields stay behind the air campaigns\' switch (rider b574)', () => {
    const med = (airCampaigns as unknown as { fields: { rows: { region: string; class: number }[] } }).fields.rows;
    for (const f of med) {
      const r = g.byName[f.region];
      if (r) expect(recordField(g, r.id), f.region).toBe(0);
    }
  });
});

// With the far side's switch off the far side's airfield table is not read (far-fields.json): the game is the game
// without it, and a record field is the air campaigns' alone (rider b574).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 0; });

import { describe, expect, it } from 'vitest';
import { farFieldRows, recordField } from '../src/campaign/aircampaigns';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { FAR_STAGE } from '../src/campaign/stage';

describe('the far side\'s airfields with the far switch off', () => {
  it('no row is read and no province has a record field', () => {
    expect(FAR_STAGE).toBe(false);
    expect(farFieldRows()).toHaveLength(0);
    const g = startOpen(createGame(7, null));
    for (const r of g.regions) expect(recordField(g, r.id), r.name).toBe(0);
  });
});

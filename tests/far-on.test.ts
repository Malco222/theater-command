// The far side with its switch on.
//
// The switch (data/campaign/stage.json, 'far') is off in the game as it ships, and the seats' tests run with it off:
// they hold the rows as they wait. This file turns the switch on for itself and runs the same files again, so that
// their second halves run too: each seat opens on the whole world's chart, and the six tests of a client's tie hold
// (far.ts). It then holds what is nobody's seat: Japan, the old powers' garrisons, the fleets, and the rows by date.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import './far-seats-pacific.test';
import './far-seats-globe.test';

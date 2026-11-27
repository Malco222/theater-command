// The Options row and the switch for 'portStores' (release 2; the UI director's words of 26 November 2026, 18:46Z,
// ui-director/port-stores-shut-sea-UI-MARKS.md section 4, after the design director's ruling 5 of 18:44Z: "Its own row
// and switch, words only, working with true reach on or off. Existing lines stay unchanged."). The setter
// `setPortStores` (portcut.ts), the bridge's act('portstores:on' / ':off') and TC.portStores(on), the view's
// `portStores`, the ship list's 'portstores', and the 3D game's own Options row (CampaignPlayerController.cpp).
//
// Words only, and off until its checks: a war with the switch off is the save of the tree without it, byte for byte, on
// two seeds for 36 months (the hashes below were taken on the tree before portcut.ts, at 2a7564b7b); and a war with it
// on is the same war, its own words and marks aside.
import { describe, expect, it } from 'vitest';
import CPP from '../unreal/TheaterCommand/Source/TheaterCommand/Campaign/CampaignPlayerController.cpp?raw';
import VIEW_H from '../unreal/TheaterCommand/Source/TheaterCommand/Campaign/CampaignView.h?raw';
import SESSION from '../unreal/TheaterCommand/Source/TheaterCommand/Campaign/CampaignSession.cpp?raw';
import stage from '../data/campaign/stage.json';
import { createGame } from '../src/campaign/game';
import { PORTSTORES_ROW, portStoresOn, setPortStores } from '../src/campaign/portcut';
import { loadGame, saveGame } from '../src/campaign/save';
import { PORTSTORES_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const grand = (seed = 3, human: string | null = 'UK'): Game => startOpen(createGame(seed, human));
const fromSave = (): Game => loadGame((TC().save() as { text: string }).text)!;
const OURS = /^(portStores|portTold:|portSpent:|seaShut:|portLost:|portHad:|portCrew:)/;

async function sha256(text: string): Promise<string> {
  const d = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(text));
  return [...new Uint8Array(d)].map((b) => b.toString(16).padStart(2, '0')).join('');
}

const NAME = 'Ports cut off by sea tell you of their stores';
const LINE = "When the sea round one of your ports is shut, its news, its card and the cards of the corps there say whose ships or aircraft shut it, the month its stores run out, the month its garrison would be lost, and what opens the sea again. The side that shuts it sees on the port's card how long it has held that sea. Off: none of this is said, as before. Words only: no result changes.";

describe('the row', () => {
  it("the Options row's key, name and whole line are the UI director's section 4, in the browser's rules and the 3D game's row alike", () => {
    expect(PORTSTORES_ROW).toEqual({ id: 'portstores', name: NAME, line: LINE });
    // (The 3D game's own row: the same key, name and line, its state read from the view's portStores.)
    const esc = (t: string): string => t.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    expect(CPP).toMatch(new RegExp(`Add\\(TEXT\\("portstores"\\), TEXT\\("${esc(NAME)}"\\), TEXT\\("${esc(LINE)}"\\),\\s*Now\\.bPortStores \\? TEXT\\("ON"\\) : TEXT\\("OFF"\\), Now\\.bPortStores\\);`));
    expect(CPP).toContain('|| Id == TEXT("portstores") ||');
    expect(CPP).toContain('Id == TEXT("portstores") ? Now.bPortStores :');
    expect(VIEW_H).toContain('bool bPortStores = false;');
    expect(SESSION).toContain('TryGetBoolField(TEXT("portStores"), Out.bPortStores)');
  });

  it('off until its checks: stage.json leaves it off, and a new grand campaign opens without the flag', () => {
    expect((stage as Record<string, unknown>).portStores).toBe(false);
    expect(PORTSTORES_STAGE).toBe(false);
    const g = grand();
    expect(g.flags.portStores).toBeUndefined();
    expect(portStoresOn(g)).toBe(false);
  });
});

describe('the switch through the bridge', () => {
  it('on and off in a running grand campaign, by act() and by TC.portStores()', () => {
    TC().load(saveGame(grand()));
    expect((TC().view() as { portStores: boolean }).portStores).toBe(false);
    const on = TC().act('portstores:on') as { ok: boolean; note: string; view: { portStores: boolean } };
    expect([on.ok, on.note, on.view.portStores]).toEqual([true, '', true]);
    expect(fromSave().flags.portStores).toBe(1);
    const off = TC().act('portstores:off') as { ok: boolean; note: string; view: { portStores: boolean } };
    expect([off.ok, off.note, off.view.portStores]).toEqual([true, '', false]);
    expect(fromSave().flags.portStores).toBeUndefined();
    expect(TC().portStores(true)).toEqual({ set: true, on: true });
    expect((TC().view() as { portStores: boolean }).portStores).toBe(true);
    expect(TC().portStores(false)).toEqual({ set: true, on: false });
    expect((TC().view() as { portStores: boolean }).portStores).toBe(false);
  });

  it('refused outside a grand campaign', () => {
    const h = createGame(3, 'UK');
    expect(setPortStores(h, true)).toBe(false);
    expect(h.flags.portStores).toBeUndefined();
    TC().load(saveGame(createGame(3, 'UK')));
    const on = TC().act('portstores:on') as { ok: boolean; view: { portStores: boolean } };
    expect([on.ok, on.view.portStores]).toEqual([false, false]);
    expect(TC().portStores(true)).toEqual({ set: false, on: false });
    expect(fromSave().flags.portStores).toBeUndefined();
    expect(TC().r2Ship(['portstores'])).toBeNull();
  });

  it('the ship list takes it once; a later off stays off', () => {
    TC().load(saveGame(grand()));
    expect(TC().r2Ship(['portstores'])).not.toBeNull();
    expect((TC().view() as { portStores: boolean }).portStores).toBe(true);
    expect((TC().act('portstores:off') as { ok: boolean }).ok).toBe(true);
    expect(TC().r2Ship(['portstores'])).toBeNull();
    expect((TC().view() as { portStores: boolean }).portStores).toBe(false);
    const back = fromSave();
    expect(back.flags['r2:portstores']).toBe(1);
    expect(back.flags.portStores).toBeUndefined();
  });

  it('with the switch never touched, the save holds no key of it, through the bridge and back', () => {
    const g = grand();
    runTurnAI(g);
    const text = saveGame(g);
    expect(Object.keys(g.flags).filter((k) => OURS.test(k))).toEqual([]);
    TC().load(text);
    TC().view();
    const back = fromSave();
    expect(Object.keys(back.flags).filter((k) => OURS.test(k))).toEqual([]);
    // (The province and corps of the view carry no field of it either.)
    const v = TC().view() as { corps: Record<string, unknown>[] };
    expect(v.corps.some((c) => 'portLine' in c)).toBe(false);
    const malta = (TC().province as Call)(361) as Record<string, unknown>;
    expect('portCut' in malta).toBe(false);
  });

  it('on and off at once leaves the save of a war never switched on', () => {
    const untouched = grand(), quick = grand();
    expect(setPortStores(quick, true) && setPortStores(quick, false)).toBe(true);
    for (let t = 0; t < 3; t++) { runTurnAI(untouched); runTurnAI(quick); }
    expect(saveGame(quick)).toBe(saveGame(untouched));
  }, 600000);
});

// The tree without the change (2a7564b7b): each war played 36 months by the computer for every side but the seat's
// orders, `startOpen(createGame(seed, 'UK'))`, 36 runTurnAI, its whole save hashed.
const BASE = [
  { name: 'seed 3, Units decide and true reach', seed: 3, reach: true, turn: 37, length: 1510516, sha: '1362e6719b35c91f92883bfdd80a1c0a595728814fa464e7e269bd00466e1412' },
  { name: 'seed 1, the flags as the campaign opens them', seed: 1, reach: false, turn: 37, length: 1529908, sha: '22b8026cdb7dfbeacc09f2941abe3290b77d2979d906fb00d7c513653c0b3352' },
];
function war(seed: number, reach: boolean, on: boolean): Game {
  const g = grand(seed, 'UK');
  if (reach) { g.flags.unitsDecide = 1; g.flags.trueReach = 1; g.supplyDirty = true; }
  if (on) expect(setPortStores(g, true)).toBe(true);
  for (let t = 0; t < 36; t++) runTurnAI(g);
  return g;
}

describe('switched off, the war is the war without it', () => {
  for (const b of BASE) {
    it(`${b.name}: 36 months, the whole save byte-equal to the tree without the change`, async () => {
      const g = war(b.seed, b.reach, false);
      const text = saveGame(g);
      expect([g.turn, text.length]).toEqual([b.turn, b.length]);
      expect(text).not.toMatch(/portStores|portTold|portSpent|seaShut|portLost|portHad|portCrew/);
      expect(await sha256(text)).toBe(b.sha);
    }, 600000);
  }
});

describe('switched on, words only', () => {
  it('seed 3, Units decide and true reach, 36 months: the same war, its own words and marks aside; and Malta was told of', () => {
    const off = war(3, true, false), on = war(3, true, true);
    const told = on.headlines.filter((h) => /^(No ship reaches |Ships reach |Cut off (on|in) .*(has|have) starved and (is|are) lost)|'s stores are spent and no ship has got through/.test(h.text));
    expect(told.length).toBeGreaterThan(0);
    // (Its words are news, and the week's paper reads the news: the paper's own flag goes with them.)
    const strip = (g: Game): string => {
      const h = structuredClone(g) as Game;
      h.headlines = h.headlines.filter((x) => !told.some((t) => t.text === x.text && t.turn === x.turn));
      for (const k of Object.keys(h.flags)) if (OURS.test(k) || k === 'newsweek') delete h.flags[k];
      return saveGame(h);
    };
    expect(strip(on)).toBe(strip(off));
  }, 600000);
});

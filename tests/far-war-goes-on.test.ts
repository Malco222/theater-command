// Ruling 201 on the whole-world chart (release 2, Japan and the Pacific, 24 November 2026). The owner chose on
// 17 November that the war counts as won "when the whole world war is won": the test reads seven great powers,
// Europe's five with America and Japan, so Germany's fall while Japan fights on decides nothing. And the two lines
// the UI director drew say so: the news line on the fall's band, and the line at the head of the Powers tab. Ruled
// and never built: the playtester's Japan wars read "war decided February 1945" with Japan still standing.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { warDecided, warGoesOn, whyNotDomination } from '../src/campaign/domination';
import { capitulate, createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, lastAct, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

function war(g: Game, a: string, b: string): void {
  if (!g.nations[a].atWar.includes(b)) g.nations[a].atWar.push(b);
  if (!g.nations[b].atWar.includes(a)) g.nations[b].atWar.push(a);
}
function peace(g: Game, a: string, b: string): void {
  g.nations[a].atWar = g.nations[a].atWar.filter((x) => x !== b);
  g.nations[b].atWar = g.nations[b].atWar.filter((x) => x !== a);
}
/** A grand campaign on the whole world in the player's first month, with the great powers' wars set by hand. */
function playing(human: string, wars: [string, string][]): Game {
  const g = startOpen(createGame(3, human));
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
  const great = ['GER', 'ITA', 'UK', 'FRA', 'SOV', 'USA', 'JAP'];
  for (const a of great) for (const b of great) if (a < b) peace(g, a, b);
  for (const [a, b] of wars) war(g, a, b);
  return g;
}
const last = (g: Game, nat: string): string => [...g.headlines].reverse().find((h) => h.kind === 'fall' && h.nat === nat)!.text;

describe('the war goes on in the Pacific', () => {
  it("Germany's fall decides nothing while America and Britain still fight Japan", () => {
    const g = playing('USA', [['GER', 'USA'], ['GER', 'UK'], ['JAP', 'USA'], ['JAP', 'UK']]);
    expect(warGoesOn(g)).toBeNull(); // (no great power has fallen yet)
    capitulate(g, 'GER', 'USA');
    expect(warDecided(g)).toBeNull();
    expect(last(g, 'GER')).toMatch(/ Germany is beaten\. The war goes on in the Pacific: America, Britain and Japan are still at war\.$/);
    const line = 'The war between the alliances is not decided yet: America, Britain and Japan are still at war. It is decided once no two great powers are at war with each other.';
    expect(warGoesOn(g)).toMatchObject({ where: 'the Pacific', powers: expect.arrayContaining(['USA', 'UK', 'JAP']), line });
    expect(whyNotDomination(g)).toBe(line);
    expect(lastAct(g)).toMatchObject({ open: false, why: line, decided: null });
    // The month Japan falls the war is decided, and its fall says nothing of a war going on.
    capitulate(g, 'JAP', 'USA');
    expect(warDecided(g)).toMatchObject({ beaten: ['axis'], victor: 'allies', fallen: ['GER', 'JAP'] });
    expect(last(g, 'JAP')).not.toMatch(/war goes on/);
    expect(warGoesOn(g)).toBeNull();
    expect(whyNotDomination(g)).toBeNull();
  }, 300000);

  it('names Europe when Japan falls first, and both when both wars stand', () => {
    const g = playing('USA', [['GER', 'UK'], ['GER', 'SOV'], ['JAP', 'USA']]);
    capitulate(g, 'JAP', 'USA');
    expect(warDecided(g)).toBeNull();
    expect(last(g, 'JAP')).toMatch(/ Japan is beaten\. The war goes on in Europe: Britain, Germany and the Soviet Union are still at war\.$/);
    const both = playing('UK', [['GER', 'UK'], ['GER', 'SOV'], ['JAP', 'USA'], ['ITA', 'UK']]);
    capitulate(both, 'ITA', 'UK');
    expect(last(both, 'ITA')).toMatch(/ Italy is beaten\. The war goes on in Europe and the Pacific: America, Britain, Germany, Japan and the Soviet Union are still at war\.$/);
  }, 300000);

  it('a Japan at peace with every great power does not hold the card back', () => {
    const g = playing('UK', [['GER', 'UK']]);
    capitulate(g, 'GER', 'UK');
    expect(warDecided(g)).toMatchObject({ beaten: ['axis'], victor: 'allies', fallen: ['GER'] });
    expect(g.nations.JAP.alive).toBe(true);
    expect(last(g, 'GER')).not.toMatch(/war goes on/);
  }, 300000);

  it('a Japan that alone falls outside both alliances beats no alliance (ruling 201, point 3)', () => {
    const g = playing('USA', [['JAP', 'USA']]);
    expect(g.nations.JAP.align).toBe('leaning_axis');
    capitulate(g, 'JAP', 'USA');
    expect(warDecided(g)).toBeNull();
    expect(warGoesOn(g)).toBeNull();
    expect(last(g, 'JAP')).not.toMatch(/war goes on/);
    expect(whyNotDomination(g)).toMatch(/a great power must fall/);
  }, 300000);

  it('Japan as the Axis\'s co-belligerent: its fall after Germany\'s beats the Axis', () => {
    const g = playing('UK', [['GER', 'UK'], ['JAP', 'UK'], ['JAP', 'USA']]);
    g.nations.JAP.align = 'axis_cobelligerent';
    capitulate(g, 'GER', 'UK');
    expect(warDecided(g)).toBeNull();
    capitulate(g, 'JAP', 'USA');
    expect(warDecided(g)).toMatchObject({ beaten: ['axis'], victor: 'allies', fallen: ['GER', 'JAP'] });
  }, 300000);

  it("a small country's fall carries no such line", () => {
    const g = playing('USA', [['GER', 'USA'], ['JAP', 'USA']]);
    capitulate(g, 'GER', 'USA');
    war(g, 'THA', 'USA');
    capitulate(g, 'THA', 'USA');
    expect(last(g, 'THA')).not.toMatch(/war goes on|is beaten/);
  }, 300000);
});

describe('the two lines on the 3D game\'s diplomacy page', () => {
  it('head the Powers tab and close the fallen power\'s own card while the war goes on', async () => {
    await import('../unreal/TheaterCommand/Rules/campaign-bridge');
    const { saveGame } = await import('../src/campaign/save');
    const TC = (globalThis as unknown as { TC: Record<string, (...a: unknown[]) => any> }).TC;
    const g = playing('USA', [['GER', 'USA'], ['GER', 'UK'], ['JAP', 'USA'], ['JAP', 'UK']]);
    TC.load(saveGame(g));
    expect(TC.diplomacy().undecided).toBe('');
    capitulate(g, 'GER', 'USA');
    TC.load(saveGame(g));
    const page = TC.diplomacy();
    const line = 'The war between the alliances is not decided yet: America, Britain and Japan are still at war. It is decided once no two great powers are at war with each other.';
    expect(page.undecided).toBe(line);
    expect(page.wind.startsWith(line)).toBe(true);
    const ger = page.powers.find((p: { nat: string }) => p.nat === 'GER');
    expect(ger.note).toMatch(/^Fell in September 1939: asked the United States for terms\. Germany is beaten\. The war goes on in the Pacific: America, Britain and Japan are still at war\.$/);
    capitulate(g, 'JAP', 'USA');
    TC.load(saveGame(g));
    expect(TC.diplomacy().undecided).toBe('');
    expect(TC.diplomacy().powers.find((p: { nat: string }) => p.nat === 'GER').note).not.toMatch(/war goes on/);
  }, 300000);
});

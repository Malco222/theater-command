// Ruling 228, 1a: the trap before the Winter War. The Soviet call-up (east.json 'mobilisation') starts with a war on a
// great power (diplomacy.json 'majors') and runs while one lasts. A war with Finland, Estonia or any other small
// country calls up nobody, so the waves of 1941 still come in 1941.
// Ruling 228, 1b: the Winter War. The Soviet staff's war on Finland in the grand campaign, the Mannerheim Line it
// meets, and the peace of Moscow: by the book in March 1940, otherwise once Viipuri falls or Moscow runs out of time,
// and offered to a player on either side as a choice.
import { describe, expect, it } from 'vitest';
import { EASTERN, atWar, createGame, declareWar, setWar, standingCallUp, startPhase } from '../src/campaign/game';
import { bookMonth, readable } from '../src/campaign/claims';
import { HISTORY, setStop } from '../src/campaign/history';
import { loadGame, saveGame } from '../src/campaign/save';
import { staffPhase, startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, orderWinterPeace, runTurnAI, stepAI, whyNotOrderWinterPeace } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import { openWinter, winterCardDue, winterMonth, winterPeace, winterSince } from '../src/campaign/winter';

const NOVEMBER_39 = 3, JUNE_41 = 22;
const rifle = (g: Game): number => g.corps.filter((c) => c.nat === 'SOV' && !c.dead && c.name.endsWith(EASTERN.mobilisation.SOV.name)).length;

describe('the Soviet call-up and a war with a small country (ruling 228, 1a)', () => {
  for (const kind of ['history', 'grand'] as const) {
    it(`a war with Finland in November 1939 leaves the call-up as it was (${kind})`, () => {
      const g = kind === 'grand' ? startOpen(createGame(3, null)) : createGame(3, null);
      g.turn = NOVEMBER_39;
      setWar(g, 'SOV', 'FIN');
      for (const t of [3, 4, 5, 6]) { g.turn = t; startPhase(g, 'SOV'); }
      expect(g.flags['mobilised:SOV']).toBeUndefined();
      expect(rifle(g)).toBe(0);
      expect(g.headlines.some((h) => /general mobilisation/.test(h.text))).toBe(false);
      if (kind === 'grand') expect(standingCallUp(g, 'SOV')).toBeNull();
      // The war with Germany starts it, with the first wave of the plan, however long the war with Finland has run.
      g.turn = JUNE_41;
      declareWar(g, 'GER', 'SOV');
      startPhase(g, 'SOV');
      expect(g.flags['mobilised:SOV']).toBe(JUNE_41);
      expect(rifle(g)).toBeGreaterThan(0);
      expect(rifle(g)).toBeLessThanOrEqual(EASTERN.mobilisation.SOV.perMonth[0]);
      expect(g.headlines.some((h) => /general mobilisation/.test(h.text) && h.nat === 'SOV')).toBe(true);
    });
  }
});

// ---- 1b: the Winter War -----------------------------------------------------------------------------------------

const NOV_39 = 3, MAR_40 = 7, MAY_40 = 9;
const grand = (seed = 3, human: string | null = null): Game => startOpen(createGame(seed, human));
/** A grand campaign in the player's first month. */
function playing(human: string, seed = 3): Game {
  const g = grand(seed, human);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** The Winter War begun in this month, as the month's opening would read it. */
function begun(g: Game, at = NOV_39): Game {
  g.turn = at;
  declareWar(g, 'SOV', 'FIN');
  winterMonth(g);
  return g;
}
const finland = (g: Game): string[] => g.regions.filter((r) => r.owner === 'FIN').map((r) => `${r.name}:${r.ctrl}`);

describe('the Winter War (ruling 228, 1b)', () => {
  it('lays the Mannerheim Line on the isthmus when the grand campaign opens, and leaves the history campaigns alone', () => {
    expect(grand().byName.Karelia.fort).toEqual({ lv: 1, vs: ['SOV'], by: 'FIN' });
    const h = createGame(3, null);
    expect(h.byName.Karelia.fort).toBeNull();
    h.turn = NOV_39;
    setWar(h, 'SOV', 'FIN');
    winterMonth(h);
    expect(Object.keys(h.flags).filter((k) => k.startsWith('wnt:'))).toEqual([]);
  });

  it('goes by the book at stop 1: war in November 1939, peace in March 1940, and Karelia given up', () => {
    const g = grand(101);
    setStop(g, 1, true);
    while (g.turn <= MAR_40) runTurnAI(g);
    expect(g.flags['war:SOV:FIN']).toBe(NOV_39);
    const war = g.headlines.find((h) => h.kind === 'war' && h.text.startsWith('The Soviet Union declares war on Finland.'))!;
    expect(war.text).toContain('Helsinki would not move its frontier back from Leningrad');
    expect(g.headlines.filter((h) => h.kind === 'war' && /declares war on Finland/.test(h.text))).toHaveLength(1);
    expect(atWar(g, 'SOV', 'FIN')).toBe(false);
    expect(g.flags['wnt:done']).toBe(MAR_40);
    const K = g.byName.Karelia;
    expect([K.owner, K.ctrl, K.fort]).toEqual(['SOV', 'SOV', null]);
    expect(g.nations.FIN.alive).toBe(true);
    // Every other piece of Finland is Finland's again.
    expect(finland(g).every((x) => x.endsWith(':FIN'))).toBe(true);
    const peace = g.headlines.filter((h) => /peace of Moscow/.test(h.text));
    expect(peace).toHaveLength(1);
    expect(peace[0].turn).toBe(MAR_40);
    expect(peace[0].text).toMatch(/^Finland has signed the peace of Moscow\. After a hundred and five days of war it gives up Karelia, with Viipuri/);
  }, 60_000);

  it('off the book, makes peace once Viipuri has fallen: Karelia goes, and every other piece of Finland goes back', () => {
    const g = begun(grand());
    expect(winterSince(g)).toBe(NOV_39);
    g.turn = NOV_39 + 1;
    g.byName.Karelia.ctrl = 'SOV';
    g.byName.Petsamo.ctrl = 'SOV';
    winterMonth(g);
    expect(atWar(g, 'SOV', 'FIN')).toBe(false);
    expect([g.byName.Karelia.owner, g.byName.Karelia.fort]).toEqual(['SOV', null]);
    expect(g.byName.Petsamo.ctrl).toBe('FIN');
    expect(g.corps.some((c) => !c.dead && c.nat === 'FIN' && c.reg === g.byName.Karelia.id)).toBe(false);
    const text = g.headlines.at(-1)!.text;
    expect(text).toContain('It gives up Karelia, which the Red Army holds');
    expect(text).toMatch(/The Red Army hands back Petsamo\.$/);
    expect(winterSince(g)).toBe(0);
  });

  it('off the book, makes peace on the frontier as it stands when Moscow has taken nothing by its last month', () => {
    const g = begun(grand());
    // (No early draw since ruling b486: with Viipuri still Finnish the war goes on through March and April 1940.)
    for (g.turn = MAR_40; g.turn < MAY_40; g.turn++) {
      winterMonth(g);
      expect(atWar(g, 'SOV', 'FIN')).toBe(true);
    }
    g.turn = MAY_40;
    winterMonth(g);
    expect(atWar(g, 'SOV', 'FIN')).toBe(false);
    expect([g.byName.Karelia.owner, g.byName.Karelia.fort]).toEqual(['FIN', { lv: 1, vs: ['SOV'], by: 'FIN' }]);
    expect(g.headlines.at(-1)!.text).toBe('Finland and the Soviet Union have made peace in Moscow. The frontier stays where it was.');
  });

  it('gives a war begun later its months: the peace comes two months on at the soonest, and four at the latest', () => {
    const g = begun(grand(), 12);
    g.turn = 13;
    winterMonth(g);
    expect(atWar(g, 'SOV', 'FIN')).toBe(true);
    g.turn = 16;
    winterMonth(g);
    expect(atWar(g, 'SOV', 'FIN')).toBe(false);
  });

  it('leaves a war the great war has swallowed to the great war, and never makes one of it', () => {
    const g = begun(grand());
    setWar(g, 'GER', 'SOV');
    g.turn = MAY_40;
    winterMonth(g);
    expect(atWar(g, 'SOV', 'FIN')).toBe(true);
    expect([winterSince(g), g.flags['wnt:done']]).toEqual([0, MAY_40]);
    const h = grand();
    setWar(h, 'GER', 'SOV');
    begun(h);
    expect([winterSince(h), h.flags['wnt:done']]).toEqual([0, NOV_39]);
  });

  it('offers a Soviet player the peace as a card once and a row under Powers, never signs it for him, and takes it on his word', () => {
    const g = begun(playing('SOV'), 1);
    expect(winterPeace(g)).toBeNull();
    // (Nothing held, from the op's first month of peace: the offer on the frontier as it stands, with no word of 1940.)
    const turn0 = g.turn;
    g.turn = MAR_40;
    winterMonth(g);
    expect(winterPeace(g)).toMatchObject({ row: 'Asks for peace on the frontier as it stands.', note: '', cede: [] });
    g.turn = turn0;
    delete g.flags['wnt:offer'];
    g.byName.Karelia.ctrl = 'SOV';
    winterMonth(g);
    const wp = winterPeace(g)!;
    expect(wp).toMatchObject({ from: 'FIN', to: 'SOV', title: 'Finland asks for peace', yes: 'Make peace', no: 'Fight on', cede: ['Karelia'], back: [] });
    expect(wp.note).toMatch(/^In March 1940 Moscow took these terms/);
    expect(winterCardDue(g)).toBe(true);
    expect(winterCardDue(g)).toBe(false);
    // Terms that change in the month wait for the next one's card.
    g.byName.Petsamo.ctrl = 'SOV';
    expect(winterPeace(g)!.back).toEqual(['Petsamo']);
    expect(winterCardDue(g)).toBe(false);
    g.flags['wnt:cardAt'] = g.turn - 1;
    expect(winterCardDue(g)).toBe(true);
    // The computer never signs it for him, however long the war runs.
    const turn = g.turn;
    g.turn = MAY_40 + 6;
    winterMonth(g);
    expect(atWar(g, 'SOV', 'FIN')).toBe(true);
    g.turn = turn;
    expect(whyNotOrderWinterPeace(g)).toBeNull();
    expect(orderWinterPeace(g)).toBe(true);
    expect(atWar(g, 'SOV', 'FIN')).toBe(false);
    expect([g.byName.Karelia.owner, g.byName.Petsamo.ctrl]).toEqual(['SOV', 'FIN']);
    expect(winterPeace(g)).toBeNull();
    expect(orderWinterPeace(g)).toBe(false);
  });

  it('offers a Finnish player Moscow\'s terms, and takes them only in his own month', () => {
    const g = begun(playing('FIN'), 1);
    g.byName.Karelia.ctrl = 'SOV';
    winterMonth(g);
    expect(winterPeace(g)).toMatchObject({ from: 'SOV', to: 'FIN', title: 'Moscow offers peace', yes: 'Take the terms', row: 'Offers peace for Karelia.' });
    endHuman(g);
    expect(whyNotOrderWinterPeace(g)).toBe('Orders are given in your own month.');
    expect(orderWinterPeace(g)).toBe(false);
    for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
    beginHuman(g);
    // (The Army of the Isthmus, still in its field, retook Viipuri as Finland's month closed: the Red Army is put back in it.)
    g.byName.Karelia.ctrl = 'SOV';
    expect(winterPeace(g)).toMatchObject({ cede: ['Karelia'], row: 'Offers peace for Karelia.' });
    expect(orderWinterPeace(g)).toBe(true);
    expect(atWar(g, 'SOV', 'FIN')).toBe(false);
    expect([g.byName.Karelia.owner, g.byName.Karelia.fort]).toEqual(['SOV', null]);
    expect(g.corps.some((c) => !c.dead && c.nat === 'FIN' && c.reg === g.byName.Karelia.id)).toBe(false);
  }, 60_000);

  it('reads the card once a month: terms that change after the month\'s first reading wait for the next month', () => {
    const g = begun(playing('SOV'), 1);
    g.byName.Karelia.ctrl = 'SOV';
    winterMonth(g);
    expect(winterCardDue(g)).toBe(true);
    g.turn += 1;
    expect(winterCardDue(g)).toBe(false);
    g.byName.Petsamo.ctrl = 'SOV';
    expect(winterCardDue(g)).toBe(false);
    g.turn += 1;
    expect(winterCardDue(g)).toBe(true);
  });

  it('withdraws the offer the moment the great war swallows the Winter War, before the month\'s opening closes it', () => {
    const g = begun(playing('FIN'), 1);
    g.byName.Karelia.ctrl = 'SOV';
    winterMonth(g);
    expect(winterPeace(g)).not.toBeNull();
    setWar(g, 'GER', 'SOV');
    expect(winterPeace(g)).toBeNull();
    expect(whyNotOrderWinterPeace(g)).toBe('There is no offer of peace.');
    expect(orderWinterPeace(g)).toBe(false);
    expect(atWar(g, 'SOV', 'FIN')).toBe(true);
  });

  it('keeps the Red Army to the isthmus while the Winter War lasts, wins back its own ground, and fights a later war in full', () => {
    const g = begun(grand());
    g.flags['st:done:SOV:winter'] = NOV_39;
    g.turn = NOV_39 + 1;
    g.byName.Petrozavodsk.ctrl = 'FIN';
    const ph = staffPhase(g, 'SOV')!;
    expect([ph.near, ph.strict]).toEqual([1, true]);
    expect([...ph.targets!].sort()).toEqual(['Karelia', 'Petrozavodsk', 'Petsamo']);
    expect(ph.main).toEqual(['Karelia', 'Petsamo']);
    // The peace made, a war with Finland alone is fought as any other.
    g.byName.Petrozavodsk.ctrl = 'SOV';
    g.turn = MAY_40;
    winterMonth(g);
    expect(atWar(g, 'SOV', 'FIN')).toBe(false);
    g.turn = MAY_40 + 3;
    declareWar(g, 'FIN', 'SOV');
    expect(staffPhase(g, 'SOV')?.near).toBeUndefined();
  });

  it('asks history\'s mark for Finland only of a campaign opened before the war\'s months, and lays the line in an older save', () => {
    const early = createGame(3, null), late = createGame(3, null);
    late.turn = 17;
    openWinter(early);
    openWinter(late);
    expect([early.flags['wnt:late'], late.flags['wnt:late']]).toEqual([undefined, 17]);
    expect(late.byName.Karelia.fort).toEqual({ lv: 1, vs: ['SOV'], by: 'FIN' });
    // At stop 1, a campaign that had its months and never had the war fails the mark; one opened after them does not.
    for (const was of [undefined, 17]) {
      const g = grand();
      setStop(g, 1, true);
      if (was) g.flags['wnt:late'] = was;
      g.nations.POL.alive = false;
      g.turn = MAR_40;
      expect(bookMonth(g).filter((m) => m.theatre === 'finland'), `late ${was}`).toHaveLength(was ? 0 : 1);
    }
    // A grand campaign saved before the line was laid has it laid when it is loaded, and one from after the war's months is marked late.
    const old = grand();
    old.byName.Karelia.fort = null;
    expect(loadGame(saveGame(old))!.byName.Karelia.fort).toEqual({ lv: 1, vs: ['SOV'], by: 'FIN' });
    old.turn = 17;
    const back = loadGame(saveGame(old))!;
    expect([back.byName.Karelia.fort, back.flags['wnt:late']]).toEqual([{ lv: 1, vs: ['SOV'], by: 'FIN' }, 17]);
    expect(loadGame(saveGame(grand()))!.flags['wnt:late']).toBeUndefined();
  });

  it('keeps the war and the line in a saved game', () => {
    const g = begun(grand());
    const back = loadGame(saveGame(g))!;
    expect(back.byName.Karelia.fort).toEqual({ lv: 1, vs: ['SOV'], by: 'FIN' });
    expect(winterSince(back)).toBe(NOV_39);
  });

  it('presses no claim on the isthmus while the Winter War may still come or once it has, only after a war that never came', () => {
    const amb = HISTORY.ambitions.find((a) => a.id === 'karelia')!;
    expect(amb.op).toBe('winter');
    let seen = 0;
    for (let s = 1; s < 40 && seen < 2; s++) {
      const g = grand(s);
      setStop(g, 4, true);
      g.turn = 6;
      if (!readable(g, amb)) continue;
      seen++;
      for (const t of [1, 3, 5]) { g.turn = t; expect(readable(g, amb), `seed ${s} turn ${t}`).toBe(false); }
      g.turn = 6;
      g.flags['st:done:SOV:winter'] = 4;
      expect(readable(g, amb)).toBe(false);
    }
    expect(seen).toBe(2);
  });
});

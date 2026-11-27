// No campaign ends on the player without a red band at least a month before: the design director's ruling 117,
// point 4, extending rulings 99 to 103.
//
// What was wrong (the Playtester, Version 82.1's page): in the history campaign of July 1943 played as Italy, the
// fall of Mussolini was one line in the Newsreel, the card that opened August had no red band, and the next END
// MONTH ended the campaign. The stake was stated on the menu card only.
//
// The player's own Italy is put out of the war by history only from that one seat (ruling 103), and there the band
// "Rome wavers" is on every card: while no ground is lost it names the six places Rome watches; when one is lost,
// which, and the month Mussolini falls; and from the month he falls it says an armistice ends the campaign next
// month while an Ally holds one of the six, naming those held, and what stops it. News only: no rule reads it.
//
// The month it falls due the warning is no band but the lead story of the card (ruling 118(f)): THE STAKE, the month
// whose opening ends it, LAST MONTH, what happened, the sentence that names every place held, and two figures.
import { describe, expect, it } from 'vitest';
import { ROME_HOLDS, monthlyDiplomacy, stakeDue } from '../src/campaign/diplomacy';
import { createGame, setWar } from '../src/campaign/game';
import { openGoals } from '../src/campaign/goals';
import { markFor, monthCard } from '../src/campaign/monthcard';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const JULY_43 = 47, AUG_43 = 48, SEPT_43 = 49, TORCH = 39;
const SIX = 'Sicily, Sardinia, Calabria, Apulia, Naples';
const seat = (me: string | null, from?: number): Game => { const g = createGame(25, me); if (me && from !== undefined) openGoals(g, me, from); setWar(g, 'ITA', 'UK'); return g; };
const month = (g: Game, t: number): Game => { g.turn = t; monthlyDiplomacy(g); return g; };
const warns = (g: Game): string[] => g.headlines.filter((h) => h.warn === 'Rome wavers').map((h) => h.text);

describe('the player\'s own Italy in the campaign of July 1943', () => {
  it('is told from its first month which six places Rome watches, and what follows', () => {
    const g = month(seat('ITA', JULY_43), JULY_43);
    expect(warns(g)).toEqual([`Mussolini falls if the Allies take one of six places. They are ${SIX} and Rome, and they count from August 1943. The month after he falls, Italy signs an armistice if one is still held, and that ends your campaign.`]);
    const h = g.headlines.find((x) => x.warn === 'Rome wavers')!;
    expect(h).toMatchObject({ nat: 'ITA', about: 'ITA' });
    // Every month while he holds all six, and with no date once August has come.
    month(g, AUG_43);
    expect(warns(g)).toHaveLength(2);
    expect(warns(g)[1]).toBe(`Mussolini falls if the Allies take one of six places. They are ${SIX} and Rome. The month after he falls, Italy signs an armistice if one is still held, and that ends your campaign.`);
    expect(g.flags['event:mussolini-falls']).toBeUndefined();
  });

  it('when a ground is lost before August, names it and the month Mussolini falls', () => {
    const g = seat('ITA', JULY_43);
    g.byName.Sardinia.ctrl = 'UK';
    const mark = (g.turn = JULY_43, markFor(g, 'ITA'));
    month(g, JULY_43);
    expect(warns(g)).toEqual([`Mussolini falls in August 1943 unless Sardinia is taken back. The month after, Italy signs an armistice while the Allies hold any of ${SIX} or Rome, and that ends your campaign.`]);
    expect(g.headlines.find((x) => x.warn === 'Rome wavers')!.region).toBe(g.byName.Sardinia.id);
    // It is a red band of that month's card, and no stake is due yet.
    const card = monthCard(g, mark, 'ITA');
    expect(card.bands.find((b) => b.title === 'Rome wavers')).toMatchObject({ word: 'WARNING', head: 'Mussolini falls in August 1943 unless Sardinia is taken back', nats: ['ITA'] });
    expect(stakeDue(g)).toBeNull();
    expect(card.lead?.stake).toBeUndefined();
  });

  it('the month he falls says an armistice ends the campaign next month, names what is held and what stops it', () => {
    const g = seat('ITA', JULY_43);
    g.byName.Sardinia.ctrl = 'UK';
    g.byName.Sicily.ctrl = 'UK';
    const mark = (g.turn = JULY_43, markFor(g, 'ITA'));
    month(g, AUG_43);
    expect(g.flags['event:mussolini-falls']).toBe(AUG_43);
    expect(g.nations.ITA.alive).toBe(true);
    expect(warns(g)).toEqual([`Italy signs an armistice next month unless Sicily and Sardinia are taken back. Mussolini has fallen, and Rome signs while the Allies hold any of ${SIX} or Rome: that ends your campaign. Take back what they hold this month and Italy fights on.`]);
    // It is the lead story of the card that opens August, a month before the campaign can end, and no band says it
    // twice (ruling 118(f)).
    const card = monthCard(g, mark, 'ITA');
    expect(card.bands.find((b) => b.title === 'Rome wavers')).toBeUndefined();
    expect(card.lead).toMatchObject({
      stake: true, kind: 'THE STAKE', when: 'as September opens', tag: 'LAST MONTH', ours: 'ITA', picture: null, head: 'Mussolini has fallen', line: 'Rome seeks an armistice',
      follows: 'An armistice ends your campaign as September opens, unless Sicily and Sardinia are retaken first.',
      facts: [{ n: '1', label: 'month left' }, { n: '2', label: 'of the six places lost' }], region: g.byName.Sicily.id,
    });
    expect(card.lead!.report).toBeUndefined();
    expect(card.rows).not.toContain(card.lead);
    // He does not take them back: Italy signs in September, and nothing more is said.
    month(g, SEPT_43);
    expect(g.flags['event:italian-armistice']).toBe(SEPT_43);
    expect(g.nations.ITA.alive).toBe(false);
    expect(warns(g)).toHaveLength(1);
  });

  it('one place held is named alone, and more than three are counted and then named', () => {
    const g = seat('ITA', JULY_43);
    g.byName.Sicily.ctrl = 'UK';
    month(g, AUG_43);
    expect(stakeDue(g)).toMatchObject({ title: 'Rome wavers', nat: 'ITA', due: SEPT_43, month: 'September', left: 1, of: 6, lost: ['Sicily'], sentence: 'An armistice ends your campaign as September opens, unless Sicily is retaken first.' });
    for (const nm of ['Sardinia', 'Calabria', 'Apulia']) g.byName[nm].ctrl = 'UK';
    expect(stakeDue(g)!.sentence).toBe('An armistice ends your campaign as September opens, unless all four places the Allies hold are retaken first: Sicily, Sardinia, Calabria and Apulia.');
    expect(stakeDue(g)!.figures).toEqual([{ n: '1', label: 'month left' }, { n: '4', label: 'of the six places lost' }]);
    // It is his own card's and nobody else's.
    expect(stakeDue(g, 'GER')).toBeNull();
  });

  it('taking the ground back stops it, and the band of that phase says the month of grace is spent (ruling 120)', () => {
    const g = seat('ITA', JULY_43);
    g.byName.Sardinia.ctrl = 'UK';
    month(g, AUG_43);
    expect(stakeDue(g)).not.toBeNull();
    g.byName.Sardinia.ctrl = 'ITA';
    expect(stakeDue(g)).toBeNull();
    const mark = markFor(g, 'ITA');
    month(g, SEPT_43);
    // A band is back on the card under a new title, and nothing leads it as a stake.
    const card = monthCard(g, mark, 'ITA');
    expect(card.bands.find((b) => b.title === 'Rome wavers')).toBeUndefined();
    // (Headed by its own title, marked as the band with no month of grace, and what to do is on its face.)
    expect(card.bands.find((b) => b.title === ROME_HOLDS)).toMatchObject({ word: 'WARNING', head: 'Rome holds, for now', spent: true, note: 'There will be no last month: hold all six.', nats: ['ITA'] });
    expect(card.bands.find((b) => b.title === ROME_HOLDS)!.text).toMatch(/^Mussolini has fallen\. If the Allies hold any of the six as a month opens, Rome signs at once and your campaign ends\./);
    expect(card.lead?.stake).toBeUndefined();
    expect(g.flags['event:italian-armistice']).toBeUndefined();
    expect(g.nations.ITA.alive).toBe(true);
    expect(warns(g)).toHaveLength(1);
    expect(g.headlines.filter((h) => h.warn === ROME_HOLDS).map((h) => h.text)).toEqual([`Mussolini has fallen. If the Allies hold any of the six as a month opens, Rome signs at once and your campaign ends. There will be no last month: hold all six. They are ${SIX} and Rome.`]);
  });
});

describe('where Rome\'s fall cannot end the player, nothing is said of it', () => {
  it('from any other seat a player\'s Italy is spared, and has no band', () => {
    const g = seat('ITA', TORCH);
    g.byName.Sardinia.ctrl = 'UK';
    month(g, JULY_43); month(g, AUG_43); month(g, SEPT_43);
    expect(warns(g)).toEqual([]);
    expect(g.nations.ITA.alive).toBe(true);
  });

  it('in the grand campaign a player\'s Italy stands or falls by its cabinet, and has no band', () => {
    const g = startOpen(createGame(25, 'ITA'));
    setWar(g, 'ITA', 'UK');
    g.byName.Sardinia.ctrl = 'UK';
    month(g, JULY_43); month(g, AUG_43);
    expect(warns(g)).toEqual([]);
  });

  it('with nobody at the table nothing is said', () => {
    const g = seat(null);
    g.byName.Sardinia.ctrl = 'UK';
    month(g, JULY_43); month(g, AUG_43);
    expect(warns(g)).toEqual([]);
  });
});

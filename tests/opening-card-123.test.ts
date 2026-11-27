// The card that opens a campaign, and a stake's own band (the design director's rulings 121(c) and 123).
// 1. A seat's stake speaks for itself: its band is made from the stake as it stands when he reads the card, so the
//    card that opens the campaign carries it before any month's events have run.
// 2. The band's small line names the next moment the test is made, never the date of the news.
// 3. An opening card reports no month of his: it is headed by the moment ('As July 1943 opens') and its fronts line
//    never calls the month quiet. Every card after it is headed by the month it reports.
import { describe, expect, it } from 'vitest';
import { ROME_HOLDS, monthlyDiplomacy, stakeDue, stakeWarning } from '../src/campaign/diplomacy';
import { createGame, setWar } from '../src/campaign/game';
import { openGoals } from '../src/campaign/goals';
import { NOT_YET_FOUGHT, cardHead, dateWords, markFor, monthCard, opensCampaign } from '../src/campaign/monthcard';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const JULY_43 = 47, AUG_43 = 48, SEPT_43 = 49;
const SIX = 'Sicily, Sardinia, Calabria, Apulia, Naples';
const seat = (me: string | null, from?: number): Game => { const g = createGame(25, me); if (me && from !== undefined) openGoals(g, me, from); setWar(g, 'ITA', 'UK'); return g; };
const month = (g: Game, t: number): Game => { g.turn = t; monthlyDiplomacy(g); return g; };

describe('the card that opens a campaign', () => {
  it('is known by its mark: taken in the month now opening, and never in a war played from its first month', () => {
    const g = seat('ITA', JULY_43);
    g.turn = JULY_43;
    const mark = markFor(g, 'ITA');
    expect(opensCampaign(g, mark)).toBe(true);
    g.turn = AUG_43;
    expect(opensCampaign(g, mark)).toBe(false);
    const first = createGame(3, 'UK');
    expect(first.turn).toBe(1);
    expect(opensCampaign(first, markFor(first, 'UK'))).toBe(false);
  });

  it('is headed by the moment, and every card after it by the month it reports', () => {
    const g = seat('ITA', JULY_43);
    g.turn = JULY_43;
    const mark = markFor(g, 'ITA');
    const eve = monthCard(g, mark, 'ITA', { opening: opensCampaign(g, mark) });
    expect(eve.opening).toBe(true);
    expect(eve.month).toBe('July 1943');
    expect(cardHead(eve)).toEqual({ before: 'As', month: 'July 1943', after: 'opens' });
    // The card that reports his first month, told from the same mark as August opens.
    month(g, AUG_43);
    const next = monthCard(g, mark, 'ITA', { opening: opensCampaign(g, mark) });
    expect(next.opening).toBeUndefined();
    expect(cardHead(next)).toEqual({ before: '', month: 'July 1943', after: '' });
  });

  it('never calls the month quiet: there is no month yet to be quiet', () => {
    // A grand campaign taken over in a later year, with nothing fought since the mark.
    const g = startOpen(createGame(7, 'GER'));
    g.turn = 20;
    const mark = markFor(g, 'GER');
    expect(monthCard(g, mark, 'GER').quiet).toBe('A quiet month.');
    const eve = monthCard(g, mark, 'GER', { opening: opensCampaign(g, mark) });
    expect(eve.quiet).toBe(NOT_YET_FOUGHT);
    expect(eve.quiet).toBe('Nothing has been fought on your fronts yet.');
    expect(cardHead(eve)).toEqual({ before: 'As', month: 'April 1941', after: 'opens' });
    // Under a band too it does not speak of a month.
    const it43 = seat('ITA', JULY_43);
    it43.turn = JULY_43;
    const m = markFor(it43, 'ITA');
    const card = monthCard(it43, m, 'ITA', { opening: true });
    expect(card.bands).toHaveLength(1);
    expect(card.quiet).toBe(NOT_YET_FOUGHT);
  });
});

describe('a stake speaks for itself', () => {
  it('is on the card that opens the campaign, before any month\'s events have run', () => {
    const g = seat('ITA', JULY_43);
    g.turn = JULY_43;
    // No month's events have run: there is no news of it at all.
    expect(g.headlines.filter((h) => h.warn)).toEqual([]);
    const text = `Mussolini falls if the Allies take one of six places. They are ${SIX} and Rome, and they count from August 1943. The month after he falls, Italy signs an armistice if one is still held, and that ends your campaign.`;
    expect(stakeWarning(g)).toEqual({ title: 'Rome wavers', text, nat: 'ITA', about: 'ITA', region: g.byName.Rome.id, tested: 'August' });
    const mark = markFor(g, 'ITA');
    const card = monthCard(g, mark, 'ITA', { opening: true });
    expect(card.bands).toHaveLength(1);
    // The six are in its whole text (under the pointer), SHOW ON MAP has a place, and the small line names the first test.
    expect(card.bands[0]).toMatchObject({ word: 'WARNING', title: 'Rome wavers', nats: ['ITA'], head: 'Mussolini falls if the Allies take one of six places', text, when: 'as August opens', region: g.byName.Rome.id });
    expect(dateWords(card.bands[0])).toBe('as August opens');
  });

  it('names the next moment the test is made on every card after, not the date of the news', () => {
    const g = seat('ITA', JULY_43);
    g.turn = JULY_43;
    const mark = markFor(g, 'ITA');
    month(g, JULY_43);
    month(g, AUG_43);
    // Two months' news of it since the mark, and one band: the card he reads as August opens.
    expect(g.headlines.filter((h) => h.warn === 'Rome wavers')).toHaveLength(2);
    const card = monthCard(g, mark, 'ITA');
    const bands = card.bands.filter((b) => b.title === 'Rome wavers');
    expect(bands).toHaveLength(1);
    expect(bands[0]).toMatchObject({ head: 'Mussolini falls if the Allies take one of six places', when: 'as September opens' });
    // The news is not told a second time lower on the card.
    expect(card.rows.filter((r) => r.tag === 'WARNING')).toEqual([]);
  });

  it('says the stake as it stands when he reads the card: a place lost since the news was written is on the band', () => {
    const g = seat('ITA', JULY_43);
    g.turn = JULY_43;
    const mark = markFor(g, 'ITA');
    month(g, JULY_43);
    expect(g.headlines.find((h) => h.warn === 'Rome wavers')!.text).toMatch(/^Mussolini falls if the Allies take one of six places\./);
    // The Allies move before him: Sardinia is theirs by the time his month opens.
    g.byName.Sardinia.ctrl = 'UK';
    const card = monthCard(g, mark, 'ITA', { opening: true });
    expect(card.bands.find((b) => b.title === 'Rome wavers')).toMatchObject({ head: 'Mussolini falls in August 1943 unless Sardinia is taken back', when: 'as August opens', region: g.byName.Sardinia.id });
  });

  it('gives way to the lead in the month the stake falls due, and keeps its own head once the month of grace is spent', () => {
    const g = seat('ITA', JULY_43);
    g.byName.Sicily.ctrl = 'UK';
    const mark = (g.turn = JULY_43, markFor(g, 'ITA'));
    month(g, AUG_43);
    expect(stakeDue(g)).not.toBeNull();
    const last = monthCard(g, mark, 'ITA');
    expect(last.bands.filter((b) => b.word === 'WARNING')).toEqual([]);
    expect(last.lead).toMatchObject({ stake: true, when: 'as September opens', tag: 'LAST MONTH' });
    // The ground taken back: the campaign goes on, and the band of the months after says there will be no last month.
    g.byName.Sicily.ctrl = 'ITA';
    const m2 = markFor(g, 'ITA');
    month(g, SEPT_43);
    expect(g.nations.ITA.alive).toBe(true);
    const after = monthCard(g, m2, 'ITA');
    expect(after.bands.find((b) => b.title === ROME_HOLDS)).toMatchObject({ head: 'Rome holds, for now', spent: true, note: 'There will be no last month: hold all six.', when: 'as October opens' });
  });

  it('is nobody else\'s: no stake, no band, from another seat or in the grand campaign', () => {
    const ger = seat('GER', JULY_43);
    ger.turn = JULY_43;
    expect(stakeWarning(ger)).toBeNull();
    expect(monthCard(ger, markFor(ger, 'GER'), 'GER', { opening: true }).bands).toEqual([]);
    const open = startOpen(createGame(25, 'ITA'));
    open.turn = JULY_43;
    expect(stakeWarning(open)).toBeNull();
  });
});

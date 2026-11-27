// The cities version: "the" before some places (the UI director, 17 November 2026: her words of 21:25Z and her list
// and rule of 21:56Z, data/campaign/places.json). Running text writes "the Ruhr", "The Ruhr" at a sentence's start and
// "the Ruhr's output"; a longer name that holds one keeps its own form; map labels and headings stay bare (the page's).
import { describe, expect, it } from 'vitest';
import places from '../data/campaign/places.json';
import { createGame, headline, placeName, theWords } from '../src/campaign/game';
import { WORLD_MAP } from '../src/campaign/world';
import mapData from '../src/generated/campaign-map.json';

describe('"the" before some places', () => {
  it('writes the article in running text, "The" at the head of a sentence, and never twice', () => {
    expect(theWords('Bombers over Ruhr and Saar.')).toBe('Bombers over the Ruhr and the Saar.');
    expect(theWords("Crimea's ports are shut. Crimea falls.")).toBe("The Crimea's ports are shut. The Crimea falls.");
    expect(theWords('a raid on the Ruhr')).toBe('a raid on the Ruhr');
    expect(theWords('The Ruhr is bombed.')).toBe('The Ruhr is bombed.');
    expect(theWords('Right-click Ruhr to send it.')).toBe('Right-click the Ruhr to send it.');
    expect(theWords('British forces take Falkland Islands into their keeping.')).toBe('British forces take the Falkland Islands into their keeping.');
    // The nations' articles are as before.
    expect(theWords('Against United States in Crimea.')).toBe('Against the United States in the Crimea.');
  });

  it('leaves a longer name its own form, and a word that only begins like one', () => {
    expect(theWords('Rio Grande do Sul falls.')).toBe('Rio Grande do Sul falls.');
    expect(theWords('Mexican troops cross into Rio Grande.')).toBe('Mexican troops cross into the Rio Grande.');
    expect(theWords('Azores Waters are mined.')).toBe('Azores Waters are mined.');
    expect(theWords('Army Group Kuban holds Kuban.')).toBe('Army Group Kuban holds the Kuban.');
    expect(theWords('General Mackenzie lands in Mackenzie.')).toBe('General Mackenzie lands in the Mackenzie.');
    expect(theWords('Yukon Flats and Yukon.')).toBe('The Yukon Flats and the Yukon.');
    expect(theWords('From Krakow to Kra, and Benin to Beni.')).toBe('From Krakow to the Kra, and Benin to the Beni.');
  });

  it('reads a piece of a longer text on from what comes before it', () => {
    expect(theWords('Ruhr falls.', 'Bombers over ')).toBe('the Ruhr falls.');
    expect(theWords('Ruhr', 'Week 1. ')).toBe('The Ruhr');
    expect(theWords('Ruhr', 'over the ')).toBe('Ruhr');
    expect(placeName('Ruhr')).toBe('the Ruhr');
    expect(placeName('Ruhr', true)).toBe('The Ruhr');
    expect(placeName('Warsaw')).toBe('Warsaw');
  });

  it('every name on the list is a province of a chart, and the news takes it', () => {
    const names = new Set([...mapData.regions.map((r) => r.name), ...(WORLD_MAP.regions as { name: string }[]).map((r) => r.name)]);
    // (The far side's sixteen wait for step 13's chart.)
    const far = ['Yukon Flats', 'Arctic Slope', 'Eastern Marshalls', 'Admiralty Islands', 'Gobi', 'Volga Delta', 'Rub al Khali', 'Nullarbor', 'Seychelles', 'Kra', 'Vogelkop', 'Changtang', 'Tsaidam', 'Kolyma', 'Copper River', 'Kuskokwim'];
    for (const n of places.the) if (!far.includes(n)) expect(names.has(n), n).toBe(true);
    expect(places.the.length).toBe(70);
    const g = createGame(1, null);
    headline(g, 'event', 'Ruhr burns.');
    expect(g.headlines[g.headlines.length - 1].text).toBe('The Ruhr burns.');
  });
});

// How each sea is said in running text (the UI director, 18 November 2026: ui-director/words-sea-names.md): a 'where'
// form ("in the North Sea", "off the Azores", "in Hudson Bay") and the 'what' form that comes from it.
import { describe, expect, it } from 'vitest';
import { seaAction, seaHead, seaIs, seaPlural, seaSaid, seaWhat, seaWhatList, seaWhere, seaWhereList } from '../src/campaign/sea-words';
import { plainTicket } from '../src/battle/sea/ticket';
import words from '../data/campaign/sea-words.json';
import europe from '../data/campaign/seas.json';
import world from '../data/campaign/world-seas.json';
import far from '../data/campaign/far-seas.json';

const names = [...europe.zones, ...world.zones, ...far.zones].map((z: { name: string }) => z.name);

describe('the sea words', () => {
  it('name only seas on the map, the 52 of the UI director\'s list', () => {
    const listed = Object.keys(words.where);
    expect(names.length).toBe(145);
    expect(listed.length).toBe(52);
    for (const n of listed) expect(names, n).toContain(n);
  });

  it('say where a sea is: "in the" and its map name, or as listed', () => {
    expect(seaWhere('North Sea')).toBe('in the North Sea');
    expect(seaWhere('Bay of Biscay')).toBe('in the Bay of Biscay');
    expect(seaWhere('Patagonian Channels')).toBe('in the Patagonian Channels');
    expect(seaWhere('Azores Waters')).toBe('off the Azores');
    expect(seaWhere('Amazon Mouth')).toBe('off the mouth of the Amazon');
    expect(seaWhere('Hudson Bay')).toBe('in Hudson Bay');
    expect(seaWhere('Leyte Gulf')).toBe('in Leyte Gulf');
    expect(seaWhere('Hawaiian Approaches')).toBe('in the approaches to Hawaii');
  });

  it('make a sea a sentence\'s object from the where form', () => {
    expect(seaWhat('North Sea')).toBe('the North Sea');
    expect(seaWhat('Azores Waters')).toBe('the waters off the Azores');
    expect(seaWhat('Hudson Bay')).toBe('Hudson Bay');
    expect(seaWhat('Hawaiian Approaches')).toBe('the approaches to Hawaii');
    expect(seaHead('Azores Waters')).toBe('The waters off the Azores');
    expect(seaHead('Leyte Gulf')).toBe('Leyte Gulf');
    // Every sea has both forms, and no form is left with a doubled article or an empty name.
    for (const n of names) {
      expect(seaWhere(n), n).toMatch(/^(in|off) \S/);
      expect(seaWhat(n), n).not.toMatch(/^in |the the /);
    }
  });

  it('take a plural verb where the head noun is plural', () => {
    for (const n of ['Azores Waters', 'Hawaiian Approaches', 'Western Approaches', 'Straits of Florida', 'Patagonian Channels', 'Atlantic Narrows', 'Grand Banks', 'River Plate'])
      expect([n, seaPlural(n), seaIs(n)]).toEqual([n, true, 'are']);
    for (const n of ['North Sea', 'Hudson Bay', 'Gulf of St Lawrence', 'Mozambique Channel', 'Tsushima Strait', 'Leyte Gulf', 'Gulf of Aden', 'Sea of Okhotsk'])
      expect([n, seaPlural(n), seaIs(n)]).toEqual([n, false, 'is']);
  });

  it('join several seas', () => {
    expect(seaWhereList(['North Sea'])).toBe('in the North Sea');
    expect(seaWhereList(['North Sea', 'Azores Waters'])).toBe('in the North Sea and off the Azores');
    expect(seaWhatList(['North Sea', 'Bay of Biscay', 'Azores Waters'])).toBe('the North Sea, the Bay of Biscay and the waters off the Azores');
  });

  it('name a fleet action by where it was fought', () => {
    const fleet = { name: 'A', nat: 'UK', ships: { bb: 1, dd: 2 } };
    expect(plainTicket({ att: [fleet], def: [{ ...fleet, nat: 'GER' }], seed: 1 }).title).toBe('The action in the North Sea');
    expect(plainTicket({ att: [fleet], def: [{ ...fleet, nat: 'GER' }], seed: 1, zoneName: 'Azores Waters' }).title).toBe('The action off the Azores');
    expect(plainTicket({ att: [fleet], def: [{ ...fleet, nat: 'GER' }], seed: 1, zoneName: 'Hudson Bay' }).title).toBe('The action in Hudson Bay');
  });

  it('name an action, and the second in the same sea and month', () => {
    expect(seaAction('North Sea')).toBe('The action in the North Sea');
    expect(seaAction('Falkland Waters', 2)).toBe('The second action off the Falklands');
    expect(seaAction('Hudson Bay', 3)).toBe('The third action in Hudson Bay');
    expect(seaAction(null)).toBe('The action at sea');
  });

  it('find the sea a line of news names, by its where form', () => {
    const seas = ['North Sea', 'Azores Waters', 'Hudson Bay', 'Hawaiian Approaches', 'Hawaiian Waters', 'North Pacific', 'North-West Pacific'];
    const at = (t: string) => { const s = seaSaid(t, seas); return s && [seas[s.i], s.said]; };
    expect(at('A submarine torpedoes and sinks a cruiser of the Home Fleet off the Azores: the fleet has too few destroyers.')).toEqual(['Azores Waters', ' off the Azores']);
    expect(at('Bombers sink 2 destroyers of the Pacific Fleet in Hudson Bay.')).toEqual(['Hudson Bay', ' in Hudson Bay']);
    expect(at('The 1st Fleet declines battle in the approaches to Hawaii and makes for port.')).toEqual(['Hawaiian Approaches', ' in the approaches to Hawaii']);
    expect(at('Raiders 3 against escorts 2 in the North-West Pacific.')).toEqual(['North-West Pacific', ' in the North-West Pacific']);
    // The earliest sea in the line wins; a where form inside a longer name is no match.
    expect(at('Ships in the North Sea and off the Azores.')).toEqual(['North Sea', ' in the North Sea']);
    expect(at('Nothing at sea here.')).toBeNull();
  });
});

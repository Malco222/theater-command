// The UI director's two word fixes from the naval review (b458): a fleet's name takes "the" only where the service
// said it ("the Home Fleet", "the Polish Navy", plain "Force H"), and Italy's flotillas are what the game builds.
import { describe, expect, it } from 'vitest';
import traits from '../data/campaign/traits.json';
import { fleetName } from '../src/campaign/navy';

describe('the naval review\'s words (b458)', () => {
  it('says "Force H", never "the Force H"', () => {
    expect(['Home Fleet', 'Polish Navy', 'Force H'].map((name) => fleetName({ name }))).toEqual(['the Home Fleet', 'the Polish Navy', 'Force H']);
  });
  it('names Italy\'s cheap ships for what they are: destroyers and submarines', () => {
    const craft = JSON.stringify(traits).match(/\{[^{}]*"id": ?"craft"[^{}]*/)![0];
    expect(craft).toContain('"name":"Destroyer and submarine flotillas"');
    expect(craft).toContain('"text":"Built in numbers for the narrow seas of the Mediterranean."');
  });
});

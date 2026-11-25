// SKY.tied (the design director, 25 November 2026: "SKY.tied: delete (an unread figure)"). It was meant as the share of
// effort a defender caught between escort and bombers gives the escort, but nothing read it; the split between escort
// and bombers is to come by cause (R7). The escort that still counts over its box ('tied', AirSim's tied()) stays.
import { describe, expect, it } from 'vitest';
import { SKY } from '../src/battle/air/book';
import { AirSim } from '../src/battle/air/sim';

describe('SKY.tied is gone, the escort tied to its box is not', () => {
  // (No rules file can read one either: SKY is typed by its own literal, so a `SKY.tied` anywhere fails the type check.)
  it('the book has no tied figure', () => {
    expect('tied' in SKY).toBe(false);
  });

  it("the escort fought out or held off still counts over its own box (AirSim's tied)", () => {
    expect(typeof (AirSim.prototype as unknown as Record<string, unknown>).tied).toBe('function');
  });
});

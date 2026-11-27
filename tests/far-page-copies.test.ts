// The round world's copies of the map, and what they may cost the page (the far side's branch, 18 November 2026).
//
// Where the chart goes all the way round the world the page draws the far end of it again beyond the seam (two copies
// of the map, 'wrapw' and 'wrape'). As first built the copies were two `<use>` of one group laid round all the map's
// layers, there on every page and hidden. The browser charged for both on the map of Europe too: about 150 ms a
// redraw for the hidden copies (it keeps each one's own tree of the map and builds it again after every change) and
// about 100 ms more for the group (one layer changed, the whole map painted and composed again). With that quarter
// second on every redraw the page check of Version 84.3.1 missed three steps: a second Enter 40 ms after SEND THEM IN
// came in after the key guard's half second and put the WON slip down unread. The game itself read the same, so no
// test of the rules and neither history check saw it.
//
// The page has no browser to stand in here, so this reads the page's own source: the layers stand straight under the
// map as they do on main, and a copy is empty until it is shown. The timing itself is the page check's to hold
// (checks/ui-v8431.cjs, steps 2e, 3b and 3c).
import { describe, expect, it } from 'vitest';
import page from '../src/campaign/ui/main.ts?raw';

describe("the map's copies beyond the seam", () => {
  const svg = page.match(/<svg id="map"[^>]*>([\s\S]*?)<\/svg>/);
  const body = (svg ? svg[1] : '').replace(/<defs>[\s\S]*?<\/defs>/, '').trim();

  it('leaves the layers of the map straight under the map, no group round them', () => {
    expect(body.startsWith('<g id="seas"></g><g id="lanes"></g><g id="land"></g>')).toBe(true);
    // every layer is empty in the page as it is laid, so no group opens inside another
    expect(body).not.toMatch(/<g [^>]*>\s*<g /);
    expect(body.match(/<g id="[a-z]+"><\/g>/g)!.length).toBeGreaterThanOrEqual(23);
  });

  it('keeps a copy empty, pointing at nothing, until the view reaches past the seam', () => {
    expect(body).not.toMatch(/<use\b/);
    expect(body.endsWith('<g id="wrapw" class="wrapcopy" display="none"></g><g id="wrape" class="wrapcopy" display="none"></g>')).toBe(true);
    // and the one place a copy is filled empties it again when it is put away
    const show = page.slice(page.indexOf('function showCopy('), page.indexOf('function unwrapPts'));
    expect(show).toMatch(/if \(!on\) \{[\s\S]*?el\.innerHTML = '';[\s\S]*?return;/);
    expect(show).toContain('<use href="#${id}"/>');
  });
});

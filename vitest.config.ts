import { fileURLToPath } from 'node:url';
import { defineConfig } from 'vitest/config';

// Many tests here play months or years of a whole war in one go. On a slow or busy machine those run past
// vitest's default five seconds without anything being wrong, so every test gets half a minute unless it
// asks for more itself.
//
// The aliases: land gate tests open their battles on the owner's field and seat them by the ONE shared gate tool
// (the design director, 27 November 2026, 01:59Z: gates/gatehead.ts is the one tool; import it, never copy it, and
// keep no list of rows in this tree). '@gates' is where that tool lives (the project's shared folder); '@tc' is the
// alias the tool itself uses to read this tree's data/campaign/stage.json. A test that imports '@gates/gatehead'
// fails to load where the shared folder is not mounted: that is meant, a land gate cannot be read without it.
export default defineConfig({
  resolve: { alias: { '@tc': fileURLToPath(new URL('./src', import.meta.url)), '@gates': '/mnt/project-files/gates' } },
  test: { testTimeout: 30_000 },
});

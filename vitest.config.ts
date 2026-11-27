import { defineConfig } from 'vitest/config';

// Many tests here play months or years of a whole war in one go. On a slow or busy machine those run past
// vitest's default five seconds without anything being wrong, so every test gets half a minute unless it
// asks for more itself.
export default defineConfig({ test: { testTimeout: 30_000 } });

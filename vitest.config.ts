import { defineConfig } from 'vitest/config';
// TRIAL (far-side seats rig, never for main): the seats overlay, so that the tests see the same data files the
// rig's bundle does. By default only the placeholder rows are laid down (every owner code on the final chart
// must be a country, or the wider world cannot be attached at all); TC_SEATS=file,file adds seats files.
import { mergedText, plan } from './tools/trial/seats.mjs';

const TRIAL = plan((process.env.TC_SEATS ?? '').split(',').filter(Boolean));

// Many tests here play months or years of a whole war in one go. On a slow or busy machine those run past
// vitest's default five seconds without anything being wrong, so every test gets half a minute unless it
// asks for more itself.
export default defineConfig({
  plugins: [{
    name: 'trial-seats',
    enforce: 'pre',
    load(id: string) {
      const file = id.split('?')[0];
      return file.endsWith('.json') ? mergedText(TRIAL, file) : null;
    },
  }],
  test: { testTimeout: 30_000 },
});

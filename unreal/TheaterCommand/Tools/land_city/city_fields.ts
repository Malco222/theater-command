// Test fields for the ruined city ground (release 2, the trial "city"): the rules' own field builder (src/battle/land
// field.ts, buildField with layCity) laid for a few kinds of city, written as a recording with no men, so the cloud test
// (Tools/land_field/run.sh) lays and paints exactly the ground a campaign battle there would open on. Plain test data:
// no campaign reads it.
//
//   npx esbuild unreal/TheaterCommand/Tools/land_city/city_fields.ts --bundle --platform=node --format=esm \
//     --outfile=<scratch>/city-fields.mjs --log-level=warning
//   node <scratch>/city-fields.mjs <out folder>
//
// Each place is generic (no real city's streets: the art director's "never" list), only its latitude and longitude
// and its size taken from the kind of place it stands for, so the style of its walls and roofs follows the region.
import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { buildField, builtShare, GROUNDS, type LandPlace } from '../../../../src/battle/land/field';

interface Want { key: string; size: 'capital' | 'city'; lat: number; lon: number; date: string; seed: number; stream?: LandPlace['stream'] }

const WANTS: Want[] = [
  // a capital of the plains of the east (plaster and timber, plank and iron roofs)
  { key: 'city-capital-east', size: 'capital', lat: 52.2, lon: 21.0, date: 'September 1944', seed: 4401 },
  // a city of the north-west (brick and slate)
  { key: 'city-northwest', size: 'city', lat: 49.2, lon: 6.9, date: 'November 1944', seed: 4402 },
  // a city along its great river as a strip (the east's steppe edge)
  { key: 'city-strip-volga', size: 'city', lat: 48.7, lon: 44.5, date: 'October 1942', seed: 4403,
    stream: { name: 'River', lie: 'shore', flows: 'south', side: 'back', strip: true, great: true } },
  // a city of the south (stone and plaster, tile roofs)
  { key: 'city-south', size: 'city', lat: 41.5, lon: 13.8, date: 'May 1944', seed: 4404 },
];

const out = process.argv[2] ?? 'city-fields';
mkdirSync(out, { recursive: true });
for (const w of WANTS) {
  const place: LandPlace = {
    region: -1, name: w.key, terrain: 'urban', river: null, bridgeDown: false, town: null, townSize: w.size, fort: 0, mines: 0,
    ...(w.stream ? { stream: w.stream } : {}),
  };
  const f = buildField(place, w.seed);
  const rec = {
    key: w.key,
    date: w.date,
    seed: w.seed,
    sky: { latLon: [w.lat, w.lon] },
    field: {
      cols: f.cols, rows: f.rows, cell: f.cell, grounds: [...GROUNDS], ground: f.ground, height: f.height.map((v) => Math.round(v * 100) / 100),
      ...(f.storeys ? { storeys: f.storeys } : {}), ...(f.landmark ? { landmark: f.landmark } : {}),
      town: f.town, place: { terrain: place.terrain, townSize: w.size },
    },
    men: [],
  };
  writeFileSync(join(out, `${w.key}.json`), JSON.stringify(rec));
  console.log(`${w.key}: ${f.cols} x ${f.rows}, built ${(builtShare(f) * 100).toFixed(0)}%, town ${f.town ? `${Math.round(f.town.x)}, ${Math.round(f.town.y)}` : 'none'}, landmark ${f.landmark?.kind ?? 'none'}`);
}

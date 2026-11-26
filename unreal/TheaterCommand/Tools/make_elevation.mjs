// Turns NOAA's elevation data into the height map the campaign table is modelled from.
//
// The source is ETOPO 2022 (NOAA National Centers for Environmental Information, public domain,
// doi:10.25921/fd45-gt74) at one arc-minute, for the part of the world the map covers. It is
// fetched once as an OPeNDAP ".dods" file:
//
//   https://www.ngdc.noaa.gov/thredds/dodsC/global/ETOPO2022/60s/60s_surface_elev_netcdf/
//     ETOPO_2022_v1_60s_N90W180_surface.nc.dods?z.z[7112:1:9688][10136:1:13624]
//
// which is rows of latitude from 28.5417 N northward and columns of longitude from 11.0583 W
// eastward, a sixtieth of a degree apart, in metres (negative under the sea).
//
// The output is the same ground laid out the way the campaign map is (the projection in
// tools/build-geo-campaign.mjs): one height for every whole map unit, as 16-bit metres.
//
//   node Tools/make_elevation.mjs <etopo.dods> <elevation.bin>

import fs from 'node:fs';

const [, , input, output] = process.argv;
const raw = fs.readFileSync(input);

// The file opens with a description in text, then "Data:", then the count twice and the values, big-endian.
const marker = raw.indexOf('Data:\n');
const text = raw.subarray(0, marker).toString('latin1');
const dims = /z\[lat = (\d+)\]\[lon = (\d+)\]/.exec(text);
const ROWS = Number(dims[1]), COLS = Number(dims[2]);
let at = marker + 6;
const count = raw.readUInt32BE(at);
at += 8;
if (count !== ROWS * COLS) throw new Error(`expected ${ROWS * COLS} heights, the file has ${count}`);
const src = new Float32Array(count);
for (let i = 0; i < count; i++) src[i] = raw.readFloatBE(at + i * 4);

const LAT0 = 28.541666666666667, LON0 = -11.058333333333337, STEP = 1 / 60;

function sample(lat, lon) { // between the four nearest readings
  const r = Math.min(Math.max((lat - LAT0) / STEP, 0), ROWS - 1.001);
  const c = Math.min(Math.max((lon - LON0) / STEP, 0), COLS - 1.001);
  const r0 = Math.floor(r), c0 = Math.floor(c), fr = r - r0, fc = c - c0;
  const a = src[r0 * COLS + c0], b = src[r0 * COLS + c0 + 1], d = src[(r0 + 1) * COLS + c0], e = src[(r0 + 1) * COLS + c0 + 1];
  return (a * (1 - fc) + b * fc) * (1 - fr) + (d * (1 - fc) + e * fc) * fr;
}

// The campaign map's projection: x east and y south from the north-west corner, 40 units to a degree of latitude.
const W = -11, N = 71.4, E = 47, S = 28.6, SCALE = 40, KX = Math.cos((50 * Math.PI) / 180);
const width = Math.round((E - W) * KX * SCALE) + 1, height = Math.round((N - S) * SCALE) + 1;

const out = Buffer.alloc(12 + width * height * 2);
out.write('TCE1', 0);
out.writeUInt32LE(width, 4);
out.writeUInt32LE(height, 8);
let lowest = 0, highest = 0, land = 0;
for (let y = 0; y < height; y++) {
  for (let x = 0; x < width; x++) {
    const lon = W + x / (KX * SCALE), lat = N - y / SCALE;
    // A map unit is wider than one reading, so each height is the average of the ground it covers.
    let sum = 0, n = 0;
    for (let dy = -1; dy <= 1; dy++) {
      for (let dx = -1; dx <= 1; dx++) {
        sum += sample(lat + dy * 0.4 / SCALE, lon + dx * 0.4 / (KX * SCALE));
        n++;
      }
    }
    const metres = Math.round(sum / n);
    lowest = Math.min(lowest, metres); highest = Math.max(highest, metres);
    if (metres > 0) land++;
    out.writeInt16LE(Math.max(-32768, Math.min(32767, metres)), 12 + (y * width + x) * 2);
  }
}
fs.mkdirSync(new URL('.', new URL(output, `file://${process.cwd()}/`)), { recursive: true });
fs.writeFileSync(output, out);
console.log(`${output}: ${width} x ${height} heights, from ${lowest} m to ${highest} m, ${(100 * land / (width * height)).toFixed(0)}% above the sea, ${(out.length / 1e6).toFixed(1)} MB`);

// A few places everyone knows, as a check that the ground is where it should be.
const spot = (name, lat, lon) => `${name} ${Math.round(sample(lat, lon))} m`;
console.log([spot('Mont Blanc', 45.83, 6.86), spot('Berlin', 52.52, 13.4), spot('Madrid', 40.42, -3.7), spot('Elbrus', 43.35, 42.44), spot('mid-Mediterranean', 35.5, 18), spot('North Sea', 56, 3), spot('Ankara', 39.93, 32.86)].join(', '));

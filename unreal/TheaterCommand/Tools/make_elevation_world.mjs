// Turns NOAA's elevation data into the height map of the whole world, laid out the way the world map is.
//
// The source is ETOPO 2022 (NOAA National Centers for Environmental Information, public domain,
// doi:10.25921/fd45-gt74), every second reading of the one arc-minute grid, from 56 S to 71.4 N. It was
// fetched once as two OPeNDAP ".dods" files (the server will not hand over the whole belt in one piece):
//
//   .../ETOPO_2022_v1_60s_N90W180_surface.nc.dods?z.z[2040:2:9684][2340:2:21599]   (141 W eastward to 180 E)
//   .../ETOPO_2022_v1_60s_N90W180_surface.nc.dods?z.z[2040:2:9684][0:2:2339]       (180 W eastward to 141 W)
//
// under https://www.ngdc.noaa.gov/thredds/dodsC/global/ETOPO2022/60s/60s_surface_elev_netcdf/ . Rows run
// northward from 55.9917 S and columns eastward, a thirtieth of a degree apart, in metres (negative under the sea).
//
// The output is that ground in the world map's own frame (the rules' contract for the world): x runs east from
// the seam at 30.45 W, 25.7115 units to a degree of longitude, right round the world to 9256.14; y runs south
// from 71.4 N, 40 units to a degree of latitude, to 5096 at 56 S. One height for every STEP map units, as 16-bit
// metres, behind a small header:
//
//   "TCE2", width (uint32), height (uint32), step (float32)
//
//   node Tools/make_elevation_world.mjs <world_a.dods> <world_b.dods> <elevation-world.bin> [step]

import fs from 'node:fs';

const [, , fileA, fileB, output, stepArg] = process.argv;
const STEP = Number(stepArg) || 2;

function read(path) {
  const raw = fs.readFileSync(path);
  const marker = raw.indexOf('Data:\n');
  const text = raw.subarray(0, marker).toString('latin1');
  const dims = /z\[lat = (\d+)\]\[lon = (\d+)\]/.exec(text);
  const rows = Number(dims[1]), cols = Number(dims[2]);
  let at = marker + 6;
  const count = raw.readUInt32BE(at);
  at += 8;
  if (count !== rows * cols) throw new Error(`${path}: expected ${rows * cols} heights, the file has ${count}`);
  // (kept as the file has them, big-endian, and read where they are wanted: the two files together are 165 MB)
  return { rows, cols, at, raw };
}

const A = read(fileA), B = read(fileB);
if (A.rows !== B.rows) throw new Error('the two files do not cover the same latitudes');
const ROWS = A.rows, COLS = A.cols + B.cols; // B is the western end: 180 W to 141 W; A runs on from there to 180 E
const LAT0 = -90 + (2040 + 0.5) / 60, LON0 = -180 + 0.5 / 60, GRID = 1 / 30;

const at = (r, c) => { // one reading, by its row and its column counted eastward from 180 W, round the world
  c = ((c % COLS) + COLS) % COLS;
  return c < B.cols ? B.raw.readFloatBE(B.at + (r * B.cols + c) * 4) : A.raw.readFloatBE(A.at + (r * A.cols + (c - B.cols)) * 4);
};

function sample(lat, lon) { // between the four nearest readings
  const r = Math.min(Math.max((lat - LAT0) / GRID, 0), ROWS - 1.001);
  const c = (lon - LON0) / GRID;
  const r0 = Math.floor(r), c0 = Math.floor(c), fr = r - r0, fc = c - c0;
  return (at(r0, c0) * (1 - fc) + at(r0, c0 + 1) * fc) * (1 - fr) + (at(r0 + 1, c0) * (1 - fc) + at(r0 + 1, c0 + 1) * fc) * fr;
}

// The world map's frame.
const SEAM = -30.45, N = 71.4, S = -56, SCALE = 40, KX = Math.cos((50 * Math.PI) / 180);
const WIDE = 360 * KX * SCALE, HIGH = (N - S) * SCALE;
const width = Math.ceil(WIDE / STEP), height = Math.floor(HIGH / STEP) + 1;

const out = Buffer.alloc(16 + width * height * 2);
out.write('TCE2', 0);
out.writeUInt32LE(width, 4);
out.writeUInt32LE(height, 8);
out.writeFloatLE(STEP, 12);
let lowest = 0, highest = 0, land = 0;
// Each height is the average of the ground it covers: three readings each way across its patch.
const span = STEP / 3;
for (let y = 0; y < height; y++) {
  const lat = N - (y * STEP) / SCALE;
  for (let x = 0; x < width; x++) {
    const lon = SEAM + (x * STEP) / (KX * SCALE);
    let sum = 0, n = 0;
    for (let dy = -1; dy <= 1; dy++) {
      for (let dx = -1; dx <= 1; dx++) {
        sum += sample(lat + (dy * span) / SCALE, lon + (dx * span) / (KX * SCALE));
        n++;
      }
    }
    const metres = Math.round(sum / n);
    lowest = Math.min(lowest, metres); highest = Math.max(highest, metres);
    if (metres > 0) land++;
    out.writeInt16LE(Math.max(-32768, Math.min(32767, metres)), 16 + (y * width + x) * 2);
  }
}
fs.mkdirSync(new URL('.', new URL(output, `file://${process.cwd()}/`)), { recursive: true });
fs.writeFileSync(output, out);
console.log(`${output}: ${width} x ${height} heights every ${STEP} map units, from ${lowest} m to ${highest} m, ${(100 * land / (width * height)).toFixed(0)}% above the sea, ${(out.length / 1e6).toFixed(1)} MB`);

// A few places everyone knows, as a check that the ground is where it should be.
const spot = (name, lat, lon) => `${name} ${Math.round(sample(lat, lon))} m`;
console.log([spot('Mont Blanc', 45.83, 6.86), spot('Berlin', 52.52, 13.4), spot('Denver', 39.74, -104.99), spot('Mount Fuji', 35.36, 138.73), spot('Kilimanjaro', -3.07, 37.35), spot('Aconcagua', -32.65, -70.01),
  spot('mid-Pacific', 20, -170), spot('mid-Atlantic', 30, -45), spot('Lhasa', 29.65, 91.1), spot('Sydney', -33.87, 151.0)].join(', '));

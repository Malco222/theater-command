// Film critic: re-checks the step files of this folder against the game's own rules bundle, in node, as far as the
// rules can: every name found (TC.find), every fleet order accepted (TC.forceOrder, the string the game sends), every
// raid card's title and buttons (TC.page), every land battle's ticket (TC.battleLook), the verbs' words against what
// the step player takes (CampaignStepPlayer.cpp). Run it after the game's head moves:
//   node Tools/film_critic/scenes/check_steps.cjs [file.steps.txt ...]
// It cannot play the 3D scene: framing, timing and the battles' own course are only known from a night's pictures.
const fs = require('fs');
const path = require('path');
const ROOT = path.resolve(__dirname, '../../../../..');
function load(opts = {}) {
  const read = (p) => JSON.parse(fs.readFileSync(path.join(ROOT, p), 'utf8'));
  try {
    globalThis.__data = { map: read('src/generated/campaign-map.json'), nations: read('data/campaign/nations.json'), setup: read('data/campaign/setup-1939.json') };
  } catch (e) { /* (the bundle carries its own data) */ }
  if (opts.far) globalThis.TC_FAR = 1; else delete globalThis.TC_FAR;
  (0, eval)(fs.readFileSync(path.join(ROOT, 'unreal/TheaterCommand/Content/Rules/campaign-rules.js'), 'utf8'));
  return globalThis.TC;
}
// The war switches release 2 ships (Release2Ship.cpp, the CampaignRule rows that are ON)
load.SWITCHES = ["nicknames","marked","taskforcerule","rearguardrule","rearbridgesrule","shadowrule","voyagelogrule","seaminesrule","heavyseasrule","shadowrelayrule","fleetwhyrule","settledtownsrule","columns","logistics:lorryShift","logistics:wagons","logistics:dumps"];
const DIR = __dirname;
const files = process.argv.slice(2).length ? process.argv.slice(2) : fs.readdirSync(DIR).filter(f => f.endsWith('.steps.txt'));
const FAR = new Set(['far-pacific.steps.txt']);
const AIR_KEYS = new Set(['skip', 'continue', 'return', 'enter', 'escape', 'esc', 'chase', 'c', 'next', 'prev', 'up', 'down', 'right', 'left', 'pause', 'play', 'space', 'x0.5', 'x1', 'x2', 'x4']);
const LAND_KEYS = new Set(['l', 'esc', 'escape', 'h', 'g', 'r', 'f', 'x', 'k', 't', 'b', 'tab', 'c', 'v', 'w', 'a', 's', 'd', 'q', 'e', 'up', 'down', 'left', 'right', 'space', 'enter', 'return', '1', '2', '3']);
const ORDER_KEYS = new Set(['l', 'h', 'g', 'r', 'f', 'x', 'k', 't', 'b']);
const MAP_KEYS = new Set(['tab', 'enter', 'return', 'esc', 'escape', 'space', 't', 'r', 'y']);
let bad = 0;
for (const file of files) {
  const far = FAR.has(file);
  const lines = fs.readFileSync(path.join(DIR, file), 'utf8').split('\n');
  let TC = null, human = null, hand = null, ticket = null, card = null, n = 0, from = 0;
  const saves = {};
  const out = [];
  const fail = (msg) => { bad++; out.push(`  FAIL ${String(n).padStart(3, '0')}: ${msg}`); };
  const ok = (msg) => out.push(`  ok   ${String(n).padStart(3, '0')}: ${msg}`);
  const view = () => TC.view();
  const find = (name) => TC.find(name);
  for (const raw of lines) {
    const line = raw.replace(/\s+$/, '');
    if (!line.trim() || line.trim().startsWith('#')) continue;
    const parts = line.split('\t');
    const verb = parts[0].trim().toLowerCase(), target = (parts[1] || '').trim(), expect = (parts[2] || '').trim();
    n++;
    if (parts.length > 3) fail(`more than three TAB-separated parts: ${JSON.stringify(line)}`);
    if (/ {2,}/.test(line) && !line.includes('\t')) fail('spaces but no TAB');
    try {
      if (verb === 'start') {
        const w = target.split(' ');
        const [y, m] = w[1].split('-').map(Number);
        from = (y - 1939) * 12 + m - 9 + 1;
        const seed = +w[w.indexOf('seed') + 1];
        if (!far) delete globalThis.TC_FAR;
        TC = load({ far });
        const g = TC.grand(from, 1, far);
        const offered = (g.nations || g.list || g).map(x => x.nation || x.tag || x.id);
        const names = (g.nations || g.list || g).map(x => x.name);
        TC.start({ human: w[0], from, view: 'grand', seed, level: 'colonel', world: far });
        TC.r2Ship(load.SWITCHES);
        human = w[0];
        const v = view();
        if (v.human && v.human !== human) fail(`started as ${v.human}, not ${human}`);
        ok(`start ${human} turn ${v.turn} ${v.date} (offered list: ${names.length} countries${offered[0] ? '' : ''})`);
        hand = null; ticket = null; card = null;
      } else if (verb === 'press' && !TC) {
        if (!far) delete globalThis.TC_FAR;
        const t2 = load({ far });
        const g = t2.grand(1, 1, far);
        const names = (g.nations || g.list || g).map(x => x.name);
        if (!names.includes(target)) fail(`opening screen: no country card "${target}" in ${names.slice(0, 8).join(', ')}...`);
        else ok(`opening screen card "${target}"`);
      } else if (!TC && verb !== 'open-page') {
        fail(`${verb} before any start`);
      } else if (verb === 'open-page') {
        if (!TC) { ok(`open-page ${target} on the opening screen (no-op)`); continue; }
        const p = TC.page(target);
        card = p;
        const want = expect.startsWith('card') ? expect.slice(4).trim() : '';
        if (!p || !p.title) fail(`page ${target}: no title`);
        else if (want && !p.title.includes(want)) fail(`page ${target}: title "${p.title}", wanted "${want}"`);
        else ok(`page ${target}: "${p.title}"${p.card && p.card.fly ? ` fly "${p.card.fly.label}" ${p.card.fly.act} ok=${p.card.fly.ok}` : ''}`);
      } else if (verb === 'press') {
        if (target === 'FIGHT IT YOURSELF') {
          if (!ticket) fail('FIGHT IT YOURSELF with no battle ticket from the last order');
          else ok(`FIGHT IT YOURSELF: ticket "${ticket.title}" seed ${ticket.seed}`);
        } else if (target === 'FLY IT YOURSELF') {
          const fly = card && card.card && card.card.fly;
          const m = fly && fly.act.match(/^raid:(\w+):(\d+):([\d.]+)/);
          const look = m && TC.airLook(m[1], +m[2], m[3].split('.').map(Number));
          if (!fly || !fly.ok) fail('no fly offer on the open card');
          else if (!look || !look.ticket) fail(`no air battle for ${fly.act}: ${look && look.why}`);
          else ok(`FLY IT YOURSELF beside ${fly.label}: "${look.ticket.title}"`);
        } else {
          const acts = [];
          const walk = (o) => { if (!o || typeof o !== 'object') return; if (Array.isArray(o)) return o.forEach(walk); if (typeof o.label === 'string' && typeof o.act === 'string') acts.push(o); for (const k in o) walk(o[k]); };
          walk(card);
          const b = acts.find(a => a.label === target);
          if (!b) fail(`no button "${target}" on the open card (${acts.filter(a => a.label).map(a => a.label).join(' | ')})`);
          else {
            const r = TC.act(b.act);
            if (!r || r.ok === false) fail(`"${target}" (${b.act}) refused: ${r && r.note}`);
            else ok(`"${target}" -> ${b.act}: ${(r.note || '').slice(0, 90)} (${(r.flies || []).length} sorties)`);
          }
        }
      } else if (verb === 'select-unit') {
        const f = find(target);
        const v = view();
        if (f.corps >= 0) {
          const c = v.corps.find(x => x.id === f.corps);
          if (!c || c.nat !== human) fail(`${target}: corps ${f.corps} is not ${human}'s`);
          let here = v.corps.filter(x => x.reg === c.reg && x.nat === human && x.canOrder && !x.town);
          if (!here.length) here = v.corps.filter(x => x.reg === c.reg && x.nat === human && x.canOrder);
          hand = { kind: 'corps', ids: here.map(x => x.id), names: here.map(x => x.name), reg: c.reg };
          ok(`${target}: army in region ${c.reg}: ${hand.names.join(' | ')}`);
        } else if (f.fleet >= 0) {
          const fl = v.fleets.find(x => x.id === f.fleet);
          if (!fl || fl.nat !== human) fail(`${target}: fleet ${f.fleet} is not ${human}'s (${fl && fl.nat})`);
          hand = { kind: 'fleet', id: f.fleet, name: fl.name };
          ok(`${target}: fleet ${f.fleet} in sea ${fl.zone} (${fl.stance}), reach ${fl.reach.join(',')}`);
        } else fail(`${target}: no corps or fleet`);
      } else if (verb === 'order-sea') {
        const f = find(target);
        if (f.sea < 0) fail(`${target}: no sea`);
        else if (!hand || hand.kind !== 'fleet') fail('order-sea with no fleet in hand');
        else {
          const fl = view().fleets.find(x => x.id === hand.id);
          if (!fl.reach.includes(f.sea)) fail(`${target} (${f.sea}) not in ${hand.name}'s reach ${fl.reach}`);
          const r = TC.forceOrder(`fleet:${hand.id}:${f.sea}:patrol`);
          const after = view().fleets.find(x => x.id === hand.id);
          if (!r.ok) fail(`${hand.name} to ${target} refused: ${r.note}`);
          else ok(`${hand.name} -> ${target} (${f.sea}): ${r.note}; now in ${after.zone} ${after.stance}`);
        }
      } else if (verb === 'order-province') {
        const f = find(target);
        if (f.region < 0) fail(`${target}: no province`);
        else if (!hand || hand.kind !== 'corps') fail('order-province with no army in hand');
        else {
          const bl = TC.battleLook(hand.ids, f.region);
          ticket = bl && bl.ticket;
          if (!ticket) fail(`no battle ticket for ${hand.names.join('+')} on ${target}: ${bl && bl.why}`);
          else ok(`${hand.names[0]} on ${target}: "${ticket.title}" (${ticket.date})`);
        }
      } else if (verb === 'look-at' || verb === 'pan-zoom' || verb === 'check-province' || verb === 'select-province') {
        let name = target, h = '';
        const cut = target.lastIndexOf(' ');
        if (verb === 'look-at' || verb === 'pan-zoom') {
          const w = target.slice(cut + 1).toLowerCase();
          if (['near', 'middle', 'far'].includes(w)) { name = target.slice(0, cut); h = w; }
        }
        const f = find(name);
        if (f.region < 0) fail(`${verb} ${name}: no province of that name`);
        else ok(`${verb} ${name}${h ? ' (' + h + ')' : ''}: region ${f.region}`);
      } else if (verb === 'snapshot') {
        saves[target] = TC.save().text; ok(`kept "${target}"`);
      } else if (verb === 'restore') {
        if (!saves[target]) fail(`nothing kept as ${target}`); else { TC.load(saves[target]); hand = null; ok(`restored "${target}"`); }
      } else if (verb === 'end-month') {
        TC.endMonth(); const v = view(); ok(`month ended: now ${v.date}`); hand = null;
      } else if (verb === 'close') {
        ok('close');
      } else if (verb === 'key') {
        if (!MAP_KEYS.has(target.toLowerCase())) fail(`key ${target}: the hook does not press it`); else ok(`key ${target}`);
      } else if (verb === 'air-wait' || verb === 'land-wait') {
        const w = target.toLowerCase();
        const fine = /^\d+(\.\d+)?s$/.test(w) || /^\d+(\.\d+)?$/.test(w) || (verb === 'air-wait' ? ['flyover', 'card'] : ['card', 'contact', 'begun']).includes(w);
        if (!fine) fail(`${verb} ${target}: not a wait the player takes`);
        else if (/s$/.test(w) && parseFloat(w) > 900) fail(`${verb} ${target}: over 15 minutes`);
        else ok(`${verb} ${target}`);
      } else if (verb === 'air-key') {
        if (!AIR_KEYS.has(target.toLowerCase())) fail(`air-key ${target}: not a key the hook presses`); else ok(`air-key ${target}`);
      } else if (verb === 'land-key') {
        const k = target.toLowerCase();
        if (!LAND_KEYS.has(k)) fail(`land-key ${target}: not a key the hook presses`);
        else if (ORDER_KEYS.has(k)) fail(`land-key ${target}: an ORDER key`);
        else ok(`land-key ${target}`);
      } else if (verb === 'land-click') {
        if (!['field', 'field-right', 'drag', 'all fall back', 'keep fighting', 'film', 'continue'].includes(target.toLowerCase()) && !target.toLowerCase().startsWith('action:')) fail(`land-click ${target}`);
        else ok(`land-click ${target}`);
      } else {
        fail(`unknown verb ${verb}`);
      }
    } catch (e) {
      fail(`${verb} ${target}: threw ${e.message}`);
    }
  }
  console.log(`== ${file}: ${n} steps`);
  console.log(out.join('\n'));
}
console.log(bad ? `\n${bad} FAILURES` : '\nall steps check out');

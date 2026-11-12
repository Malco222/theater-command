// STAND-IN RULES. Not the game.
//
// The real rules of the campaign live in the browser prototype (src/campaign) and will be bundled
// into campaign-rules.js beside this file; when that file exists the game uses it and ignores this
// one. Until then this stand-in answers the same questions in the same shapes, with the simplest
// rules that let the 3D game be built and tried: armies march on their own ground at one region a
// move, nothing fights, and the month only turns the calendar.
//
// What it must keep in common with the real thing is the shape of the conversation:
//
//   TC.start({ human, seed })          -> view
//   TC.view()                          -> view
//   TC.reach([corpsId, ...])           -> { regions: [{ id, cost, attack, prev }] }
//   TC.move([corpsId, ...], regionId)  -> { moved: [{ id, path: [regionId, ...] }], view }
//   TC.endMonth()                      -> { events: [...], view }
//
// and the shape of a view (see view() below).

(function () {
  'use strict';

  var MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
  var STACK = 5; // most corps of one side in a region, as in the real rules
  var WARS = [['GER', 'POL'], ['GER', 'UK'], ['GER', 'FRA']]; // who is at war in September 1939

  var g = null;

  function dateOf(turn) {
    var index = 8 + (turn - 1); // turn 1 is September 1939
    return MONTHS[index % 12] + ' ' + (1939 + Math.floor(index / 12));
  }

  function atWar(a, b) {
    return WARS.some(function (w) { return (w[0] === a && w[1] === b) || (w[0] === b && w[1] === a); });
  }

  function start(options) {
    var data = globalThis.__data;
    options = options || {};
    g = {
      turn: 1,
      human: options.human || 'GER',
      regions: data.map.regions.map(function (r) {
        return { id: r.id, name: r.name, ctrl: r.owner, terrain: r.terrain, nb: r.nb.slice() };
      }),
      byName: {},
      corps: [],
      nations: {},
    };
    g.regions.forEach(function (r) { g.byName[r.name] = r; });
    Object.keys(data.nations).forEach(function (id) {
      if (id.charAt(0) === '_') return;
      g.nations[id] = { name: data.nations[id].name, alive: true };
    });
    // Each line of the setup is [nation, name, kind, divisions, region, options].
    data.setup.corps.forEach(function (line) {
      var region = g.byName[line[4]];
      if (!region) return;
      var extra = line[5] || {};
      var moves = line[2] === 'fort' ? 0 : 4;
      g.corps.push({
        id: g.corps.length, nat: line[0], name: line[1], kind: line[2], divs: line[3],
        str: 1, org: 1, reg: region.id, ap: moves, apMax: moves, gen: extra.gen || null, posture: 'stand',
      });
    });
    return view();
  }

  function canOrder(c) {
    return c.nat === g.human && c.kind !== 'fort';
  }

  function view() {
    return {
      rules: 'stand-in',
      turn: g.turn,
      date: dateOf(g.turn),
      human: g.human,
      current: g.human,
      humanUp: true,
      regions: g.regions.map(function (r) { return { id: r.id, ctrl: r.ctrl }; }),
      corps: g.corps.map(function (c) {
        return {
          id: c.id, nat: c.nat, name: c.name, kind: c.kind, divs: c.divs, str: c.str, org: c.org,
          reg: c.reg, ap: c.ap, apMax: c.apMax, gen: c.gen, posture: c.posture, canOrder: canOrder(c),
        };
      }),
      nations: g.nations,
    };
  }

  function unitsIn(regionId) {
    return g.corps.filter(function (c) { return c.reg === regionId; });
  }

  // Every region one corps could get to this month, cheapest way first, and the enemy-held
  // neighbours it could attack.
  function reachOf(c) {
    var out = {};
    if (!canOrder(c) || c.ap <= 0) return out;
    out[c.reg] = { cost: 0, prev: -1, attack: false };
    var open = [c.reg];
    while (open.length) {
      open.sort(function (a, b) { return out[a].cost - out[b].cost; });
      var at = open.shift();
      var spent = out[at].cost;
      g.regions[at].nb.forEach(function (j) {
        var r = g.regions[j];
        if (r.ctrl !== c.nat) {
          var hostile = atWar(c.nat, r.ctrl) && unitsIn(j).some(function (u) { return atWar(c.nat, u.nat); });
          if (hostile && at === c.reg && !out[j]) out[j] = { cost: 1, prev: at, attack: true };
          return; // the stand-in never leaves its own ground
        }
        var step = r.terrain === 'mountain' ? 2 : 1;
        var cost = spent + step;
        if (cost > c.ap) return;
        if (unitsIn(j).filter(function (u) { return u.nat === c.nat && u.id !== c.id; }).length >= STACK) return;
        if (out[j] && out[j].cost <= cost) return;
        out[j] = { cost: cost, prev: at, attack: false };
        open.push(j);
      });
    }
    return out;
  }

  // Where a picked army could go: anywhere any one of its corps could.
  function reach(ids) {
    var merged = {};
    (ids || []).forEach(function (id) {
      var c = g.corps[id];
      if (!c) return;
      var mine = reachOf(c);
      Object.keys(mine).forEach(function (key) {
        var entry = mine[key];
        if (Number(key) === c.reg) return;
        var old = merged[key];
        if (!old || entry.cost < old.cost) {
          merged[key] = { id: Number(key), cost: entry.cost, attack: entry.attack || (old ? old.attack : false), prev: entry.prev };
        } else if (entry.attack) {
          old.attack = true;
        }
      });
    });
    return { regions: Object.keys(merged).map(function (key) { return merged[key]; }) };
  }

  function move(ids, to) {
    var moved = [];
    (ids || []).forEach(function (id) {
      var c = g.corps[id];
      if (!c) return;
      var mine = reachOf(c);
      var end = mine[to];
      if (!end || end.attack || to === c.reg) return;
      var path = [];
      for (var at = to; at !== c.reg && at >= 0; at = mine[at].prev) path.unshift(at);
      c.ap -= end.cost;
      c.reg = to;
      moved.push({ id: c.id, path: path });
    });
    return { moved: moved, view: view() };
  }

  function endMonth() {
    g.turn += 1;
    g.corps.forEach(function (c) { c.ap = c.apMax; });
    return { events: [{ kind: 'note', text: 'The stand-in rules only turn the calendar. Nobody else moves yet.' }], view: view() };
  }

  globalThis.TC = { start: start, view: view, reach: reach, move: move, endMonth: endMonth };
})();

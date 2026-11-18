// The browser game's sounds: a smaller set chosen from the full one, and what each is for.
//
// The browser game is a single page with no 3D map, so it has no use for sounds that are carried about (engine loops,
// a train going round) or placed in space. It needs: the desk, the month, the calls, and one short sound for each
// kind of order or report. 80 sounds, about 1.6 MB packed (2.1 MB once written into the page), all mono.
//
// "hook" is the UI director's name for the moment (ui-director/ui-style-guide.md, "Sound hooks"): the builders fire
// the hook and this table says which sound answers it.
//
// "on" is where the rules builder hooks it: a click's data-act name, a modal's kind, or a moment in the code.

export const BROWSER_SET = [
  // ---- The desk: every button and card -------------------------------------------------------------------------
  { name: 'click', hook: 'ui.press', on: 'any data-act click that has no sound of its own' },
  { name: 'ui_primary', hook: 'ui.primary', on: 'the brass button of a screen: act dodeclare, doraid, doair, dosplit, govappoint, an attack confirmed. Not END THE MONTH, which has month_ask and month_end' },
  { name: 'ui_dial', hook: 'ui.dial', on: 'act gchist, gclevel, gcdepth: a dial moved one stop' },
  { name: 'ui_back', hook: 'ui.back', on: 'act unpick, wreckno, govkeep; any Cancel, or a step back to the screen before. Not the close stud or Esc: those are card_close and screen_close' },
  { name: 'ui_tab', hook: 'ui.tab', on: 'act tab, lens, menu, tradeall, ledgerall, monthall' },
  { name: 'switch_on', hook: 'ui.toggle (on); ui.month.ready at -6 dB', on: 'a setting turned on: act fog, setask (on), watchpref, auto (on)' },
  { name: 'switch_off', hook: 'ui.toggle (off)', on: 'a setting turned off: the same acts, off' },
  { name: 'ui_error', hook: 'ui.error', on: 'an order the rules refuse (the function returns false or a reason)' },
  { name: 'ui_notify', on: 'one thing that wants the player now and is not an alert slip (an offer on the telephone, a question from the cabinet)' },
  { name: 'alert_slip', hook: 'ui.alert.in', on: 'a new alert appears in the alert strip (once for a batch)' },
  { name: 'card_open', hook: 'ui.open.card', on: 'openModal for a small card: town, unit, fallen, award, shell, raid planner steps' },
  { name: 'card_close', hook: 'ui.close.card', on: 'openModal(null) when a card was open: act close, the close stud, Esc, a click outside the card' },
  { name: 'screen_open', hook: 'ui.open.panel', on: 'openModal for a page: ledger, books, staffs, cabinet, trade, tech, level' },
  { name: 'screen_close', hook: 'ui.close.panel', on: 'that page closed' },
  { name: 'page_turn', hook: 'ui.page', on: 'moving from one report to the next; a page of the ledger or the guide' },
  { name: 'stamp', hook: 'ui.stamp.won', on: 'the WON stamp lands; an order that cannot be undone: doraid, doshell, govdecree' },
  { name: 'stamp_low', hook: 'ui.stamp.lost', on: 'the LOST stamp lands' },
  { name: 'stamp_light', hook: 'ui.order.placed', on: 'something bought or ordered in the arsenal or a city: an arsenal order, act job. Not a move on the map' },
  { name: 'pickup', hook: 'ui.piece.up', on: 'act pick, pickall, fleet, airfleet, wing (something taken in hand)' },
  { name: 'putdown', hook: 'ui.piece.down', on: 'act standmonth, joinarmy, joinall, gotmarch (the piece set down)' },
  { name: 'order_march', hook: 'ui.order.march', on: 'march() when the player orders a move by road' },
  { name: 'order_attack', hook: 'ui.order.attack', on: 'act attack, storm, sally, probe (an attack ordered)' },
  { name: 'order_cancel', hook: 'ui.order.cancel', on: 'act stopmarch, stopjob, cancelbuild, sendback' },
  { name: 'select_region', hook: 'ui.select', on: 'selectRegion() from a click on the map' },
  { name: 'coin_run', hook: 'ui.coin', on: 'the treasury figure counts' },
  { name: 'coins', on: 'act cash, drafts: money raised by the player\'s own order' },
  { name: 'pen_sign', hook: 'ui.research.start; ui.sign', on: 'a technology goes into work; act tradesign, govpromise: a treaty or a promise signed' },
  { name: 'radio', on: 'act spygo, spymission; an intelligence report opened' },

  // ---- The month ---------------------------------------------------------------------------------------------
  { name: 'month_ask', hook: 'ui.month.ask', on: 'openModal({kind: "askend"})' },
  { name: 'month_end', hook: 'ui.month.end', on: 'nextMonth() when the month really ends' },
  { name: 'month_passing_loop', hook: 'ui.month.passing', on: 'runOthers() starts it; it stops the moment month_yours plays', loop: true },
  { name: 'month_yours', hook: 'ui.month.yours', on: 'arrive(): the month is the player\'s again' },
  { name: 'month_card_open', hook: 'ui.month.card', on: 'openModal({kind: "news"}) with a month card' },
  { name: 'month_card_good', hook: 'ui.month.card (second sound)', on: '0.6 s after month_card_open, when the card has more good rows than bad' },
  { name: 'month_card_bad', hook: 'ui.month.card (second sound)', on: 'the same, more bad than good' },
  { name: 'month_card_quiet', hook: 'ui.month.card (second sound)', on: 'the same, even or nearly empty' },

  // ---- First Command -----------------------------------------------------------------------------------------
  { name: 'note_arrives', hook: 'ui.note.in', on: 'a staff note appears' },
  { name: 'note_done', hook: 'ui.note.done', on: 'a staff note is stamped DONE: played as the stamp lands' },
  { name: 'note_closing', hook: 'ui.note.closing', on: 'the closing note appears' },

  // ---- Calls: the great moments ----------------------------------------------------------------------------------
  { name: 'campaign_start', on: 'newGame() from the opening screen (act start)' },
  { name: 'drums', on: 'openModal({kind: "attack"}): the two sides drawn up' },
  { name: 'victory', on: 'a battle report opens and the player won; a town taken' },
  { name: 'defeat', on: 'a battle report opens and the player lost; a town fallen' },
  { name: 'war_declared', on: 'act dodeclare; any declaration of war in the month card' },
  { name: 'treaty_signed', on: 'an alliance, a peace or a pact made' },
  { name: 'city_taken', on: 'a capital or prize city taken by the player' },
  { name: 'city_lost', on: 'a capital or prize city lost by the player' },
  { name: 'nation_falls', on: 'a country is eliminated (the announcement)' },
  { name: 'nation_freed', on: 'a country is liberated (the announcement)' },
  { name: 'alert', on: 'openModal({kind: "invade"}) or "landing" against the player; followAlert for an urgent alert' },
  { name: 'research_done', hook: 'ui.research.done', on: 'research finished, in the month card' },
  { name: 'medal', on: 'openModal({kind: "award"}); act appoint, promote' },
  { name: 'drum_low', hook: 'ui.band.war', on: 'a red band appears, when the moment has no call of its own' },
  { name: 'unit_lost', on: 'openModal({kind: "fallen"}); a division or a named ship lost' },
  { name: 'world_won', on: 'openModal({kind: "worldwon"})' },
  { name: 'campaign_lost', on: 'the campaign is lost' },

  // ---- One sound for each kind of order or report -----------------------------------------------------------------
  { name: 'marching', on: 'march() by road, after order_march (0.3 s later)' },
  { name: 'rail_move', from: 'scenes', on: 'a move by rail ordered (ui.rail is on when the move is made)' },
  { name: 'ship_horn', on: 'act shipto, sail, toport, fleetgo: a fleet or a convoy sails' },
  { name: 'ship_guns', on: 'openModal({kind: "seareport"})' },
  { name: 'shore_bombardment', on: 'act doshell; openModal({kind: "shorereport"})' },
  { name: 'torpedo_hit', on: 'a ship torpedoed, in a sea report' },
  { name: 'ship_sinking', on: 'a capital ship sunk, in a sea report' },
  { name: 'landing_craft', on: 'act doland; openModal({kind: "landing"})' },
  // The bomber's sound follows the aircraft, not the hour (ruling 142): a country that flies four-engined bombers gets
  // "heavy", every other "medium".
  { name: 'raid_day_heavy', from: 'scenes', on: 'openModal({kind: "raid"}) or "airreport" for a raid flown by day (mission bomb, strike, ground, line, fleet, harbour, town) by a country whose bomber has four engines' },
  { name: 'raid_day_medium', from: 'scenes', on: 'the same by day, for a country whose bomber has two engines' },
  { name: 'raid_night_heavy', from: 'scenes', on: 'a raid flown by night (mission night) by a country whose bomber has four engines' },
  { name: 'raid_night_medium', from: 'scenes', on: 'the same by night, for a country whose bomber has two engines' },
  { name: 'siren', on: 'a raid on one of the player\'s own towns, in the month card' },
  { name: 'fighter_pass', on: 'act doair for a patrol, an escort, a scouting flight; mission superiority' },
  { name: 'parachutes', on: 'airborne troops dropped' },
  { name: 'artillery_salvo_01', on: 'act bombline, dosiege; openModal({kind: "siege"})' },
  { name: 'battle', on: 'a battle report opens (before victory or defeat)' },
  { name: 'bridge_blown', on: 'act blow, wreck' },
  { name: 'rail_cut', on: 'act cutrail' },
  { name: 'rail_mended', on: 'rails mended, in the month card' },
  { name: 'fire_start', on: 'act burnorder, scorch' },
  { name: 'dig_in', on: 'act fortify, garrison' },
  { name: 'build_works', on: 'works finished, in the month card; act job' },

  // ---- One bed ---------------------------------------------------------------------------------------------------
  { name: 'room_day_loop', on: 'from the first click to the end, very quietly: the war room', loop: true, bed: true },
];

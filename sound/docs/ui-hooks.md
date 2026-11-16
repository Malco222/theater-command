# The interface's sound hooks, and the sound that answers each

Sound director, 16 November 2026, in answer to the UI director's list (`ui-director/ui-style-guide.md`, "Sound hooks").

**How the two lists fit.** The UI director names the moments the interface raises (`ui.press`). The sound set names the files (`click`). Neither is renamed: the builders fire the hook, and this table says which file plays. It is the one list both builders work from.

## The twenty hooks

| Hook | Sound | Notes |
|---|---|---|
| `ui.press` | `click` | A bakelite switch: short, soft, the commonest sound in the game |
| `ui.primary` | `ui_primary` | The same click, heavier, with brass in it and a release |
| `ui.toggle` | `switch_on` when it turns something on, `switch_off` when off | For a segment with three or more choices use `ui_tab` |
| `ui.tab` | `ui_tab` | A card flicked forward in an index |
| `ui.open.panel` | `screen_open` | A drawer run out and a book set down; the book lands at 160 ms, as the panel finishes rising |
| `ui.close.panel` | `screen_close` | The book shut, the drawer pushed home |
| `ui.open.card` | `card_open` | Paper slid onto the desk; it lands at 160 ms |
| `ui.close.card` | `card_close` | The same slide backwards, lighter |
| `ui.error` | `ui_error` | Two dull knocks on wood. Never a buzzer |
| `ui.coin` | `coin_run` | Four coins in under half a second, inside the 400 ms count |
| `ui.order.placed` | `stamp_light` | The stamp, lightly, as a clerk marks a chit |
| `ui.research.start` | `pen_sign` | A nib across paper and a full stop |
| `ui.research.done` | `research_done` | Three small bells climbing |
| `ui.alert.in` | `alert_slip` | A telegraph sounder, three strokes. Fire it once for a batch of slips, not once a slip |
| `ui.stamp.won` | `stamp` | Fire it when the stamp lands (220 ms after it starts to fall), not when it starts |
| `ui.stamp.lost` | `stamp_low` | The same stamp, lower and heavier |
| `ui.band.war` | `drum_low` | One low drum stroke. When the moment has its own call (`war_declared`, `nation_falls`), play the call and skip this |
| `ui.month.end` | `month_end` | The calendar page torn off, the day book shut, one stroke of the clock |
| `ui.month.ready` | `switch_on`, 6 dB down | A lamp switch, quiet |
| `ui.dial` | `ui_dial` | A detent's small sharp click |

The UI director's three rules are adopted as they stand and are in the sound brief: one hook to an act and never faster than eight a second; nothing on pointer-over; `ui.error` always with the reason in words.

## Moments the sound set has that the hook list does not

The sound set also has sounds for these moments of the interface. They are offered as hooks in the same style; the hook list is the UI director's to keep.

| Suggested hook | Sound | When |
|---|---|---|
| `ui.back` | `ui_back` | Cancel, or a step back: the click, lower, so that going back sounds like going back |
| `ui.page` | `page_turn` | A page of the ledger, the guide or the roll of honour; one report to the next |
| `ui.select` | `select_region` | A province chosen on the map |
| `ui.piece.up` | `pickup` | An army or a fleet taken in hand |
| `ui.piece.down` | `putdown` | It is set down |
| `ui.order.march` | `order_march` | A move pencilled on the map |
| `ui.order.attack` | `order_attack` | An attack pencilled: two hard strokes |
| `ui.order.cancel` | `order_cancel` | An order rubbed out |
| `ui.sign` | `pen_sign` | A treaty or a promise signed |
| `ui.month.ask` | `month_ask` | The end-of-month question rises |
| `ui.month.passing` | `month_passing_loop` | While the other countries move (a loop: start and stop) |
| `ui.month.yours` | `month_yours` | The others have moved; the month is the player's |
| `ui.month.card` | `month_card_open`, then `month_card_good`, `month_card_bad` or `month_card_quiet` | The month card is laid down; the second sound follows 0.6 s later, by the card's news |
| `ui.note.in` | `note_arrives` | First Command: a staff note arrives |
| `ui.note.done` | `note_done` | First Command: the note is stamped DONE |
| `ui.note.closing` | `note_closing` | First Command: the closing note |

Everything else in the sound set (engines, guns, trains, the band's calls, the ambience) is raised by the game's events, not by the interface, and is listed by event in `implementation-3d.md` and `implementation-browser.md`.

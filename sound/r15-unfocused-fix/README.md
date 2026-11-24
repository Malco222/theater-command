# 1.5 fix: the game silent on Brian's Mac (24 Nov 2026)

**Cause (confirmed on the Mac, 17:19Z):** Unreal's BaseEngine.ini sets `[Audio] UnfocusedVolumeMultiplier=0.0`, and
on Mac the app-deactivated handler (MacApplication.cpp:1781) applies it, so the whole game goes to volume 0 whenever
macOS doesn't count it as the active app. The Play icon starts the game from Terminal, and Brian's 1.5 session was
silent from the start although the log had the theme playing, both switches on, and the speakers at 88.

**Fix:** `0001-Release-1.5-...patch`, against origin/unreal 23951a1a (1.5 launch candidate): adds
`[Audio] UnfocusedVolumeMultiplier=1.0` with a note to Config/DefaultEngine.ini. Config only; nothing else changes.
The game then keeps playing its sound when another app is in front.

**Until it is in:** switch to another app with Cmd-Tab and back to the game, which makes the Mac restore the volume.

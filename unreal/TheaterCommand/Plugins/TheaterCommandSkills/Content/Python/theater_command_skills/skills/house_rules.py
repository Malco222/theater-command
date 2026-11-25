import unreal

from toolset_registry.agent_skill import agent_skill

_INSTRUCTIONS = """\
# What this project is

An original WW2 game: a grand campaign played like a strategy game, with battles that should look and feel like a war film. Personal use only, never sold. The 3D project is the real product; the browser version in the same repository is a test kitchen for the rules and doesn't need to look good.

# Where things really live

- The rules are not in Unreal. They are the browser prototype's TypeScript, bundled into one JavaScript file the game runs, and every question to them crosses as JSON. Never re-implement or tune a rule in C++ or Blueprint: change the prototype, rebuild the bundle, and let its tests judge the change. A scene that nudges an outcome makes the game drift from the tested rules.
- The levels are empty on purpose. The campaign table, armies, battlefields and effects are built in code from data when the game starts. Don't place, edit or save actors in a level to change what the player sees; change the code or data that builds it.
- The few binary assets are produced by the content build script under Tools. Change the script and rerun it instead of editing the asset by hand, or the next rebuild silently undoes the work.
- Rules and data sit in plain folders under Content, not as assets. Any new folder of that kind must be added to the packaging stage list, or a packaged build will quietly ship without it.

# What decides whether a change is right

- Battles are decided by what is on the field: forces, equipment, training, doctrine, supply, terrain and air cover. Never by the date or by what happened historically. Real records only calibrate how much each factor weighs.
- Drawing never changes who wins. The screen shows the numbers each side really brought: one drawn figure stands for the same number of men, tanks, planes or ships on both sides, and no side is trimmed to look even. When full scale can't be drawn, state the ratio.
- Most shots miss, but every hit shows on what it struck. Units, equipment, markings and doctrine are accurate for the nation and the year.
- No lockstep. A squad, battery, formation or fleet never does the same thing at the same instant; stagger timing, spacing and choices so it reads as people, not a machine.
- Every theatre matters as much as Normandy. A ground, light or effects change has to hold up in desert, steppe, snow, ruined cities, Italy and jungle. Per-place looks go through that place's own settings and leave the other grounds unchanged.
- Same inputs, same war. Outcomes never depend on frame rate, machine or draw order. Each side system keeps its own seeded random stream so it never shifts another system's dice, and fast or fused floating-point math stays off so checks run elsewhere match the Mac.

# Originality

Real WW2 units, places and people are welcome. Nothing comes from any other game or film: no names, art, UI, maps, levels, music or sound. Films and games used as references are mood words only; never fetch, trace or embed their frames or assets.

# How new work lands

- Every new feature arrives behind its own switch, off, with an entry in the release's ship list and an Options row the player can use to turn it off again.
- It goes on by default only after it has run at speed on the Mac and its stills pass the look scorecard. A feature that fails stays off, with a note saying why.
- A shipped release is never changed without the owner's explicit OK. New work goes on the next release's branch.

# The look scorecard

Score every still or clip 0, 1 or 2 on colour, light, lens, air (smoke, dust, weather), damage, scale, readable, original and speed. It ships only with no 0 and a 2 on readable, original and speed. Readable means the film never hides the game: the player can always tell who is where and what is happening. Speed means 45 fps at real battle size on the target Mac.

# The target machine

A MacBook Air, and the game is GPU-bound there, so look at GPU cost first. Timing runs need the game window in front (a background window is frame-capped) and the screen awake (a locked screen stalls the run). Don't start test passes while the owner is playing.

# Reporting to the owner

The owner is not a game developer. Lead with what they can see or play, in plain words, with a still where it helps. Leave out commit hashes and build numbers. Put any questions last, as a short ranked list with a recommendation.
"""


@agent_skill
class TheaterCommandHouseRulesSkill(unreal.AgentSkill):
    """House rules for working on Theater Command, an original WW2 grand-strategy game with real-time 3D battles.
    Apply this skill before changing anything in this project: code, scenes, looks, battle behaviour, rules data or settings."""

    instructions = _INSTRUCTIONS

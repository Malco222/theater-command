# Theater Command Skills

The project's house rules, packaged as an Agent Skill for any AI assistant working inside the Unreal editor (through Unreal 5.8's Toolset Registry and its MCP server). The skill tells that assistant what this project expects before it changes anything: where the rules really live, why the levels are empty, how new features land behind switches, the look scorecard and the speed bar.

It is **off**. Nothing here loads, and the game is unchanged, until the plugin is switched on.

## Switch it on

Add to `TheaterCommand.uproject` under `Plugins`:

```json
{ "Name": "TheaterCommandSkills", "Enabled": true }
```

That pulls in the engine's Toolset Registry. For an outside assistant to reach the editor as well, also enable `ModelContextProtocol` (the editor's MCP server) and the toolsets it should use (for example `EditorToolset`).

## Check it

With the editor open, list the skills: `TheaterCommandHouseRulesSkill` should appear with its description. Read it back and check that the text is the one in `Content/Python/theater_command_skills/skills/house_rules.py`.

## Add or change a skill

One Python file per skill under `Content/Python/theater_command_skills/skills/`, imported in that folder's `__init__.py`. The class docstring is the description the assistant sees when choosing skills; `instructions` is what it reads once the skill is chosen. Keep both short, and write only what the assistant could not learn by looking at the project itself.

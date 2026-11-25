#!/usr/bin/env python3
"""Asks the open Unreal editor, over its Claude link (MCP), for this plugin's skill.

Run it while the editor is open with the link awake (Tools/open_editor_with_link.sh):

    python3 Plugins/TheaterCommandSkills/check_skill.py [port]

It does what an assistant would do: lists the toolsets, finds the skills toolset,
lists the skills, then reads ours back and compares it with house_rules.py.
Prints PASS or FAIL lines and exits 0 only if every check passes. Read-only.
"""
import ast
import json
import os
import sys
import urllib.request

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
URL = f"http://127.0.0.1:{PORT}/mcp"
SKILL_CLASS = "TheaterCommandHouseRulesSkill"
HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "Content", "Python", "theater_command_skills", "skills", "house_rules.py")

session_id = None
next_id = 0
failures = 0


def report(ok, what):
    global failures
    print(("PASS " if ok else "FAIL ") + what, flush=True)
    if not ok:
        failures += 1


def rpc(method, params=None, notify=False):
    global session_id, next_id
    body = {"jsonrpc": "2.0", "method": method}
    if params is not None:
        body["params"] = params
    if not notify:
        next_id += 1
        body["id"] = next_id
    req = urllib.request.Request(URL, data=json.dumps(body).encode(), method="POST")
    req.add_header("Content-Type", "application/json")
    req.add_header("Accept", "application/json, text/event-stream")
    if session_id:
        req.add_header("Mcp-Session-Id", session_id)
    with urllib.request.urlopen(req, timeout=120) as resp:
        session_id = resp.headers.get("Mcp-Session-Id") or session_id
        text = resp.read().decode("utf-8", "replace")
    if notify or not text.strip():
        return None
    if text.lstrip().startswith("{"):
        msg = json.loads(text)
    else:  # event stream: the answer is the last data line
        lines = [l[5:].strip() for l in text.splitlines() if l.startswith("data:")]
        msg = json.loads(lines[-1])
    if "error" in msg:
        raise RuntimeError(f"{method}: {msg['error']}")
    return msg.get("result")


def tool(name, arguments=None):
    result = rpc("tools/call", {"name": name, "arguments": arguments or {}})
    if result and result.get("isError"):
        raise RuntimeError(f"{name}: {result}")
    if result and "structuredContent" in result:
        return result["structuredContent"]
    texts = [c.get("text", "") for c in (result or {}).get("content", []) if c.get("type") == "text"]
    joined = "\n".join(texts)
    try:
        return json.loads(joined)
    except ValueError:
        return joined


def expected_text():
    tree = ast.parse(open(SOURCE, encoding="utf-8").read())
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(getattr(t, "id", "") == "_INSTRUCTIONS" for t in node.targets):
            return ast.literal_eval(node.value)
    raise RuntimeError("no _INSTRUCTIONS in " + SOURCE)


def find_key(mapping, needle):
    if isinstance(mapping, dict):
        for key in mapping:
            if needle in key:
                return key
    return None


def unwrap(value):
    # A tool's answer may arrive as the map itself or wrapped in one field.
    if isinstance(value, dict) and len(value) == 1:
        inner = next(iter(value.values()))
        if isinstance(inner, dict):
            return inner
    return value


try:
    rpc("initialize", {"protocolVersion": "2025-06-18", "capabilities": {},
                       "clientInfo": {"name": "skill-check", "version": "1"}})
    rpc("notifications/initialized", notify=True)
    report(True, f"the editor's link answers on port {PORT}")

    toolsets = tool("list_toolsets")
    names = []
    if isinstance(toolsets, dict):
        names = [t.get("name", "") if isinstance(t, dict) else str(t)
                 for t in toolsets.get("toolsets", toolsets.keys())]
    elif isinstance(toolsets, list):
        names = [t.get("name", "") if isinstance(t, dict) else str(t) for t in toolsets]
    else:
        names = [w.strip(",:") for w in str(toolsets).split() if "Skill" in w]
    skill_sets = [n for n in names if "skill" in n.lower()]
    report(bool(skill_sets), f"a skills toolset is listed ({', '.join(skill_sets) or 'none of ' + str(len(names))})")
    toolset = skill_sets[0]

    described = json.dumps(tool("describe_toolset", {"toolset_name": toolset}))
    list_name = "list_skills" if "list_skills" in described else "ListSkills"
    get_name = "get_skills" if "get_skills" in described else "GetSkills"
    paths_arg = "skill_paths" if "skill_paths" in described else "SkillPaths"

    listed = unwrap(tool("call_tool", {"toolset_name": toolset, "tool_name": list_name}))
    path = find_key(listed, SKILL_CLASS)
    report(path is not None, f"ListSkills includes {SKILL_CLASS} ({len(listed) if isinstance(listed, dict) else '?'} skills listed)")
    if path:
        report("Theater Command" in str(listed[path]), "its description is the one in house_rules.py")

        details = unwrap(tool("call_tool", {"toolset_name": toolset, "tool_name": get_name,
                                            "arguments": {paths_arg: [path]}}))
        got = details.get(path) if isinstance(details, dict) else None
        text = (got or {}).get("Instructions") or (got or {}).get("instructions") if isinstance(got, dict) else None
        report(text is not None, "GetSkills returns its instructions")
        if text is not None:
            report(text.strip() == expected_text().strip(),
                   f"the instructions match house_rules.py word for word ({len(text.split())} words)")
except Exception as exc:  # report, never hang the run
    report(False, f"stopped: {exc}")

print("RESULT " + ("PASS" if failures == 0 else f"FAIL ({failures})"))
sys.exit(0 if failures == 0 else 1)

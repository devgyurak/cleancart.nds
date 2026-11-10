@AGENTS.md

## Claude Code specifics

- Rules in `.claude/rules/` load automatically when you touch matching paths.
- Skills live in `.agents/skills/` (shared with Codex); `.claude/skills` is a
  symlink to it. Invoke them by name.
- Subagents: `.claude/agents/` (canonical). Run `python3 scripts/sync_agents.py`
  after editing one so Codex's `.codex/agents/*.toml` stay in sync.
- The terminal has Accessibility trust but no Screen Recording or Automation
  permission: `screencapture` and `osascript` System Events don't work. Use the
  headless emulator for pictures and `tools/melonds/` for melonDS input checks.

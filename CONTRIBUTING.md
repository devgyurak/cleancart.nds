[한국어](CONTRIBUTING-KO.md)

# Contributing

This project does not take pull requests from outside. Every game here is written by the
maintainer so that all code and assets have one author and the licence stays clean.
A pull request from a fork fails the `pr-guard` check and is not merged (merging needs the maintainer's approval).

Two kinds of GitHub issues are welcome:

1. **A game request.** Tell us which hardware area an emulator needs a test for and the
   maintainer may build a game around it. Use the *Game request* form.
2. **A bug report or a question.** Something in a game is wrong, or you want to know why a
   result looks the way it does. Use the *Bug report or question* form.

## What to put in an issue

- **No code or assets.** We don't use code, art, sound or levels pasted into issues (the
  repository must stay licence-clean). Describe the behaviour, name the documentation it
  comes from (for example a GBATEK section) and give hardware results if you have them.
- **Game request:** the hardware area, the registers involved and the GBATEK section, the
  expected difficulty level (see [docs/LEVELS.md](docs/LEVELS.md)), and links to related
  emulator issues.
- **Bug report or question:** the game and release tag, the emulator name, version and renderer,
  the `judge.py` output or the QA log, a screenshot (both screens when the problem is on the bottom one), and the steps to reproduce.

The maintainer is the only person who opens pull requests here. Branch flow and commit
conventions are in [AGENTS.md](AGENTS.md) and are for the maintainer and the coding agents they use.

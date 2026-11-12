<!-- SPDX-License-Identifier: Apache-2.0 -->
# Stage overrides

`tools/mkassets.py` builds each stage from the procedural layout in
`tools/cvtools/stages.py`, unless a `stageN.json` file exists here.

```sh
python3 tools/stage_editor.py export 2 tools/stages/stage2.json   # start from the current layout
# edit heights / spawns
python3 tools/stage_editor.py import tools/stages/stage2.json      # validate + ASCII preview
./build.sh                                                          # the ROM now uses the edit
```

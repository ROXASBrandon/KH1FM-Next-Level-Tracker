# Development

Public release v0.1.2 contains the same native helper as the installed layout update. Internal filenames remain `kh1_next_level_popup.lua` and `kh1_next_level_popup.dll` so existing builds retain their payload names.

Install `ziglang==0.16.0` for Python, then run `python native/build.py`. The build runs EXP/transition and HUD layout/fade tests; Windows builds also run the synthetic renderer probe. Run `python tools/package.py` to rebuild the OpenKH ZIP and checksum.

The helper verifies the supported executable, chains the game frame callback, and redirects one guarded native notification render call. Only notifications containing its private title/body pointers use the minimal renderer. Other native notifications receive their original arguments. EXP tables and save data are read-only. Removing the helper requires a full game restart.

Tests and signatures do not establish every menu or HUD compatibility case. F6 enables a quick visual check with Scan/lock-on active.

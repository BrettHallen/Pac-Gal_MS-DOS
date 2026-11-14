# Analysis tools

You need Python 3.10+ and `pip install unicorn capstone`.

- `emu.py`: a minimal real-mode DOS/BIOS emulator, just enough to run `Pac-gal.exe` headless. It covers the INT 10h text video, INT 16h keyboard, INT 21h, IRQ0 timer, IRQ1 keyboard via port 0x60, and PIT/speaker port logging.
- `harness.py`: helpers on top of the emulator to read and poke the game's variables and to read the 80×25 screen. DGROUP sits at segment 0x8FF0 when the image is loaded at 0x1000.
- `annot.py`: the annotated disassembler. Run it as
  `python3 annot.py <image.bin> names.json varnames.json`.
  `image.bin` is the EXE load image without its header: skip the first 0x1600 bytes of `Pac-gal.exe`.
- `mml.py`: an IBM BASIC `PLAY` string parser that returns (Hz, seconds) lists.

Quick start:

```python
import sys; sys.path.insert(0, 'Analysis/tools')
from harness import *
e = H('Original/Pac-gal.exe', keys=keystr(50, "1000\r") + [(90, LEFT)])
e.run(max_ticks=200)
print(e.text()); print(e.state())
```

The emulator's sense of time is approximate, so timing-dependent behaviour (music, the 15-second wait) runs at an arbitrary speed.

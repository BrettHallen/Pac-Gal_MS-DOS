# PAC-GAL (Al J. Jiménez, May 1982): how it works

A reverse-engineering write-up of `Original/Pac-gal.exe`, written as a spec for rebuilding the game on a modern Mac.

Everything below comes from the binary, in three ways:

1. A static, annotated disassembly of the compiled program (`pacgal_user_code.asm`).
2. Runs of the real EXE in a small headless 8086/DOS emulator, used to check every number quoted here. That emulator is in `tools/`.
3. A reconstructed BASIC source (`PACGAL_reconstructed.bas`). It runs to completion in PC-BASIC, the GW-BASIC emulator.

---

## 1. Summary

- **What it is:** a 1982 IBM PC program written in **IBM BASIC** and compiled with the **IBM Personal Computer BASIC Compiler** (Microsoft BASCOM 1.x). The runtime library is linked into the EXE. Nothing is drawn with graphics: the game runs in **80×25 colour text mode**, using code-page-437 characters.
- **No score:** the only counters are **dots remaining** (468 at the start) and **lives** (3).
- **One board:** if you clear the board, "You did it!!!" appears for 15 seconds and the game then asks "Play again???". There is no second level and the difficulty doesn't carry over.
- **The screen is the map.** No maze array exists anywhere. Every collision test calls BASIC's `SCREEN(row,col)` to read the character at a cell, and `SCREEN(row,col,1)` to read its colour.
- **Ghosts move once for every Pac-Gal step**, and get extra moves the longer you go without dying or eating a ghost: one more every 180 seconds.
- **Turns are buffered, as in the arcade game.** The last arrow key pressed is remembered and applied as soon as that turn is possible.
- **The "ghost randomness" variable `P` is always 0.** Several lines of code double or halve it, but it starts at 0 and is an integer, so it never changes. In practice the ghosts **always** try to chase.

---

## 2. The executable

| Item | Value |
|---|---|
| File | `Pac-gal.exe`, 39,296 bytes, MZ executable |
| Header | 5,632 bytes, 1,361 relocations, entry `0000:001A`, stack `0863:0200` |
| Toolchain | IBM BASIC Compiler runtime: error-message table ("RETURN without GOSUB", …), "Licensed Material Program Property of…" banner, and the BASIC `RANDOMIZE` prompt "Random Number Seed (-32768 to 32767)" |

Segment layout of the load image:

| Segment | Size | Contents |
|---|---|---|
| `0000` | 0x2EC0 (11.9 KB) | **Compiled game code** |
| `02EC` | ~18.7 KB | BASIC runtime (PRINT, LOCATE, SCREEN, PLAY, SOUND, floating point, event traps, keyboard, timer) |
| `077E` | 0x1E0 | Constant pool: string descriptors, string literals, MBF floating-point constants |
| `079C` | small | Start-up stub. It copies the constant pool and jumps into the runtime, which moves DGROUP to the top of memory. |
| `0845`, `0863` | – | Copy area and stack |

### 2.1 What compiled BASCOM code looks like

- **Each BASIC statement starts with an `INT 3` byte (`CC`).** The runtime hooks INT 3 and uses it to check for pending event traps (`ON KEY`). This is how the arrow keys reach the game: the runtime also hooks the keyboard interrupt (INT 9), recognises the cursor-key scan codes and sets the trap flags.
- **Integer expressions are compiled inline** (`mov ax,[var]; add ax,…`). Every variable is a 16-bit integer, which means the program has `DEFINT A-Z`.
- **Everything else is a far call into the runtime**, with arguments in BX/DX/CX. A few calls are followed by inline bytes: floating-point temporary-slot numbers, `GOSUB` target addresses and `INPUT` type lists.
- **Strings** are 4-byte descriptors (length, pointer). **Floats** are Microsoft Binary Format: single precision, plus double precision for the time function.

Runtime entry points identified (offsets in segment `02EC`):

| Offset | BASIC | Offset | BASIC |
|---|---|---|---|
| `2A42` / `28FA` / `28F5` / `28E6` / `290E` | `PRINT` begin / string `;` / integer `;` / string `,` / string + newline | `039A`+`03B4` | `LOCATE r,c` |
| `208B`+`20A5` | `COLOR f,b` | `200E` | `CLS` |
| `216F` | `KEY OFF` / `KEY ON` | `2A0C` | `PRINT USING` |
| `0262` | string `LET` | `0298` | `+` (concat) |
| `1245` | `CHR$` | `1361` / `1368` | `STRING$(n,code)` / `STRING$(n,s$)` |
| `23C0` | `SCREEN(r,c[,z])` | `1228` | `INKEY$` |
| `02D2` | string compare | `1399` | `VARPTR$` |
| `249D` | `PLAY` | `1EBA` | `SOUND` |
| `09C1` | `RND(x)` | `0A43` | `RANDOMIZE n` |
| `2702` | `CINT` (single precision) | `26CF` | `CINT` (double precision) |
| `1185` | `GOSUB` (target inline) | `11AE` | `RETURN` |
| `1770` / `17AC` | `ON KEY(n) GOSUB` / `KEY(n) ON` | `1495`, `12F6`, `12FF`, `130D`, `0EEB` | `TIME$`, `LEFT$`, `RIGHT$`, `MID$`, `VAL` |
| `2451`/`27BE`/`2885` | `INPUT` | `08E7` | `END` |

---

## 3. Program flow

```
start ─► CLS, "How fast (0-30000)? " INPUT SP
      ─► set tune strings
      ┌► CLS, intro animation (with intro tune)                ◄─────────────┐
      │  draw maze, power pellets, status line                                │
      │  init lives/dots/ghosts, KEY(11..14) ON                               │
      │  MAIN LOOP:  delay(SP) → move Pac-Gal → move ghosts ×(1+⌊(t−1)/180⌋)  │
      │      ├─ collision with ghost ─► blinking? eat it : lose a life        │
      │      ├─ last life lost ──────────────────────► "Play again???" ──y/Y──┘
      │      └─ dots = 0 ─► "You did it!!!" (15 s) ──► "Play again???"   n ─► "...Hope you had a good time", END
```

### 3.1 Start-up

1. `CLS`, then `PRINT "How fast (0-30000)";:INPUT SP`. `SP` is the number of passes through an empty `FOR` loop run before every step: 0 is fastest, 30000 slowest.
2. Three `PLAY` strings are stored: the eat-dot chirp, the power-pellet jingle and the intro tune (§7).
3. **Intro animation** on row 14, with the intro tune playing in the background:
   - `☻` (CHR$ 2) runs **right to left** from column 60 to column 1.
   - The tune restarts. The parade ` ♥   ♦♣♠       ☻` then moves **left to right** from column 1 to 60: Pac-Gal in front, the four suit-symbol ghosts behind.
   - Each animation step waits `FOR J=1 TO 25`.
4. `CLS` and `KEY OFF`, which frees row 25. Row 25 gets `    P A C - G A L ` in bright white (COLOR 15) at column 28, then `        Al J. Jiménez, May 1982` in grey (é is CHR$ 130).
5. The maze is drawn (§4) and the 10 power pellets are over-printed in COLOR 26 (blinking light green).
6. `LV=3`, `DT=468`, `P=0`, `T0=FNT(2)`, `RANDOMIZE T0`. The status line shows `dots 468` and `☺☺☺` at column 15.
7. Entities are initialised (§5), the four cursor keys are armed and the main loop starts.

"Play again" with `y`/`Y` jumps back to step 3. The speed prompt is not asked again.

---

## 4. Screen and maze

The play-field is screen rows **1–24**. Row **25** is the status line. Moving pieces live on a **24 × 40 logical grid**: logical cell `(R, C)` is screen row `R` and screen column **`2·C+1`** (1-based). Even screen columns only carry the joining strokes of walls. Every dot sits in an odd column.

Exact screen at the start of play. Pac-Gal and the ghosts aren't drawn until the first game step:

```
▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄
█ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ █
█ ∙ ║ ∙ ╔══════ ∙ ║ ∙ ════════╦════ ∙ ║ ∙ ════╦════════ ∙ ║ ∙ ════════╗ ∙ ║ ∙ █
█ ∙ ║ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ ∙ ∙ ║ ∙ ∙ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ ║ ∙ █
█ ∙ ║ ∙ ║ ∙ ═══════════════ ∙ ║ ∙ ════╩════ ∙ ║ ∙ ═════════════════ ∙ ║ ∙ ║ ∙ █
█ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ █
█ ∙ ║ ∙ ═════════ ∙ ══════╗ ∙ ╔═══════╦═══════╦════ ∙ ╔════ ∙ ═════════ ∙ ║ ∙ █
█ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ╔═╝ ∙ ║ ∙ ∙ ∙ ║ ∙ ∙ ∙ ║ ∙ ∙ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ █
█ ∙ ║ ∙ ║ ∙ ════════╗ ∙ ║ ∙ ∙ ∙ ∙ ║ ∙ ∙ ∙ ║ ∙ ∙ ∙ ║ ∙ ║ ∙ ╔════════ ∙ ║ ∙ ║ ∙ █
█ ∙ ∙ ∙ ║ ∙ ∙ ∙ ∙ ∙ ║ ∙ ║ ∙ ║ ∙ ══╩═══════╩════ ∙ ║ ∙ ║ ∙ ║ ∙ ∙ ∙ ∙ ∙ ║ ∙ ∙ ∙ █
▀▀▀▀▀▀▀▀▀ ∙ ║ ∙ ║ ∙ ∙ ∙ ║ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ ║ ∙ ∙ ∙ ║ ∙ ║ ∙ ▀▀▀▀▀▀▀▀▀
─── ∙ ∙ ∙ ∙ ║ ∙ ║ ∙ ║ ∙ ║ ∙ ║ ∙ ╔════     ════╗ ∙ ║ ∙ ║ ∙ ║ ∙ ║ ∙ ║ ∙ ∙ ∙ ∙ ───
▄▄▄▄▄▄▄▄▄ ∙ ║ ∙ ║ ∙ ║ ∙ ║ ∙ ∙ ∙ ║             ║ ∙ ∙ ∙ ║ ∙ ║ ∙ ║ ∙ ║ ∙ ▄▄▄▄▄▄▄▄▄
█ ∙ ∙ ∙ ║ ∙ ∙ ∙ ║ ∙ ║ ∙ ║ ∙ ║ ∙ ║             ║ ∙ ║ ∙ ║ ∙ ║ ∙ ║ ∙ ∙ ∙ ║ ∙ ∙ ∙ █
█ ∙ ║ ∙ ║ ∙ ║ ∙ ∙ ∙ ║ ∙ ║ ∙ ║ ∙ ║             ║ ∙ ║ ∙ ║ ∙ ║ ∙ ∙ ∙ ║ ∙ ║ ∙ ║ ∙ █
█ ∙ ║ ∙ ║ ∙ ╚════ ∙ ║ ∙ ║ ∙ ║ ∙ ╚═════════════╝ ∙ ║ ∙ ║ ∙ ║ ∙ ════╝ ∙ ║ ∙ ║ ∙ █
█ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ █
█ ∙ ║ ∙ ════════════╝ ∙ ╚═════════════════════════════╝ ∙ ╚════════════ ∙ ║ ∙ █
█ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙   ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ █
█ ∙ ║ ∙ ║ ∙ ═══════════════ ∙ ║ ∙ ════╦════ ∙ ║ ∙ ═════════════════ ∙ ║ ∙ ║ ∙ █
█ ∙ ║ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ ∙ ∙ ║ ∙ ∙ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ ║ ∙ █
█ ∙ ║ ∙ ╚══════ ∙ ║ ∙ ════════╩════ ∙ ║ ∙ ════╩════════ ∙ ║ ∙ ════════╝ ∙ ║ ∙ █
█ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ║ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ █
▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
dots 468      ☺☺☺             P A C - G A L         Al J. Jiménez, May 1982
```

Machine-readable copies are in `maze.txt` (UTF-8), `maze_ascii.txt` (ASCII, one character per screen cell) and `maze.json` (CP437 codes and colour attributes for all 80×25 cells).

`maze_ascii.txt` legend:

| ASCII | CP437 | Glyph | Role |
|---|---|---|---|
| `#` | 219 | █ | outer side wall |
| `_` | 220 | ▄ | top wall, tunnel-mouth walls |
| `^` | 223 | ▀ | bottom wall, tunnel-mouth walls |
| `=` | 205 | ═ | horizontal wall |
| `\|` | 186 | ║ | vertical wall |
| `r` `7` `L` `J` | 201 187 200 188 | ╔ ╗ ╚ ╝ | corners |
| `T` `W` | 203 202 | ╦ ╩ | T-junctions |
| `-` | 196 | ─ | **tunnel marker** (row 12, columns 1–3 and 77–79) |
| `.` | 249 | ∙ | dot (468 in total, including the 10 power pellets) |
| space | 32 | | empty floor |

Key positions, as (row R, logical column C → screen column 2C+1):

| What | Where |
|---|---|
| Pac-Gal start / respawn | (19, 19), screen column 39. It's the gap in row 19 with no dot. |
| Ghost start | (14, 18), (14, 19), (14, 20), (14, 21) |
| Ghost respawn after you die | (14, 19…22) for ghosts 2…5. One column further right than at the start. |
| Ghost return after being eaten | (14, 18…21) |
| Ghost-house door | row 12, columns 38–42 blank (C = 19, 20) |
| Power pellets (10) | (2,15) (2,25) (6,1) (6,38) (12,9) (12,30) (19,1) (19,38) (23,15) (23,25) |
| Tunnel | row 12. Stepping onto `─` at C=1 or C=38 sends you to C = 39 − current C (C=2 ↔ C=37). |

The original program builds each row out of short string variables (`" "`, `█`, `" ║"`, `" ∙"`, `" █"`, `══`, `═`, `─`) printed with about 900 `PRINT` items. Row 24 is written with `PRINT USING "&"`, presumably so that printing there can't scroll the screen.

---

## 5. Entities and variables

Every variable is an integer (`DEFINT A-Z`). Ten parallel arrays are indexed 1…5: **1 = Pac-Gal**, **2–5 = ghosts**. The names are mine; the addresses are DGROUP offsets.

| Array | Addr | Meaning | Pac-Gal init | Ghost k init |
|---|---|---|---|---|
| `CH()` | 094C | character code | 1 (☺) | k+1 → 3 ♥, 4 ♦, 5 ♣, 6 ♠ |
| `CL()` | 0958 | colour | 7 | 7 (26 while frightened) |
| `R()` `C()` | 0964 / 0970 | position | 19, 19 | 14, k+16 |
| `U()` | 097C | character underneath (to restore) | 32 | 32 |
| `F()` | 0988 | fright timer, in ghost steps | 0 | 0 |
| `DR()` `DC()` | 0994 / 09A0 | current direction | 0, 0 | random: either (0, ±1) or (±1, 0) |
| `KR()` `KC()` | 09AC / 09B8 | **wanted** direction (last arrow key) | 0, 0 | unused |

Scalars: `SP` speed, `LV` lives, `DT` dots left, `P` randomness (always 0), `T0` timer start, `EL` elapsed time, `S` character read by `SCREEN`, `NR`/`NC` candidate cell, `SG` random sign, `K` loop index.

Random numbers are always `RND(2)` assigned to an integer. BASIC's `RND(x>0)` returns a fraction in 0–1 whatever `x` is, and assigning it to an integer rounds it. So `SG=RND(2)` is a 50/50 choice of 0 or 1 (and `IF SG=0 THEN SG=-1` turns it into ±1). The author may have expected `RND(2)` to give a whole number in some range, as it does in other BASICs. It works anyway.

---

## 6. Game rules, exactly as coded

### 6.1 Main loop and timing

```
loop: GOSUB delay            ' D=0: FOR L=1 TO SP: D=D+1: NEXT
      GOSUB movePacGal
      EL = FNT(2) - T0 : IF EL <= 1 THEN EL = 1
      FOR T = 1 TO EL STEP 180 : GOSUB moveGhosts : NEXT
      GOTO loop
```

- **One step per loop.** Pac-Gal moves at most one cell. Each ghost moves at most one cell per `moveGhosts` call.
- **Ghosts speed up over time.** They make `1 + ⌊(EL−1)/180⌋` moves per Pac-Gal step: 1 for the first 3 minutes, 2 from 3 minutes, 3 from 6 minutes, and so on. `T0` is reset at the start, after every death, after eating a ghost, and when the board is cleared.
- **The clock function** is `FNT = VAL(hh)*360 + VAL(mm)*60 + VAL(ss)`, built from `TIME$`. It multiplies hours by **360, not 3600** (the constant in the EXE is the double-precision value 360.0). Within an hour this is a correct seconds count. Across an hour boundary `EL` goes negative and is clamped to 1, which silently resets the speed-up. The `*360` may also have been deliberate: with `*3600` the integer would overflow from about 9:06 a.m.
- **Real-world speed.** There's no frame timer: speed depends entirely on the CPU. In the emulator one step at `SP=0` runs about 9,400 x86 instructions plus about 62 BIOS video calls. Each unit of `SP` adds about 18 instructions. On a 4.77 MHz PC that is very roughly 15 steps per second at `SP=0` and about 8 per second at `SP=1000`. That's an estimate, not a measurement. Players with modern machines and DOSBox report using 2000–7000.

### 6.2 Moving Pac-Gal (once per loop)

The arrow keys are event traps that only **set the wanted direction**: Up (−1,0), Left (0,−1), Right (0,+1), Down (+1,0). Movement then happens like this:

1. **Try the wanted direction** `(KR,KC)`. Read `S = SCREEN(NR, 2·NC+1)`.
   - `S` is 32 (space) or 249 (dot): accept the move and make it the current direction.
   - `S` is 3–6 (a ghost): **collision** (§6.4). This fires even though Pac-Gal hasn't turned yet.
   - Row 12 and `S` is 196 (tunnel): wrap `NC = 39 − C` and re-test.
2. Otherwise **try the current direction** `(DR,DC)` with the same tests. If it's free, move without changing direction.
3. Otherwise **stop**: `DR=DC=0` and `SOUND 4000,.2`, a tiny click. Standing still against a wall, or never having moved, clicks on every step.
4. **On a move:**
   - If the target's colour attribute is greater than 7 (only power pellets are, blinking green 0x8A), it's a **power pellet**: `PLAY` the power jingle and set all four ghosts to COLOR 26 with `F = CINT((DT/5 + 20) / LV²)`.
   - Erase the old cell (print `U(1)`, always a space), draw ☺ at the new cell in colour 7, `U(1)=32`.
   - If `S=249`: `DT=DT−1`, print the count at (25,5), `PLAY` the eat chirp. Otherwise `SOUND 150,.1`, a short tick for every step onto empty floor.
   - If `DT<1`: **board cleared** (§6.5).

Fright time worked through (confirmed in the emulator):

| Dots left | Lives | F (ghost steps) |
|---|---|---|
| 468 | 3 | 13 |
| 468 | 2 | 28 |
| 468 | 1 | 114 |
| 100 | 3 | 4 |
| 10 | 1 | 22 |

Fright lasts longer early in the board and **much** longer with fewer lives. `F` is counted in ghost moves, so it runs out faster once the time-based speed-up starts.

### 6.3 Moving the ghosts (`FOR K = 2 TO 5`)

Each ghost, every call:

```
F(K) = F(K) − 1                              ' counts below 0 forever; only "<= 0" matters
NR,NC = R(K),C(K)
if 18 <= C(K) <= 23:                         ' ghost-house columns
    if R(K) is 11 or 12 → STRAIGHT           ' push out through the door
    if 13 <= R(K) <= 15 → RANDOM TURN        ' jiggle about inside the house
if RND < P → STRAIGHT                        ' P is always 0, so never taken
CHASE-ROW: if DR(K)=0 and R(K)<>R(1):  NR = R(K)+SGN(R(1)−R(K)), NC = C(K)
           free (32/249) → ACCEPT;  Pac-Gal (1) → HIT
CHASE-COL: if DC(K)=0 and C(K)<>C(1):  NR = R(K), NC = C(K)+SGN(C(1)−C(K))
           free → ACCEPT;  Pac-Gal → HIT
STRAIGHT:  NR,NC = R+DR, C+DC              free → ACCEPT;  Pac-Gal → HIT
RANDOM TURN: SG = ±1 (coin flip)
           (R+DC·SG, C+DR·SG)   one perpendicular   free → ACCEPT; Pac-Gal → HIT
           (R−DC·SG, C−DR·SG)   the other one        "
           (R−DR,    C−DC)      reverse              "
           else stay put (direction unchanged)
ACCEPT:  DR,DC = NR−R, NC−C
DRAW:    print U(K) at the old cell (in colour 7)
         if F(K) <= 0 → CL(K) = 7              ' fright over
         draw CH(K) in CL(K) at the new cell; U(K) = what was there (32 or 249)
HIT:     NR,NC = the ghost's own cell; go to COLLISION
```

What this produces:

- **Ghosts only turn toward Pac-Gal at a junction**, never by reversing. A ghost moving horizontally can only take a vertical chase step, and the reverse. The row step is tried before the column step.
- **Frightened ghosts chase too.** Nothing in the AI checks `F`, so blinking ghosts still home in on you.
- **Ghosts treat everything other than space and dot as a wall:** other ghosts, the tunnel `─` and the walls. So ghosts never use the tunnel and never overlap one another.
- **The ghost-house rule is a column band** (C 18–23), not a box. A ghost on rows 11–12 in those columns goes straight on; on rows 13–15 it only makes random turns until it happens to line up with the door.

### 6.4 Collision

This is reached three ways: Pac-Gal steps toward a ghost cell, a ghost steps toward Pac-Gal, or the wanted-direction test finds a ghost. `NR,NC` is the ghost's cell.

- **Ghost is blinking** (`SCREEN(NR,2NC+1,1) > 7`): **eat it.**
  - Play a rising chromatic run over three octaves.
  - Flash a blinking ☻ at that cell (100 redraws).
  - Reset `T0`. `P=P*.5` (a no-op).
  - **If `LV<3`, gain a life** and redraw the ☺ row.
  - For every ghost on that cell: back to colour 7 at (14, k+16). If it was standing on a dot, that **dot counts as eaten** (`DT−1`).
  - Every ghost's `F` also drops by 1.
  - Pac-Gal then **jumps onto the ghost's cell**. When the ghost bumped into Pac-Gal, that cell is next to Pac-Gal, so this is a one-cell move outside the normal movement rules.
- **Otherwise you die.**
  - Death tune. Erase ☺, then 200 redraws of a blinking `¿` (CHR$ 168) at the ghost's cell.
  - `IF DT>300 AND P<.1 THEN P=P*2` (a no-op).
  - If `LV<=1` → **game over**.
  - Otherwise `LV−1`, redraw the lives. Every entity restores its under-character and resets (ghosts to (14, k+17), frightened state cleared). Pac-Gal goes back to (19,19), stopped, with no wanted direction. `T0` is reset.
  - The dots stay as they were.

### 6.5 Board clear, game over, play again

- **`DT` reaches 0:** at (25,10), in blinking green, `You did it!!!`. Wait 15 seconds (by `FNT`), then go on to game over.
- **Game over:** at (25,10), in blinking green, `Play again???`.
  - `POKE 106,0`, the BASICA idiom for flushing the keyboard buffer.
  - Then wait on `INKEY$`. `y`/`Y` restarts from the intro. Only lowercase `n` quits: it does `CLS`, `KEY ON`, prints `...Hope you had a good time` and `END`s. Any other key, including `N`, is ignored.

### 6.6 Status line (row 25)

| Column | Content |
|---|---|
| 1 | `dots` + count (BASIC number format with leading and trailing space). The count is updated at column 5. |
| 15 | `STRING$(LV, 1)` (☺ per life) + 4 spaces |
| 10 | "You did it!!!" / "Play again???" (over-prints the dots area) |
| 28 | title (bright white) + author (grey) |

---

## 7. Sound

All music uses `PLAY` with **MB (music background)**, so the game never waits for it. `SOUND` also goes into the background queue. IBM BASIC rules: octave 3 starts at middle C, the default is O4, T is quarter notes per minute, MN plays each note for 7/8 of its length. PLAY state (octave, tempo, length) **carries over between PLAY statements**.

| When | String | What it sounds like |
|---|---|---|
| Intro (×2) | `mbt190o2l8bbbl16cecl8bbp8bbbl16cl8edcc` | B3 B3 B3 C3 E3 C3 B3 B3 (rest) B3 B3 B3 C3 E3 D3 C3 C3; about 2.4 s |
| Eat a dot | `t255mbl64o1afgao4d` | 5 very short notes A2 F2 G2 A2 D5; about 0.07 s |
| Power pellet | `mbl64abceabceebceabceagaa` | 20 fast notes (octave and tempo from the previous tune, usually O4 T255 after an eat); about 0.3 s |
| Eat a ghost | `"mbl24o2x"+VARPTR$(UP$)+"l32o3x"+VARPTR$(UP$)+"l64o4x"+VARPTR$(UP$)`, `UP$="cc#dd#eff#gg#aa#b"` | a chromatic scale played three times, each an octave higher and faster; about 1 s |
| Death | `mbl8t255o4fego3abcdefgo0l1g-g` | F5 E5 G5, then A4 B4 C4…G4, then two whole-note rumbles G♭1 and G1 (46 and 49 Hz); about 3 s |
| Step onto empty floor | `SOUND 150,.1` | 150 Hz, about 5 ms tick |
| Blocked or standing still | `SOUND 4000,.2` | 4 kHz, about 11 ms click |

A second scale, `DN$="ba#ag#gf#fed#dc#c"`, is defined but never played.

---

## 8. Colours and glyphs

| Thing | Glyph (CP437) | Colour |
|---|---|---|
| Walls, dots, text | box drawing, ∙ 249 | 7 (light grey on black) |
| Power pellet | ∙ 249 (and the space before it) | 26 = blink + light green (attribute 0x8A) |
| Pac-Gal | ☺ 1 (☻ 2 in the intro and the eat-ghost flash) | 7 (flash in 26) |
| Ghosts | ♥ 3, ♦ 4, ♣ 5, ♠ 6 | 7; 26 while frightened |
| Death marker | ¿ 168 | 26 |
| Title | "P A C - G A L" | 15 (bright white) |
| Messages | "You did it!!!", "Play again???" | 26 |

The game tells frightened ghosts and power pellets apart from everything else **only** by colour (attribute > 7). A recreation has to keep that information somewhere if it doesn't use a real character/attribute screen buffer.

---

## 9. Quirks and bugs (keep them or fix them, but know they're there)

1. **`P` never changes from 0.** Ghosts always chase. The intended difficulty curve is dead code: less chasing after deaths early in the board, more below 50 dots and after eating ghosts.
2. **`FNT` uses hours×360.** The time-based speed-up resets at every hour boundary.
3. **Ghosts erase power pellets.** A ghost crossing a pellet restores the `∙` in colour 7, so the pellet loses its power.
4. **Frightened ghosts still chase.** The only effect of fright is that touching one eats it.
5. **Eating a ghost can teleport Pac-Gal one cell** onto the ghost's square. If the ghost was on a dot, that dot is counted.
6. **Pac-Gal can walk into the ghost house** through the door. Nothing blocks the player there.
7. **A ghost in the buffered-turn direction counts as a collision**, even if Pac-Gal is carrying straight on.
8. **The respawn column differs** after a death (C = k+17) from the start and from eaten-ghost returns (C = k+16).
9. **The `F()` timers run below zero forever.** Harmless.
10. **Only lowercase `n` quits.**
11. **No score, no second level.** Bonus lives come only from eating ghosts, capped at 3.
12. **The lives redraw after a death ends with a comma** (`PRINT STRING$(LV,1);A$;A$;A$;A$,`). That pads with spaces to the next 14-column print zone, so it blanks row 25 up to column 28. This is invisible, but a recreation that compares screens cell by cell has to do the same.

---

## 10. Rebuilding it on a modern Mac

### 10.1 Suggested architecture

- **Model the 80×25 text screen** as a `[[Cell]]` buffer where `Cell = (code: UInt8, attr: UInt8)`. Port the game logic almost line for line from `PACGAL_reconstructed.bas`, with `screen(r,c)` and `screenAttr(r,c)` reading that buffer. This is the most faithful approach and the quickest to get right, because the original's behaviour, including the quirks above, depends on reading the screen.
- **Render** the buffer with SpriteKit, a SwiftUI `Canvas`, or a Metal or CoreText grid. Use a real CP437 font such as **Px437 IBM VGA 8x16** or **IBM CGA** from the *Ultimate Oldschool PC Font Pack* (int10h.org, CC BY-SA 4.0), or map codes to Unicode (the table in §4 and `maze.txt` already do). Draw at 640×400 with a 4:3 aspect so it looks like a CGA screen.
- **Blink:** attribute bit 7 means blink. On real hardware that's a hardware blink of roughly 2 Hz. Use a timer to toggle visibility of blinking cells.
- **Input:** `NSEvent` key-down on the arrow keys sets `(KR,KC)` straight away, like the event trap. Don't move on key-repeat. The game's own loop does the moving.
- **Timing:** run a fixed game step on a timer instead of the busy-wait. 8–12 steps per second feels like an original PC at `SP≈1000`. Keep a "speed" setting. Keep the 180-second extra-ghost-move rule with a real monotonic clock. Decide whether to keep the hour-boundary reset (§9.2).
- **Sound:** a square-wave synth on `AVAudioEngine` (`AVAudioSourceNode`) with a small MML player. `tools/mml.py` is a reference parser for exactly the strings above, including state carry-over and `X`+`VARPTR$`. Queue notes so they play in the background.

### 10.2 Order of operations to copy

Getting this order right is what makes it play like the original:

1. Delay.
2. Pac-Gal: wanted direction, then current direction, then stop. Draw. Count the dot. Check for the board being clear.
3. Ghosts 2→5 in index order, each one erasing, then deciding, then drawing, using the screen as it is at that moment. Later ghosts see where earlier ghosts moved this step.
4. Extra ghost passes from the 180-second rule.

### 10.3 Test vectors for your port

- Board starts with 468 dots, 10 of them power pellets. Lives = 3.
- Pac-Gal starts at (19,19), stopped. Ghosts start at (14,18…21), each heading a random one of four directions.
- From (12,2) moving left, the next step lands at (12,37) (tunnel).
- Fright with 468 dots and 3 lives = 13 ghost steps. With 1 life = 114 (see §6.2).
- Eating a ghost with 2 lives gives 3 lives. With 3 lives it stays at 3.
- Standing still beside a ghost gets you caught. A ghost reaching you calls the same collision code.

### 10.4 Things you might choose to improve (not original behaviour)

- Give `P` a real starting value, such as 0.25, so the intended difficulty tweaks work.
- Fix the clock (hours×3600 using a real timer), stop ghosts erasing pellets, add a score and further levels.

---

## 11. Files

| File | What it is |
|---|---|
| `PAC-GAL_Analysis.md` | this document |
| `PACGAL_reconstructed.bas` | complete reconstructed program (GW-BASIC syntax, ASCII, CRLF). Runs in PC-BASIC. |
| `pacgal_user_code.asm` | annotated disassembly of the 11.9 KB of game code. Runtime calls and variables are named; string constants are shown inline. |
| `maze.txt`, `maze_ascii.txt`, `maze.json` | the maze as text, as ASCII mnemonics, and as raw codes and attributes |
| `tools/emu.py`, `tools/harness.py` | headless unicorn-based DOS/BIOS emulator that runs the original EXE, with helpers to read and poke game variables |
| `tools/annot.py`, `tools/names.json`, `tools/varnames.json` | the disassembly annotator and its name tables |
| `tools/mml.py` | IBM BASIC `PLAY` string parser that gives frequency and duration lists |
| `../Remake/` | a C + SDL3 port built from this spec. It runs in a window on macOS, Linux and Windows and was checked step by step against the original EXE. |

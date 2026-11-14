"""compare_trace.py - check the PAC-GAL remake against the original EXE.
Remake using Claude Opus 5.5, Nov 2026
Guided by Brett Hallen

Run the ORIGINAL Pac-gal.exe (in the Analysis/tools emulator) with the same
scripted input as tests/trace.c and compare the game state step by step.

usage: python3 compare_trace.py <Pac-gal.exe> <analysis-tools-dir> <pacgal_trace binary> [steps] [dump-step] [key-seed] [rnd-seed]
"""
import subprocess, sys

exe, tools, trace_bin = sys.argv[1:4]
steps = int(sys.argv[4]) if len(sys.argv) > 4 else 300
sys.path.insert(0, tools)
from harness import *  # noqa: E402

class LCG:
    def __init__(self, x): self.x = x
    def next(self):
        self.x = (self.x * 1103515245 + 12345) & 0x7fffffff
        return self.x

KSEED = int(sys.argv[6]) if len(sys.argv) > 6 else 777
RSEED = int(sys.argv[7]) if len(sys.argv) > 7 else 12345
lr, lk = LCG(RSEED), LCG(KSEED)
DUMP = int(sys.argv[5]) if len(sys.argv) > 5 else -1
dump = []
DIRS = [(-1, 0), (0, -1), (0, 1), (1, 0)]
e = H(exe, keys=keystr(50, "0\r"))
out, n = [], [0]
BASE = 0x10000

def rows():
    m = e.uc.mem_read(0xB8000, 4000)
    s = 0
    for i in range(2000):
        ch = m[2 * i] or 32
        s = (s * 31 + (ch | m[2 * i + 1] << 8)) % 1000000007
    return s

def record(step):
    st = e.state()
    parts = [str(step)]
    for k in range(5):
        parts.append(','.join(str(st[f][k]) for f in ('R', 'C', 'DR', 'DC', 'UNDER', 'FRT', 'COL')))
    parts.append(f"LV={st['lives']} DT={st['dots']} scr={rows()}")
    out.append(' '.join(parts))
    if step == DUMP:
        m = e.uc.mem_read(0xB8000, 4000)
        for r in range(25):
            dump.append(f'ROW{r+1:02d}' + ''.join(f' {(m[2*(r*80+c)] or 32):02x}{m[2*(r*80+c)+1]:02x}' for c in range(80)))

def loop_top(uc, addr, size, ud):
    n[0] += 1
    if n[0] > 1:
        record(n[0] - 1)
    if n[0] > steps:
        e.done = True; e.uc.emu_stop(); return
    if n[0] % 6 == 1:
        d = (lk.next() >> 17) % 4
        e.setvar(0x9ae, DIRS[d][0]); e.setvar(0x9ba, DIRS[d][1])
    if e.var(0xa00) == 1:
        e.setvar(0xa00, 3)

def sg_hook(uc, addr, size, ud):
    e.setvar(0x9d2, (lr.next() >> 16) & 1)

def dr_hook(uc, addr, size, ud):
    k = e.var(0x9ce)
    e.setvar(0x994 + 2 * k, (lr.next() >> 16) & 1)

e.uc.hook_add(UC_HOOK_CODE, loop_top, None, BASE + 0x2097, BASE + 0x2097)
for a in (0x1f5e, 0x2aa0):
    e.uc.hook_add(UC_HOOK_CODE, sg_hook, None, BASE + a, BASE + a)
e.uc.hook_add(UC_HOOK_CODE, dr_hook, None, BASE + 0x1f95, BASE + 0x1f95)
e.run(max_ticks=10 ** 7)

port = subprocess.run([trace_bin, str(steps), str(DUMP), str(KSEED), str(RSEED)], capture_output=True, text=True).stdout.split('\n')
portdump = [l for l in port if l.startswith('ROW')]
port = [l for l in port if not l.startswith('ROW')]
for a, b in zip(dump, portdump):
    if a != b:
        ca, cb = a.split()[1:], b.split()[1:]
        print(a.split()[0], [(i + 1, x, y) for i, (x, y) in enumerate(zip(ca, cb)) if x != y])
bad = 0
for i, line in enumerate(out):
    if i >= len(port) or port[i] != line:
        print('MISMATCH at step', i + 1)
        print('  EXE :', line)
        print('  port:', port[i] if i < len(port) else '(missing)')
        bad += 1
        if bad > 3: break
print(f'compared {len(out)} steps: {"OK" if not bad else "DIFFERENT"}')
sys.exit(1 if bad else 0)

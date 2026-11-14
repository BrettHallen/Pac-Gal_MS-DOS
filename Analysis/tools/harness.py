import sys, struct
sys.path.insert(0, '/home/claude/tools')
from emu import *

DSB = 0x8FF0 * 16
UP, LEFT, RIGHT, DOWN = 0x4800, 0x4B00, 0x4D00, 0x5000

GLYPH = {0: ' ', 1: '☺', 2: '☻', 3: '♥', 4: '♦', 5: '♣', 6: '♠', 7: '•'}


SC = {'0': 0x0B, '\r': 0x1C, 'y': 0x15, 'Y': 0x15, 'n': 0x31}
SC.update({str(d): d + 1 for d in range(1, 10)})


def keystr(t, s):
    return [(t + 2 * i, (SC.get(ch, 0x39) << 8) | (13 if ch == '\r' else ord(ch))) for i, ch in enumerate(s)]


class H(DOS):
    def var(self, a, signed=True):
        v = self.rw(DSB + a)
        return v - 65536 if signed and v >= 32768 else v

    def setvar(self, a, v):
        self.ww(DSB + a, v)

    def arr(self, base):
        return [self.var(base + 2 * i) for i in range(6)]

    def state(self):
        names = dict(CH=0x94c, COL=0x958, R=0x964, C=0x970, UNDER=0x97c, FRT=0x988,
                     DR=0x994, DC=0x9a0, KDR=0x9ac, KDC=0x9b8)
        s = {k: self.arr(v)[1:] for k, v in names.items()}
        s.update(lives=self.var(0xa00), dots=self.var(0xa02), P=self.var(0xa04),
                 t0=self.var(0xa06), I=self.var(0x9c4), speed=self.var(0x932))
        return s

    def text(self):
        m = self.uc.mem_read(0xB8000, 4000)
        out = []
        for r in range(25):
            row = ''
            for c in range(80):
                ch = m[(r * 80 + c) * 2]
                row += GLYPH.get(ch) if ch < 8 else bytes([ch]).decode('cp437')
            out.append(row.rstrip())
        return '\n'.join(out)

    def cell(self, row, col):  # 1-based
        a = 0xB8000 + ((row - 1) * 80 + (col - 1)) * 2
        ch, at = self.uc.mem_read(a, 2)
        return ch, at

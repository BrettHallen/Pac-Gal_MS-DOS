"""IBM BASIC PLAY (MML) -> list of (freq_hz or 0, seconds). State persists across calls like BASIC."""
import re
NOTE = {'c': 0, 'd': 2, 'e': 4, 'f': 5, 'g': 7, 'a': 9, 'b': 11}


class Player:
    def __init__(self):
        self.o, self.l, self.t, self.mode = 4, 4, 120, 7 / 8

    def freq(self, n):  # n = 0..83, n=0 is C of octave 0; octave 3 starts at middle C
        midi = 12 * (n // 12) + (n % 12) + 24  # O0 C = C1 (MIDI 24)
        return 440.0 * 2 ** ((midi - 69) / 12)

    def play(self, s, subs=None):
        s = s.lower(); i = 0; out = []
        def num():
            nonlocal i
            m = re.match(r'\d+', s[i:])
            if m: i += len(m.group()); return int(m.group())
            return None
        while i < len(s):
            ch = s[i]; i += 1
            if ch in NOTE:
                n = NOTE[ch]
                if i < len(s) and s[i] in '#+': n += 1; i += 1
                elif i < len(s) and s[i] == '-': n -= 1; i += 1
                ln = num() or self.l
                dots = 0
                while i < len(s) and s[i] == '.': dots += 1; i += 1
                dur = (60 / self.t) * 4 / ln * (1.5 ** dots)
                out.append((round(self.freq(self.o * 12 + n), 1), round(dur * self.mode, 4)))
                if self.mode < 1: out.append((0, round(dur * (1 - self.mode), 4)))
            elif ch == 'o': self.o = num()
            elif ch == 'l': self.l = num()
            elif ch == 't': self.t = num()
            elif ch == 'p':
                ln = num(); out.append((0, round((60 / self.t) * 4 / ln, 4)))
            elif ch == 'm':
                m = s[i]; i += 1
                if m == 'n': self.mode = 7 / 8
                elif m == 'l': self.mode = 1
                elif m == 's': self.mode = 3 / 4
            elif ch == '>': self.o += 1
            elif ch == '<': self.o -= 1
            elif ch == 'x':
                out += self.play(subs)
        return out

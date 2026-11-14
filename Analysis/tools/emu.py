"""Minimal real-mode DOS/BIOS emulator (unicorn) for running PAC-GAL headless.

Records: text screen (B800), PC-speaker tones via PIT/port 0x61, runtime call trace.
"""
import struct, sys
from unicorn import *
from unicorn.x86_const import *

LOAD_SEG = 0x1000          # program image segment
PSP_SEG = LOAD_SEG - 0x10
STUB_SEG = 0xF000          # BIOS interrupt stubs: F000:n*4 -> "CD n CF"
TOP_SEG = 0xA000


class DOS:
    def __init__(self, exe_path, instr_per_tick=40000, keys=None, log=False):
        self.d = open(exe_path, 'rb').read()
        self.uc = Uc(UC_ARCH_X86, UC_MODE_16)
        self.uc.mem_map(0, 0x100000)
        self.log = log
        self.instr_per_tick = instr_per_tick
        self.ticks = 0
        self.keys = keys or []      # list of (tick, scancode<<8|ascii)
        self.keybuf = []
        self.cursor = [0, 0]
        self.done = False
        self.tones = []             # (tick, freq or 0)
        self.speaker_on = False
        self.pit_latch = []
        self.pit_div = 0
        self.int_counts = {}
        self.unknown = set()
        self.load()

    # ---------- helpers
    def r(self, reg):
        return self.uc.reg_read(reg)

    def w(self, reg, v):
        self.uc.reg_write(reg, v & 0xFFFF)

    def rb(self, a):
        return self.uc.mem_read(a, 1)[0]

    def rw(self, a):
        return struct.unpack('<H', self.uc.mem_read(a, 2))[0]

    def ww(self, a, v):
        self.uc.mem_write(a, struct.pack('<H', v & 0xFFFF))

    def set_cf(self, on):
        f = self.r(UC_X86_REG_EFLAGS)
        f = (f | 1) if on else (f & ~1)
        self.uc.reg_write(UC_X86_REG_EFLAGS, f)

    def set_zf(self, on):
        f = self.r(UC_X86_REG_EFLAGS)
        f = (f | 0x40) if on else (f & ~0x40)
        self.uc.reg_write(UC_X86_REG_EFLAGS, f)

    # ---------- loading
    def load(self):
        d = self.d
        h = struct.unpack('<14H', d[:28])
        hsz = h[4] * 16
        img = bytearray(d[hsz:])
        for i in range(h[3]):
            o, s = struct.unpack('<HH', d[h[12] + 4 * i:h[12] + 4 * i + 4])
            a = s * 16 + o
            v = struct.unpack('<H', img[a:a + 2])[0]
            img[a:a + 2] = struct.pack('<H', (v + LOAD_SEG) & 0xFFFF)
        self.uc.mem_write(LOAD_SEG * 16, bytes(img))
        # PSP
        psp = PSP_SEG * 16
        self.uc.mem_write(psp, b'\xcd\x20')
        self.ww(psp + 2, TOP_SEG)
        self.uc.mem_write(psp + 0x80, b'\x00\x0d')
        # IVT -> stubs
        for n in range(256):
            self.ww(n * 4, n * 4)
            self.ww(n * 4 + 2, STUB_SEG)
            self.uc.mem_write(STUB_SEG * 16 + n * 4, bytes([0xCD, n, 0xCF, 0x90]))
        # BIOS data area
        bda = 0x400
        self.ww(bda + 0x10, 0x0021 | (2 << 4))   # equipment: 80x25 colour
        self.ww(bda + 0x13, 640)
        self.uc.mem_write(bda + 0x49, bytes([3]))
        self.ww(bda + 0x4A, 80)
        self.ww(bda + 0x4C, 4000)
        self.ww(bda + 0x63, 0x3D4)
        self.uc.mem_write(bda + 0x84, bytes([24]))
        # clear screen
        self.uc.mem_write(0xB8000, b'\x20\x07' * 2000)
        self.w(UC_X86_REG_CS, LOAD_SEG + h[11])
        self.w(UC_X86_REG_IP, h[10])
        self.w(UC_X86_REG_SS, LOAD_SEG + h[7])
        self.w(UC_X86_REG_SP, h[8])
        self.w(UC_X86_REG_DS, PSP_SEG)
        self.w(UC_X86_REG_ES, PSP_SEG)
        self.uc.reg_write(UC_X86_REG_EFLAGS, 0x202)
        self.uc.hook_add(UC_HOOK_INTR, self.on_int)
        self.uc.hook_add(UC_HOOK_INSN, self.on_in, None, 1, 0, UC_X86_INS_IN)
        self.uc.hook_add(UC_HOOK_INSN, self.on_out, None, 1, 0, UC_X86_INS_OUT)
        self.uc.hook_add(UC_HOOK_MEM_UNMAPPED, self.on_bad)

    def on_bad(self, uc, access, addr, size, value, ud):
        print('UNMAPPED', hex(addr)); return False

    # ---------- ports
    def on_in(self, uc, port, size, ud):
        if port == 0x3DA:
            self._retr = getattr(self, '_retr', 0) ^ 9
            return self._retr
        if port == 0x61:
            return 0x30 | (3 if self.speaker_on else 0)
        if port == 0x21:
            return getattr(self, 'pic_mask', 0xB8)
        if port == 0x60:
            return getattr(self, 'port60', 0)
        return 0xFF

    def on_out(self, uc, port, size, value, ud):
        value &= 0xFF
        if port == 0x43:
            self.pit_latch = []
        elif port == 0x42:
            self.pit_latch.append(value)
            if len(self.pit_latch) == 2:
                self.pit_div = self.pit_latch[0] | (self.pit_latch[1] << 8)
                self.pit_latch = []
                self._tone()
        elif port == 0x61:
            on = (value & 3) == 3
            if on != self.speaker_on:
                self.speaker_on = on
                self._tone()
        elif port == 0x21:
            self.pic_mask = value
        elif port == 0x40:
            pass

    def _tone(self):
        f = round(1193182 / self.pit_div) if (self.speaker_on and self.pit_div) else 0
        if not self.tones or self.tones[-1][1] != f:
            self.tones.append((self.ticks, f))

    # ---------- interrupts
    def dispatch(self, n, ret_ip=None):
        """Perform a real interrupt into the IVT target."""
        uc = self.uc
        ip = self.r(UC_X86_REG_IP) if ret_ip is None else ret_ip
        cs = self.r(UC_X86_REG_CS)
        fl = self.r(UC_X86_REG_EFLAGS)
        sp = (self.r(UC_X86_REG_SP) - 6) & 0xFFFF
        ss = self.r(UC_X86_REG_SS)
        self.ww(ss * 16 + sp + 4, fl)
        self.ww(ss * 16 + sp + 2, cs)
        self.ww(ss * 16 + sp, ip)
        self.w(UC_X86_REG_SP, sp)
        uc.reg_write(UC_X86_REG_EFLAGS, fl & ~0x300)
        self.w(UC_X86_REG_IP, self.rw(n * 4))
        self.w(UC_X86_REG_CS, self.rw(n * 4 + 2))

    def on_int(self, uc, n, ud):
        cs = self.r(UC_X86_REG_CS)
        tgt_cs, tgt_ip = self.rw(n * 4 + 2), self.rw(n * 4)
        if cs != STUB_SEG and not (tgt_cs == STUB_SEG and tgt_ip == n * 4):
            # program-installed handler: real dispatch
            self.dispatch(n)
            return
        self.int_counts[n] = self.int_counts.get(n, 0) + 1
        h = getattr(self, f'int_{n:02x}', None)
        if h:
            h()
        else:
            self.unknown.add((n, self.r(UC_X86_REG_AH)))

    def int_08(self):
        self.ww(0x46C, self.rw(0x46C) + 1)

    def int_09(self):
        sc = getattr(self, 'port60', 0)
        if sc < 0x80 and getattr(self, 'pending_word', None) is not None:
            self.keybuf.append(self.pending_word)
        self.pending_word = None

    def int_1c(self):
        pass

    def int_1b(self):
        pass

    def int_11(self):
        self.w(UC_X86_REG_AX, self.rw(0x410))

    def int_12(self):
        self.w(UC_X86_REG_AX, 640)

    def int_1a(self):
        ah = self.r(UC_X86_REG_AH)
        if ah == 0:
            t = self.rw(0x46C)
            self.w(UC_X86_REG_CX, 0); self.w(UC_X86_REG_DX, t); self.w(UC_X86_REG_AX, 0)

    def int_17(self):
        self.w(UC_X86_REG_AX, 0x9000)

    # --- keyboard
    def pending_keys(self):
        # keys are delivered through the hardware path (IRQ1) in run()
        return

    def wait_key(self):
        if not self.keys and not getattr(self, 'hwq', None):
            self.done = True; self.uc.emu_stop(); return
        if self.keys:
            self.ticks = max(self.ticks, self.keys[0][0] - 1)
        # re-execute the INT instruction after the next tick
        self.w(UC_X86_REG_IP, self.r(UC_X86_REG_IP) - 2)
        self.uc.emu_stop()

    def int_16(self):
        ah = self.r(UC_X86_REG_AH)
        self.pending_keys()
        if ah == 0:
            if not self.keybuf:
                self.wait_key(); return
            self.w(UC_X86_REG_AX, self.keybuf.pop(0))
        elif ah == 1:
            if self.keybuf:
                self.w(UC_X86_REG_AX, self.keybuf[0]); self.set_zf(False)
            else:
                self.set_zf(True)
        elif ah == 2:
            self.w(UC_X86_REG_AX, 0)

    # --- video
    def put(self, ch, attr=None, row=None, col=None):
        r, c = (self.cursor if row is None else (row, col))
        a = 0xB8000 + (r * 80 + c) * 2
        self.uc.mem_write(a, bytes([ch]))
        if attr is not None:
            self.uc.mem_write(a + 1, bytes([attr]))

    def scroll_up(self, n, top=0, left=0, bot=24, right=79, attr=7):
        for r in range(top, bot + 1):
            src = r + n
            for c in range(left, right + 1):
                a = 0xB8000 + (r * 80 + c) * 2
                if n and src <= bot:
                    self.uc.mem_write(a, bytes(self.uc.mem_read(0xB8000 + (src * 80 + c) * 2, 2)))
                else:
                    self.uc.mem_write(a, bytes([32, attr]))

    def int_10(self):
        ah = self.r(UC_X86_REG_AH); al = self.r(UC_X86_REG_AL)
        bx = self.r(UC_X86_REG_BX); cx = self.r(UC_X86_REG_CX); dx = self.r(UC_X86_REG_DX)
        if ah == 0:
            self.uc.mem_write(0x449, bytes([al]))
            self.ww(0x44A, 40 if al < 2 else 80)
            self.uc.mem_write(0xB8000, b'\x20\x07' * 2000)
            self.video_mode = al
        elif ah == 1:
            pass
        elif ah == 2:
            self.cursor = [dx >> 8, dx & 0xFF]
            self.ww(0x450, dx)
        elif ah == 3:
            self.w(UC_X86_REG_DX, (self.cursor[0] << 8) | self.cursor[1]); self.w(UC_X86_REG_CX, 0x0607)
        elif ah == 5:
            pass
        elif ah == 6 or ah == 7:
            n = al if al else 25
            top, left, bot, right = cx >> 8, cx & 0xFF, dx >> 8, dx & 0xFF
            if ah == 6:
                self.scroll_up(n if al else 99, top, left, bot, right, bx >> 8)
            else:
                self.scroll_up(99, top, left, bot, right, bx >> 8)
        elif ah == 8:
            r, c = self.cursor
            a = 0xB8000 + (r * 80 + c) * 2
            ch, at = self.uc.mem_read(a, 2)
            self.w(UC_X86_REG_AX, (at << 8) | ch)
        elif ah == 9 or ah == 0xA:
            r, c = self.cursor
            for k in range(cx):
                p = r * 80 + c + k
                if p >= 2000: break
                self.uc.mem_write(0xB8000 + p * 2, bytes([al]))
                if ah == 9:
                    self.uc.mem_write(0xB8000 + p * 2 + 1, bytes([bx & 0xFF]))
        elif ah == 0xE:
            r, c = self.cursor
            if al == 13: c = 0
            elif al == 10: r += 1
            elif al == 8: c = max(0, c - 1)
            elif al == 7: pass
            else:
                self.put(al); c += 1
                if c >= 80: c = 0; r += 1
            if r > 24:
                self.scroll_up(1); r = 24
            self.cursor = [r, c]
        elif ah == 0xF:
            self.w(UC_X86_REG_AX, (80 << 8) | self.rb(0x449)); self.w(UC_X86_REG_BX, 0)
        elif ah == 0xB:
            pass
        else:
            self.unknown.add((0x10, ah))

    # --- DOS
    def int_20(self):
        self.done = True; self.uc.emu_stop()

    def int_21(self):
        ah = self.r(UC_X86_REG_AH); al = self.r(UC_X86_REG_AL)
        if self.log:
            print('INT21', hex(ah))
        if ah == 0x30:
            self.w(UC_X86_REG_AX, 0x0002)   # DOS 2.0
        elif ah == 0x25:
            n = al
            self.ww(n * 4, self.r(UC_X86_REG_DX)); self.ww(n * 4 + 2, self.r(UC_X86_REG_DS))
        elif ah == 0x35:
            n = al
            self.w(UC_X86_REG_BX, self.rw(n * 4)); self.w(UC_X86_REG_ES, self.rw(n * 4 + 2))
        elif ah in (0x4C, 0x00):
            self.done = True; self.uc.emu_stop()
        elif ah == 0x4A:
            self.set_cf(False)
        elif ah == 0x2C:
            secs = 12 * 3600 + int(self.rw(0x46C) / 18.2)
            h, m, sc = secs // 3600, (secs // 60) % 60, secs % 60
            self.w(UC_X86_REG_CX, (h << 8) | m); self.w(UC_X86_REG_DX, sc << 8)
        elif ah == 0x2A:
            self.w(UC_X86_REG_CX, 1982); self.w(UC_X86_REG_DX, 0x0501)
        elif ah == 0x0B:
            self.pending_keys()
            self.w(UC_X86_REG_AX, (ah << 8) | (0xFF if self.keybuf else 0))
        elif ah == 0x06 and self.r(UC_X86_REG_DL) == 0xFF:
            self.pending_keys()
            if self.keybuf:
                self.w(UC_X86_REG_AX, (ah << 8) | (self.keybuf.pop(0) & 0xFF)); self.set_zf(False)
            else:
                self.set_zf(True)
        elif ah in (0x01, 0x07, 0x08):
            self.pending_keys()
            if not self.keybuf:
                self.wait_key(); return
            k = self.keybuf.pop(0) & 0xFF
            self.w(UC_X86_REG_AX, (ah << 8) | k)
        elif ah == 0x02 or (ah == 0x06):
            self.tty(self.r(UC_X86_REG_DL))
        elif ah == 0x09:
            a = self.r(UC_X86_REG_DS) * 16 + self.r(UC_X86_REG_DX)
            while True:
                c = self.rb(a); a += 1
                if c == 0x24: break
                self.tty(c)
        elif ah == 0x0E:
            self.w(UC_X86_REG_AX, (ah << 8) | 1)
        elif ah == 0x19:
            self.w(UC_X86_REG_AX, (ah << 8) | 0)
        elif ah == 0x0D:
            pass
        elif ah == 0x0A:
            # buffered input
            a = self.r(UC_X86_REG_DS) * 16 + self.r(UC_X86_REG_DX)
            mx = self.rb(a); s = b''
            while True:
                self.pending_keys()
                if not self.keybuf:
                    if not self.keys:
                        self.done = True; self.uc.emu_stop(); return
                    self.ticks = self.keys[0][0]; self.pending_keys()
                k = self.keybuf.pop(0) & 0xFF
                if k == 13: break
                s += bytes([k]); self.tty(k)
            self.uc.mem_write(a + 1, bytes([len(s)]) + s + b'\r')
        else:
            self.unknown.add((0x21, ah))
            self.set_cf(True); self.w(UC_X86_REG_AX, 1)

    def tty(self, c):
        save = self.r(UC_X86_REG_AX)
        self.w(UC_X86_REG_AX, 0x0E00 | c)
        self.int_10()
        self.w(UC_X86_REG_AX, save)

    # ---------- running
    def run(self, max_ticks=2000, on_tick=None):
        while not self.done and self.ticks < max_ticks:
            cs, ip = self.r(UC_X86_REG_CS), self.r(UC_X86_REG_IP)
            try:
                self.uc.emu_start(cs * 16 + ip, 0xFFFFFFFF, count=self.instr_per_tick)
            except UcError as e:
                print('ERR', e, hex(self.r(UC_X86_REG_CS)), hex(self.r(UC_X86_REG_IP)))
                break
            if self.done: break
            self.ticks += 1
            if self.r(UC_X86_REG_EFLAGS) & 0x200 and not (getattr(self, 'pic_mask', 0) & 2):
                hw = getattr(self, 'hwq', [])
                self.hwq = hw
                while self.keys and self.keys[0][0] <= self.ticks:
                    w = self.keys.pop(0)[1]
                    hw.append((w >> 8, w)); hw.append(((w >> 8) | 0x80, None))
                if hw:
                    sc, w = hw.pop(0)
                    self.port60 = sc; self.pending_word = w
                    self.dispatch(9)
                    continue
            if self.r(UC_X86_REG_EFLAGS) & 0x200 and not (getattr(self, 'pic_mask', 0) & 1):
                self.dispatch(8)
            if on_tick: on_tick(self)

    def screen(self, attrs=False):
        m = self.uc.mem_read(0xB8000, 4000)
        rows = []
        for r in range(25):
            rows.append(bytes(m[(r * 80 + c) * 2] for c in range(80)))
        return rows

    def screen_text(self):
        out = []
        for row in self.screen():
            out.append(row.decode('cp437').rstrip())
        return '\n'.join(out)

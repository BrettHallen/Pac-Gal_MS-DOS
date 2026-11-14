"""Annotated disassembly of PAC-GAL compiled-BASIC user code (segment 0)."""
import sys, struct, json
from capstone import *

img = open(sys.argv[1], 'rb').read()
names = json.load(open(sys.argv[2])) if len(sys.argv) > 2 else {}
varnames = json.load(open(sys.argv[3])) if len(sys.argv) > 3 else {}
md = Cs(CS_ARCH_X86, CS_MODE_16)
RT = 0x2EC0
DSL = 0x77E0
CONST_BASE = 0xA30          # runtime home of the 0x1E0-byte constant block
block = img[DSL:DSL + 0x1E0]
rt = img[RT:DSL]


def cblock(addr, n):
    o = addr - CONST_BASE
    if 0 <= o < len(block):
        return block[o:o + n]
    return None


def strdesc(addr):
    b = cblock(addr, 4)
    if b is None or len(b) < 4: return None
    ln, p = struct.unpack('<HH', b)
    s = cblock(p, ln)
    if s is None or len(s) != ln or ln > 200: return None
    return s


def mbf(b):
    # Microsoft Binary Format single (4 bytes: m0 m1 m2|sign exp)
    if b[3] == 0: return 0.0
    exp = b[3] - 129
    sign = -1 if b[2] & 0x80 else 1
    mant = ((b[2] | 0x80) << 16) | (b[1] << 8) | b[0]
    return sign * mant / float(1 << 23) * (2.0 ** exp)


def inline_count(t):
    if t in (0x2451,): return 1
    if t == 0x27be: return None
    o = t
    for i in md.disasm(rt[o:o + 40], o):
        if i.mnemonic == 'call' and i.op_str == '0x35f': return 1
        if i.mnemonic in ('ret', 'retf', 'jmp') or i.mnemonic.startswith('j'):
            if i.mnemonic == 'jmp' and i.op_str.startswith('0x'):
                tt = int(i.op_str, 16)
                for k in md.disasm(rt[tt:tt + 30], tt):
                    if k.mnemonic == 'call' and k.op_str == '0x35f': return 1
                    if k.mnemonic in ('ret', 'retf') or k.mnemonic.startswith('j') or k.mnemonic == 'call': break
            return 0
        if i.mnemonic == 'call': return 0
    return 0


def fmt_str(s):
    return '"' + ''.join(chr(c) if 32 <= c < 127 else f'\\x{c:02x}' for c in s) + '"'


def vname(a):
    return varnames.get(f'{a:04x}', f'v{a:03x}')


code = img[:RT]
off = 0x1A
out = []
import re
while off < len(code):
    i = next(md.disasm(code[off:off + 16], off), None)
    if i is None:
        out.append(f'{off:04x}:   db {code[off]:02x}'); off += 1; continue
    txt = f'{i.mnemonic} {i.op_str}'
    note = ''
    nxt = i.address + i.size
    if i.mnemonic == 'int3':
        out.append(f'{i.address:04x}: ------------------------------------ (stmt)')
        off = nxt; continue
    if i.mnemonic == 'lcall' and i.op_str.startswith('0x2ec'):
        t = int(i.op_str.split(',')[1], 16)
        nm = names.get(f'{t:04x}', f'rt_{t:04x}')
        txt = f'call {nm}'
        if t == 0x1185:
            tg = struct.unpack('<H', code[nxt:nxt+2])[0]
            note = f'-> {tg:04x}'; nxt += 2
        elif t == 0x27be:
            n = code[nxt]; ty = list(code[nxt + 1:nxt + 1 + n])
            note = f'inline n={n} types={ty}'; nxt += 1 + n
        else:
            k = inline_count(t)
            if k:
                note = f'[slot {code[nxt] & 0x7f}{"*" if code[nxt] & 0x80 else ""}]'
                nxt += k
    # immediate / memory annotations
    for m in re.finditer(r'0x([0-9a-f]{3,4})', i.op_str):
        a = int(m.group(1), 16)
        if 0x900 <= a < CONST_BASE:
            txt = txt.replace(m.group(0), vname(a))
        elif CONST_BASE <= a < CONST_BASE + 0x1E0:
            s = strdesc(a)
            if s is not None:
                note += ' ' + fmt_str(s)
            else:
                b = cblock(a, 4)
                note += f' const@{a:x}={mbf(b):g}'
    out.append(f'{i.address:04x}:   {txt:40s} {note}')
    off = nxt
print('\n'.join(out))

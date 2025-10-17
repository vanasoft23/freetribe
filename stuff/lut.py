import math

def ieee754_to_q131(x: float) -> int:
    """
    Converts an IEEE-754 float to Fract32 (Q1.31 format).
    Input range: [-1.0, 1.0)
    Returns a signed 32-bit integer.
    """
    # Clamp input to valid range
    if x >= 1.0:
        x = 1.0 - 2**-31
    elif x < -1.0:
        x = -1.0

    # Scale and convert to integer
    result = int(round(x * (1 << 31)))

    # Ensure it's a 32-bit signed int
    if result >= (1 << 31):
        result -= (1 << 32)
    return result

def dec_to_32bit_hex(value: int) -> str:
    """
    Converts a signed 32-bit integer to a 32-bit hex string (8 digits, 0-padded).
    Handles both positive and negative integers correctly.
    """
    return f"0x{value & 0xFFFFFFFF:08X}"

print("fract32 NOTE_FREQS[128] = {")

f_norms = []
for n in range(-64, 64, 1):
    f=440*2**(n/12)
    f_norm = f/48000
    f_norms.append(f_norm)

txt = ''
for i, f_norm in enumerate(f_norms):
    if i % 10 == 0:
        txt += '    '
    txt += f"{dec_to_32bit_hex(ieee754_to_q131(f_norm))}"
    if i != len(f_norms)-1: txt += ', '
    if i % 10 == 9:
        txt += '\n'
print(txt)

print("};")
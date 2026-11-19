"""
Things I wanted to check for myself instead of just taking the AI's word for it.

Run from the repo root:
    python3 python/experiments.py
"""

import math
import os
import zlib
from collections import Counter

from huffman import encode, decode, encoded_bits, entropy, huffman_codes

HERE = os.path.dirname(os.path.abspath(__file__))
SAMPLE = os.path.join(HERE, "..", "examples", "sample.txt")


def header_bytes(data):
    # same header layout as the C version: 14 bytes + 5 per distinct symbol
    return 14 + 5 * len(set(data))


def show_codes(codes):
    for sym, code in sorted(codes.items(), key=lambda kv: (len(kv[1]), kv[0])):
        print(f"    {sym}: {code}")


def title(s):
    print()
    print(s)
    print("-" * len(s))


# 1. the running example
title("1. ABRACADABRA")
word = "ABRACADABRA"
codes = huffman_codes(word)
show_codes(codes)
bits = encode(word, codes)
fixed = math.ceil(math.log2(len(set(word))))
print(f"  ASCII:              {len(word) * 8} bits")
print(f"  best fixed-length:  {len(word) * fixed} bits ({fixed} bits each)")
print(f"  huffman:            {len(bits)} bits -> {bits}")
print(f"  entropy limit:      {entropy(word) * len(word):.1f} bits")
print(f"  decodes back to:    {''.join(decode(bits, codes))}")
print(f"  decode 1100111:     {''.join(decode('1100111', codes))}")

# 2. ties
title("2. breaking ties a different way")
other = huffman_codes(word, prefer_new_nodes=True)
show_codes(other)
print(f"  total: {encoded_bits(word, other)} bits (first tree was {encoded_bits(word, codes)})")
print("  different codes, same total")

# 3. equal frequencies
title("3. four letters, all equally common")
even = "ABCD" * 25
show_codes(huffman_codes(even))
print("  the most common letter doesn't automatically get 1 bit")

# 4. one letter is almost everything
title("4. 99% 'a', 1% 'b'")
skewed = "a" * 990 + "b" * 10
c = huffman_codes(skewed)
print(f"  huffman: {encoded_bits(skewed, c) / len(skewed):.3f} bits per symbol")
print(f"  entropy: {entropy(skewed):.3f} bits per symbol")
print("  huffman can't go below 1 bit per symbol, even when the data is very predictable")

# 5. real text, and compared with zlib (DEFLATE = LZ77 + Huffman, what zip/gzip/png use)
def compare(name, data):
    c = huffman_codes(data)
    huff = header_bytes(data) + (encoded_bits(data, c) + 7) // 8
    z = len(zlib.compress(data, 9))
    print(f"  {name}")
    print(f"    original:            {len(data):>7} bytes")
    print(f"    huffman only:        {huff:>7} bytes  ({100 * huff / len(data):.1f}%)")
    print(f"    zlib (LZ77+huffman): {z:>7} bytes  ({100 * z / len(data):.1f}%)")
    print(f"    entropy:             {entropy(data):.3f} bits/byte, "
          f"huffman got {encoded_bits(data, c) / len(data):.3f}")

title("5. english text vs random bytes vs tiny input")
with open(SAMPLE, "rb") as f:
    compare("examples/sample.txt", f.read())
compare("100,000 random bytes", os.urandom(100_000))
compare("b'hi'", b"hi")
print("  random data and tiny files come out BIGGER than they started")

# 6. flip one bit
title("6. flipping one bit in the middle")
msg = "ABRACADABRA"
bits = encode(msg, codes)
i = 4
broken = bits[:i] + ("1" if bits[i] == "0" else "0") + bits[i + 1:]
print(f"  original: {''.join(decode(bits, codes))}")
print(f"  1 bit off: {''.join(decode(broken, codes))}")
print("  one wrong bit gave two wrong letters (RA -> AB) before the decoder happened")
print("  to land back in sync. nothing in the data tells you something went wrong,")
print("  which is why gzip and png also store a checksum (CRC)")

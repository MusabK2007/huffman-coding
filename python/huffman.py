"""
Short Python version of Huffman coding, used for the experiments.
The C version in src/ is the "real" one; this one is easier to poke at.
"""

import heapq
import math
from collections import Counter


class Node:
    def __init__(self, freq, symbol=None, left=None, right=None):
        self.freq = freq
        self.symbol = symbol
        self.left = left    # 0
        self.right = right  # 1


def build_tree(freqs, prefer_new_nodes=False):
    """
    freqs: dict symbol -> count

    When two trees have the same count, heapq needs something else to compare.
    By default older trees win ties. prefer_new_nodes=True flips that, which
    builds a different (but still valid) Huffman tree.
    """
    heap = []
    order = 0
    for symbol in sorted(freqs):
        heap.append((freqs[symbol], order, Node(freqs[symbol], symbol)))
        order += 1
    heapq.heapify(heap)

    if len(heap) == 1:
        # one distinct symbol: give it the code "0"
        _, _, only = heap[0]
        return Node(only.freq, left=only)

    while len(heap) > 1:
        fa, _, a = heapq.heappop(heap)
        fb, _, b = heapq.heappop(heap)
        tie = -order if prefer_new_nodes else order
        heapq.heappush(heap, (fa + fb, tie, Node(fa + fb, left=a, right=b)))
        order += 1

    return heap[0][2]


def make_codes(node, prefix="", codes=None):
    if codes is None:
        codes = {}
    if node is None:
        return codes
    if node.symbol is not None:
        codes[node.symbol] = prefix
        return codes
    make_codes(node.left, prefix + "0", codes)
    make_codes(node.right, prefix + "1", codes)
    return codes


def huffman_codes(data, prefer_new_nodes=False):
    return make_codes(build_tree(Counter(data), prefer_new_nodes))


def encode(data, codes):
    return "".join(codes[x] for x in data)


def decode(bits, codes):
    # flip the table around and read until the buffer matches a code.
    # this only works because no code is a prefix of another one.
    lookup = {code: sym for sym, code in codes.items()}
    out = []
    current = ""
    for b in bits:
        current += b
        if current in lookup:
            out.append(lookup[current])
            current = ""
    return out


def entropy(data):
    counts = Counter(data)
    n = len(data)
    return -sum(c / n * math.log2(c / n) for c in counts.values())


def encoded_bits(data, codes):
    counts = Counter(data)
    return sum(counts[s] * len(codes[s]) for s in counts)


if __name__ == "__main__":
    import sys

    text = sys.argv[1] if len(sys.argv) > 1 else "ABRACADABRA"
    codes = huffman_codes(text)
    bits = encode(text, codes)
    for sym, code in sorted(codes.items(), key=lambda kv: (len(kv[1]), kv[0])):
        print(f"{sym!r}: {code}")
    print("encoded:", bits, f"({len(bits)} bits)")
    print("decoded:", "".join(decode(bits, codes)))

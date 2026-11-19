# python version of huffman, the C one is the real one
# this one is just easier to mess around with

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
    # freqs = {symbol: count}
    # when 2 counts tie heapq needs something else to compare so i use order.
    # normally older trees win ties, prefer_new_nodes flips it which gives a
    # different tree (still a valid huffman tree)
    heap = []
    order = 0
    for symbol in sorted(freqs):
        heap.append((freqs[symbol], order, Node(freqs[symbol], symbol)))
        order += 1
    heapq.heapify(heap)

    if len(heap) == 1:
        # only 1 kind of symbol, give it "0"
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
    # flip the table and read bits until it matches a code
    # only works because no code is the start of another one
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

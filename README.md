# Huffman Coding

My COMP 211 Checkoff 3 project. The topic is **Data Compression: Huffman coding**.

The question I started with: zip files, PNGs and MP3s make files smaller without
losing anything, so how does that actually work at the bit level? Huffman coding turned out
to be one of the main pieces, and it's simple enough to build yourself in C with the stuff
we already learned: bits, bitwise operators, structs, pointers and malloc/free.

This repo has a working compressor/decompressor in C, a shorter Python version I used for
experiments, my slides, and the sources I used.

## The short version

ASCII uses 8 bits for every character, so `e` costs as much space as `z`, even though `e`
shows up way more often. Huffman coding gives **common symbols short codes and rare symbols
long codes**, so the average comes out under 8 bits.

To build the codes:

1. Count how often each symbol shows up.
2. Make each symbol its own node.
3. Keep taking the **two smallest counts** and joining them under a new node whose count is
   their sum, until there's only one tree left.
4. Left = `0`, right = `1`. A symbol's code is the path from the root down to it.

Since symbols are only ever at the leaves, no code can be the start of another code
(a "prefix-free" code). That's what lets you decode a stream of bits with no separators:
follow the bits down the tree, output the symbol when you hit a leaf, go back to the
root, repeat.

### Example: ABRACADABRA

| symbol | count | code |
|--------|-------|------|
| A      | 5     | `0`   |
| B      | 2     | `110` |
| R      | 2     | `111` |
| C      | 1     | `100` |
| D      | 1     | `101` |

```
            (11)
           /    \
         A:5    (6)
               /    \
            (2)      (4)
           /  \     /   \
         C:1  D:1  B:2  R:2
```

`ABRACADABRA` becomes `01101110100010101101110`, which is **23 bits**:

| method | bits |
|---|---|
| ASCII (8 bits each) | 88 |
| best fixed-length code (5 letters need 3 bits each) | 33 |
| Huffman | **23** |
| entropy (theoretical minimum) | 22.4 |

The fair comparison is 33 vs 23, not 88 vs 23. ASCII has room for 256 characters, and this
word only uses 5.

## Things I found interesting

**There's more than one right answer.** When two counts tie you can merge either one. I tried
it both ways in Python and got different codes (`B = 110` in one tree, `B = 111` in the other,
`R = 111` vs `R = 10`), but both came out to exactly 23 bits. The tree isn't unique, but the
total length is always the same.

**Compressing can make files bigger.** The decoder needs the code table, so it has to be stored
in the file. My header is 14 bytes plus 5 bytes per distinct symbol. Results from my C program:

| file | original | compressed | |
|---|---|---|---|
| `examples/sample.txt` (English) | 2,968 B | 1,925 B | 64.9% |
| `examples/random.bin` (100 KB random) | 100,000 B | 101,294 B | 101.3% |
| `examples/abracadabra.txt` | 11 B | 42 B | 381.8% |

For the random file, every byte still got exactly 8 bits, because all 256 values show up
about equally, so the tree ends up perfectly balanced. Then the header gets added on top.
It's the same reason zipping a zip file doesn't help.

**The common letter doesn't always get 1 bit.** With `ABCD` repeated (all equally common),
every letter gets 2 bits. And when the data is 99% `a`, Huffman still can't go below 1 bit per
symbol, even though the entropy is only 0.081 bits. That's the main weakness, and why newer
methods (arithmetic coding, ANS in zstd) exist: they can effectively spend fractions of a bit.

**English text is close to the limit.** On `sample.txt` my Huffman codes averaged 4.409 bits
per character, and the entropy was 4.368. So for "one code per character" there's barely
anything left to gain. zlib still did better (48.5% vs my 64.9%) because DEFLATE runs LZ77 first
to catch repeated words and phrases, and *then* uses Huffman. Huffman by itself only looks at
single characters.

**One flipped bit.** I flipped one bit in the middle of the ABRACADABRA bits. It decoded as
`ABABCADABRA`, so two letters were wrong and then it got back in sync. Nothing in the data
tells you anything went wrong, which is why gzip and PNG also store a CRC checksum.

## Where it's used

- **DEFLATE** (zip, gzip, PNG): LZ77 + Huffman
- **JPEG**: the lossy steps throw away detail, then Huffman packs what's left (that step itself is lossless)
- **MP3**: same idea, Huffman is the last step
- **HTTP/2**: request headers are compressed with a fixed Huffman table that browsers and servers both already know (HPACK)

## How it connects to COMP 211

- **Bitwise operators**: you can't write one bit to a file, so the encoder shifts bits into a
  byte (`byte = (byte << 1) | bit`) and writes it out every 8 bits. The decoder reads them
  back with `(byte >> n) & 1`. The header integers get split into bytes with `(x >> 8*i) & 0xFF`.
- **Structs and pointers**: each tree node is a `struct Node` with `left` and `right` pointers.
- **malloc/free**: every merge mallocs a node, and the tree gets freed recursively
  (children first). I ran it through valgrind and there were no leaks.
- **Arrays**: counting is just `freq[byte]++` on an array of 256, since a byte only has 256 values.
- I also had to write a min-heap so "grab the two smallest" is fast.

## Running the code

### C version (`src/huffman.c`)

Works in the COMP 211 container, or anywhere with `gcc` and `make`.

```bash
make              # builds ./huffman
make test         # compresses + decompresses every example and checks the output matches with cmp

./huffman stats examples/abracadabra.txt     # counts, codes, sizes, entropy, the actual bits
./huffman stats examples/sample.txt
./huffman compress examples/sample.txt sample.huf
./huffman decompress sample.huf sample.out
cmp examples/sample.txt sample.out && echo same
```

`make test` also creates `examples/random.bin` (100 KB from `/dev/urandom`) for the
"random data gets bigger" test.

On Windows I ran it inside the course Docker image, from PowerShell in the repo folder:

```powershell
docker run --rm -it -v "${PWD}:/work" -w /work learncli/comp211 bash
```

### Python version (`python/`)

Needs Python 3, no extra packages.

```bash
python3 python/huffman.py MISSISSIPPI    # codes + encoded bits for any word
python3 python/experiments.py            # all the experiments from the section above
```

## Files

```
src/huffman.c          compressor / decompressor / stats in C
Makefile               build + round-trip tests
python/huffman.py      shorter Python version
python/experiments.py  ties, equal counts, 1-bit minimum, random data, zlib comparison, bit flip
examples/              test inputs
slides/                presentation slides
```

## Slides

[slides/huffman-slides.pdf](slides/huffman-slides.pdf)

## How I learned this

I used Claude (an AI assistant) to make a study guide, then put it into NotebookLM and listened
to it as a podcast, since I learn better by listening than by reading. After that I built the
code and used the experiments to check the claims myself, which is how I found the bit-flip
result above. More detail is in my written reflection.

## Things I still want to look into

- How arithmetic coding / ANS get around the 1-bit minimum
- Canonical Huffman codes (DEFLATE stores only the code *lengths*, not the tree, which would make my header a lot smaller)
- Adaptive Huffman, where the tree updates while it reads, so no table has to be sent at all

## Resources

- D. A. Huffman, "A Method for the Construction of Minimum-Redundancy Codes," *Proceedings of the IRE*, vol. 40, no. 9, pp. 1098-1101, 1952. The original paper.
- [Huffman coding, Wikipedia](https://en.wikipedia.org/wiki/Huffman_coding). Good overview, and it has the story about the term paper.
- [RFC 1951: DEFLATE Compressed Data Format](https://www.rfc-editor.org/rfc/rfc1951). Section 3.2 explains how DEFLATE uses Huffman codes and canonical codes.
- [RFC 7541: HPACK (HTTP/2 header compression)](https://www.rfc-editor.org/rfc/rfc7541). Appendix B is the actual Huffman table browsers use.
- Cormen, Leiserson, Rivest, Stein, *Introduction to Algorithms* (3rd ed.), Section 16.3 "Huffman codes". Proof that the greedy choice is optimal.
- [Python `heapq` docs](https://docs.python.org/3/library/heapq.html) and [`zlib` docs](https://docs.python.org/3/library/zlib.html)
- Claude (Anthropic) and NotebookLM (Google). AI tools I used for learning, see the reflection.

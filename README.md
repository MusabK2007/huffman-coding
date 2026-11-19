# huffman coding

comp 211 checkoff 3. my topic was data compression -> huffman coding

basically I wanted to know how zip files and pngs make stuff smaller without losing anything. turns out huffman coding is a big part of it and its simple enough that you can write it in C with stuff we already did in class (bits, bitwise ops, structs, pointers, malloc/free)

this repo has my C version, a shorter python version I used to test things, my slides, and the sources I used

## how it works (short version)

normally every character is 8 bits (ASCII). so 'e' takes the same space as 'z' even though e shows up way more. huffman gives common characters short codes and rare ones long codes so on average you use less than 8 bits

building the codes:
1. count how many times each character shows up
2. take the two smallest counts and join them under a new node (count = the two added together)
3. keep doing that until theres only one tree left
4. going left = 0, going right = 1. the path from the top down to a letter is its code

since letters are only ever at the ends of branches, no code is the start of another code ("prefix-free"). thats why you can decode without any spaces between letters, you just follow the bits down the tree until you hit a letter then go back to the top

### example: ABRACADABRA

counts: A=5 B=2 R=2 C=1 D=1

```
            (11)
           /    \
         A:5    (6)
               /    \
            (2)      (4)
           /  \     /   \
         C:1  D:1  B:2  R:2
```

codes: A=0, C=100, D=101, B=110, R=111

ABRACADABRA -> `01101110100010101101110` which is 23 bits

- ASCII: 88 bits
- best fixed length code (5 letters so 3 bits each): 33 bits
- huffman: 23 bits
- entropy (the theoretical minimum): about 22.4 bits

88 vs 23 isnt really a fair comparison tho because ASCII has room for 256 characters and this word only uses 5. 33 vs 23 is the real comparison, about 30% smaller

## stuff I found interesting

**theres more than one right tree.** when counts tie you can merge either one. I tried it both ways in python and got different codes (B was 110 in one and 111 in the other) but both were still 23 bits total. so the tree isnt unique but the total is always the same

**compressing can make files bigger.** the decoder needs the code table so it has to be saved in the file too (my header is 14 bytes + 5 bytes per different character). results from my C program:

- sample.txt (english text): 2968 bytes -> 1925 bytes (64.9%)
- random.bin (100KB random bytes): 100000 -> 101294 bytes (101.3%, bigger!!)
- abracadabra.txt: 11 -> 42 bytes (lol)

for the random one every byte still got exactly 8 bits because all 256 values show up about the same amount, so the tree is perfectly balanced and theres nothing to save. then the header gets added on top. same reason zipping a zip doesnt do anything

**the most common letter doesnt always get 1 bit.** if A B C D all show up the same amount they all get 2 bits. and if the data is 99% 'a' huffman still cant go under 1 bit per character even though the entropy is only 0.081. apparently thats why stuff like arithmetic coding exists, it can kind of use fractions of a bit

**english is already close to the limit.** on sample.txt my codes averaged 4.409 bits per character and the entropy was 4.368 so theres barely anything left. zlib still did better than me (48.5% vs 64.9%) because it runs LZ77 first to catch repeated words and then does huffman. huffman alone only looks at single characters

**flipping one bit.** I flipped one bit in the middle of the ABRACADABRA bits and it decoded as ABABCADABRA. so 2 letters were wrong and then it got back in sync. I expected it to mess up everything after it. the bigger problem is nothing tells you something went wrong, which is why gzip and png also have a CRC checksum

## where its used

- zip / gzip / png (they use DEFLATE which is LZ77 + huffman)
- jpeg and mp3 (the last step, after the lossy part)
- http/2 headers (theres a fixed huffman table that browsers and servers both already know)

## connection to comp 211

- bitwise ops: you cant write a single bit to a file so the encoder does `byte = (byte << 1) | bit` and writes it out every 8 bits. decoding reads them back with `(byte >> n) & 1`. the header numbers get split into bytes with `(x >> 8*i) & 0xFF`
- each tree node is a struct with left and right pointers
- every merge mallocs a node and the tree gets freed recursively (children first). ran it through valgrind, no leaks
- counting is just `freq[byte]++` on an array of 256
- I had to write a min heap so getting the two smallest is fast

## running it

### C (src/huffman.c)

needs gcc and make, works in the comp 211 container

```bash
make
make test
./huffman stats examples/abracadabra.txt
./huffman stats examples/sample.txt
./huffman compress examples/sample.txt sample.huf
./huffman decompress sample.huf sample.out
cmp examples/sample.txt sample.out && echo same
```

`make test` compresses and decompresses every example and checks the output is the exact same with cmp. it also makes examples/random.bin (100KB from /dev/urandom)

on windows I just ran it in the course docker image from powershell in the repo folder:

```powershell
docker run --rm -it -v "${PWD}:/work" -w /work learncli/comp211 bash
```

### python (python/)

just python 3, nothing to install

```bash
python3 python/huffman.py MISSISSIPPI
python3 python/experiments.py
```

experiments.py is all the tests from the "interesting" section above

## files

- src/huffman.c - compress / decompress / stats
- Makefile - build + round trip tests
- python/huffman.py - shorter python version
- python/experiments.py - ties, equal counts, 1 bit minimum, random data, zlib, bit flip
- examples/ - test files
- slides/ - my slides

## slides

[slides/huffman-slides.pptx](slides/huffman-slides.pptx) (speaker notes are in there too)

## how I learned it

I used Claude (AI) to make a study guide and then put it in NotebookLM and listened to it like a podcast since I learn better listening than reading. then I made the code and used the experiments to actually check what it told me, thats how I found the bit flip thing. more in my reflection

## stuff I still want to look at

- how arithmetic coding / ANS get past the 1 bit minimum
- canonical huffman codes (DEFLATE only stores the code lengths not the whole tree, would make my header way smaller)
- adaptive huffman where the tree updates as it reads so you dont have to send a table at all

## resources

- D. A. Huffman, "A Method for the Construction of Minimum-Redundancy Codes", Proceedings of the IRE, 1952 (the original paper)
- https://en.wikipedia.org/wiki/Huffman_coding - good overview + the term paper story
- RFC 1951 (DEFLATE) https://www.rfc-editor.org/rfc/rfc1951 - section 3.2 is how DEFLATE uses huffman codes
- RFC 7541 (HPACK) https://www.rfc-editor.org/rfc/rfc7541 - appendix B is the actual huffman table for http/2
- CLRS Introduction to Algorithms 3rd ed, section 16.3 - proof that the greedy way is optimal
- python heapq and zlib docs
- Claude and NotebookLM (see reflection)

slide images are all from wikimedia commons:
- huffman tree for "to be or not to be" by huffman.ooz.ie, CC0 (I recolored it)
- MIT great dome by Daderot, public domain
- morse telegraph key at Tekniska museet by Daderot, CC0
- paper tapes by TedColes, public domain
- floppy disk drives by Michael Holley, public domain
- slide template is "Mastering Code" from Slidesgo

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

huffman: src/huffman.c
	$(CC) $(CFLAGS) -o huffman src/huffman.c -lm

# random bytes to see what happens when theres nothing to compress
examples/random.bin:
	head -c 100000 /dev/urandom > examples/random.bin

# compress then decompress everything and check its the exact same
test: huffman examples/random.bin
	@for f in examples/*.txt examples/random.bin src/huffman.c; do \
		./huffman compress $$f /tmp/test.huf > /dev/null && \
		./huffman decompress /tmp/test.huf /tmp/test.out && \
		cmp -s $$f /tmp/test.out && echo "ok    $$f" || echo "FAIL  $$f"; \
	done

clean:
	rm -f huffman *.huf *.out examples/random.bin

.PHONY: test clean

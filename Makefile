CC      ?= gcc
CFLAGS  := -std=c99 -Wall -Wextra -O2
TARGET  := test_fracture
BENCH   := bench_fracture

.PHONY: all test bench clean

all: test

test: $(TARGET)
	./$(TARGET)

$(TARGET): test_fracture.c flux_fracture.h
	$(CC) $(CFLAGS) -o $@ test_fracture.c

bench: $(BENCH)
	./$(BENCH)

$(BENCH): bench_fracture.c flux_fracture.h
	$(CC) $(CFLAGS) -O3 -o $@ bench_fracture.c

clean:
	rm -f $(TARGET) $(BENCH)

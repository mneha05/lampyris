CC      ?= cc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Wpedantic -Werror
CORE    := firmware/lampyris/src
SRC     := $(wildcard $(CORE)/*.c)
INC     := -I$(CORE) -Isim
BUILD   := build

.PHONY: all test sim demo live figures clean

all: $(BUILD)/swarm $(BUILD)/test_lampyris

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/swarm: $(SRC) sim/world.c sim/swarm.c | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $^ -o $@ -lm

$(BUILD)/test_lampyris: $(SRC) sim/world.c test/test_lampyris.c | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $^ -o $@ -lm

test: $(BUILD)/test_lampyris
	./$(BUILD)/test_lampyris

demo: $(BUILD)/swarm
	./$(BUILD)/swarm

live: $(BUILD)/swarm
	./$(BUILD)/swarm --live -n 100 --layout scatter -t 40

figures: $(BUILD)/swarm
	./$(BUILD)/swarm -q -n 49 -t 20 --svg docs/demo.svg
	./$(BUILD)/swarm -q -s pteroptyx -n 64 --jitter 12 -t 12 --svg docs/pteroptyx.svg
	./$(BUILD)/swarm -q -s carolinus -n 36 --jitter 6 -t 150 --svg docs/carolinus.svg
	./$(BUILD)/swarm -q --layout ring -n 60 --seed 4 -t 16 --svg docs/ring.svg

clean:
	rm -rf $(BUILD)

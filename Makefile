CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -I.
CORE := game.c card.c rule.c score.c

.PHONY: all run test clean

all: saboteur

saboteur: main.c $(CORE) game.h card.h rule.h score.h
	$(CC) $(CFLAGS) main.c $(CORE) -o $@

run: saboteur
	./saboteur

test: test_card_build test_game_setup test_path_connection test_score test_sabotage_discard
	./test_card_build
	./test_game_setup
	./test_path_connection
	./test_score
	./test_sabotage_discard

test_card_build: test_card_build.c card.c game.h card.h
	$(CC) $(CFLAGS) test_card_build.c card.c -o $@

test_game_setup: test_game_setup.c game.c card.c rule.c game.h card.h rule.h
	$(CC) $(CFLAGS) test_game_setup.c game.c card.c rule.c -o $@

test_path_connection: test_path_connection.c rule.c card.c game.h card.h rule.h
	$(CC) $(CFLAGS) test_path_connection.c rule.c card.c -o $@

test_score: test_score.c score.c game.c card.c rule.c score.h game.h card.h rule.h
	$(CC) $(CFLAGS) test_score.c score.c game.c card.c rule.c -o $@

test_sabotage_discard: test_sabotage_discard.c rule.c game.h rule.h
	$(CC) $(CFLAGS) test_sabotage_discard.c rule.c -o $@

clean:
	rm -f saboteur test_card_build test_game_setup test_path_connection test_score test_sabotage_discard

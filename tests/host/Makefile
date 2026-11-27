SRC = ../../src
VERSION := $(shell sed -n 's/^VERSION = //p' ../../makefile)
CFLAGS = -std=c11 -Wall -Wextra -Werror -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -Iinclude -I$(SRC) -DAPP_VERSION=\"$(VERSION)\"
SOURCES = $(SRC)/var.c $(SRC)/json.c $(SRC)/api.c $(SRC)/db.c $(SRC)/plan.c $(SRC)/dl.c $(SRC)/http.c $(SRC)/text.c
TESTS = main.c test_var.c test_json.c test_api.c test_db.c test_plan.c test_dl.c test_http.c

all: test

host_test: $(TESTS) $(SOURCES) $(wildcard $(SRC)/*.h) check.h ../../makefile
	clang $(CFLAGS) -o $@ $(TESTS) $(SOURCES) -lcrypto

test: host_test
	./host_test

clean:
	rm -f host_test

.PHONY: all test clean

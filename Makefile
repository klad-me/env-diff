CFLAGS=-Wall -O3
LDFLAGS=
EXE=env-diff
SRC=env-diff.c

.PHONY:	all

all:	$(EXE)

install: $(EXE)
	install -m 755 -D $< /usr/bin/env-diff
	install -m 644 -D env-diff.1 /usr/share/man/man1/env-diff.1

clean:
	rm -f $(EXE) $(SRC:%=%.o)

$(EXE): $(SRC:%=%.o)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< && strip $@

%.c.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

CFLAGS += -DSHAPE -DCOLOR -Wall -pedantic -ansi -D_XOPEN_SOURCE
LDLIBS = -lXext -lX11
PREFIX ?= /usr
BIN = $(DESTDIR)$(PREFIX)/bin

MANDIR = $(DESTDIR)$(PREFIX)/share/man/man1
MANSUFFIX = 1

OBJS = 9wm.o event.o manage.o menu.o client.o grab.o cursor.o error.o config.o workspace.o spaces.o
HFILES = dat.h fns.h config.h workspace.h spaces.h

all: shrub9

shrub9: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

install: shrub9
	mkdir -p $(BIN)
	cp shrub9 $(BIN)/shrub9

install.man:
	mkdir -p $(MANDIR)
	cp 9wm.man $(MANDIR)/shrub9.$(MANSUFFIX)

$(OBJS): $(HFILES)

clean:
	rm -f shrub9 9wm *.o

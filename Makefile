PCDEPS=glib-2.0 json-glib-1.0
CFLAGS+=`pkg-config --cflags $(PCDEPS)`
LDFLAGS+=-module -export-dynamic
LDLIBS+=`pkg-config --libs $(PCDEPS)`
CC=gcc
LT=libtool
LIBS=libomegle.la
LIBPREFIX=/usr/lib/purple-2

.PHONY: all clean install

all: $(LIBS)

%.la: %.lo
	$(LT) --mode=link $(LINK.o) -rpath $(LIBPREFIX) $^ $(LOADLIBES) $(LDLIBS) -o $@

%.lo: %.c
	$(LT) --mode=compile $(COMPILE.c) $(OUTPUT_OPTION) $<

libomegle.la: libomegle.lo om_connection.lo

install:
	$(LT) --mode=install cp $(LIBS) $(DESTDIR)$(LIBPREFIX)

uninstall:
	$(LT) --mode=uninstall rm -f $(addprefix $(LIBPREFIX),$(LIBS))

clean:
	rm -f *.o *.lo *.la
	rm -rf .libs

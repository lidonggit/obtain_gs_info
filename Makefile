LIBDIR= -L/home/dol/download/dwarf-20110113/libdwarf -L/home/dol/libraries/libelf/lib
LIBS= -ldwarf -lelf
CFLAGS= -g -Wall -I/home/dol/download/dwarf-20110113/libdwarf

all: bifit_binary_parse

bifit_binary_parse: bifit_binary_parse.c
	$(CC) $(CFLAGS) bifit_binary_parse.c -o bifit_binary_parse $(LIBDIR) $(LIBS)

clean:
	@rm -f bifit_binary_parse
	@rm -f bifit_binary_parse.o

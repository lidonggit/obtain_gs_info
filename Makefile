LIBDIR= -L/home/dol/download/dwarf-20110113/libdwarf -L/home/dol/libraries/libelf/lib
LIBS= -ldwarf -lelf
CFLAGS= -g -Wall -I/home/dol/download/dwarf-20110113/libdwarf

all: bifit_binary_parser bifit_preproc

bifit_binary_parser: bifit_binary_parser.c
	gcc $(CFLAGS) bifit_binary_parser.c -o bifit_binary_parser $(LIBDIR) $(LIBS)

bifit_preproc: bifit_preproc.cpp
	g++ -o bifit_preproc bifit_preproc.cpp

clean:
	@rm -f bifit_binary_parser bifit_preproc
	@rm -f bifit_binary_parser.o bifit_preproc.o

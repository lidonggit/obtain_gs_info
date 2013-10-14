

LIBDIR= -L/home/dol/download/dwarf-20110113/libdwarf -L/home/dol/libraries/libelf/lib
LIBS= -ldwarf -lelf
CFLAGS= -g -Wall -I/home/dol/download/dwarf-20110113/libdwarf

all: obtain_static_global_variables

obtain_static_global_variables: obtain_static_global_variables.c
	$(CC) $(CFLAGS) obtain_static_global_variables.c -o obtain_static_global_variables $(LIBDIR) $(LIBS)

clean:
	rm -f obtain_static_global_variables
	rm -f obtain_static_global_variables.o

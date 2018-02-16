##OBJECT: cmcarc
#
#
##MODULES:  cmcarc.c cmcarc.h main.c pcmc.c pcmc.h utile.c utile.h
#
##Daniel Pelissier (99/11/08)
##M. Valin 2016/11/22 (stripped to the bone)
#
SWDEST = ..
EXEC = cmcarc_4.3.1u
EXEC2 = ordarc
CFLAGS=-g -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64   -Wall
#LDFLAG = -g
OBJECTS=cmcarc.o main.o pcmc.o utile.o hashtable.o hashtable_itr.o 
LIBCMCARC_OBJECTS=cmcarc.o pcmc.o utile.o hashtable.o hashtable_itr.o cmcarc_lib.o
MANDIR=$(SWDEST)/share/man/cat1
#
#
#
#
dist: $(EXEC)
	mv $(EXEC) ../bin
	cd ../bin ; ln -sf $(EXEC) cmcarc
	make clean

$(EXEC):	$(OBJECTS)
	gcc $(CFLAGS) $(OBJECTS) -o $(EXEC) -lm -lcrypto
	#gcc $(CFLAGS) $(OBJECTS) -o $(EXEC).static -lm -static -lcrypto 
	#gcc $(CFLAGS) $(OBJECTS) -o $(EXEC2).static -lm -static -lcrypto 

libcmcarc.a: $(LIBCMCARC_OBJECTS)
	ar pr libcmcarc.a $(LIBCMCARC_OBJECTS)

install: $(EXEC)
	cp $(EXEC) $(SWDEST)/bin/$(EXEC)

man:
	groff -Tascii -man cmcarc.1.man >cmcarc.1

clean:
	rm -f $(OBJECTS)
	rm -f $(LIBCMCARC_OBJECTS)
	rm -f $(EXEC2).static
	rm -f $(EXEC).static
	rm -f $(EXEC)
	rm -f core


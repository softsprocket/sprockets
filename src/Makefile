
additional_flags = -std=c11 -I../include
additional_lib_flags = -fPIC

LIBS = -ldl -lsscont -lserc

all: $(package) lib$(package).$(version).so

libobjects = sprocket_bus.o

objects = main.o

main.o: main.c
	 $(CC) -c main.c $(CFLAGS) $(additional_flags) -o $@ 

$(package): $(objects)
	-mkdir -p ../bin
	$(CC) $(CFLAGS) $(additional_flags) -o ../bin/$@ $(objects) $(LIBS)

lib$(package).$(version).so: $(libobjects)
	-mkdir -p ../lib
	$(CC) $(CFLAGS) $(additional_flags) -shared -o ../lib/$@ $(libobjects) $(LIBS)

sprocket_bus.o: sprocket_bus.c
	 $(CC) -c -D_POSIX_C_SOURCE sprocket_bus.c $(CFLAGS) $(additional_flags) $(additional_lib_flags) -o $@ 

clean:
	-rm *.o
	-rm ../bin/$(package)

install:
	install -d  $(DESTDIR)$(prefix)/bin
	install -d  $(DESTDIR)$(prefix)/include/softsprocket
	install -m 0755 ../bin/$(package)  $(DESTDIR)$(prefix)/bin
	install -m 0644 ../include/sprocket.h $(DESTDIR)$(prefix)/include/softsprocket
	
uninstall:
	-rm  $(DESTDIR)$(prefix)/bin/$(package)
	-rm -d  $(DESTDIR)$(prefix)/bin
	-rm $(DESTDIR)$(prefix)/include/softsprocket/sprocket.h
	-rm -d $(DESTDIR)$(prefix)/include/softsprocket

.PHONY: all clean install uninstall



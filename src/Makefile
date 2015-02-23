LDFLAGS = -L../lib
additional_flags = -std=c11 -I../include -pthread
additional_lib_flags = -fPIC

LIBS = -ldl -lsscont -lserc 

all: $(package) lib$(package).$(version).so $(watchdog)

libobjects = sprocket.o

objects = main.o spike.o sprocket.o

main.o: main.c
	 $(CC) -c main.c -D_POSIX_C_SOURCE $(CFLAGS) $(additional_flags) -o $@ 

spike.o: spike.c 
	 $(CC) -c spike.c -D_POSIX_C_SOURCE $(CFLAGS) $(additional_flags) -o $@ 

$(package): $(objects) lib$(package).$(version).so
	-mkdir -p ../bin
	$(CC) $(CFLAGS) $(additional_flags) -o ../bin/$@ $(objects)  $(LIBS)


lib$(package).$(version).so: $(libobjects)
	-mkdir -p ../lib
	$(CC) $(CFLAGS) $(additional_flags) -shared -o ../lib/$@ $(libobjects) $(LIBS)

sprocket.o: sprocket.c
	 $(CC) -c -D_POSIX_C_SOURCE sprocket.c $(CFLAGS) $(additional_flags) $(additional_lib_flags) -o $@ 

clean:
	-rm *.o
	-rm ../bin/$(package)
	-rm ../lib/lib$(package).$(version).so

install:
	install -d  $(DESTDIR)$(prefix)/bin
	install -d  $(DESTDIR)$(prefix)/include/softsprocket
	install -d  $(DESTDIR)$(prefix)/lib/softsprocket
	install -m 0644 ../lib/lib$(package).$(version).so  $(DESTDIR)$(prefix)/lib/softsprocket
	install -m 0755 ../bin/$(package)  $(DESTDIR)$(prefix)/bin
	install -m 0644 ../include/sprocket.h $(DESTDIR)$(prefix)/include/softsprocket
	-ln -s $(DESTDIR)$(prefix)/lib/softsprocket/lib$(package).$(version).so $(DESTDIR)$(prefix)/lib/lib$(package).so

uninstall:
	-rm  $(DESTDIR)$(prefix)/bin/$(package)
	-rm -d  $(DESTDIR)$(prefix)/bin
	-rm $(DESTDIR)$(prefix)/include/softsprocket/sprocket.h
	-rm -d $(DESTDIR)$(prefix)/include/softsprocket

.PHONY: all clean install uninstall



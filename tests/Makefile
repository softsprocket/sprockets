LDFLAGS = -L../lib
additional_flags = -std=c11 -I../include -fPIC

LIBS = -lsscont -lserc -l$(package).$(version)

all check memtest: $(test_exec)

$(test_exec): server0.so server1.so server2.so

server0.so: server0.c
	gcc $(CFLAGS) $(additional_flags) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ server0.c $(LIBS)

server1.so: server1.c
	gcc $(CFLAGS) $(additional_flags) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ server1.c $(LIBS)

server2.so: server2.c
	gcc $(CFLAGS) $(additional_flags) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ server2.c $(LIBS)

clean:
	-rm server0.o
	-rm server1.o
	-rm server2.o


.PHONY: all clean $(test_exec)


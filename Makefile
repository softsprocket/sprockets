export package = sprockets
export version = 1.0
export test_exec = $(package)
export CFLAGS = -g -O0 -Wall
export prefix = /usr

tarname = $(package)
distdir = $(tarname)-$(version)

all clean $(package) lib$(package).$(version).so install uninstall:
	cd src && $(MAKE) $@

dist: $(distdir).tar.gz

$(distdir).tar.gz: $(distdir)
	tar chof - $(distdir) | gzip -9 -c > $@
	rm -rf $(distdir)

$(distdir): FORCE
	mkdir -p $(distdir)/src $(distdir)/lib $(distdir)/tests $(distdir)/include $(distdir)/docs
	cp Makefile $(distdir)
	cp INSTALL README README.md LICENSE $(distdir)
	cp src/Makefile $(distdir)/src
	cp src/*.c $(distdir)/src
	cp include/*.h $(distdir)/include
	cp tests/* $(distdir)/tests
	cp tests/Makefile $(distdir)/tests
	cp -r docs/ $(distdir)/docs
	cp Doxyfile $(distdir)/

distcheck: $(distdir).tar.gz
	gzip -cd $(distdir).tar.gz | tar xvf -
	cd $(distdir) && $(MAKE) check
	cd $(distdir) && $(MAKE) DESTDIR=$${PWD}/tmp_inst install
	cd $(distdir) && $(MAKE) DESTDIR=$${PWD}/tmp_inst uninstall
	cd $(distdir) && $(MAKE) clean
	rm -rf $(distdir)
	rm $(distdir).tar.gz
	@echo "*** Package $(distdir).tar.gz is ready for distribution."


check: all
	cd tests && $(MAKE) $@
	export LD_LIBRARY_PATH=lib/; bin/$(test_exec) tests/test.serc

memtest: all
	cd tests && $(MAKE) $@
	valgrind --tool=memcheck --leak-check=yes  --leak-check=full --track-origins=yes -v bin/$(test_exec) tests/test.serc

docs: 
	doxygen

install-docs: docs
	cd docs && $(MAKE) $@

uninstall-docs:
	cd docs && $(MAKE) $@

clean-docs:
	cd docs && $(MAKE) $@

FORCE:
	-rm $(distdir).tar.gz >/dev/null 2>&1
	-rm -rf $(distdir) >/dev/null 2>&1


.PHONY: FORCE all clean check memcheck dist distcheck docs install-docs uninstall-docs




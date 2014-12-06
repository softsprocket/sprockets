
MANPAGES = array_to_auto_string.3 create_literal_value.3 delete_literal_value.3 int_ptr.3 serc_parse_buffer.3 storage_unit_named_float.3 storage_unit_named_set.3\
	array_to_set.3 create_map.3 delete_storage_unit.3 literal_to_auto_string.3 serc_read_file.3 storage_unit_named_heredoc.3 storage_unit_named_string.3\
	char_ptr.3 create_sequence.3 document_to_auto_string.3  map_to_auto_string.3 set_to_auto_string.3  storage_unit_named_integer.3 storage_unit_ptr.3\
	create_array.3  delete_document.3  double_ptr.3  parseable_buffer_from_string.3  storage_unit_named_array.3  storage_unit_named_map.3 str_ptr.3\
	create_id_tuple.3 delete_id_tuple.3 id_tuple_to_auto_string.3  serc_document_to_auto_string.3  storage_unit_named_char.3  storage_unit_named_sequence.3  str_return_value.3


clean-docs:
	rm -rf html latex

install-docs:
	install -d  $(DESTDIR)/usr/share/man/man3
	install -d $(DESTDIR)/usr/share/man/man7
	cd man/man3/ && install -c -m 0644 $(MANPAGES) $(DESTDIR)/usr/share/man/man3/
	cd man/man7/ && install -c -m 0644 serc.7 $(DESTDIR)/usr/share/man/man7/
	install -d $(DESTDIR)/usr/share/doc/softsprocket/libserc/html
	cp -r html/* $(DESTDIR)/usr/share/doc/softsprocket/libserc/html/
	
uninstall-docs:
	-cd $(DESTDIR)/usr/share/man/man3/ && rm $(MANPAGES)	
	-cd $(DESTDIR)/usr/share/man/man7/ && rm serc.7
	-rm -rf $(DESTDIR)/usr/share/doc/softsprocket/libserc

.PHONY: install-docs uninstall-docs clean



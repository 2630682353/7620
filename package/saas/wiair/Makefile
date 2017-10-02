DIR=saas cloud

.PHONY: all clean distclean

all dep clean distclean:
	for d in $(DIR); do \
		make -C $$d $@ || exit "$$?"; \
	done

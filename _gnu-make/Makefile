.PHONY: build test preprocess cut clean

# directories
ifeq ($(realpath .),)
  $(error your version of Make doesn't support $$(realpath names...) - please use GNU Make 3.81 or later)
else
  prefix := $(realpath ..)
  srcdir := $(realpath ..)
  buildir := $(realpath .)/build
  binsubdir := $(shell sh -c 'uname -s 2>/dev/null || echo unknown-os')-$(shell sh -c 'uname -m 2>/dev/null || echo unknown-arch')
  bindir := $(prefix)/bin/$(binsubdir)
endif

CFLAGS := -O2 -g

define _generate_rules
.PHONY: build-$(1)
build: build-$(1)
build-$(1): $$(bindir)/$(1)SelfTest $$(bindir)/$(1)SelfBench

$$(bindir)/$(1)SelfTest: $$(srcdir)/$(1).c
	mkdir -p $$(@D)
	$$(CC) -o $$@ -DPACKEDARRAY_SELF_TEST -std=c99 -pedantic $$(CFLAGS) $$<

$$(bindir)/$(1)SelfBench: $$(srcdir)/$(1).c
	mkdir -p $$(@D)
	$$(CC) -o $$@ -DPACKEDARRAY_SELF_BENCH -DNDEBUG -std=c99 -pedantic $$(CFLAGS) $$<

.PHONY: test-$(1)
test : test-$(1)
test-$(1): build-$(1)  
	$$(bindir)/$(1)SelfTest

.PHONY: preprocess-$(1)
preprocess: preprocess-$(1)
preprocess-$(1): $$(srcdir)/$(1).c
	@echo Preprocessing $(1).c to $(1).pp.c
	$$(eval tmp := $$(shell mktemp -t $(1)))
	@sed -E\
		-e 's/#include (<|").+(>|")/pp(&)/'\
		$$(srcdir)/$(1).c > $$(tmp)
	@$$(CPP) $$(CPPFLAGS) -DPACKEDARRAY_SELF=__FILE__ -xc $$(tmp) | sed -E\
		-e '/^# /d'\
		-e 's/[ \t]*$$$$//'\
		-e 's/pp\((.*)\)/\1/'\
		| cat -s > $$(srcdir)/$(1).pp.c
	@rm -f $$(tmp)

.PHONY: cut-$(1)
cut: cut-$(1)
cut-$(1): $$(srcdir)/$(1).c
	@echo Cutting down your anxiety to $(1).cut.c
	@sed -n '/- 8<.*/q;p' $$(srcdir)/$(1).c > $$(srcdir)/$(1).cut.c
endef

$(foreach p,PackedArray PackedArraySIMD, $(eval $(call _generate_rules,$(p))))

clean:
	rm -rf $(buildir)
	rm -rf $(bindir)
	rm -rf $(srcdir)/*.pp.c
	rm -rf $(srcdir)/*.cut.c

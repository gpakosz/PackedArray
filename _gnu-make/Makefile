.PHONY: build test preprocess cut assembly clean

# directories
ifeq ($(realpath .),)
  $(error your version of Make doesn't support $$(realpath names...) - please use GNU Make 3.81 or later)
endif

ifeq ($(platform),)
  __uname_s := $(shell sh -c 'uname -s 2>/dev/null | tr [A-Z] [a-z] || echo unknown-platform')
  __uname_m := $(shell sh -c 'uname -m 2>/dev/null | tr [A-Z] [a-z] || echo unknown-architecture')

  ifeq ($(__uname_s),linux)
    override platform := linux
    override architecture := $(__uname_m)
  endif
  ifeq ($(__uname_s),darwin)
    override platform := mac
    override architecture := $(__uname_m)
  endif
endif
ifeq ($(architecture),)
  override architecture := unknown-architecture
endif

prefix := $(realpath ..)
srcdir := $(realpath ..)
buildir := $(realpath .)/build
binsubdir := $(platform)-$(architecture)
bindir := $(prefix)/bin/$(binsubdir)

CFLAGS := -O2 -g

define _generate_rules
.PHONY: build-$(1)
build: build-$(1)
build-$(1): $(bindir)/$(1)SelfTest $(bindir)/$(1)SelfBench

$(bindir)/$(1)SelfTest: $(srcdir)/$(1).c $(srcdir)/PackedArray.h
	mkdir -p $$(@D)
	$(CC) -o $$@ -DPACKEDARRAY_SELF_TEST -std=c99 -pedantic $(CFLAGS) $$<
	$$(if $(postbuild),$(postbuild) $$@)

$(bindir)/$(1)SelfBench: $(srcdir)/$(1).c $(srcdir)/PackedArray.h
	mkdir -p $$(@D)
	$(CC) -o $$@ -DPACKEDARRAY_SELF_BENCH -DNDEBUG -std=c99 -pedantic $(CFLAGS) $$<
	$$(if $(postbuild),$(postbuild) $$@)

.PHONY: assembly-$(1)
assembly: assembly-$(1)
assembly-$(1): $(bindir)/$(1)SelfTest.s $(bindir)/$(1)SelfBench.s

$(bindir)/$(1)SelfTest.s: $(srcdir)/$(1).c $(srcdir)/PackedArray.h
	@echo Generating $$(@F) assembly listing
	@mkdir -p $$(@D)
	@$(CC) -o $$@ -S -fverbose-asm -g -DPACKEDARRAY_SELF_TEST -std=c99 -pedantic $(CFLAGS) $$<

$(bindir)/$(1)SelfBench.s: $(srcdir)/$(1).c $(srcdir)/PackedArray.h
	@echo Generating $$(@F) assembly listing
	@mkdir -p $$(@D)
	@$(CC) -o $$@ -S -fverbose-asm -g -DPACKEDARRAY_SELF_BENCH -DNDEBUG -std=c99 -pedantic $(CFLAGS) $$<

.PHONY: test-$(1)
test : test-$(1)
test-$(1): build-$(1)
	$(bindir)/$(1)SelfTest

.PHONY: preprocess-$(1)
preprocess: preprocess-$(1)
preprocess-$(1): $(srcdir)/$(1).c
	@echo Preprocessing $(1).c to $(1).pp.c
	$$(eval tmp := $$(shell mktemp -t $(1)))
	@sed -E\
		-e 's/#include (<|").+(>|")/pp(&)/'\
		$(srcdir)/$(1).c > $$(tmp)
	@$(CPP) $(CPPFLAGS) -DPACKEDARRAY_SELF=__FILE__ $(CFLAGS) -xc $$(tmp) | sed -E\
		-e '/^# /d'\
		-e 's/[ \t]*$$$$//'\
		-e 's/pp\((.*)\)/\1/'\
		| cat -s > $(srcdir)/$(1).pp.c
	@rm -f $$(tmp)

.PHONY: cut-$(1)
cut: cut-$(1)
cut-$(1): $(srcdir)/$(1).c
	@echo Cutting down your anxiety to $(1).cut.c
	@sed -n '/- 8<.*/q;p' $(srcdir)/$(1).c > $(srcdir)/$(1).cut.c
endef

$(foreach p,PackedArray PackedArraySIMD, $(eval $(call _generate_rules,$(p))))

clean:
	rm -rf $(buildir)
	rm -rf $(bindir)
	rm -rf $(srcdir)/*.pp.c
	rm -rf $(srcdir)/*.cut.c

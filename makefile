override MAKEFLAGS += -rR
override OUTPUT := ate

define DEFAULT_VAR =
    ifeq ($(origin $1),default)
        override $(1) := $(2)
    endif
    ifeq ($(origin $1),undefined)
        override $(1) := $(2)
    endif
endef

override DEFAULT_KCC := cc
$(eval $(call DEFAULT_VAR,KCC,$(DEFAULT_KCC)))
override DEFAULT_KLD := cc
$(eval $(call DEFAULT_VAR,KLD,$(DEFAULT_KLD)))
override DEFAULT_KCFLAGS := -Wall -Wextra -Wpedantic -g -fsanitize=address -O3
$(eval $(call DEFAULT_VAR,KCFLAGS,$(DEFAULT_KCFLAGS)))
override DEFAULT_KCPPFLAGS :=
$(eval $(call DEFAULT_VAR,KCPPFLAGS,$(DEFAULT_KCPPFLAGS)))
override DEFAULT_KNASMFLAGS :=
$(eval $(call DEFAULT_VAR,KNASMFLAGS,$(DEFAULT_KNASMFLAGS)))
override DEFAULT_KLDFLAGS :=  -fsanitize=address -g
$(eval $(call DEFAULT_VAR,KLDFLAGS,$(DEFAULT_KLDFLAGS)))

override KCFLAGS += \
    -Wall \
    -Wextra \
    -std=gnu11 \
    -m64 \
    -march=native
override KCPPFLAGS := \
    -I src \
    $(KCPPFLAGS)
override KNASMFLAGS += \
    -Wall \
    -f elf64
override KLDFLAGS += \
    -m64 \
    -lgcc -lm

override CFILES := $(shell cd src && find -L * -type f -name '*.c' | LC_ALL=C sort)
override ASFILES := $(shell cd src && find -L * -type f -name '*.S' | LC_ALL=C sort)
override NASMFILES := $(shell cd src && find -L * -type f -name '*.asm' | LC_ALL=C sort)
override OBJ := $(addprefix obj/,$(CFILES:.c=.c.o) $(ASFILES:.S=.S.o) $(NASMFILES:.asm=.asm.o))
override HEADER_DEPS := $(addprefix obj/,$(CFILES:.c=.c.d) $(ASFILES:.S=.S.d))
.PHONY: all
all: bin/$(OUTPUT)
bin/$(OUTPUT): $(OBJ)
	mkdir -p "$$(dirname $@)"
	cc -o $@ $(OBJ) $(KLDFLAGS)
-include $(HEADER_DEPS)
obj/%.c.o: src/%.c
	mkdir -p "$$(dirname $@)"
	$(KCC) $(KCFLAGS) $(KCPPFLAGS) -c $< -o $@
obj/%.S.o: src/%.s
	mkdir -p "$$(dirname $@)"
	$(KCC) $(KCFLAGS) $(KCPPFLAGS) -c $< -o $@
.PHONY: clean
clean:
	rm -rf bin obj


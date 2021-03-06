MAKEFLAGS += --check-symlink-times

CC	= gcc
AS	= gcc
AR	= ar rcs

CFLAGS	= -Wall -fPIC -O2 -std=gnu99 -fgnu89-inline -Wall -U_FORTIFY_SOURCE \
	  -fno-omit-frame-pointer \
	  -fno-stack-protector -fno-builtin \
	  -I. -I../.. -I../../../include -I../../../lib -I../../../ipc/linux \
	  -I../../../linux-3.5/graphene
ASFLAGS = -DPIC -DSHARED -fPIC -DASSEMBLER -Wa,--noexecstack \
	  -x assembler-with-cpp \
	  -I. -I../.. -I../../../include
ARFLAGS	=

host_target = libpal-Linux.a

defs	= -DIN_PAL -DPAL_LOADER_PATH="$(shell readlink -f ../../pal)" \
	  -DGDB_SCRIPT="$(CURDIR)/pal-gdb.py"
objs	= $(addprefix db_,files devices pipes sockets streams memory threading \
	    semaphore mutex events process object main rtld misc ipc \
	    exception) manifest clone-x86_64 gettimeofday-x86_64
graphene_lib = ../../../lib/graphene-lib.a
headers	= $(wildcard *.h) $(wildcard ../../*.h) $(wildcard ../../../lib/*.h)

all: $(host_target)

debug: debug = debug
debug: CC = gcc -g
debug: CFLAGS += -DDEBUG
debug: $(host_target)

$(graphene_lib):
	make -C ../../../lib $(debug)

libpal-Linux.a: $(addsuffix .o,$(objs)) $(graphene_lib)
	@echo [ host/Linux/$@ ]
	@$(AR) $(ARFLAGS) $@ $^

%.asm: %.c $(headers)
	$(CC) $(CFLAGS) $(defs) -c $< -o $<.o
	@objdump -S $<.o > $@
	@rm $<.o

%.o: %.c $(headers)
	@echo [ host/Linux/$@ ]
	@$(CC) $(CFLAGS) $(defs) -c $< -o $@

%.e: %.c $(headers)
	@echo [ host/Linux/$@ ]
	@$(CC) $(CFLAGS) $(defs) -E $< -o $@

%.o: %.S $(headers)
	@echo [ host/Linux/$@ ]
	@$(AS) $(ASFLAGS) $(defs) -c $< -o $@

%.e: %.S $(headers)
	@echo [ host/Linux/$@ ]
	@$(AS) $(ASFLAGS) $(defs) -E $< -o $@

clean:
	rm -f $(addsuffix .o,$(objs)) $(host_target)

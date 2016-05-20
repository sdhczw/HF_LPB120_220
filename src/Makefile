include ../Makefile.mk


CLEAN += %.elf $(CONTIKI_PROJECT).elf

### Compilation rules

# Don't treat %.elf %.bin as an imtermediate file!
.PRECIOUS: %.elf %.bin

OBJECTDIR=objs

CONTIKI_SRC = \
app_main.c \
./Application/custom.c \
./thirdPart/zc_hf_adpter.c \
./thirdPart/ZC/src/aes/aes_cbc.c \
./thirdPart/ZC/src/aes/aes_core.c \
./thirdPart/ZC/src/crc/crc.c \
./thirdPart/ZC/src/tropicssl/bignum.c \
./thirdPart/ZC/src/tropicssl/rsa.c \
./thirdPart/ZC/src/zc/zc_bc.c \
./thirdPart/ZC/src/zc/zc_client_manager.c \
./thirdPart/ZC/src/zc/zc_cloud_event.c \
./thirdPart/ZC/src/zc/zc_common.c \
./thirdPart/ZC/src/zc/zc_configuration.c \
./thirdPart/ZC/src/zc/zc_message_queue.c \
./thirdPart/ZC/src/zc/zc_moudle_manager.c \
./thirdPart/ZC/src/zc/zc_protocol_controller.c \
./thirdPart/ZC/src/zc/zc_sec_engine.c \
./thirdPart/ZC/src/zc/zc_timer.c \
./thirdPart/AC/src/ac_api.c \
./thirdPart/AC/src/ac_hal.c
APPCFLAGS = -DSUPPORT_UART_THROUGH \
-DZC_MODULE_VERSION=0 -DZC_MODULE_TYPE=2 -DZC_MODULE_DEV -DZC_EASY_UART -DTEST_ADDR=$(__HF_MODULE_ID__) \
-I ./ \
-I ./thirdPart \
-I ./thirdPart/ZC/inc/aes \
-I ./thirdPart/ZC/inc/tropicssl \
-I ./thirdPart/ZC/inc/zc \
-I ./thirdPart/AC/inc

CONTIKI_OBJS=${addprefix $(OBJECTDIR)/,$(CONTIKI_SRC:.c=.o) $(CONTIKI_SRC:.c=.o)}

all:
	@mkdir -p objs
	@mkdir -p objs/Application
	@mkdir -p objs/maxense
	@mkdir -p objs/thirdPart/AC/src
	@mkdir -p objs/thirdPart/ZC/src/aes
	@mkdir -p objs/thirdPart/ZC/src/crc
	@mkdir -p objs/thirdPart/ZC/src/tropicssl
	@mkdir -p objs/thirdPart/ZC/src/zc
	make userapps.a 

userapps.a: $(CONTIKI_OBJS)
	echo "$(AR) $(AROPTS) $@ $^"
	$(AR) $(AROPTS) $@ $^

print:
	echo "CONTIKI_SRC=$(CONTIKI_SRC)"
	echo "CONTIKI_OBJS=$(CONTIKI_OBJS)"

$(OBJECTDIR)/%.o: %.c | $(OBJECTDIR)
	@echo "-----------------make z1 target:$@ $(makefn3)-----------------"
	$(CC) $(CFLAGS) $(APPCFLAGS) -c $< -o $@

clean:
	/bin/rm -f *.o *.elf *.a *.bin *.asm 
	/bin/rm -rf $(OBJECTDIR)


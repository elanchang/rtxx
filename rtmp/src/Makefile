PWD		:= $(shell pwd)
INSTALL_DIR	?= $(PWD)/target
INSTALL_INC_DIR	:= $(INSTALL_DIR)/include/libmid_rtmp_client
INSTALL_LIB_DIR := $(INSTALL_DIR)/lib
OBJ_DIR		?=$(PWD)/obj

ARCH            ?= arm
TARGET          ?= $(ARCH)-linux
CROSS_COMPILE   ?= $(TARGET)-
CC		= $(CROSS_COMPILE)gcc
AR		= $(CROSS_COMPILE)ar

CFLAGS		= $(PRJ_CFLAGS)
CFLAGS		+= -I$(PWD)
CFLAGS		+= -I$(KERNEL_INC_DIR)
CFLAGS		+= -I$(DRIVER_INC_DIR)
CFLAGS		+= -I$(MIDDLEWARE_INC_DIR)
CFLAGS		+= -I$(MIDDLEWARE_LIBC_INC_DIR)
CFLAGS		+= -I$(SDK_CONF_DIR)
CFLAGS		+= -I$(MIDDLEWARE_INC_DIR)/libmid_lwip/include -I$(MIDDLEWARE_INC_DIR)/libmid_lwip/include/ipv4

STATIC_LIB = $(OBJ_DIR)/libmid_rtmp_client.a

OBJS_FILES = rtmp_string.o rtmp_mbuffer.o kvs.o rtmp_amf.o rtmp_chunk.o rtmp_cmd.o rtmp_utils.o rtmp_msg.o rtmp_h264.o rtmp_aac.o rtmp_simple_handshake.o rtmp_socket.o 
OBJS_FILES += rtmp_socket_to_buffer.o rtmp_protocol.o rtmp_client.o snx_rtmp.o

#OBJS_FILES += version.o
OBJS = $(addprefix $(OBJ_DIR)/, $(OBJS_FILES))

.PHONY:all clean distclean install prepare
all: $(OBJ_DIR) $(STATIC_LIB)
$(OBJ_DIR):
	[ -d "$@" ] || mkdir -p $@
$(STATIC_LIB):$(OBJS)
	$(AR) -r $@ $(OBJS)

$(OBJ_DIR)/rtmp_string.o:$(PWD)/rtmp_string.c
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJ_DIR)/rtmp_mbuffer.o:$(PWD)/rtmp_mbuffer.c
	$(CC) -c $(CFLAGS) $< -o $@	

$(OBJ_DIR)/kvs.o:$(PWD)/kvs.c
	$(CC) -c $(CFLAGS) $< -o $@	

$(OBJ_DIR)/rtmp_amf.o:$(PWD)/rtmp_amf.c
	$(CC) -c $(CFLAGS) $< -o $@	
	
$(OBJ_DIR)/rtmp_chunk.o:$(PWD)/rtmp_chunk.c
	$(CC) -c $(CFLAGS) $< -o $@	

$(OBJ_DIR)/rtmp_cmd.o:$(PWD)/rtmp_cmd.c
	$(CC) -c $(CFLAGS) $< -o $@	

$(OBJ_DIR)/rtmp_utils.o:$(PWD)/rtmp_utils.c
	$(CC) -c $(CFLAGS) $< -o $@	
	
$(OBJ_DIR)/rtmp_msg.o:$(PWD)/rtmp_msg.c
	$(CC) -c $(CFLAGS) $< -o $@	

$(OBJ_DIR)/rtmp_h264.o:$(PWD)/rtmp_h264.c
	$(CC) -c $(CFLAGS) $< -o $@	

$(OBJ_DIR)/rtmp_aac.o:$(PWD)/rtmp_aac.c
	$(CC) -c $(CFLAGS) $< -o $@		
	
$(OBJ_DIR)/rtmp_simple_handshake.o:$(PWD)/rtmp_simple_handshake.c
	$(CC) -c $(CFLAGS) $< -o $@	

$(OBJ_DIR)/rtmp_socket.o:$(PWD)/rtmp_socket.c
	$(CC) -c $(CFLAGS) $< -o $@	

$(OBJ_DIR)/rtmp_socket_to_buffer.o:$(PWD)/rtmp_socket_to_buffer.c
	$(CC) -c $(CFLAGS) $< -o $@	

$(OBJ_DIR)/rtmp_protocol.o:$(PWD)/rtmp_protocol.c
	$(CC) -c $(CFLAGS) $< -o $@	

$(OBJ_DIR)/rtmp_client.o:$(PWD)/rtmp_client.c
	$(CC) -c $(CFLAGS) $< -o $@	
	
$(OBJ_DIR)/snx_rtmp.o:$(PWD)/snx_rtmp.c
	$(CC) -c $(CFLAGS) $< -o $@	
	
#$(OBJ_DIR)/version.o:$(PWD)/version.c
#	$(CC) -c -I$(SDK_CONF_DIR) $^ -o $@

clean:
	rm -rf $(OBJ_DIR) $(OBJS) $(STATIC_LIB)

distclean:

install:
	[ -d "$(INSTALL_INC_DIR)" ] || mkdir -p $(INSTALL_INC_DIR)
	[ -d "$(INSTALL_LIB_DIR)" ] || mkdir -p $(INSTALL_LIB_DIR)
	cp $(STATIC_LIB) $(INSTALL_LIB_DIR)
	cp -af rtmp_error.h $(INSTALL_INC_DIR)
	cp -af rtmp_utils.h $(INSTALL_INC_DIR)
	cp -af rtmp_string.h $(INSTALL_INC_DIR)
	cp -af rtmp_client.h $(INSTALL_INC_DIR)
	cp -af rtmp_mbuffer.h $(INSTALL_INC_DIR)
	cp -af snx_rtmp.h $(INSTALL_INC_DIR)
	cp -af rtmp_h264.h $(INSTALL_INC_DIR)
	cp -af rtmp_version.h $(INSTALL_INC_DIR)
prepare:
	[ -d "$(INSTALL_INC_DIR)" ] || mkdir -p $(INSTALL_INC_DIR)
#	cp $(STATIC_LIB) $(INSTALL_LIB_DIR)
	cp -af rtmp_error.h $(INSTALL_INC_DIR)
	cp -af rtmp_utils.h $(INSTALL_INC_DIR)
	cp -af rtmp_string.h $(INSTALL_INC_DIR)
	cp -af rtmp_client.h $(INSTALL_INC_DIR)
	cp -af rtmp_mbuffer.h $(INSTALL_INC_DIR)
	cp -af snx_rtmp.h $(INSTALL_INC_DIR)
	cp -af rtmp_h264.h $(INSTALL_INC_DIR)
	cp -af rtmp_version.h $(INSTALL_INC_DIR)


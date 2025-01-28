SHELL := /bin/bash

include config.mk

LLIBNAME = lib$(LIBNAME)
LOC_INC_DIR = include
LOC_SRC_DIR = src
LOC_BUILD_DIR = build
LOC_OBJ_DIR = $(LOC_BUILD_DIR)/obj
LOC_LIB_DIR = $(LOC_BUILD_DIR)/lib
CREATE_DIR = $(LOC_OBJ_DIR) $(LOC_LIB_DIR)

LIB_VERSION = $(VMAJ).$(VMIN)
UPSTREAM_VERSION = $(LIB_VERSION).$(VREV)
DEBIAN_REVISION = $(VDEB)
VERSION = $(UPSTREAM_VERSION)-$(DEBIAN_REVISION)

VLIBNAME = $(LLIBNAME)-$(LIB_VERSION)
SONAME = $(LLIBNAME).so.$(LIB_VERSION)
ANAME = $(LLIBNAME).a

TGT_INCLUDE_BASE = $(DESTDIR)/usr/include/smx
TGT_INCLUDE = $(TGT_INCLUDE_BASE)/$(VLIBNAME)
TGT_LIB = $(DESTDIR)/usr/lib/x86_64-linux-gnu
TGT_DOC = $(DESTDIR)/usr/share/doc/$(LLIBNAME)$(LIB_VERSION)
TGT_CONF = $(DESTDIR)/etc/smx/$(LLIBNAME)$(LIB_VERSION)

STATLIB = $(LOC_LIB_DIR)/$(LLIBNAME).a
DYNLIB = $(LOC_LIB_DIR)/$(LLIBNAME).so

SOURCES = $(wildcard $(LOC_SRC_DIR)/*.c)
OBJECTS := $(patsubst $(LOC_SRC_DIR)/%.c, $(LOC_OBJ_DIR)/%.o, $(SOURCES))

INCLUDES = $(LOC_INC_DIR)/*.h

INCLUDES_DIR = -I$(LOC_INC_DIR) \
			   -I/usr/include/libbson-1.0 \
			   $(patsubst %, -I$(TGT_INCLUDE_BASE)/lib%, $(SMX_LIBS)) \
			   -I.

LINK_DIR = -L/usr/local/lib \
	$(patsubst %, -L$(TGT_LIB)/lib%, $(SMX_LIBS)) \
	-L$(TGT_LIB) $(EXT_LIBS_DIR)

LINK_FILE = -lpthread \
	-lbson-1.0 \
	$(patsubst %, -l%, $(SMX_LIBS)) \
	-ldl

CFLAGS = -Wall -fPIC -DLIBSMXRTS_VERSION=\"$(UPSTREAM_VERSION)\"
DEBUG_FLAGS = -g -O0

CC = gcc

all: directories $(STATLIB) $(DYNLIB)

# compile with dot stuff and debug flags
debug: CFLAGS += $(DEBUG_FLAGS)
debug: all

lock: CFLAGS += -DSMX_LOG_LOCK
lock: all

no-log: CFLAGS += -DSMX_LOG_DISABLE
no-log: all

$(STATLIB): $(OBJECTS)
	ar -cq $@ $^

$(DYNLIB): $(OBJECTS)
	$(CC) -shared -Wl,-soname,$(SONAME) $^ -o $@ $(LINK_DIR) $(LINK_FILE)

# compile project
$(LOC_OBJ_DIR)/%.o: $(LOC_SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES_DIR) -c $< -o $@ $(LINK_DIR) $(LINK_FILE)

.PHONY: clean install uninstall doc directories

directories: $(CREATE_DIR)

$(CREATE_DIR):
	mkdir -p $@

install:
	mkdir -p $(TGT_LIB) $(TGT_INCLUDE) $(TGT_CONF)
	cp -a $(INCLUDES) $(TGT_INCLUDE)/.
	cp -a $(LOC_LIB_DIR)/$(LLIBNAME).so $(TGT_LIB)/$(SONAME)
	cp -ar schemas $(TGT_CONF)/.
	ln -sf $(SONAME) $(TGT_LIB)/$(VLIBNAME).so
	ln -sf $(SONAME) $(TGT_LIB)/$(LLIBNAME).so

uninstall:
	rm $(addprefix $(TGT_INCLUDE)/,$(notdir $(wildcard $(INCLUDES))))
	rm $(TGT_LIB)/$(SONAME)
	rm $(TGT_LIB)/$(VLIBNAME).so
	rm $(TGT_LIB)/$(LLIBNAME).so
	rm -r $(TGT_CONF)

clean:
	rm -rf $(LOC_LIB_DIR)
	rm -rf $(LOC_OBJ_DIR)
	rm -rf $(LOC_BUILD_DIR)

doc:
	doxygen .doxygen

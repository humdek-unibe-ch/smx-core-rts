SHELL := /bin/bash

PROJECT = smxrts
VMAJ = 0
VMIN = 3
VREV = 1

VERSION_LIB = $(VMAJ).$(VMIN)

LOC_INC_DIR = include
LOC_SRC_DIR = src
LOC_OBJ_DIR = obj
LOC_LIB_DIR = lib
CREATE_DIR = $(LOC_OBJ_DIR) $(LOC_LIB_DIR)

DPKG_DIR = dpkg
DPKG_CTL_DIR = debian
DPKG_TGT = DEBIAN
DPKGS = $(DPKG_DIR)/$(LIBNAME)_$(VERSION)_amd64 \
	   $(DPKG_DIR)/$(LIBNAME)_amd64-dev

LIBNAME = lib$(PROJECT)

LIB_VERSION = $(VMAJ).$(VMIN)
UPSTREAM_VERSION = $(LIB_VERSION).$(VREV)
DEBIAN_REVISION = 0
VERSION = $(UPSTREAM_VERSION)-$(DEBIAN_REVISION)

VLIBNAME = $(LIBNAME)-$(LIB_VERSION)
SONAME = $(VLIBNAME).so.$(VREV)
ANAME = $(LIBNAME)-$(LIB_VERSION).a

TGT_INCLUDE = /opt/smx/include
TGT_DOC = /opt/smx/doc
TGT_CONF = /opt/smx/conf
TGT_LIB = /opt/smx/lib
TGT_LIB_E = \/opt\/smx\/lib

STATLIB = $(LOC_LIB_DIR)/$(LIBNAME).a
DYNLIB = $(LOC_LIB_DIR)/$(LIBNAME).so

SOURCES = $(wildcard $(LOC_SRC_DIR)/*.c)
OBJECTS := $(patsubst $(LOC_SRC_DIR)/%.c, $(LOC_OBJ_DIR)/%.o, $(SOURCES))

INCLUDES = $(LOC_INC_DIR)/*.h

INCLUDES_DIR = -I$(LOC_INC_DIR) \
			   -I/usr/include/libbson-1.0 \
			   -I.

LINK_DIR = -L/usr/local/lib

LINK_FILE = -lpthread \
	-lbson-1.0 \
	-lzlog \
	-llttng-ust \
	-ldl

CFLAGS = -Wall -fPIC -DLIBSMXRTS_VERSION=\"$(UPSTREAM_VERSION)\"
DEBUG_FLAGS = -g -O0

CC = gcc

all: directories $(STATLIB) $(DYNLIB)

# compile with dot stuff and debug flags
debug: CFLAGS += $(DEBUG_FLAGS)
debug: all

unsafe: CFLAGS += -DSMX_LOG_UNSAFE
unsafe: all

$(STATLIB): $(OBJECTS)
	ar -cq $@ $^

$(DYNLIB): $(OBJECTS)
	$(CC) -shared -Wl,-soname,$(SONAME) $^ -o $@ $(LINK_DIR) $(LINK_FILE)

# compile project
$(LOC_OBJ_DIR)/%.o: $(LOC_SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES_DIR) -c $< -o $@ $(LINK_DIR) $(LINK_FILE)

.PHONY: clean install uninstall doc directories dpkg $(DPKGS)

directories: $(CREATE_DIR)

$(CREATE_DIR):
	mkdir -p $@

install:
	mkdir -p $(TGT_LIB) $(TGT_INCLUDE) $(TGT_CONF)
	cp -a default.zlog $(TGT_CONF)/.
	cp -a $(INCLUDES) $(TGT_INCLUDE)/.
	cp -a $(LOC_LIB_DIR)/$(LIBNAME).a $(TGT_LIB)/$(ANAME)
	cp -a $(LOC_LIB_DIR)/$(LIBNAME).so $(TGT_LIB)/$(SONAME)
	ln -sf $(SONAME) $(TGT_LIB)/$(VLIBNAME).so
	ln -sf $(VLIBNAME).so $(TGT_LIB)/$(LIBNAME).so
	ln -sf $(ANAME) $(TGT_LIB)/$(LIBNAME).a

uninstall:
	rm $(addprefix $(TGT_INCLUDE)/,$(notdir $(wildcard $(INCLUDES))))
	rm $(TGT_LIB)/$(ANAME)
	rm $(TGT_LIB)/$(SONAME)
	rm $(TGT_LIB)/$(LIBNAME).a
	rm $(TGT_LIB)/$(LIBNAME).so
	rm $(TGT_LIB)/$(VLIBNAME).so

clean:
	rm -rf $(LOC_OBJ_DIR)
	rm -rf $(LOC_LIB_DIR)

doc:
	doxygen .doxygen

dpkg: $(DPKGS)
$(DPKGS):
	mkdir -p $@/$(DPKG_TGT)
	@if [[ $@ == *-dev ]]; then \
		mkdir -p $@$(TGT_INCLUDE); \
		cp $(LOC_INC_DIR)/* $@$(TGT_INCLUDE)/.; \
		echo "cp $(LOC_INC_DIR)/* $@$(TGT_INCLUDE)/."; \
		cp $(DPKG_CTL_DIR)/control-dev $@/$(DPKG_TGT)/control; \
	else \
		mkdir -p $@$(TGT_CONF); \
		cp default.zlog $(TGT_CONF)/.; \
		mkdir -p $@$(TGT_LIB); \
		cp $(LOC_LIB_DIR)/$(LIBNAME).so $@$(TGT_LIB)/$(SONAME); \
		cp $(LOC_LIB_DIR)/$(LIBNAME).a $@$(TGT_LIB)/$(ANAME); \
		mkdir -p $@$(TGT_DOC); \
		cp README.md $@$(TGT_DOC)/$(PROJECT)-$(LIB_VERSION).md; \
		cp $(DPKG_CTL_DIR)/control $@/$(DPKG_TGT)/control; \
		cp $(DPKG_CTL_DIR)/postinst $@/$(DPKG_TGT)/postinst; \
		sed -i 's/<tgt_dir>/$(TGT_LIB_E)/g' $@/$(DPKG_TGT)/postinst; \
		sed -i 's/<soname>/$(SONAME)/g' $@/$(DPKG_TGT)/postinst; \
		sed -i 's/<aname>/$(ANAME)/g' $@/$(DPKG_TGT)/postinst; \
		sed -i 's/<libname>/$(LIBNAME)/g' $@/$(DPKG_TGT)/postinst; \
		sed -i 's/<lnname>/$(VLIBNAME)/g' $@/$(DPKG_TGT)/postinst; \
		cp $(DPKG_CTL_DIR)/postrm $@/$(DPKG_TGT)/postrm; \
		sed -i 's/<tgt_dir>/$(TGT_LIB_E)/g' $@/$(DPKG_TGT)/postrm; \
		sed -i 's/<aname>/$(ANAME)/g' $@/$(DPKG_TGT)/postrm; \
		sed -i 's/<libname>/$(LIBNAME)/g' $@/$(DPKG_TGT)/postrm; \
		sed -i 's/<lnname>/$(VLIBNAME)/g' $@/$(DPKG_TGT)/postrm; \
		cp $(DPKG_CTL_DIR)/triggers $@/$(DPKG_TGT)/triggers; \
	fi
	sed -i 's/<version>/$(VERSION)/g' $@/$(DPKG_TGT)/control
	sed -i 's/<maj_version>/$(LIB_VERSION)/g' $@/$(DPKG_TGT)/control
	dpkg-deb -b $@

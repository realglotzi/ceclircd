##############################################################################
#
# Makefile for bootmenu on RaspberryPi
#
# (c) 2013 Dirk E. Wagner
#
# Based on the M740AV cross toolchain builder by Christian Schnell
#
##############################################################################
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU Library General Public License as published
# by the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
# USA.
#
##############################################################################

DEVENV_DIR = /usr/local/raspi-tc/arm-bcm2708hardfp-linux-gnueabi

TARGET       = arm-bcm2708hardfp-linux-gnueabi
PREFIX       = $(DEVENV_DIR)/arm-bcm2708hardfp-linux-gnueabi/sysroot

AR=$(DEVENV_DIR)/bin/$(TARGET)-ar
AS=$(DEVENV_DIR)/bin/$(TARGET)-asi
CC=$(DEVENV_DIR)/bin/$(TARGET)-gcc 
CPP=$(DEVENV_DIR)/bin/$(TARGET)-cpp
CXX=$(DEVENV_DIR)/bin/$(TARGET)-g++ 
LD=$(DEVENV_DIR)/bin/$(TARGET)-ld 
NM=$(DEVENV_DIR)/bin/$(TARGET)-nm 
OBJCOPY=$(DEVENV_DIR)/bin/$(TARGET)-objcopy
OBJDUMP=$(DEVENV_DIR)/bin/$(TARGET)-objdump
RANLIB=$(DEVENV_DIR)/bin/$(TARGET)-ranlib
SIZE=$(DEVENV_DIR)/bin/$(TARGET)-size
STRINGS=$(DEVENV_DIR)/bin/$(TARGET)-strings
STRIP=$(DEVENV_DIR)/bin/$(TARGET)-strip

CFLAGS="-mcpu=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard"

VERSION="0.1"
RM=rm -f
DIST=../dist
DISTSRC=../distsrc
ODIR=../OBJS
EXE=ceclircd
CXXFLAGS=-std=c++0x -D VERSION=\"$(VERSION)\" -g -Wall -Woverloaded-virtual -I $(PREFIX)/include -I .
LFLAGS= -g -L$(PREFIX)/opt/vc/lib

LIBS = -llog4cplus -lcec -ldl -lbcm_host -lvcos -lvchiq_arm

OBJS = main.o libcec.o lirc.o hdmi.o
	
all: $(EXE)

$(EXE): $(OBJS) 
	$(CXX) $(LFLAGS) -o $(EXE) $(OBJS) $(LIBS) 

cpp.o:
	$(CXX) $(CXXFLAGS) -c $<

h.o:
	$(CXX) $(CXXFLAGS) -c $<

tar: install 
	( cd $(DIST) && tar cvfz ../$(EXE)-$(VERSION).tar.gz . ; cd - )
	( cd $(DISTSRC) && tar cvfz ../$(EXE)-$(VERSION)-src.tar.gz . ; cd - )

clean:
	$(RM) -r $(DIST) $(DISTSRC)
	$(RM) *.d *.o $(EXE) ../$(EXE)-$(VERSION).tar.gz ../$(EXE)-$(VERSION)-src.tar.gz

install: all
	$(STRIP) $(EXE)
	mkdir -p $(DIST)/usr/bin
	mkdir -p $(DIST)/etc
	mkdir -p $(DIST)/usr/lib
	mkdir -p $(DISTSRC)/usr/src/ceclircd/src
	mkdir -p $(DISTSRC)/usr/src/ceclircd/libs
	mkdir -p $(DISTSRC)/usr/src/ceclircd/libs/patches
	cp $(EXE) $(DIST)/usr/bin
	cp $(PREFIX)/lib/libptypes* $(DIST)/usr/lib
#	cp *.cpp *.h Makefile $(DISTSRC)/usr/src/ceclircd/src
#	cp ../libs/Makefile $(DISTSRC)/usr/src/ceclircd/libs
#	cp ../libs/patches/Makefile.raspi ../libs/patches/ptypes_nodebug.patch $(DISTSRC)/usr/src/ceclircd/libs/patches 


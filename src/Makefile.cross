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
CXXFLAGS=-std=c++11 -D VERSION=\"$(VERSION)\" -g -Wall -Woverloaded-virtual -I $(PREFIX)/include -I .
LFLAGS=	-g -L$(PREFIX)/opt/vc/lib

LIBS = -lpthread -llog4cplus -lcec -ldl -lbcm_host -lvcos -lvchiq_arm

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
	mkdir -p $(DIST)/usr/local/bin
	mkdir -p $(DIST)/etc
	mkdir -p $(DIST)/usr/lib
	mkdir -p $(DISTSRC)/usr/src/ceclircd/src
	mkdir -p $(DISTSRC)/usr/src/ceclircd/libs
	cp $(EXE) $(DIST)/usr/local/bin
	cp *.cpp *.h Makefile $(DISTSRC)/usr/src/ceclircd/src
	cp ../libs/Makefile $(DISTSRC)/usr/src/ceclircd/libs


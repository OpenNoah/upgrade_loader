SRC	:= main.cpp mainw.cpp cgucontrol.cpp regs.cpp bootdialog.cpp boot.cpp options.cpp \
	   backup.cpp backup_nanddump.cpp backup_eraseall.cpp backup_nandwrite.cpp
HDR	:= mainw.h cgucontrol.h regs.h bootdialog.h boot.h options.h backup.h
TRG	:= syssw

MOC_SRC	:= $(HDR:%.h=moc_%.cpp)
SRC	+= $(MOC_SRC)

TOP	?= $(CURDIR)/../../build/qt-2.3.10
INC	+= -I$(TOP)/include -I$(TOP)/src/3rdparty/freetype/include
LIB	+= -L$(TOP)/lib -Wl,-rpath,/opt/qt-2.3.10/lib -lqte
DEF	+= -DQWS -D_REENTRANT
CFLAGS	= -Os -pipe -D_FILE_OFFSET_BITS=64
FLAGS	?= -Os -pipe -fno-rtti -D_FILE_OFFSET_BITS=64
CROSS	?= mipsel-linux-
MOC	?= ../../bin/moc

.PHONY: all
all: $(TRG)

.PHONY: send
send: $(TRG)
	-cat $(TRG) | ncat np1380 1234 --send-only

$(TRG): $(SRC:%.cpp=%.o)
	$(CROSS)g++ -s -o $@ $^ $(LIB) $(FLAGS)

nanddump nandwrite flash_eraseall: %: %.c crc32.c
	$(CROSS)gcc -s -o $@ $^ $(CFLAGS)

%.o: %.cpp
	$(CROSS)g++ -o $@ -c $(INC) $^ $(DEF) $(FLAGS)

moc_%.cpp: %.h
	$(MOC) -o $@ $<

.PHONY: clean
clean:
	rm -f $(TRG) $(SRC:%.cpp=%.o) $(MOC_SRC)

.PHONY: gdb
gdb: $(TRG)
	mipsel-linux-gdb $(TRG) \
		-ex 'target extended-remote NP1380:1234' \
		-ex 'set sysroot $(TOP)/rootfs'

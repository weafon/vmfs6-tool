LDFLAGS := $(DLOPEN_LDFLAGS)
debugvmfs.o_CFLAGS := -DVERSION=\"$(VERSION)\"
REQUIRES := libvmfs libreadcmd

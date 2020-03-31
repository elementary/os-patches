
bwrap_SOURCES = \
	bubblewrap/bubblewrap.c \
	bubblewrap/bind-mount.h \
	bubblewrap/bind-mount.c \
	bubblewrap/network.h \
	bubblewrap/network.c \
	bubblewrap/utils.h \
	bubblewrap/utils.c \
	$(NULL)

bwrap_CFLAGS = $(AM_CFLAGS)
bwrap_LDADD = $(SELINUX_LIBS)

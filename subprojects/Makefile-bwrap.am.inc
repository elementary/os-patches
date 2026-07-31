bwrap_SOURCES = \
	subprojects/bubblewrap/bubblewrap.c \
	subprojects/bubblewrap/bind-mount.h \
	subprojects/bubblewrap/bind-mount.c \
	subprojects/bubblewrap/network.h \
	subprojects/bubblewrap/network.c \
	subprojects/bubblewrap/utils.h \
	subprojects/bubblewrap/utils.c \
	$(NULL)

bwrap_CFLAGS = $(AM_CFLAGS)
bwrap_LDADD = $(SELINUX_LIBS)

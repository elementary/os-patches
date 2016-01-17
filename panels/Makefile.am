SUBDIRS= \
	appearance \
	common \
	datetime \
	screen \
	power \
	color \
	display \
	mouse \
	region \
	info \
	sound \
	keyboard \
	universal-access \
	user-accounts \
	bluetooth

if BUILD_WACOM
SUBDIRS += wacom
endif

if BUILD_PRINTERS
SUBDIRS += printers
endif

if BUILD_NETWORK
SUBDIRS += network
endif

-include $(top_srcdir)/git.mk

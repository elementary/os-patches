themedir = $(datadir)/plymouth/themes/ubuntu-logo
nodist_theme_DATA = ubuntu-logo.plymouth
dist_theme_DATA =   ubuntu-logo.script                                \
                    ubuntu-logo.grub                                  \
                    ubuntu-logo.png                                   \
                    ubuntu-logo16.png                                 \
                    noise-texture.png                                 \
                    password-field.png                                \
                    password-field16.png                              \
                    progress-dot-off.png                              \
                    progress-dot-off16.png                            \
                    progress-dot-on.png                               \
                    progress-dot-on16.png



MAINTAINERCLEANFILES = Makefile.in ubuntu-logo.plymouth
CLEANFILES = ubuntu-logo.plymouth

ubuntu-logo.plymouth: $(srcdir)/ubuntu-logo.plymouth.in
	sed -e 's,[@]PLYMOUTH_THEME_PATH[@],$(PLYMOUTH_THEME_PATH),g' \
	       $(srcdir)/ubuntu-logo.plymouth.in > ubuntu-logo.plymouth

EXTRA_DIST = ubuntu-logo.plymouth.in

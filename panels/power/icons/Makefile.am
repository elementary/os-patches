
SUBDIRS = 24x24 48x48

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

if MAINTAINER_MODE

install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

clean-local :
	rm -f *~

endif

-include $(top_srcdir)/git.mk

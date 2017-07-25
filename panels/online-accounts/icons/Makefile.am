
SUBDIRS = 16x16 22x22 24x24 32x32 48x48 256x256

EXTRA_DIST = meson.build

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

endif

clean-local :
	rm -f *~


-include $(top_srcdir)/git.mk

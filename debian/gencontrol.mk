# Generate control files.

ifeq ("$(GTK2_MA)","1")
        GTK2_ACT=sed -i 's/@GTK2_MA@/Multi-Arch: same/g' debian/control
else
        GTK2_ACT=sed -i '/@GTK2_MA@/d' debian/control
endif

ifeq ("$(GTK3_MA)","1")
        GTK3_ACT=sed -i 's/@GTK3_MA@/Multi-Arch: same/g' debian/control
else
        GTK3_ACT=sed -i '/@GTK3_MA@/d' debian/control
endif

ifeq ("$(QT4_MA)","1")
        QT4_ACT=sed -i 's/@QT4_MA@/Multi-Arch: same/g' debian/control
else
        QT4_ACT=sed -i '/@QT4_MA@/d' debian/control
endif

ifeq ("$(GIR_MA)","1")
        GIR_ACT=sed -i 's/@GIR_MA@/Multi-Arch: same/g' debian/control
else
        GIR_ACT=sed -i '/@GIR_MA@/d' debian/control
endif

gen_control:
	cp debian/control.in debian/control
	$(GTK2_ACT)
	$(GTK3_ACT)
	$(QT4_ACT)
	$(GIR_ACT)
	sed 's/@GTK2_LIBDIR@/$(GTK2_LIBDIR)/g' \
	    debian/fcitx-frontend-gtk2.install.in > \
	    debian/fcitx-frontend-gtk2.install
	sed 's/@GTK3_LIBDIR@/$(GTK3_LIBDIR)/g' \
	    debian/fcitx-frontend-gtk3.install.in > \
	    debian/fcitx-frontend-gtk3.install
	sed 's/@QT4_LIBDIR@/$(QT4_LIBDIR)/g' \
	    debian/fcitx-frontend-qt4.install.in > \
	    debian/fcitx-frontend-qt4.install
	sed 's/@GIR_LIBDIR@/$(GIR_LIBDIR)/g' \
	    debian/gir1.2-fcitx-1.0.install.in > \
	    debian/gir1.2-fcitx-1.0.install

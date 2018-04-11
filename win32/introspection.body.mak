GtkSource-3.0.gir: GtkSource_3_0_gir_list 
	@-echo Generating $@...
	$(PYTHON) $(G_IR_SCANNER)	\
	--verbose -no-libtool	\
	--namespace=GtkSource	\
	--nsversion=3.0	\
	--pkg=gtk+-3.0 --pkg=gdk-3.0	\
	--library=gtksourceview-3.0	\
		\
	--add-include-path=$(G_IR_INCLUDEDIR)	\
	--include=Gtk-3.0 --include=Gdk-3.0	\
	--pkg-export=gtksourceview-3.0	\
	--cflags-begin	\
	-I.. -DGTK_SOURCE_COMPILATION	\
	--cflags-end	\
	--c-include=gtksourceview/gtksource.h --warn-all --filelist=GtkSource_3_0_gir_list	\
	--filelist=GtkSource_3_0_gir_list	\
	-o $@

GtkSource-3.0.typelib: GtkSource-3.0.gir
	@-echo Compiling $@...
	$(G_IR_COMPILER)	\
	--includedir=. --debug --verbose	\
	GtkSource-3.0.gir	\
	-o $@


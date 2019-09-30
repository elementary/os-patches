# Rules to more easily specify a C# build for automake.
#
# Inspired and adapted from Banshee and GNOME Do's build systems

SOURCES_BUILD = $(addprefix $(srcdir)/, $(FILES))

RESOURCES_EXPANDED = $(addprefix $(srcdir)/, $(RESOURCES))
RESOURCES_BUILD = $(foreach resource, $(RESOURCES_EXPANDED), \
        -resource:$(resource),$(notdir $(resource)))

BUILD_DIR = $(top_builddir)/build

ASSEMBLY_EXTENSION = $(strip $(patsubst library, dll, $(TARGET)))
ASSEMBLY_FILE = $(BUILD_DIR)/$(ASSEMBLY).$(ASSEMBLY_EXTENSION)

STD_REFERENCES = $(foreach ref,$(filter-out -r:%,$(REFERENCES)),-r:$(ref))
BUILD_REFERENCES = $(filter -r:%,$(REFERENCES) $(STD_REFERENCES))
COMPONENT_REFERENCES = $(foreach ref, $(PROJECT_REFERENCES),-r:$(BUILD_DIR)/$(ref).dll)
COMPONENT_DEPS = $(foreach ref,$(PROJECT_REFERENCES),$(BUILD_DIR)/$(ref).dll)

OUTPUT_FILES = \
        $(ASSEMBLY_FILE)

moduledir = $(pkglibdir)
# Install libraries as data; there's no need for them to be excutable
module_DATA = $(foreach file,$(filter %.dll,$(OUTPUT_FILES)),$(file) $(file).mdb) $(foreach file,$(filter %.exe,$(OUTPUT_FILES)),$(file).mdb)
# Install executables as scripts
module_SCRIPTS = $(filter %.exe,$(OUTPUT_FILES))

MCS_FLAGS =  $(MCS_LINQ_FLAG) -noconfig -codepage:utf8 -warn:4 -debug

all: $(ASSEMBLY_FILE)

$(ASSEMBLY_FILE).mdb: $(ASSEMBLY_FILE)

build-debug :
	@echo $(COMPONENT_REFERENCES)

$(ASSEMBLY_FILE): $(SOURCES_BUILD) $(RESOURCES_EXPANDED) $(COMPONENT_DEPS)
	@mkdir -p $(BUILD_DIR)
	@colors=no; \
	case $$TERM in \
                "xterm" | "rxvt" | "rxvt-unicode") \
                        test "x$$COLORTERM" != "x" && colors=yes ;; \
                "xterm-color") colors=yes ;; \
	esac; \
	if [ "x$$colors" = "xyes" ]; then \
                tty -s && true || { colors=no; true; } \
	fi; \
	test "x$$colors" = "xyes" && \
	        echo -e "\033[1mCompiling $(notdir $@)...\033[0m" || \
	        echo "Compiling $(notdir $@)...";
	@$(MCS) $(MCS_FLAGS) -target:$(TARGET) -out:$@ $(BUILD_DEFINES) $(BUILD_REFERENCES) $(COMPONENT_REFERENCES) $(RESOURCES_BUILD) $(SOURCES_BUILD)
	@if [ -e $(srcdir)/$(notdir $@.config) ]; then \
	        cp $(srcdir)/$(notdir $@.config) $(BUILD_DIR) ; \
	fi;

#
# pkg-config handling
#
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(PKG_CONFIG_FILES)

%.pc : %.pc.in
	@colors=no; \
	case $$TERM in \
                "xterm" | "rxvt" | "rxvt-unicode") \
                        test "x$$COLORTERM" != "x" && colors=yes ;; \
                "xterm-color") colors=yes ;; \
	esac; \
	if [ "x$$colors" = "xyes" ]; then \
                tty -s && true || { colors=no; true; } \
	fi; \
	test "x$$colors" = "xyes" && \
	        echo -e "\033[1mGenerating $(notdir $@)...\033[0m" || \
	        echo "Generating $(notdir $@)...";
	@sed "s,\@ABI_VERSION\@,$(ABI_VERSION),g; s,\@expanded_libdir\@,$(expanded_libdir),g; s,\@PACKAGE\@,$(PACKAGE),g" < $< > $@

#
# Clean and dist targets
#
EXTRA_DIST = $(SOURCES_BUILD) $(RESOURCES_EXPANDED) $(THEME_ICONS_SOURCE) \
	$(foreach pkgcfg_file, $(PKG_CONFIG_FILES), $(pkgcfg_file).in)

CLEANFILES = $(OUTPUT_FILES) $(pkgconfig_DATA)
DISTCLEANFILES = *.pidb
MAINTAINERCLEANFILES = Makefile.in

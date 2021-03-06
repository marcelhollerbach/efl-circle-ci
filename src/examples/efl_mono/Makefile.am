MAINTAINERCLEANFILES = Makefile.in

if HAVE_CSHARP

include $(top_srcdir)/src/Makefile_Eolian_Helper.am
include $(top_srcdir)/src/Makefile_Eolian_Mono_Helper.am
include $(top_srcdir)/src/Makefile_Efl_Mono_MSBuild_Gen_Helper.am

SRCS =
EXTRA_FILES =
EXTRA_PROGRAMS =
GENERATED =
CLEANFILES =

# Simple object used by the examples

EOLIAN_FLAGS := $(EOLIAN_FLAGS)
EOLIAN_FLAGS += \
-I$(srcdir) \
-I$(top_srcdir)/src/lib/eo \
-I$(top_srcdir)/src/lib/efl

COMMON_FLAGS = \
@EINA_CFLAGS@ @EINA_LDFLAGS@ \
-L$(top_builddir)/src/lib/eina/.libs -leina \
@EO_CFLAGS@ @EO_LDFLAGS@ \
-L$(top_builddir)/src/lib/eo/.libs -leo \
@EFL_CFLAGS@ @EFL_LDFLAGS@ \
-L$(top_builddir)/src/lib/efl/.libs -lefl

if HAVE_WIN32
COMMON_FLAGS += \
@EVIL_CFLAGS@ @EVIL_LDFLAGS@ \
@EFL_WINDOWS_VERSION_CFLAGS@ \
-L$(top_builddir)/src/lib/evil/.libs -levil
else
COMMON_FLAGS += \
-fPIC -DPIC
endif


%.eo.c: %.eo
	$(AM_V_EOL)$(EOLIAN_GEN) $(EOLIAN_FLAGS) -gc -o c:$@ $<

%.eo.h: %.eo
	$(AM_V_EOL)$(EOLIAN_GEN) $(EOLIAN_FLAGS) -gh -o h:$@ $<

example_numberwrapper.c: example_numberwrapper.eo.h example_numberwrapper.eo.c

noinst_lib_LTLIBRARIES = libexample_numberwrapper.la
noinst_libdir = $(abs_top_builddir)

libexample_numberwrapper_la_SOURCES = example_numberwrapper.c
libexample_numberwrapper_la_LDFLAGS = -shared
libexample_numberwrapper_la_CFLAGS = $(COMMON_FLAGS) -DEFL_BETA_API_SUPPORT

SRCS += example_numberwrapper.c
EXTRA_FILES += example_numberwrapper.eo
GENERATED += example_numberwrapper.eo.h example_numberwrapper.eo.c EoInherit01.exe.config

# CSharp examples

SRCS += \
EoInherit01.cs \
FunctionPointer01.cs

EXTRA_PROGRAMS += \
EoInherit01.exe \
FunctionPointer01.exe

example_numberwrapper.eo.cs: example_numberwrapper.eo
	$(AM_V_EOLMONO) \
	$(EOLIAN_MONO) $(EOLIAN_FLAGS) $(EOLIAN_MONO_FLAGS) --dllimport example_numberwrapper -o $@ -r $(top_builddir)/src/bindings/mono/efl_mono/efl_libs.csv $<

EoInherit01_exe_SOURCES = EoInherit01.cs
EoInherit01_srcs = $(EoInherit01_exe_SOURCES) example_numberwrapper.eo.cs
EoInherit01.exe: $(EoInherit01_srcs) $(am_dirstamp) $(top_builddir)/src/lib/efl_mono/libefl_mono.dll libexample_numberwrapper.la EoInherit01.exe.config
	@rm -f $@
	$(AM_V_MCS) $(MCS) $(MCS_FLAGS) -r:$(abs_top_builddir)/src/lib/efl_mono/libefl_mono.dll -out:$@ $(filter %.cs, $(^))

$(abs_top_builddir)/EoInherit01.csproj: $(EoInherit01_srcs) $(am_dirstamp) $(numberwrapper_lib_name)
	@rm -f $@
	$(EFL_MONO_MSBUILD_GEN) $(MSBUILD_GEN_FLAGS) -o $@ -a EoInherit01.exe -r libefl_mono.dll -t exe $(patsubst %.cs,src/examples/efl_mono/%.cs,$(filter %.cs, $(^)))
EoInherit01.exe.config:
	echo "<configuration>" > $@
	echo "  <dllmap dll=\"example_numberwrapper\" target=\"$(abs_top_builddir)/src/examples/efl_mono/.libs/libexample_numberwrapper.so\"/>" >> $@
	echo "  <dllmap dll=\"efl_mono\" target=\"$(abs_top_builddir)/src/lib/efl_mono/libefl_mono.dll\"/>" >> $@
	echo "  <runtime>" >> $@
	echo "    <assemblyBinding xmlns=\"urn:schemas-microsoft-com:asm.v1\">" >> $@
	echo "      <probing privatePath=\"/home/felipe/dev/samsung/upstream/efl/build/src/lib/efl_mono\" />" >> $@
	echo "    </assemblyBinding>" >> $@
	echo "  </runtime>" >> $@
	echo "</configuration>" >> $@

FunctionPointer01_exe_SOURCES = FunctionPointer01.cs
FunctionPointer01_srcs = $(FunctionPointer01_exe_SOURCES) example_numberwrapper.eo.cs
FunctionPointer01.exe: $(FunctionPointer01_srcs) $(am_dirstamp) $(top_builddir)/src/lib/efl_mono/libefl_mono.dll $(numberwrapper_lib_name)
	@rm -f $@
	$(AM_V_MCS) $(MCS) $(MCS_FLAGS) -r:$(abs_top_builddir)/src/lib/efl_mono/libefl_mono.dll -out:$@ $(filter %.cs, $(^))

$(abs_top_builddir)/FunctionPointer01.csproj: $(FunctionPointer01_srcs) $(am_dirstamp) $(numberwrapper_lib_name)
	@rm -f $@
	$(EFL_MONO_MSBUILD_GEN) $(MSBUILD_GEN_FLAGS) -o $@ -a FunctionPointer01.exe -r libefl_mono.dll -t exe $(patsubst %.cs,src/examples/efl_mono/%.cs,$(filter %.cs, $(^)))

msbuildcsprojs: $(abs_top_builddir)/EoInherit01.csproj $(abs_top_builddir)/FunctionPointer01.csproj

GENERATED += example_numberwrapper.eo.cs


# Finishing

EXTRA_DIST = $(EXTRA_FILES)

CLEANFILES += $(GENERATED)

examples: $(EXTRA_PROGRAMS)

clean-local:
	rm -f $(EXTRA_PROGRAMS) $(GENERATED)

install-examples:
	$(MKDIR_P) $(DESTDIR)$(datadir)/efl_mono/examples
	cd $(srcdir) && $(install_sh_DATA) -c $(SRCS) $(EXTRA_FILES) $(DESTDIR)$(datadir)/efl_mono/examples

uninstall-local:
	for f in $(SRCS) $(EXTRA_FILES); do \
	  rm -f $(DESTDIR)$(datadir)/efl_mono/examples/$$f ; \
	done

if ALWAYS_BUILD_EXAMPLES
noinst_PROGRAMS = $(EXTRA_PROGRAMS)
endif

endif

bin_PROGRAMS = linuxsampler
linuxsampler_SOURCES = linuxsampler.cpp audioio.cpp audioio.h audiothread.cpp audiothread.h diskthread.cpp diskthread.h global.cpp global.h midiin.cpp midiin.h ringbuffer.h stream.cpp stream.h thread.cpp thread.h voice.cpp voice.h rtelmemorypool.h atomic.h modulationsystem.cpp eg_vca.cpp alsaio.cpp alsaio.h jackio.cpp jackio.h lfo.cpp eg_d.cpp rtmath.cpp

# set the include path found by configure
INCLUDES= $(all_includes)

# to prevent compile errors on some systems
AM_CXXFLAGS = -pedantic

# the library search path.
linuxsampler_LDFLAGS = $(all_libraries) $(JACK_LIBS)
lib_LIBRARIES =  libgig.a
libgig_a_SOURCES =  DLS.cpp DLS.h gig.cpp gig.h RIFF.cpp RIFF.h
linuxsampler_LDADD = $(top_builddir)/src/network/liblscpserver.a $(top_builddir)/src/libgig.a -lpthread -lasound
noinst_HEADERS = modulationsystem.h eg_vca.h filter.h lfo.h eg_d.h rtmath.h
SUBDIRS =  network

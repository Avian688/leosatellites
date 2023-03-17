#
# OMNeT++/OMNEST Makefile for $(LIB_PREFIX)leosatellites
#
# This file was generated with the command:
#  opp_makemake --make-so -f --deep -KINET4_4_PROJ=../../inet4.4 -KOS3_PROJ=../../os3 -DINET_IMPORT -I/opt/local/include/igraph -I$$\(OS3_PROJ\)/src -I$$\(INET4_4_PROJ\)/src -I/opt/homebrew/Cellar/curl/7.83.1/include/curl -L/opt/local/lib -L$$\(INET4_4_PROJ\)/src -L$$\(OS3_PROJ\)/src -ligraph -lxml2 -lz -lgmp -lblas -lglpk -llapack -larpack -lINET$$\(D\) -los3$$\(D\)
#

# Name of target to be created (-o option)
TARGET_DIR = .
TARGET_NAME = $(LIB_PREFIX)leosatellites$(D)
TARGET = $(TARGET_NAME)$(SHARED_LIB_SUFFIX)
TARGET_IMPLIB = $(TARGET_NAME)$(IMPLIB_SUFFIX)
TARGET_IMPDEF = $(TARGET_NAME)$(IMPDEF_SUFFIX)
TARGET_FILES = $(TARGET_DIR)/$(TARGET)

# C++ include paths (with -I)
INCLUDE_PATH = -I/opt/local/include/igraph -I$(OS3_PROJ)/src -I$(INET4_4_PROJ)/src -I/opt/homebrew/Cellar/curl/7.83.1/include/curl

# Additional object and library files to link with
EXTRA_OBJS =

# Additional libraries (-L, -l options)
LIBS = $(LDFLAG_LIBPATH)/opt/local/lib $(LDFLAG_LIBPATH)$(INET4_4_PROJ)/src $(LDFLAG_LIBPATH)$(OS3_PROJ)/src  -ligraph -lxml2 -lz -lgmp -lblas -lglpk -llapack -larpack -lINET$(D) -los3$(D)

# Output directory
PROJECT_OUTPUT_DIR = ../out
PROJECTRELATIVE_PATH = src
O = $(PROJECT_OUTPUT_DIR)/$(CONFIGNAME)/$(PROJECTRELATIVE_PATH)

# Object files for local .cc, .msg and .sm files
OBJS = \
    $O/common/LeoChannelConstructor.o \
    $O/common/LeoFlowScheduler.o \
    $O/libnorad/cEcef.o \
    $O/libnorad/cNoradBaseA.o \
    $O/libnorad/cNoradSGP4A.o \
    $O/libnorad/cOrbitA.o \
    $O/mobility/GroundStationMobility.o \
    $O/mobility/INorad.o \
    $O/mobility/NoradA.o \
    $O/mobility/NoradTLE.o \
    $O/mobility/SatelliteMobility.o \
    $O/networklayer/configurator/ipv4/LeoIpv4NetworkConfigurator.o \
    $O/networklayer/configurator/ipv4/LeoIpv4NodeConfigurator.o \
    $O/networklayer/configurator/ipv4/LeoNetworkConfigurator.o \
    $O/networklayer/configurator/ipv4/MatcherOS3.o \
    $O/networklayer/configurator/ipv4/SatelliteNetworkConfigurator.o \
    $O/networklayer/configurator/ipv4/SatelliteNodeConfigurator.o \
    $O/networklayer/ipv4/LeoIpv4.o \
    $O/networklayer/ipv4/LeoIpv4NetworkLayer.o \
    $O/networklayer/ipv4/LeoIpv4RoutingTable.o \
    $O/physicallayer/apskradio/SatelliteApskScalarTransmission.o \
    $O/physicallayer/apskradio/SatelliteApskScalarTransmitter.o \
    $O/physicallayer/propagation/SatellitePropagation.o \
    $O/physicallayer/unitdisk/SatelliteUnitDiskTransmission.o \
    $O/physicallayer/unitdisk/SatelliteUnitDiskTransmitter.o

# Message files
MSGFILES =

# SM files
SMFILES =

# Other makefile variables (-K)
INET4_4_PROJ=../../inet4.4
OS3_PROJ=../../os3

#------------------------------------------------------------------------------

# Pull in OMNeT++ configuration (Makefile.inc)

ifneq ("$(OMNETPP_CONFIGFILE)","")
CONFIGFILE = $(OMNETPP_CONFIGFILE)
else
CONFIGFILE = $(shell opp_configfilepath)
endif

ifeq ("$(wildcard $(CONFIGFILE))","")
$(error Config file '$(CONFIGFILE)' does not exist -- add the OMNeT++ bin directory to the path so that opp_configfilepath can be found, or set the OMNETPP_CONFIGFILE variable to point to Makefile.inc)
endif

include $(CONFIGFILE)

# Simulation kernel and user interface libraries
OMNETPP_LIBS = -loppenvir$D $(KERNEL_LIBS) $(SYS_LIBS)
ifneq ($(PLATFORM),win32)
LIBS += -Wl,-rpath,$(abspath /opt/local/lib) -Wl,-rpath,$(abspath $(INET4_4_PROJ)/src) -Wl,-rpath,$(abspath $(OS3_PROJ)/src)
endif

COPTS = $(CFLAGS) $(IMPORT_DEFINES) -DINET_IMPORT $(INCLUDE_PATH) -I$(OMNETPP_INCL_DIR)
MSGCOPTS = $(INCLUDE_PATH)
SMCOPTS =

# we want to recompile everything if COPTS changes,
# so we store COPTS into $COPTS_FILE (if COPTS has changed since last build)
# and make the object files depend on it
COPTS_FILE = $O/.last-copts
ifneq ("$(COPTS)","$(shell cat $(COPTS_FILE) 2>/dev/null || echo '')")
  $(shell $(MKPATH) "$O")
  $(file >$(COPTS_FILE),$(COPTS))
endif

# On Windows, the target has additional file(s). An import lib and an optional debug symbol file is created too.
ifeq ($(PLATFORM),win32)
  TARGET_FILES+= $(TARGET_DIR)/$(TARGET_IMPLIB)
  LDFLAGS+= $(LDFLAG_IMPLIB)$O/$(TARGET_IMPLIB)
  ifeq ($(TOOLCHAIN_NAME),clang-msabi)
    ifeq ($(MODE),debug)
      TARGET_FILES+=$(TARGET_DIR)/$(TARGET_NAME).pdb
    endif
  else
    TARGET_FILES+= $(TARGET_DIR)/$(TARGET_IMPDEF)
    LDFLAGS+= $(LDFLAG_IMPDEF)$O/$(TARGET_IMPDEF)
  endif
endif

#------------------------------------------------------------------------------
# User-supplied makefile fragment(s)
-include makefrag

#------------------------------------------------------------------------------

# Main target
all: $(TARGET_FILES)

$(TARGET_DIR)/% :: $O/%
	@mkdir -p $(TARGET_DIR)
	$(Q)$(LN) $< $@
ifeq ($(TOOLCHAIN_NAME),clang-msabi)
	-$(Q)-$(LN) $(<:%.dll=%.lib) $(@:%.dll=%.lib) 2>/dev/null

$O/$(TARGET_NAME).pdb: $O/$(TARGET)
endif

$O/$(TARGET) $O/$(TARGET_IMPDEF) $O/$(TARGET_IMPLIB) &: $(OBJS)  $(wildcard $(EXTRA_OBJS)) Makefile $(CONFIGFILE)
	@$(MKPATH) $O
	@echo Creating shared library: $@
	$(Q)$(SHLIB_LD) -o $O/$(TARGET) $(OBJS) $(EXTRA_OBJS) $(AS_NEEDED_OFF) $(WHOLE_ARCHIVE_ON) $(LIBS) $(WHOLE_ARCHIVE_OFF) $(OMNETPP_LIBS) $(LDFLAGS)
	$(Q)$(SHLIB_POSTPROCESS) $O/$(TARGET)
ifeq ($(PLATFORM),win32)
	$(Q)llvm-ar d $O/$(TARGET_IMPLIB) $(TARGET) # WORKAROUND: throw away the first file from the archive to make the LLD generated import lib valid
endif

.PHONY: all clean cleanall depend msgheaders smheaders

.SUFFIXES: .cc

$O/%.o: %.cc $(COPTS_FILE) | msgheaders smheaders
	@$(MKPATH) $(dir $@)
	$(qecho) "$<"
	$(Q)$(CXX) -c $(CXXFLAGS) $(COPTS) -o $@ $<

%_m.cc %_m.h: %.msg
	$(qecho) MSGC: $<
	$(Q)$(MSGC) -s _m.cc -MD -MP -MF $O/$(basename $<)_m.h.d $(MSGCOPTS) $?

%_sm.cc %_sm.h: %.sm
	$(qecho) SMC: $<
	$(Q)$(SMC) -c++ -suffix cc $(SMCOPTS) $?

msgheaders: $(MSGFILES:.msg=_m.h)

smheaders: $(SMFILES:.sm=_sm.h)

clean:
	$(qecho) Cleaning $(TARGET)
	$(Q)-rm -rf $O
	$(Q)-rm -f $(TARGET_FILES)
	$(Q)-rm -f $(call opp_rwildcard, . , *_m.cc *_m.h *_sm.cc *_sm.h)

cleanall:
	$(Q)$(CLEANALL_COMMAND)
	$(Q)-rm -rf $(PROJECT_OUTPUT_DIR)

help:
	@echo "$$HELP_SYNOPSYS"
	@echo "$$HELP_TARGETS"
	@echo "$$HELP_VARIABLES"
	@echo "$$HELP_EXAMPLES"

# include all dependencies
-include $(OBJS:%=%.d) $(MSGFILES:%.msg=$O/%_m.h.d)

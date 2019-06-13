#===============================================================================
#
#     Filename: Makefile
#  Description:
#
#        Usage: make              (generate executable                      )
#               make clean        (remove objects, executable, prerequisits )
#               make tarball      (generate compressed archive              )
#               make zip          (generate compressed archive              )
#
#      Version: 1.0
#      Created:
#     Revision: ---
#
#       Author:	dnsnat
#      Company: dnsnat.com
#        Email: 393731869@qq.com
#
#        Notes: This is a GNU make (gmake) makefile.
#               C   extension   :  c
#               C++ extensions  :  cc cpp C
#               C and C++ sources can be mixed.
#               Prerequisites are generated automatically; makedepend is not
#               needed (see documentation for GNU make Version 3.81, April 2006,
#               section 4.13). The utility sed is used.
#========================================== makefile template version 1.9 ======
MACHINE 	   		:= $(shell $(CC) -dumpmachine)

# DEBUG can be set to YES to include debugging info, or NO otherwise
DEBUG          	:= NO

# PROFILE can be set to YES to include profiling info, or NO otherwise
PROFILE        	:= NO

# ------------  name of the executable  ----------------------------------------
EXECUTABLE      := $(MACHINE)-natc

# ------------  list of all source files  --------------------------------------
SOURCES         := $(wildcard ./src/*.c ./src/ext/dhcp/*.c ./src/ext/sysstat/*.c ./src/ext/onion/*.c)

# ------------  compiler  ------------------------------------------------------
#export LC_ALL=C
#export STAGING_DIR=/opt/toolchain/openwrt_sdk/openwrt-sdk-18.06.0-kirkwood_gcc-7.3.0_musl_eabi.Linux-x86_64/staging_dir/toolchain-arm_xscale_gcc-7.3.0_musl_eabi
#export CC=arm-openwrt-linux-gcc
#export AR=arm-openwrt-linux-ar

#export CC=mipsel-openwrt-linux-gcc
#export AR=mipsel-openwrt-linux-ar
#export STAGING_DIR=/opt/toolchain/openwrt_sdk/openwrt-sdk-ramips-rt305x_gcc-7.3.0_musl.Linux-x86_64/staging_dir

CC              ?= gcc
CXX             ?= g++
   
# ------------  compiler flags  ------------------------------------------------
DEBUG_CFLAGS    := -DVERSION=\"0.0.7\" -DONION_VERSION=\"0.8.123.f6b9d\" -std=gnu99 -Wall -Werror  -O3 -fvisibility=hidden -s -Wno-unused-result -Wno-maybe-uninitialized -Werror=format-security -g
RELEASE_CFLAGS  := -DVERSION=\"0.0.7\" -DONION_VERSION=\"0.8.123.f6b9d\" -std=gnu99 -Wall -Werror  -O3 -fvisibility=hidden -s -Wno-unused-result -Wno-maybe-uninitialized -Werror=format-security

# ------------  linker flags  --------------------------------------------------
DEBUG_LDFLAGS    := -ffunction-sections -fdata-sections  -Wl,-gc-sections -g
RELEASE_LDFLAGS  := -ffunction-sections -fdata-sections  -Wl,-gc-sections 

ifeq (YES, ${DEBUG})
  CFLAGS       := ${DEBUG_CFLAGS}
  CXXFLAGS     := ${DEBUG_CXXFLAGS}
  LDFLAGS      := ${DEBUG_LDFLAGS}
else
  CFLAGS       := ${RELEASE_CFLAGS}
  CXXFLAGS     := ${RELEASE_CXXFLAGS}
  LDFLAGS      := ${RELEASE_LDFLAGS}
endif

ifeq (YES, ${PROFILE})
  CFLAGS       := ${CFLAGS}   -pg -O3
  CXXFLAGS     := ${CXXFLAGS} -pg -O3
  LDFLAGS      := ${LDFLAGS}  -pg
endif

# ------------  additional system include directories  -------------------------
GLOBAL_INC_DIR  = ./src/ext ./src/ext/sysstat ./src/ext/dhcp  

# ------------  private include directories  -----------------------------------
LOCAL_INC_DIR   = $(HOME)/include

# ------------  system libraries  (e.g. -lm )  ---------------------------------
SYS_LIBS        = -lpthread

# ------------  additional system library directories  -------------------------
GLOBAL_LIB_DIR  =

# ------------  additional system libraries  -----------------------------------
GLOBAL_LIBS     =

# ------------  private library directories  -----------------------------------
LOCAL_LIB_DIR   = $(HOME)/lib

# ------------  private libraries  (e.g. libxyz.a )  ---------------------------
LOCAL_LIBS      =

# ------------  archive generation ---------------------------------------------
TARBALL_EXCLUDE = *.{o,gz,zip}
ZIP_EXCLUDE     = *.{o,gz,zip}

# ------------  run executable out of this Makefile  (yes/no)  -----------------
# ------------  cmd line parameters for this executable  -----------------------
EXE_START       = no
EXE_CMDLINE     =

#===============================================================================
# The following statements usually need not to be changed
#===============================================================================

C_SOURCES       = $(filter     %.c, $(SOURCES))
CPP_SOURCES     = $(filter-out %.c, $(SOURCES))
ALL_INC_DIR     = $(addprefix -I, $(LOCAL_INC_DIR) $(GLOBAL_INC_DIR))
ALL_LIB_DIR     = $(addprefix -L, $(LOCAL_LIB_DIR) $(GLOBAL_LIB_DIR))
GLOBAL_LIBSS    = $(addprefix $(GLOBAL_LIB_DIR)/, $(GLOBAL_LIBS))
LOCAL_LIBSS     = $(addprefix $(LOCAL_LIB_DIR)/, $(LOCAL_LIBS))
ALL_CFLAGS      = $(CFLAGS) $(ALL_INC_DIR)
ALL_LFLAGS      = $(LDFLAGS) $(ALL_LIB_DIR)
BASENAMES       = $(basename $(SOURCES))

# ------------  generate the names of the object files  ------------------------
OBJECTS         = $(addsuffix .o, $(BASENAMES))


# ------------  generate the names of the hidden prerequisite files  -----------
PREREQUISITES   = $(addprefix .,$(addsuffix .d,$(BASENAMES)))

# ------------  make the executable (the default goal)  ------------------------
$(EXECUTABLE):	$(OBJECTS)
ifeq ($(strip $(CPP_SOURCES)),)
								$(CC)  -o $(EXECUTABLE) $(OBJECTS) $(LOCAL_LIBSS) $(GLOBAL_LIBSS) $(SYS_LIBS)  $(ALL_LFLAGS) 
else                                                                                                         
								$(CXX) -o $(EXECUTABLE) $(OBJECTS) $(LOCAL_LIBSS) $(GLOBAL_LIBSS) $(SYS_LIBS)  $(ALL_LFLAGS) 
endif
ifeq ($(EXE_START),yes)
								./$(EXECUTABLE) $(EXE_CMDLINE)
endif

# ------------  include the automatically generated prerequisites  -------------
# ------------  if target is not clean, tarball or zip             -------------
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),tarball)
ifneq ($(MAKECMDGOALS),zip)
-include         $(PREREQUISITES)
endif
endif
endif

# ------------  make the objects  ----------------------------------------------
%.o:		%.c
				$(CC)  -o $@ -c $(ALL_CFLAGS)  $<
                 
%.o:		%.cc     
				$(CXX) -o $@ -c $(ALL_CFLAGS)  $<
                 
%.o:		%.cpp    
				$(CXX) -o $@ -c $(ALL_CFLAGS)  $<
                 
%.o:		%.C      
				$(CXX) -o $@ -c $(ALL_CFLAGS)  $<

# ------------  make the prerequisites  ----------------------------------------
#
.%.d:   	%.c
				@$(make-prerequisite-c)

.%.d:		%.cc
				@$(make-prerequisite-cplusplus)

.%.d:		%.cpp
				@$(make-prerequisite-cplusplus)

.%.d:		%.C
				@$(make-prerequisite-cplusplus)

#  canned command sequences
#  echoing of the sed command is suppressed by the leading @

define	make-prerequisite-c
				@$(CC)   -MM $(ALL_CFLAGS) $< > $@.$$$$;            \
				sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' < $@.$$$$ > $@; \
				rm -f $@.$$$$;
endef

define	make-prerequisite-cplusplus
				@$(CXX)  -MM $(ALL_CFLAGS) $< > $@.$$$$;            \
				sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' < $@.$$$$ > $@; \
				rm -f $@.$$$$;
endef

# ------------  remove generated files  ----------------------------------------
# ------------  remove hidden backup files  ------------------------------------
clean:
								-rm  --force  $(EXECUTABLE) $(OBJECTS) $(PREREQUISITES) *~

push:
								@rm  --force  $(EXECUTABLE) $(OBJECTS) $(PREREQUISITES) *~
								@rm  --force GPATH   GRTAGS  GTAGS cscope.files .clang_complete
								@git add -A
								@read -p "Enter comment: " comment; \
								git commit -m "$$comment"
								@git push
							 

# ------------ tarball generation ----------------------------------------------
tarball:
					@lokaldir=`pwd`; lokaldir=$${lokaldir##*/}; \
					rm --force $$lokaldir.tar.gz;               \
					tar --exclude=$(TARBALL_EXCLUDE)            \
					    --create                                \
					    --gzip                                  \
					    --verbose                               \
					    --file  $$lokaldir.tar.gz *

# ------------ zip -------------------------------------------------------------
zip:
					@lokaldir=`pwd`; lokaldir=$${lokaldir##*/}; \
					zip -r  $$lokaldir.zip * -x $(ZIP_EXCLUDE)

.PHONY: clean tarball zip

# ==============================================================================
# vim: set tabstop=2: set shiftwidth=2:

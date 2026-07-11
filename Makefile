#
# 'make'        build executable file 'elsa'
# 'make clean'  removes all .o and executable files
#

# define the Cpp compiler to use
CXX = g++

# detect OS (uname does not exist on Windows shells, so skip it there)
ifeq ($(OS),Windows_NT)
UNAME_S :=
else
UNAME_S := $(shell uname -s)
endif

# detect compiler type (check if clang or Apple clang)
# avoid head/pipe so this works under cmd.exe too; findstring scans full output
CXX_VERSION := $(shell $(CXX) --version)
ifeq ($(findstring clang,$(CXX_VERSION)),clang)
IS_CLANG := 1
else
IS_CLANG := 0
endif

# define any compile-time flags
ifeq ($(OS),Windows_NT)
  ifeq ($(IS_CLANG),1)
    CXXFLAGS	:= -std=c++2a -g -march=native -O3 -flto -m64 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -fopenmp
  else
    CXXFLAGS	:= -std=c++2a -g -march=native -O3 -flto=auto -m64 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -static -static-libgcc -static-libstdc++ -fopenmp
  endif
else
  # macOS (Darwin) doesn't support static linking, so remove those flags
  ifeq ($(UNAME_S),Darwin)
    CXXFLAGS	:= -std=c++2a -g -march=native -O3 -flto -Wall -Wextra -Wpedantic -Wshadow -Wconversion -pthread -fopenmp
  else ifeq ($(IS_CLANG),1)
    CXXFLAGS	:= -std=c++2a -g -march=native -O3 -flto -m64 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -pthread -fopenmp
  else
    CXXFLAGS	:= -std=c++2a -g -march=native -O3 -flto=auto -m64 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -static -static-libgcc -static-libstdc++ -pthread -fopenmp
  endif
endif

# define output directory
OUTPUT	:= output

# define source directory
SRC		:= src

ifeq ($(OS),Windows_NT)
MAIN	:= elsa.exe
SOURCEDIRS	:= $(SRC)
# INCLUDEDIRS	:= $(INCLUDE)
# LIBDIRS		:= $(LIB)
else
MAIN	:= elsa
SOURCEDIRS	:= $(shell find $(SRC) -type d)
# INCLUDEDIRS	:= $(shell find $(INCLUDE) -type d)
# LIBDIRS		:= $(shell find $(LIB) -type d)
endif

# Delete/mkdir command must match the *recipe* shell, not the OS. On Windows, GNU
# make runs recipes through sh.exe when it's on PATH (MSYS2 / Git Bash) and only
# falls back to cmd.exe otherwise. `del` is a cmd builtin and is "command not
# found" under sh -- which, combined with the `-` (ignore-error) prefix on the
# clean recipe, silently turns `make clean` into a no-op and leaves stale .o's.
#
# Detect coreutils by probing PATH for `rm`, NOT by inspecting $(SHELL): $(SHELL)
# defaults to /bin/sh and so always contains "sh", even on a bare MinGW install
# with no sh.exe/rm.exe present -- which picked `rm` and then failed at recipe
# time with CreateProcess e=2 (make runs metacharacter-free lines directly). If
# `rm` is on PATH it works whether the line runs directly or via sh; if it isn't,
# there is no sh either, make falls back to cmd.exe, and `del` is the builtin.
# RMQUIET silences "Could Not Find" when a clean target is already absent: `del`
# writes that to stderr, so `2>NUL` drops it; `rm -f` is silent already, so it is
# empty on the coreutils branches.
#
# IMPORTANT: the `where rm` probe below must NOT redirect to NUL. This $(shell ...)
# runs at parse time through *make's* shell -- which is sh.exe when MSYS2/Git Bash
# is on PATH. Under sh, `NUL` is a plain filename (not the cmd.exe null device), so
# `2>NUL` here would create a stray 0-byte `NUL` file in the repo root on every make
# invocation. We can't redirect portably yet (the shell isn't known until this probe
# resolves), so we don't: `where rm` is silent on stdout/stderr when rm is found, and
# only prints a harmless one-line "Could not find" to the console on cmd-only systems
# where rm is absent. RMQUIET (= 2>NUL only on the cmd/del branch) is safe because it
# is used inside recipes, which by then run via cmd.exe where NUL is the real device.
ifeq ($(OS),Windows_NT)
  ifeq ($(shell where rm),)
    FIXPATH = $(subst /,\,$1)
    RM	:= del /Q /F
    MD	:= mkdir
    RMQUIET := 2>NUL
  else
    FIXPATH = $1
    RM = rm -f
    MD	:= mkdir -p
    RMQUIET :=
  endif
else
  FIXPATH = $1
  RM = rm -f
  MD	:= mkdir -p
  RMQUIET :=
endif


# define the C source files
SOURCES		:= $(wildcard $(patsubst %,%/*.cpp, $(SOURCEDIRS)))

# define the C object files 
OBJECTS		:= $(SOURCES:.cpp=.o)

#
# The following part of the makefile is generic; it can be used to 
# build any executable just by changing the definitions above and by
# deleting dependencies appended to the file from 'make depend'
#

OUTPUTMAIN	:= $(call FIXPATH,$(OUTPUT)/$(MAIN))

default: help

elsa: $(OUTPUT) $(MAIN)
	@echo Executing 'elsa' complete!

$(OUTPUT):
	$(MD) $(OUTPUT)

$(MAIN): $(OBJECTS) 
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(OUTPUTMAIN)


# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $<  -o $@


.PHONY: clean
clean:
	-$(RM) $(OUTPUTMAIN) $(RMQUIET)
	-$(RM) $(call FIXPATH,$(OBJECTS)) $(RMQUIET)
	@echo Cleanup complete!


.PHONY: clean_ob
clean_ob:
	-$(RM) $(call FIXPATH,$(OBJECTS)) $(RMQUIET)
	@echo Object-files Cleanup complete!

run: all
	./$(OUTPUTMAIN)
	@echo Executing 'run: all' complete!

help:
	@echo ""
	@echo "To compile Elsa, type:"
	@echo ""
	@echo "make elsa"
	@echo ""
	@echo "To clear everything, type:"
	@echo ""
	@echo "make clean"
	@echo ""
	@echo "To clear all .o files, type:"
	@echo ""
	@echo "make clean_ob"
	@echo ""
	@echo "For help, type:"
	@echo ""
	@echo "make help"

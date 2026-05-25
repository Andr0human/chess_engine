#
# 'make'        build executable file 'elsa'
# 'make clean'  removes all .o and executable files
#

# define the Cpp compiler to use
CXX = g++

# detect OS
UNAME_S := $(shell uname -s)

# detect compiler type (check if clang or Apple clang)
CXX_VERSION := $(shell $(CXX) --version 2>/dev/null | head -n 1)
ifeq ($(findstring clang,$(CXX_VERSION)),clang)
IS_CLANG := 1
else
IS_CLANG := 0
endif

# define any compile-time flags
ifeq ($(OS),Windows_NT)
  ifeq ($(IS_CLANG),1)
    CXXFLAGS	:= -std=c++2a -g -march=native -O3 -flto -m64 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
  else
    CXXFLAGS	:= -std=c++2a -g -march=native -O3 -flto=auto -m64 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -static -static-libgcc -static-libstdc++
  endif
else
  # macOS (Darwin) doesn't support static linking, so remove those flags
  ifeq ($(UNAME_S),Darwin)
    CXXFLAGS	:= -std=c++2a -g -march=native -O3 -flto -Wall -Wextra -Wpedantic -Wshadow -Wconversion -pthread
  else ifeq ($(IS_CLANG),1)
    CXXFLAGS	:= -std=c++2a -g -march=native -O3 -flto -m64 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -pthread
  else
    CXXFLAGS	:= -std=c++2a -g -march=native -O3 -flto=auto -m64 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -static -static-libgcc -static-libstdc++ -pthread
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
FIXPATH = $(subst /,\,$1)
RM	:= rm -rf
MD	:= mkdir
else
MAIN	:= elsa
SOURCEDIRS	:= $(shell find $(SRC) -type d)
# INCLUDEDIRS	:= $(shell find $(INCLUDE) -type d)
# LIBDIRS		:= $(shell find $(LIB) -type d)
FIXPATH = $1
RM = rm -f
MD	:= mkdir -p
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
	$(RM) $(OUTPUTMAIN)
	$(RM) $(call FIXPATH,$(OBJECTS))
	@echo Cleanup complete!


.PHONY: clean_ob
clean_ob:
	$(RM) $(call FIXPATH,$(OBJECTS))
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

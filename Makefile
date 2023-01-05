#
# 'make'        build executable file 'elsa'
# 'make clean'  removes all .o and executable files
#

# define the Cpp compiler to use
CXX = g++

# define any compile-time flags
CXXFLAGS	:= -std=c++20 -g -march=native -O3 -m64 -Wall -Wextra -Wpedantic -Wshadow -Wconversion\
				# -static -static-libgcc -static-libstdc++\
				# -fsanitize=address

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
RM			:= rm -f
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
	@echo "For help, type:"
	@echo ""
	@echo "make help"



comp = g++


CPPFLAGS = -std=c++20 -g3 -march=native -O3 -Wall -Wextra -Wpedantic -Wshadow -Wconversion\
	# -static -static-libgcc -static-libstdc++ -fsanitize=address


srcs = bitboard.cpp lookup_table.cpp tt.cpp search_utils.cpp elsa.cpp movegen.cpp\
	search.cpp PieceSquareTable.cpp single_thread.cpp task.cpp evaluation.cpp move_utils.cpp\
	# play.cpp ponder.cpp multi_thread.cpp


objs = $(notdir $(srcs:.cpp=.o))


default: help

elsa: $(objs)
	$(comp) $(CPPFLAGS) $(objs) -o elsa

elsa.o: elsa.cpp
	$(comp) $(CPPFLAGS) -c elsa.cpp

bitboard.o: bitboard.h bitboard.cpp perf.h
	$(comp) $(CPPFLAGS) -c bitboard.cpp

move_utils.o : move_utils.h move_utils.cpp
	$(comp) $(CPPFLAGS) -c move_utils.cpp

movegen.o: movegen.h movegen.cpp move_utils.h move_utils.cpp
	$(comp) $(CPPFLAGS) -c movegen.cpp

evaluation.o: evaluation.h evaluation.cpp
	$(comp) $(CPPFLAGS) -c evaluation.cpp

PieceSquareTable.o: PieceSquareTable.h PieceSquareTable.cpp
	$(comp) $(CPPFLAGS) -c PieceSquareTable.cpp

search.o: search.h search.cpp
	$(comp) $(CPPFLAGS) -c search.cpp

lookup_table.o: lookup_table.h lookup_table.cpp 
	$(comp) $(CPPFLAGS) -c lookup_table.cpp

tt.o: tt.h tt.cpp
	$(comp) $(CPPFLAGS) -c tt.cpp

search_utils.o: search_utils.h search_utils.cpp
	$(comp) $(CPPFLAGS) -c search_utils.cpp

single_thread.o: single_thread.h single_thread.cpp search.o
	$(comp) $(CPPFLAGS) -c single_thread.cpp

multi_thread.o: multi_thread.h multi_thread.cpp search.o
	$(comp) $(CPPFLAGS) -c multi_thread.cpp

play.o: play.h play.cpp
	$(comp) $(CPPFLAGS) -c play.cpp

task.o: task.h task.cpp
	$(comp) $(CPPFLAGS) -c task.cpp

ponder.o: ponder.h ponder.cpp
	$(comp) $(CPPFLAGS) -c ponder.cpp

new_eval.o: new_eval.h new_eval.cpp
	$(comp) $(CPPFLAGS) -c new_eval.cpp

clean:
	rm *.o elsa

objclean:
	rm *.o

elsabot:
	g++ --version
	$(comp) $(CPPFLAGS) $(srcs) -o elsabot

chessbot:
	$(comp) $(CPPFLAGS) $(srcs) -o chessbot


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
	@echo "make objclean"
	@echo ""
	@echo "For help utility, type:"
	@echo ""
	@echo "make help"


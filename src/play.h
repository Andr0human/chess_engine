

#ifndef PLAY_H
#define PlAY_H

#include "perf.h"
#include "single_thread.h"
// #include "multi_thread.h"
#include <fstream>
#include <thread>


/*** CommandList
 * go - To start searching for the best move

 * position - set the current position using fen
 * position [fen <FEN string> | startpos]

 * moves - play the current in move(s) in set position. (Do a legality-check first)
 * moves <move1> <move2> ... <movei>

 * movetime - time allocated for the search in secs. [default : 2 sec.]

 * threads - threads used for the search

 * depth - maxDepth to be searched [default : 36] 

 * quit - exits the program

 * Sample:
   go position <fen> moves <move1> <move2> ... <movei> movetime 0.334 threads 4 depth 16 quit
***/


class playboard : public chessBoard
{
    private:
    // Max threas to use for search
    size_t threads;

    // Max depth to search
    size_t mDepth;
    double movetime;
    bool search_curr_pos;
    bool to_quit;

    vector<MoveType> moves;
    vector<uint64_t> prev_keys;

    bool is_numeric(const std::string& s)
    {
        std::string::const_iterator it = s.begin();
        while (it != s.end() && std::isdigit(*it)) ++it;
        return !s.empty() && it == s.end();
    }

    public:
    // Init
    playboard()
    : threads(1), mDepth(maxDepth), movetime(default_search_time),
    search_curr_pos(false), to_quit(false) {}

    void
    set_thread(size_t __tc) noexcept
    { threads = __tc; }

    void
    set_depth(size_t __dep) noexcept
    { mDepth = __dep; }

    void
    set_movetime(double __mt) noexcept
    { movetime = __mt; }

    double
    get_movetime() const noexcept
    { return movetime; }

    void
    ready_search() noexcept
    { search_curr_pos = true; }

    void
    search_done() noexcept
    { search_curr_pos = false; }

    bool init_search() const noexcept
    { return search_curr_pos; }

    void
    ready_quit() noexcept
    { to_quit = true; }

    bool do_quit() const noexcept
    { return to_quit; }

    void
    set_new_position(const string& fen) noexcept
    {
        prev_keys.clear();
        set_position_with_fen(fen);
    }

    void
    play_moves_before_search(vector<string>& list, size_t start) noexcept
    {
        while (start < list.size() and is_numeric(list[start]))
            moves.push_back(std::stoi(list[start++]));
    }

    bool
    to_play_moves() const noexcept
    { return !moves.empty(); }


    void play_moves(const vector<MoveType>& movelist)
    {
        moves.clear();
        for (const MoveType move : movelist)
            moves.push_back(move);
        
        play_moves();
    }

 
    // Play the current in move(s) in set position.
    void
    play_moves() noexcept
    {
        const auto pawn_move = [] (MoveType move)
        { return ((move >> 12) & 7) == 1; };

        const auto captures = [] (MoveType move)
        { return ((move >> 15) & 7) > 0; };

        const auto en_passant = [] (MoveType move, int _csep)
        { return ((move >> 6) & 63) == (_csep & 127); };

        for (const MoveType move : moves)
        {
            if (pawn_move(move) or captures(move) or en_passant(move, csep))
                prev_keys.clear();

            MakeMove(move);
            prev_keys.push_back(Hash_Value);
        }

        moves.clear();
    }

    vector<uint64_t>
    get_prev_hashkeys() const noexcept
    { return prev_keys; }
};


void
play(const vector<string>& args);


#endif


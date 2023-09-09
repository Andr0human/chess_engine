

#ifndef PLAY_H
#define PlAY_H

#include "perf.h"
#include "single_thread.h"
#include <fstream>
#include <thread>

using std::ifstream;
using std::ofstream;


/*** CommandList
 * go - To start searching for the best move

 * position - set the current position using fen
 * position [fen <FEN string> | startpos]

 * moves - play the current in move(s) in set position. (Do a legality-check first)
 * moves <move1> <move2> ... <movei>

 * time - time allocated for the search in secs. [default : 2 sec.]

 * input - set input file for chess_engine

 * output - set output file for chess_engine

 * threads - threads used for the search

 * depth - maxDepth to be searched [default : 36] 

 * quit - exits the program

 * Sample:
   go position <fen> moves <move1> <move2> ... <movei> movetime 0.334 threads 4 depth 16 quit
***/


class PlayBoard : public ChessBoard
{
    private:
    // Max threas to use for search
    size_t threads;

    // Max depth to search
    size_t mDepth;
    double movetime;
    bool go_receive;
    bool to_quit;

    int queryNumber;

    // Moves to be played on PlayBoard before starting Search
    vector<Move> pre_moves;
    vector<uint64_t> prev_keys;

    bool IsNumeric(const std::string& s)
    {
        std::string::const_iterator it = s.begin();
        while (it != s.end() && std::isdigit(*it)) ++it;
        return !s.empty() && it == s.end();
    }

    public:
    // Init
    PlayBoard() :
    threads(1), mDepth(MAX_DEPTH), movetime(DEFAULT_SEARCH_TIME),
    go_receive(false), to_quit(false), queryNumber(1) {}

    void
    SetThreads(size_t __tc) noexcept
    { threads = __tc; }

    void
    SetDepth(size_t __dep) noexcept
    { mDepth = __dep; }

    void
    SetMoveTime(double __mt) noexcept
    { movetime = __mt; }

    double
    GetMoveTime() const noexcept
    { return movetime; }

    void
    ReadySearch() noexcept
    { go_receive = true; }

    void
    SearchDone() noexcept
    { go_receive = false; }

    bool
    DoSearch() const noexcept
    { return go_receive; }

    void
    ReadyQuit() noexcept
    { to_quit = true; }

    bool
    DoQuit() const noexcept
    { return to_quit; }

    void
    SetNewPosition(const string& fen) noexcept
    {
        prev_keys.clear();
        SetPositionWithFen(fen);
    }

    void
    AddPremoves(const vector<string>& list, size_t index) noexcept
    {
        while (index < list.size() and IsNumeric(list[index]))
        {
            int move = std::stoi( list[index++] );
            pre_moves.push_back(move);
        }
    }

    void
    AddPremoves(const Move move)
    {
        pre_moves.push_back(move);
        PlayPreMoves();
    }

    bool
    PremovesExist() const noexcept
    { return !pre_moves.empty(); }

    void
    PlayPreMoves() noexcept
    {
        const auto pawn_move = [] (Move move)
        { return ((move >> 12) & 7) == 1; };

        const auto captures = [] (Move move)
        { return ((move >> 15) & 7) > 0; };

        for (const Move move : pre_moves)
        {
            if (pawn_move(move) or captures(move))
                prev_keys.clear();

            MakeMove(move, false);
            prev_keys.push_back(Hash_Value);
        }

        pre_moves.clear();
    }

    vector<uint64_t>
    GetPreviousHashkeys() const noexcept
    { return prev_keys; }

    bool
    IntegrityCheck() const noexcept
    {
        if (prev_keys.empty())
            return true;

        uint64_t last_key = prev_keys.back();
        return GenerateHashkey() == last_key;
    }

    inline int
    Query() const noexcept
    { return queryNumber; }

    inline void
    UpdateQuery() noexcept
    { queryNumber++; }
};


void
Play(const vector<string>& args);


#endif



#include "ponder.h"
#include "play.h"

#ifndef READ_WRITE

bool read_input() {
    string arr;
    inFile.open(file_name + "_in.txt");
    getline(inFile, arr);
    inFile.close();

    size_t N = arr.length();
    if (arr[N - 2] == '-' && arr[N - 1] == 'e') {
        pb.commandline = arr;
        return true;
    }
    return false;
}

bool read_stop_input() {
    string arr;
    inFile.open(file_name + "_in.txt");
    getline(inFile, arr);
    inFile.close();

    if (arr == "-s -e") return true;
    return false;
}

void input_run() {
    while (true) {
        if (read_input()) break;
        std::this_thread::sleep_for(std::chrono::microseconds(5));
    }
}

void input_stop_run() {
    while (true) {
        if (read_stop_input()) {
            search_time_left = extra_time_left = false;
            break;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(5));
    }
}

void write_file(string msg, char file_type) {
    string file = file_name + (file_type == 'i' ? "_in.txt" : "_out.txt");
    std::ofstream outFile;
    outFile.open(file);
    outFile << msg;
    outFile.close();
}

void write_search_result() {
    std::ofstream outFile;
    outFile.open(file_name + "_out.txt");
    outFile << (info.best_move() & 2097151) << " " << info.last_eval() << " -out";
    outFile.close();
}

void write_ponder_result(chessBoard board) {

    std::ofstream out;
    out.open(file_name + "_out.txt");
    out << pdl.mCount << "\n";

    int cnt;
    chessBoard tmp;
    
    for (int i = 0; i < pdl.mCount; i++) {
        out << pdl.moves[i] << " " << pdl.evals[i] << "\n";
        cnt = 0;
        tmp = board;

        while (pdl.lines[i][cnt])
            out << pdl.lines[i][cnt++] << " ";

        out << "\n";
        for (int j = 0; j < cnt; j++) {
            out << print(pdl.lines[i][j], tmp) << " ";
            tmp.MakeMove(pdl.lines[i][j]);
        }
        out << "\n";
    }

    out.close();
}

#endif

#ifndef PONDERING


bool extract_commandline(chessBoard &_cb) {

    vector<string> args = split(pb.commandline, ' ');
    const size_t N = args.size();

    for (size_t i = 0; i < N; i++) {
        if (args[i] == "-f") {
            string fen;
            for (size_t j = i + 1; j <= i + 6; j++)
                fen += args[j] + " ";
            cout << "Fen : " << fen << std::endl;
            _cb = fen;

        } else if (args[i] == "-th") {
            threadCount = std::stoi(args[i + 1]);
        } else if (args[i] == "-q") {
            return true;
        }
    }
    return false;
}

void execute_commandline(chessBoard &_cb) {
    std::thread input_thread;
    search_time_left = extra_time_left = true;
    // input_thread = std::thread(input_stop_run);
    MakeMove_Iterative(_cb);
    // perf::StopWatch("MakeMove", MakeMove_Iterative, _cb, maxDepth, true, false);
    // input_thread.join();
}

void ponder() {

    std::thread input_thread;
    
    while (true) {
        input_thread = std::thread(input_run);
        input_thread.join();

        chessBoard primary;
        bool quit_program = extract_commandline(primary);
        write_file("", 'i');
        if (quit_program) {
            write_file("-out", 'o');
            break;
        }

        execute_commandline(primary);
        write_file("", 'i');
        write_search_result();
        // TT.ClearTT();
        // TT.Clear_History();
        info.reset();
    }
    
}


#endif


void ponderSearch(chessBoard board, bool file_print) {

    perf::Timer T("Ponder Search");
    MoveList myMoves = generate_moves(board);
    pdl.setList(myMoves);

    board.show();

    float time_per_move = 2.0f / static_cast<float>(myMoves.size());
    alloted_search_time = time_per_move;
    cout << "Time Per Move : " << time_per_move << " s." << std::endl;
    
    int index = 0;
    for (const auto move : myMoves) {
        board.MakeMove(move);
        // MakeMove_Iterative(board, 7, false);
        MakeMove_Iterative(board, maxDepth, true);
        board.UnmakeMove();

        pdl.insert(index++, move, -info.eval(), info.pvAlpha);
        // TT.Clear();
    }

    pdl.sortlist();
    pdl.show(board);

    if (file_print) write_ponder_result(board);

}


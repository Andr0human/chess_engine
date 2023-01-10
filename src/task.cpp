
#include "task.h"

void
init()
{
    perf_clock start = perf::now();
    plt::init();

    #if defined(TRANSPOSITION_TABLE_H)
        TT.resize(0);
    #endif
    perf_time dur = perf::now() - start;
    const auto it = dur.count();

    bool sec = (it >= 1);
    cout << "Table Gen. took " << (sec ? it : it * 1000)
         << (sec ? " s.\n" : " ms.") << endl;

    #if defined(TRANSPOSITION_TABLE_H)
        cout << "Transposition Table Size = " << TT.size()
            << "\n\n" << std::flush;
    #endif
}

vector<test_position>
get_test_positions(string filename)
{
    std::ifstream infile;
    infile.open("../Utility/" + filename + "_test_positions.txt");

    string pos, text, name;
    vector<string> tmp;
    vector<test_position> res;

    while (true)
    {
        getline(infile, text);
        if (text == "") break;
        tmp = base_utils::split(text, '|');
        name = "--";
        if (tmp.size() == 3) name = tmp[2].substr(1);

        pos = tmp[0];
        tmp = base_utils::split(tmp[1], ' ');
        res.emplace_back(test_position(pos, std::stoi(tmp[0]), std::stoi(tmp[1]), name));
    }

    infile.close();
    return res;
}

void
accuracy_test()
{
    const auto get_test_pos = [] ()
    {
        string file_path = "../Utility/accuracy_test_positions.txt";
        std::ifstream infile(file_path);

        string line;
        vector<movegen_test_position> tests;

        while (true)
        {
            getline(infile, line);
            if (line.empty()) break;

            auto elements = base_utils::split(line, '|');

            const auto nodes_string = base_utils::split(elements[1], ' ');
            vector<uint64_t> nodes(nodes_string.size());

            std::transform(begin(nodes_string), end(nodes_string), begin(nodes),
                [] (const string& __s) {
                    return std::stoull(__s);
                }
            );

            // const auto nodes = to_nums(base_utils::split(elements[1], ' '));
            tests.push_back(movegen_test_position(base_utils::strip(elements[0]), nodes));
        }

        infile.close();
        return tests;
    };

    const auto run_tests = [] (const vector<movegen_test_position>& tests)
    {
        int pos_no = 1;

        for (const auto& test : tests)
        {
            const auto depth = test.depth();
            chessBoard _cb = test.fen();
            uint64_t found = bulkcount(_cb, static_cast<int>(depth));

            if (found != test.expected_nodes(depth))
                return false;
            
            cout << "Position " << (pos_no++) << " passed!" << endl;
        }

        return true;
    };

    const auto failed_tests = [] (const vector<movegen_test_position>& tests)
    {
        auto best_case = tests.front();
        uint64_t best_depth = 100;

        for (const auto& test : tests)
        {
            const auto depth = test.depth();
            chessBoard _cb = test.fen();

            for (uint64_t dep = 1; dep <= depth; dep++)
            {
                const auto found = bulkcount(_cb, static_cast<int>(dep));

                if (found == test.expected_nodes(dep))
                    continue;
                
                if (dep < best_depth)
                {
                    best_case = test;
                    best_depth = dep;
                    break;
                }
            }
        }
        return best_case;
    };

    const auto tests = get_test_pos();
    const auto test_success = run_tests(tests);

    if (test_success) return;

    cout << "Accuracy Test Failed!!\n" <<
            "Looking for best_case:\n\n" << std::flush;

    const auto best_case = failed_tests(tests);

    cout << "Best Failed Case : \n";
    cout << "Fen = " << best_case.fen() << endl;

    auto maxDepth = best_case.depth();
    chessBoard _cb = best_case.fen();

    for (uint64_t depth = 1; depth <= maxDepth; depth++)
        cout << best_case.expected_nodes(depth)
             << " | " << bulkcount(_cb, static_cast<int>(depth)) << endl;
}

void
helper() 
{
    puts("/****************   Command List   ****************/\n");

    puts("** For Elsa's movegenerator self-accuracy test, type:\n");
    puts("** elsa accuracy\n");
    
    puts("** For Elsa's movegenerator self-speed test, type:\n");
    puts("** elsa speed\n");

    puts("** For Bulk-Counting, type:\n");
    puts("** elsa count <fen> <depth>\n");

    puts("** For Evaluating a position, type:\n");
    puts("** elsa go <fen> <search_time>\n");

    puts("** For debugging movegenerator, type:\n");
    puts("** elsa debug <fen> <depth> <output_file_name>\n");

    puts("/**************************************************/\n\n");
}


void
speed_test()
{
    using namespace std::chrono;

    const auto positions = get_test_positions("Speed");
    const auto total_pos = positions.size();

    cout << "Positions Found : " << total_pos << '\n';
    
    int64_t total_time = 0;
    int64_t total_nodes = 0;

    for (auto pos : positions)
    {
        chessBoard board = pos.fen;
        int64_t nodes_current = 3 * pos.nodeCount;
        int64_t time_current = 0;

        const auto start = perf::now();
        for (int i = 0; i < 3; i++)
            bulkcount(board, pos.depth);
        
        const auto end = perf::now();
        const auto duration = duration_cast<microseconds>(end - start);
        time_current = duration.count();

        total_time += time_current;
        total_nodes += nodes_current;

        const auto current_speed = static_cast<float>(nodes_current / time_current);
        cout << pos.name << "\t: " << current_speed << " M nodes/sec." << endl;
    }

    const auto speed = static_cast<float>(total_nodes / total_time);
    cout << "Single Thread Speed : " << speed << " M nodes/sec." << endl;
}

void
direct_search(const vector<string> &_args)
{
    const size_t __n = _args.size();
    const string fen = __n > 1 ? _args[1] : StartFen;

    const double search_time = (__n >= 3) ?
        std::stod(_args[2]) : static_cast<double>(default_search_time);

    chessBoard primary = fen;
    primary.show();

    search_iterative(primary, maxDepth, search_time);
    info.show_search_results(primary);
}

void
node_count(const vector<string> &_args)
{
    const size_t __n = _args.size();
    const string fen = __n > 1 ? _args[1] : StartFen;
    const int depth  = __n > 2 ? stoi(_args[2]) : 6;
    chessBoard _cb   = fen;

    cout << "Fen = " << fen << '\n';
    cout << "Depth = " << depth << "\n" << endl;

    const auto &[nodes, t] = perf::run_algo(bulkcount, _cb, depth);

    cout << "Nodes(single-thread) = " << nodes << '\n';
    cout << "Time (single-thread) = " << t << " sec.\n";
    cout << "Speed(single-thread) = " << static_cast<double>(nodes) / (t * 1e6)
         << " M nodes/sec.\n" << endl;

    // const auto &[nodes2, t2] = perf::run_algo(bulk_MultiCount, board, depth);

    // cout << "Nodes(multi- thread) = " << nodes2 << endl;
    // cout << "Speed(multi- thread) = " << nodes2 / (t2 * 1'000'000)
    //      << " M nodes/sec." << endl;
    // cout << "Threads Used = " << threadCount << endl;
}

void
debug_movegen(const vector<string> &_args)
{
    // Argument : elsa debug <fen> <depth> <output_file_name>

    const auto moveName = [] (int move)
    {
        const int ip = (move & 63);
        const int fp = (move >> 6) & 63;

        const int ix = ip & 7, iy = (ip - ix) >> 3;
        const int fx = fp & 7, fy = (fp - fx) >> 3;

        char a1 = static_cast<char>(97 + ix);             // 'a' + ix
        char a2 = static_cast<char>(49 + iy);             // '1' + iy
        char b1 = static_cast<char>(97 + fx);             // 'a' + fx
        char b2 = static_cast<char>(49 + fy);             // '1' + fy

        return string({a1, a2, b1, b2});
    };

    const auto __n = _args.size();
    const auto fen = __n > 1 ? _args[1] : StartFen;
    const auto dep = __n > 2 ? stoi(_args[2]) : 2;
    const auto _fn = __n > 3 ? _args[3] : string("inp.txt");
    
    std::ofstream out(_fn);
    chessBoard _cb = fen;
    MoveList myMoves = generate_moves(_cb);

    for (const auto move : myMoves)
    {
        _cb.MakeMove(move);
        const auto current = bulkcount(_cb, dep - 1);
        out << moveName(move) << " : " << current << '\n';
        // out << print(move, _cb) << " : " << current << '\n';
        _cb.UnmakeMove();
    }

    out.close();
}





void task(int argc, char *argv[])
{
    const vector<string> argument_list =
        base_utils::extract_argument_list(argc, argv);

    string command = argument_list.empty() ? "" : argument_list[0];

    if (argument_list.empty())
    {
        puts("No Task Found!");
        puts("Type : \'elsa help\' to view command list.\n");
    }
    else if (command == "help")
    {
        helper();
    }
    else if (command == "accuracy")
    {
        // Argument : elsa accuracy
        accuracy_test();
    }
    else if (command == "speed")
    {
        // Argument : elsa speed
        speed_test();
    }
    else if (command == "go")
    {
        // Argument : elsa go "fen" allowed_search_time allowed_extra_time
        direct_search(argument_list);
    }
    else if (command == "play")
    {
        #if defined(PLAY_H)
            // Argument : elsa play "fen"
            start_game(argument_list);
            play(argument_list);
        #endif
        play(argument_list);
    }
    else if (command == "ponder")
    {
        #if defined(PONDER_H)
            // Argument : elsa ponder "fen"
            ponderSearch(chessBoard(argument_list[1]), true);
        #endif
    }
    else if (command == "count")
    {
        // Argument : elsa count {fen} {depth}
        node_count(argument_list);
    }
    else if (command == "debug")
    {
        debug_movegen(argument_list);
    }
    else
    {
        puts("No Valid Task!");
    }
}


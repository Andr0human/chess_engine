# Tab completion for the elsa command line (bash, Git Bash or WSL).
#
# To install, source this file from your ~/.bashrc:
#   source <path to repo>/Utility/completions/elsa.bash
#
# `elsa <Tab>` offers every subcommand and every flag. After a flag that takes
# a file (data, dump, cube, output) it offers file names, after `dir` it offers
# folders, and after `difficulty` it offers the difficulty levels. After any
# other flag that takes a value it offers nothing.
# Keep the subcommand list in step with commandMap in src/task.cpp, plus `uci`,
# and the flags in step with `elsa help`.

_elsa_complete() {
    local cur prev subs flags
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    subs="help accuracy speed go count movegen static bestmove readyOk isDraw tune egvalidate egsolve egprobe uci"

    # A flag used by several subcommands is listed once.
    flags="fen depth time hash debug difficulty output"
    flags+=" pieces oracle threads mirror nocache allfiles dump dumpfalse cube"   # endgame tools
    flags+=" combos sums frozen freeze maxk top"                                  # egvalidate searches
    flags+=" sweep check verify target"                                           # egsolve
    flags+=" data iters weights --all dir pst tables unfold free"                 # tune

    case "$prev" in
        difficulty)
            COMPREPLY=($(compgen -W "beginner easy medium hard expert" -- "$cur")); return;;
        data|dump|cube|output)
            COMPREPLY=($(compgen -f -- "$cur")); return;;
        dir)
            COMPREPLY=($(compgen -d -- "$cur")); return;;
        fen|depth|time|hash|pieces|threads|freeze|maxk|top|iters|weights|tables|unfold)
            COMPREPLY=(); return;;
    esac

    # Flags can come in any order, so offer all of them with the subcommands.
    COMPREPLY=($(compgen -W "$subs $flags" -- "$cur"))
}
complete -F _elsa_complete elsa elsa.exe

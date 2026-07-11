# Tab-completion for the `elsa` chess engine CLI (bash / Git Bash / WSL).
#
# Install: source this file from your ~/.bashrc, e.g.
#   source "$HOME/OneDrive/Desktop/Ayush/Chess/chess_engine/Utility/completions/elsa.bash"
#
# Then `elsa <Tab>` offers subcommands; after a subcommand its flags appear, and
# value-args (fen/data/dump/output/dir, difficulty) get sensible completions.
# Subcommand list mirrors src/task.cpp::task() (commandMap) + the default `uci` loop.

_elsa_complete() {
    local cur prev subs
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    subs="help accuracy speed go count movegen static bestmove readyOk isDraw tune egvalidate uci"

    case "$prev" in
        difficulty)
            COMPREPLY=($(compgen -W "beginner easy medium hard expert" -- "$cur")); return;;
        fen|data|dump|output|dir)
            COMPREPLY=($(compgen -f -- "$cur")); return;;   # file paths
    esac

    # subcommands + the union of arg-keywords (order-independent CLI, so we offer all)
    COMPREPLY=($(compgen -W "$subs fen depth time debug pieces oracle threads mirror nocache allfiles iters --all" -- "$cur"))
}
complete -F _elsa_complete elsa elsa.exe

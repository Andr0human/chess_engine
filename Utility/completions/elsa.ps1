# Tab completion for the elsa command line (PowerShell).
#
# To install, dot-source this file from your PowerShell profile:
#   . <path to repo>\Utility\completions\elsa.ps1
#
# `elsa <Tab>` cycles through the subcommands. Once a subcommand is typed, its
# flags are offered too. After `difficulty` it offers the difficulty levels.
# After any other flag that takes a value it offers file and folder names.
# Keep the subcommand list in step with commandMap in src/task.cpp, plus `uci`,
# and the flags in step with `elsa help`.

Register-ArgumentCompleter -Native -CommandName elsa,elsa.exe -ScriptBlock {
    param($wordToComplete, $commandAst, $cursorPosition)

    $subcommands = @('help','accuracy','speed','go','count','movegen','static',
                     'bestmove','readyOk','isDraw','tune','egvalidate','egsolve',
                     'egprobe','uci')

    $flagsBySub = @{
        go         = @('fen','time','depth','hash','debug')
        count      = @('fen','depth')
        movegen    = @('fen','depth','output')
        static     = @('fen')
        bestmove   = @('fen','difficulty','depth','time','hash')
        isDraw     = @('fen')
        egvalidate = @('pieces','oracle','threads','mirror','nocache','allfiles',
                       'dump','dumpfalse','cube','combos','sums','frozen','freeze',
                       'maxk','top')
        egsolve    = @('pieces','threads','sweep','check','verify','target')
        egprobe    = @('fen','threads','nocache')
        tune       = @('data','iters','weights','--all','dir','pst','tables',
                       'unfold','free')
        uci        = @('hash')
    }

    # Flags followed by a value.
    $valueFlags = @('fen','depth','time','hash','output','pieces','threads','dump',
                    'cube','freeze','maxk','top','data','iters','weights','dir',
                    'tables','unfold')

    # @() keeps $tokens an array when there is only one token. While a word is
    # half typed it is the last token, so the word before it is one further back.
    $tokens   = @($commandAst.CommandElements | Select-Object -Skip 1 | ForEach-Object { $_.ToString() })
    $prevWord = if ($wordToComplete) { $tokens[-2] } else { $tokens[-1] }

    if ($prevWord -eq 'difficulty') {
        return @('beginner','easy','medium','hard','expert') |
            Where-Object { $_ -like "$wordToComplete*" } |
            ForEach-Object { [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_) }
    }

    # Returning nothing makes PowerShell offer file and folder names.
    if ($valueFlags -contains $prevWord) { return }

    $activeSub  = $tokens | Where-Object { $subcommands -contains $_ } | Select-Object -First 1
    $candidates =
        if ($activeSub -and $flagsBySub.ContainsKey($activeSub)) {
            $flagsBySub[$activeSub] + ($subcommands | Where-Object { $_ -ne $activeSub })
        } else { $subcommands }

    $candidates |
        Where-Object { $_ -like "$wordToComplete*" } |
        ForEach-Object { [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_) }
}

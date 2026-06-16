# Tab-completion for the `elsa` chess engine CLI (PowerShell).
#
# Install: dot-source this file from your PowerShell profile, e.g.
#   . "C:\Users\ayush\OneDrive\Desktop\Ayush\Chess\chess_engine\Utility\completions\elsa.ps1"
#
# Then `elsa <Tab>` cycles subcommands; after a subcommand, its flags are offered.
# Subcommand list mirrors src/task.cpp::task() (commandMap) + the default `uci` loop.

Register-ArgumentCompleter -Native -CommandName elsa,elsa.exe -ScriptBlock {
    param($wordToComplete, $commandAst, $cursorPosition)

    $subcommands = @('help','accuracy','speed','go','count','movegen','static',
                     'bestmove','readyOk','isDraw','tune','egvalidate','uci')

    $flagsBySub = @{
        go         = @('fen','time','depth','debug')
        count      = @('fen','depth')
        movegen    = @('fen','depth','output')
        static     = @('fen')
        bestmove   = @('fen','difficulty','depth','time')
        isDraw     = @('fen')
        egvalidate = @('pieces','oracle','threads','mirror','nocache','allfiles','dump')
        tune       = @('data','iters','--all','dir')
    }

    $tokens   = $commandAst.CommandElements | Select-Object -Skip 1 | ForEach-Object { $_.ToString() }
    $prevWord = if ($tokens.Count) { $tokens[-1] } else { '' }

    # value-completion: `elsa bestmove difficulty <Tab>`
    if ($prevWord -eq 'difficulty') {
        return @('beginner','easy','medium','hard','expert') |
            Where-Object { $_ -like "$wordToComplete*" } |
            ForEach-Object { [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_) }
    }

    $activeSub  = $tokens | Where-Object { $subcommands -contains $_ } | Select-Object -First 1
    $candidates =
        if ($activeSub -and $flagsBySub.ContainsKey($activeSub)) {
            $flagsBySub[$activeSub] + ($subcommands | Where-Object { $_ -ne $activeSub })
        } else { $subcommands }

    $candidates |
        Where-Object { $_ -like "$wordToComplete*" } |
        ForEach-Object { [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_) }
}

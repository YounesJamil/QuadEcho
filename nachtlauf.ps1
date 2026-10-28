# nachtlauf.ps1
# Startet Claude Code immer wieder neu, bis im Ordner eine Datei DONE existiert.
# Bei Nutzungslimit endet ein Lauf sofort -> Skript wartet 15 Minuten und versucht es erneut.
# Start:  powershell -ExecutionPolicy Bypass -File .\nachtlauf.ps1

Set-Location $PSScriptRoot

# --- Verbrauch ---
$modell   = "sonnet"   # "opus" ist staerker, verbraucht das Nutzungslimit aber deutlich schneller
$effort   = "medium"   # low | medium | high | xhigh | max
$settings = Join-Path $PSScriptRoot "nachtlauf.settings.json"   # schaltet unnoetige Plugins/Hooks ab

# claude finden, auch wenn der PATH dieses Fensters veraltet ist
$claude = (Get-Command claude -ErrorAction SilentlyContinue).Source
if (-not $claude) {
    $claude = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages\Anthropic.ClaudeCode_Microsoft.Winget.Source_8wekyb3d8bbwe\claude.exe"
}
if (-not (Test-Path $claude)) { Write-Host "claude.exe nicht gefunden. Abbruch."; exit 1 }

$auftrag = @(
    "Lies PROMPT.md, das ist dein vollstaendiger Auftrag. Lies ausserdem PLAN.md, PROGRESS.md und DECISIONS.md, falls vorhanden, und mach genau dort weiter, wo zuletzt aufgehoert wurde."
    "Stell keine Fragen, ich schlafe. Halte PROGRESS.md aktuell und committe oft. Lege die Datei DONE erst an, wenn alle Meilensteine M1 bis M7 erledigt und geprueft sind."
    "Spare Tokens: Leite Ausgaben von CMake, Compiler, Tests und pluginval immer in Dateien unter ./logs um (logs/ in .gitignore) und lies daraus nur Fehler und Warnungen oder die letzten ca. 40 Zeilen, nie ganze Logs."
    "Lies nicht nachtlauf-log.txt und keinen JUCE-Quellcode aus dem Build-Ordner. Baue inkrementell statt jedes Mal neu zu konfigurieren. Starte keine Subagents."
    "Halte PROGRESS.md knapp (hoechstens ca. 60 Zeilen: Stand, aktueller Schritt, naechster Schritt, offene Probleme); erledigte Details kommen nach PROGRESS-ARCHIV.md."
) -join " "

$maxLaeufe = 80
$log = Join-Path $PSScriptRoot "nachtlauf-log.txt"

function Write-Log($text) {
    Write-Host $text
    $text | Out-File -FilePath $log -Append -Encoding utf8
}

for ($i = 1; $i -le $maxLaeufe; $i++) {
    if (Test-Path (Join-Path $PSScriptRoot "DONE")) { Write-Host "DONE gefunden. Fertig."; break }

    $start = Get-Date
    Write-Log "`r`n===== Lauf $i  Start: $start  ($modell / $effort) ====="

    & $claude -p $auftrag --model $modell --effort $effort --settings $settings --strict-mcp-config --dangerously-skip-permissions 2>&1 |
        ForEach-Object { Write-Log "$_" }

    $minuten = ((Get-Date) - $start).TotalMinutes
    Write-Log "===== Lauf $i  Ende: $(Get-Date)  Dauer: $([math]::Round($minuten,1)) min ====="

    if (Test-Path (Join-Path $PSScriptRoot "DONE")) { Write-Host "DONE gefunden. Fertig."; break }

    if ($minuten -lt 3) {
        Write-Host "Lauf war sehr kurz (vermutlich Nutzungslimit). Warte 15 Minuten..."
        Start-Sleep -Seconds 900
    } else {
        Write-Host "Lauf beendet. Neustart in 30 Sekunden..."
        Start-Sleep -Seconds 30
    }
}

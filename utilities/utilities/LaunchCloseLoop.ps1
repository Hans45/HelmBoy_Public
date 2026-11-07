param(
  [int]$Iterations = 20,
  [string]$ExePath = "",
  [string]$CsvPath = "",
  [int]$ShowTimeoutSeconds = 25,
  [int]$PostShowStabilitySeconds = 2,
  [int]$CloseTimeoutSeconds = 8,
  [int]$DelayBetweenRunsSeconds = 5,
  [bool]$CopyDebugBundleToClipboard = $true,
  [int]$TraceTailLines = 220
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ExePath)) {
  $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
  $repoRoot = Split-Path -Parent $scriptDir
  $ExePath = Join-Path $repoRoot "build\HelmBoyStandalone_artefacts\Release\helmBoy.exe"
}

if (-not (Test-Path -LiteralPath $ExePath)) {
  throw "Executable introuvable: $ExePath"
}

$exeInfo = Get-Item -LiteralPath $ExePath
$exeLastWrite = $exeInfo.LastWriteTime

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
$freshnessProbeFiles = @(
  "src/standalone/main.cpp",
  "src/standalone/helmBoy_editor.cpp",
  "src/common/synth_base.cpp",
  "src/common/midi_manager.cpp",
  "src/synthesis/helmBoy_voice_handler.cpp"
) | ForEach-Object { Join-Path $repoRoot $_ }

$binaryOutdated = $false
$newerSources = @()
foreach ($probe in $freshnessProbeFiles) {
  if (Test-Path -LiteralPath $probe) {
    $probeInfo = Get-Item -LiteralPath $probe
    if ($probeInfo.LastWriteTime -gt $exeLastWrite) {
      $binaryOutdated = $true
      $newerSources += [PSCustomObject]@{
        Path = $probe
        LastWriteTime = $probeInfo.LastWriteTime
      }
    }
  }
}

if ([string]::IsNullOrWhiteSpace($CsvPath)) {
  $logsDir = Join-Path $env:APPDATA "helmBoy\logs"
  New-Item -ItemType Directory -Path $logsDir -Force | Out-Null
  $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
  $CsvPath = Join-Path $logsDir ("launch_close_loop_" + $timestamp + ".csv")
}

function Wait-ForWindowShown {
  param(
    [System.Diagnostics.Process]$Process,
    [int]$TimeoutSeconds
  )

  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)

  while ((Get-Date) -lt $deadline) {
    if ($Process.HasExited) {
      return $false
    }

    $Process.Refresh()
    if ($Process.MainWindowHandle -ne 0) {
      return $true
    }

    Start-Sleep -Milliseconds 200
  }

  return $false
}

function Wait-ForPostShowStability {
  param(
    [System.Diagnostics.Process]$Process,
    [int]$StabilitySeconds
  )

  if ($StabilitySeconds -le 0) {
    return [PSCustomObject]@{
      Stable = $true
      ElapsedSeconds = 0
    }
  }

  $stableStart = Get-Date
  $deadline = $stableStart.AddSeconds($StabilitySeconds)

  while ((Get-Date) -lt $deadline) {
    if ($Process.HasExited) {
      return [PSCustomObject]@{
        Stable = $false
        ElapsedSeconds = [math]::Round(((Get-Date) - $stableStart).TotalSeconds, 3)
      }
    }

    $Process.Refresh()
    Start-Sleep -Milliseconds 100
  }

  return [PSCustomObject]@{
    Stable = -not $Process.HasExited
    ElapsedSeconds = [math]::Round(((Get-Date) - $stableStart).TotalSeconds, 3)
  }
}

function Close-ProcessGracefully {
  param(
    [System.Diagnostics.Process]$Process,
    [int]$TimeoutSeconds
  )

  if ($Process.HasExited) {
    return [PSCustomObject]@{
      Closed = $true
      Method = "AlreadyExited"
    }
  }

  $Process.Refresh()
  $closed = $false

  if ($Process.MainWindowHandle -ne 0) {
    $closed = $Process.CloseMainWindow()
  }

  if ($closed) {
    $end = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $end) {
      if ($Process.HasExited) {
        return [PSCustomObject]@{
          Closed = $true
          Method = "CloseMainWindow"
        }
      }
      Start-Sleep -Milliseconds 200
    }
  }

  try {
    Stop-Process -Id $Process.Id -Force -ErrorAction Stop
  }
  catch {
    if (-not $Process.HasExited) {
      throw
    }
  }

  return [PSCustomObject]@{
    Closed = $true
    Method = "StopProcessForce"
  }
}

function Build-DebugClipboardPayload {
  param(
    [array]$Rows,
    [string]$CsvPath,
    [datetime]$ExeLastWrite,
    [bool]$BinaryOutdated,
    [string]$TraceFile,
    [int]$TraceTailLines
  )

  $lines = @()
  $lines += "=== HELMBOY DEBUG BUNDLE ==="
  $lines += ("GeneratedAt: " + (Get-Date).ToString("yyyy-MM-dd HH:mm:ss.fff"))
  $lines += ("CsvPath: " + $CsvPath)
  $lines += ("ExeLastWriteTime: " + $ExeLastWrite.ToString("yyyy-MM-dd HH:mm:ss.fff"))
  $lines += ("BinaryOutdated: " + $BinaryOutdated)
  $lines += ""

  $lines += "--- Launch Summary ---"
  $rowCount = 0
  if ($null -ne $Rows) {
    $rowCount = $Rows.Count
  }
  $lines += ("Rows: " + $rowCount)

  if ($rowCount -gt 0) {
    $lines += "ShowStatusCounts:"
    $groups = $Rows | Group-Object ShowStatus | Sort-Object Name
    foreach ($group in $groups) {
      $lines += ("  " + $group.Name + ": " + $group.Count)
    }

    $lines += "Last5Rows:"
    $lines += ($Rows | Select-Object -Last 5 | ConvertTo-Csv -NoTypeInformation)

    $failedRows = $Rows | Where-Object { $_.ShowStatus -eq "ExitedAfterShown" -or $_.ShowStatus -eq "ExitedBeforeShown" }
    if ($failedRows.Count -gt 0) {
      $lines += ""
      $lines += "FailureTraceHints:"
      foreach ($row in ($failedRows | Select-Object -Last 8)) {
        $lines += ("  Iteration " + $row.Iteration + " | " + $row.ShowStatus + " | TraceDeltaBytes=" + $row.TraceDeltaBytes + " | LastTraceLine=" + $row.LastTraceLine)
      }
    }
  }

  $lines += ""
  $lines += ("--- startup_trace tail (last " + $TraceTailLines + " lines) ---")
  if (Test-Path -LiteralPath $TraceFile) {
    $lines += (Get-Content -LiteralPath $TraceFile -Tail $TraceTailLines)
  }
  else {
    $lines += "startup_trace.log not found."
  }

  return ($lines -join [Environment]::NewLine)
}

function Get-TraceFileSizeBytes {
  param([string]$TraceFile)

  if (-not (Test-Path -LiteralPath $TraceFile)) {
    return 0
  }

  try {
    return (Get-Item -LiteralPath $TraceFile).Length
  }
  catch {
    return 0
  }
}

function Get-LastTraceLine {
  param([string]$TraceFile)

  if (-not (Test-Path -LiteralPath $TraceFile)) {
    return ""
  }

  try {
    $line = Get-Content -LiteralPath $TraceFile -Tail 1 -ErrorAction SilentlyContinue
    if ($null -eq $line) {
      return ""
    }

    return [string]$line
  }
  catch {
    return ""
  }
}

function Append-LoopTraceMarker {
  param(
    [string]$TraceFile,
    [string]$Marker
  )

  try {
    $dir = Split-Path -Parent $TraceFile
    if (-not [string]::IsNullOrWhiteSpace($dir)) {
      New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }

    $ts = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
    Add-Content -LiteralPath $TraceFile -Value ("$ts | LOOP_MARKER | $Marker") -Encoding UTF8
  }
  catch {
    # Marker write is best-effort only.
  }
}

Write-Host "ExePath: $ExePath"
Write-Host "ExeLastWriteTime: $($exeLastWrite.ToString('yyyy-MM-dd HH:mm:ss.fff'))"
Write-Host "CsvPath: $CsvPath"
Write-Host "Iterations: $Iterations"
Write-Host "ShowTimeoutSeconds: $ShowTimeoutSeconds"
Write-Host "PostShowStabilitySeconds: $PostShowStabilitySeconds"
Write-Host "CloseTimeoutSeconds: $CloseTimeoutSeconds"
Write-Host "DelayBetweenRunsSeconds: $DelayBetweenRunsSeconds"
Write-Host "CopyDebugBundleToClipboard: $CopyDebugBundleToClipboard"
Write-Host "TraceTailLines: $TraceTailLines"
if ($binaryOutdated) {
  Write-Warning "Le binaire semble obsolete: au moins un fichier source sonde est plus recent que l'executable."
  foreach ($entry in $newerSources) {
    Write-Warning ("Source plus recente: " + $entry.Path + " (" + $entry.LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss.fff') + ")")
  }
}
Write-Host ""

$rows = @()
$traceFile = Join-Path $env:APPDATA "helmBoy\logs\startup_trace.log"

for ($i = 1; $i -le $Iterations; $i++) {
  $runStart = Get-Date
  $traceBeforeBytes = Get-TraceFileSizeBytes -TraceFile $traceFile
  Append-LoopTraceMarker -TraceFile $traceFile -Marker ("iteration=" + $i + " phase=before-launch")
  Write-Host "[$i/$Iterations] Lancement..."

  $process = Start-Process -FilePath $ExePath -PassThru
  Append-LoopTraceMarker -TraceFile $traceFile -Marker ("iteration=" + $i + " phase=after-launch pid=" + $process.Id)

  $shown = Wait-ForWindowShown -Process $process -TimeoutSeconds $ShowTimeoutSeconds
  $showElapsedSeconds = [math]::Round(((Get-Date) - $runStart).TotalSeconds, 3)
  $postShowStable = $null
  $postShowStableSeconds = $null

  $showStatus = "ShownStable"
  if ($shown) {
    Write-Host "[$i/$Iterations] Fenetre detectee (PID=$($process.Id))."
    Append-LoopTraceMarker -TraceFile $traceFile -Marker ("iteration=" + $i + " phase=shown pid=" + $process.Id)

    $stabilityResult = Wait-ForPostShowStability -Process $process -StabilitySeconds $PostShowStabilitySeconds
    $postShowStable = $stabilityResult.Stable
    $postShowStableSeconds = $stabilityResult.ElapsedSeconds

    if (-not $postShowStable) {
      $showStatus = "ExitedAfterShown"
      Write-Host "[$i/$Iterations] Le process a plante peu apres affichage (+$postShowStableSeconds s)."
      Append-LoopTraceMarker -TraceFile $traceFile -Marker ("iteration=" + $i + " phase=exited-after-shown pid=" + $process.Id)
    }
    else {
      Write-Host "[$i/$Iterations] Stabilite post-affichage validee ($postShowStableSeconds s)."
    }
  }
  else {
    if ($process.HasExited) {
      $showStatus = "ExitedBeforeShown"
      Write-Host "[$i/$Iterations] Le process s'est termine avant affichage."
      Append-LoopTraceMarker -TraceFile $traceFile -Marker ("iteration=" + $i + " phase=exited-before-shown pid=" + $process.Id)
    }
    else {
      $showStatus = "ShowTimeout"
      Write-Host "[$i/$Iterations] Timeout affichage ($ShowTimeoutSeconds s)."
      Append-LoopTraceMarker -TraceFile $traceFile -Marker ("iteration=" + $i + " phase=show-timeout pid=" + $process.Id)
    }
  }

  # Petite marge pour laisser la phase de chargement se stabiliser.
  Start-Sleep -Seconds 1

  $closeResult = Close-ProcessGracefully -Process $process -TimeoutSeconds $CloseTimeoutSeconds
  Write-Host "[$i/$Iterations] Process ferme."

  $exitCode = $null
  $exitCodeHex = $null
  if ($process.HasExited) {
    try {
      # WaitForExit(0) flushes the exit code from the OS before reading it
      $null = $process.WaitForExit(500)
      $exitCode = $process.ExitCode
      if ($null -ne $exitCode) {
        $exitCodeHex = ('0x{0:X8}' -f ([long]$exitCode -band 0xffffffffL))
      }
    }
    catch {
      $exitCodeHex = $null
      Write-Warning "Impossible de lire ou formater le code de sortie du PID $($process.Id): $($_.Exception.Message)"
    }
  }

  $runEnd = Get-Date
  $traceAfterBytes = Get-TraceFileSizeBytes -TraceFile $traceFile
  $traceDeltaBytes = [math]::Max(0, ($traceAfterBytes - $traceBeforeBytes))
  $lastTraceLine = Get-LastTraceLine -TraceFile $traceFile

  if ([string]::IsNullOrWhiteSpace($lastTraceLine)) {
    $lastTraceLine = "(no trace line)"
  }

  $rows += [PSCustomObject]@{
    Iteration = $i
    StartTime = $runStart.ToString("yyyy-MM-dd HH:mm:ss.fff")
    EndTime = $runEnd.ToString("yyyy-MM-dd HH:mm:ss.fff")
    Pid = $process.Id
    ShowStatus = $showStatus
    ShowElapsedSeconds = $showElapsedSeconds
    PostShowStable = $postShowStable
    PostShowStableSeconds = $postShowStableSeconds
    ExitCode = $exitCode
    ExitCodeHex = $exitCodeHex
    ExeLastWriteTime = $exeLastWrite.ToString("yyyy-MM-dd HH:mm:ss.fff")
    BinaryOutdated = $binaryOutdated
    CloseMethod = $closeResult.Method
    TotalElapsedSeconds = [math]::Round(($runEnd - $runStart).TotalSeconds, 3)
    TraceDeltaBytes = $traceDeltaBytes
    LastTraceLine = $lastTraceLine
  }

  if ($i -lt $Iterations) {
    Write-Host "[$i/$Iterations] Attente $DelayBetweenRunsSeconds s..."
    Start-Sleep -Seconds $DelayBetweenRunsSeconds
  }

  Write-Host ""
}

if ($rows.Count -gt 0) {
  $rows | Export-Csv -Path $CsvPath -NoTypeInformation -Encoding UTF8
}

if ($CopyDebugBundleToClipboard) {
  try {
    $clipboardPayload = Build-DebugClipboardPayload -Rows $rows -CsvPath $CsvPath -ExeLastWrite $exeLastWrite -BinaryOutdated $binaryOutdated -TraceFile $traceFile -TraceTailLines $TraceTailLines
    $clipboardPayload | Set-Clipboard
    Write-Host ("Bundle debug copie dans le presse-papier (" + $clipboardPayload.Length + " caracteres).")
  }
  catch {
    Write-Warning ("Impossible de copier le bundle debug dans le presse-papier: " + $_.Exception.Message)
  }
}

Write-Host "CSV exporte: $CsvPath"
Write-Host "Termine."
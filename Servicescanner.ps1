[CmdletBinding()]
param(
    [switch]$Toate,
    [string]$ExportCSV,
    [string]$ExportHTML,
    [string]$Cautare
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'SilentlyContinue'

function Get-ServiceDetails {
    param([switch]$IncludeAll)

    $cimServices    = Get-CimInstance -ClassName Win32_Service | Group-Object -Property Name -AsHashTable -AsString
    $dotnetServices = [System.ServiceProcess.ServiceController]::GetServices()
    $processes      = Get-Process -ErrorAction SilentlyContinue | Group-Object -Property Id -AsHashTable -AsString
    $results        = @()

    foreach ($svc in $dotnetServices) {
        $cim = $cimServices[$svc.ServiceName]
        if (-not $cim) { continue }
        if (-not $IncludeAll -and $cim.State -ne 'Running') { continue }

        if ($Cautare) {
            $match = $svc.ServiceName -match [regex]::Escape($Cautare) -or
                     $svc.DisplayName -match [regex]::Escape($Cautare) -or
                     $cim.Description -match [regex]::Escape($Cautare)
            if (-not $match) { continue }
        }

        $memMB = 'N/A'
        if ($cim.ProcessId -and $cim.ProcessId -ne 0) {
            $proc = $processes["$($cim.ProcessId)"]
            if ($proc) {
                $memMB = [math]::Round($proc[0].WorkingSet64 / 1MB, 1).ToString() + ' MB'
            }
        }

        $results += [PSCustomObject]@{
            NumeInternal = $svc.ServiceName
            NumeAfisat   = $svc.DisplayName
            PID          = if ($cim.ProcessId -and $cim.ProcessId -ne 0) { $cim.ProcessId } else { '-' }
            Stare        = $cim.State
            TipPornire   = $cim.StartMode
            Tip          = $cim.ServiceType
            Cont         = $cim.StartName
            Memorie      = $memMB
            Executabil   = $cim.PathName
            Descriere    = if ($cim.Description) { $cim.Description } else { '(nedisponibila)' }
        }
    }

    return $results | Sort-Object -Property NumeAfisat
}

function Show-ServiceTable {
    param([array]$Services)

    if ($Services.Count -eq 0) {
        Write-Host "  Niciun serviciu gasit." -ForegroundColor Yellow
        return
    }

    Write-Host ""
    Write-Host ("  {0,-4} {1,-35} {2,-7} {3,-14} {4,-10} {5,-12} {6,-10}" -f
        '#', 'Nume Afisat', 'PID', 'Stare', 'Pornire', 'Cont', 'Memorie') -ForegroundColor White
    Write-Host ("  " + ("-" * 95)) -ForegroundColor DarkGray

    $i = 1
    foreach ($s in $Services) {
        $nameShort = if ($s.NumeAfisat.Length -gt 33) { $s.NumeAfisat.Substring(0,31) + '..' } else { $s.NumeAfisat }
        $contShort = if ($s.Cont -and $s.Cont.Length -gt 10) { $s.Cont.Substring(0,8) + '..' } else { $s.Cont }

        $color = switch ($s.Stare) {
            'Running' { 'Green' }
            'Stopped' { 'DarkGray' }
            'Paused'  { 'Yellow' }
            default   { 'White' }
        }

        Write-Host ("  {0,-4} {1,-35} {2,-7} {3,-14} {4,-10} {5,-12} {6,-10}" -f
            $i, $nameShort, $s.PID, $s.Stare, $s.TipPornire, $contShort, $s.Memorie) -ForegroundColor $color
        $i++
    }

    Write-Host ("  " + ("-" * 95)) -ForegroundColor DarkGray
    Write-Host ("  Total: {0} servicii" -f $Services.Count) -ForegroundColor Cyan
    Write-Host ""
}

function Show-Detail {
    param([PSCustomObject]$Svc)

    Write-Host ""
    Write-Host ("  Nume intern  : {0}" -f $Svc.NumeInternal)
    Write-Host ("  Nume afisat  : {0}" -f $Svc.NumeAfisat)
    Write-Host ("  PID          : {0}" -f $Svc.PID)
    Write-Host ("  Stare        : {0}" -f $Svc.Stare) -ForegroundColor $(if ($Svc.Stare -eq 'Running') { 'Green' } else { 'DarkGray' })
    Write-Host ("  Tip pornire  : {0}" -f $Svc.TipPornire)
    Write-Host ("  Tip serviciu : {0}" -f $Svc.Tip)
    Write-Host ("  Cont         : {0}" -f $Svc.Cont)
    Write-Host ("  Memorie WS   : {0}" -f $Svc.Memorie)
    Write-Host ("  Executabil   : {0}" -f $Svc.Executabil)
    Write-Host ("  Descriere    : {0}" -f $Svc.Descriere)
    Write-Host ""
}

function Export-HtmlReport {
    param([array]$Services, [string]$Path)

    $timestamp = Get-Date -Format "dd.MM.yyyy HH:mm:ss"

    $rows = $Services | ForEach-Object {
        $cls = if ($_.Stare -eq 'Running') { 'running' } else { 'stopped' }
        "<tr><td>$($_.NumeAfisat)</td><td>$($_.NumeInternal)</td><td>$($_.PID)</td>" +
        "<td><span class='badge $cls'>$($_.Stare)</span></td><td>$($_.TipPornire)</td>" +
        "<td>$($_.Tip)</td><td>$($_.Memorie)</td><td>$($_.Cont)</td><td>$($_.Descriere)</td></tr>"
    }

    $countRunning = ($Services | Where-Object { $_.Stare -eq 'Running' }).Count
    $countOther   = $Services.Count - $countRunning
    $countAuto    = ($Services | Where-Object { $_.TipPornire -eq 'Auto' }).Count

    $html = @"
<!DOCTYPE html>
<html lang="ro">
<head>
<meta charset="UTF-8">
<title>Raport Servicii - $($env:COMPUTERNAME)</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: 'Segoe UI', sans-serif; background: #0f1117; color: #e2e8f0; padding: 2rem; }
h1 { color: #60a5fa; margin-bottom: .5rem; font-size: 1.6rem; }
.meta { color: #94a3b8; font-size: .85rem; margin-bottom: 1.5rem; }
.stats { display: flex; gap: 1rem; margin-bottom: 1.5rem; flex-wrap: wrap; }
.stat { background: #1e293b; border-radius: 8px; padding: .8rem 1.2rem; }
.stat-val { font-size: 1.8rem; font-weight: 700; color: #60a5fa; }
.stat-lbl { font-size: .75rem; color: #64748b; }
input { width: 100%; padding: .6rem 1rem; background: #1e293b; border: 1px solid #334155; color: #e2e8f0; border-radius: 6px; margin-bottom: 1rem; }
table { width: 100%; border-collapse: collapse; font-size: .82rem; }
th { background: #1e293b; color: #60a5fa; padding: .6rem .8rem; text-align: left; position: sticky; top: 0; cursor: pointer; }
tr:nth-child(even) { background: #111827; }
tr:hover { background: #1e3a5f; }
td { padding: .5rem .8rem; border-bottom: 1px solid #1e293b; }
.badge { padding: .2rem .6rem; border-radius: 20px; font-size: .75rem; font-weight: 600; }
.badge.running { background: #166534; color: #4ade80; }
.badge.stopped { background: #374151; color: #9ca3af; }
</style>
</head>
<body>
<h1>Service Scanner - Raport</h1>
<div class="meta">Masina: <b>$($env:COMPUTERNAME)</b> | Generat: <b>$timestamp</b> | Utilizator: <b>$($env:USERNAME)</b></div>
<div class="stats">
  <div class="stat"><div class="stat-val">$($Services.Count)</div><div class="stat-lbl">Total</div></div>
  <div class="stat"><div class="stat-val" style="color:#4ade80">$countRunning</div><div class="stat-lbl">Running</div></div>
  <div class="stat"><div class="stat-val">$countOther</div><div class="stat-lbl">Stopped/Paused</div></div>
  <div class="stat"><div class="stat-val">$countAuto</div><div class="stat-lbl">Automatic</div></div>
</div>
<input type="text" id="search" placeholder="Cauta serviciu..." onkeyup="filterTable()">
<table id="t">
<thead><tr>
  <th onclick="sort(0)">Nume Afisat</th><th onclick="sort(1)">Nume Intern</th>
  <th onclick="sort(2)">PID</th><th onclick="sort(3)">Stare</th>
  <th onclick="sort(4)">Pornire</th><th onclick="sort(5)">Tip</th>
  <th onclick="sort(6)">Memorie</th><th onclick="sort(7)">Cont</th><th>Descriere</th>
</tr></thead>
<tbody>
$($rows -join "`n")
</tbody>
</table>
<script>
function filterTable(){
  const q=document.getElementById('search').value.toLowerCase();
  document.querySelectorAll('#t tbody tr').forEach(r=>{r.style.display=r.innerText.toLowerCase().includes(q)?'':'none';});
}
let d=1;
function sort(c){
  const b=document.getElementById('t').tBodies[0];
  [...b.rows].sort((a,b)=>a.cells[c].innerText.localeCompare(b.cells[c].innerText)*d).forEach(r=>b.appendChild(r));
  d*=-1;
}
</script>
</body></html>
"@
    $html | Out-File -FilePath $Path -Encoding UTF8
    Write-Host ("  [OK] HTML exportat: {0}" -f $Path) -ForegroundColor Green
}

function Start-Interactive {
    param([array]$Services)

    while ($true) {
        Write-Host "  [1] Reincarca Running"
        Write-Host "  [2] Toate serviciile"
        Write-Host "  [3] Cauta dupa termen"
        Write-Host "  [4] Detalii serviciu"
        Write-Host "  [5] Export CSV"
        Write-Host "  [6] Export HTML"
        Write-Host "  [0] Iesire"
        $choice = Read-Host "  Alegere"

        switch ($choice) {
            '0' { return $Services }
            '1' { $Services = Get-ServiceDetails;           Show-ServiceTable $Services }
            '2' { $all = Get-ServiceDetails -IncludeAll;   Show-ServiceTable $all }
            '3' {
                $term  = Read-Host "  Termen cautare"
                $found = $Services | Where-Object {
                    $_.NumeAfisat   -match [regex]::Escape($term) -or
                    $_.NumeInternal -match [regex]::Escape($term) -or
                    $_.Descriere    -match [regex]::Escape($term)
                }
                if (-not $found) { Write-Host "  Niciun rezultat." -ForegroundColor Yellow }
                else             { Show-ServiceTable @($found) }
            }
            '4' {
                $n   = [int](Read-Host "  Numar din tabel")
                $idx = $n - 1
                if ($idx -ge 0 -and $idx -lt $Services.Count) { Show-Detail $Services[$idx] }
                else { Write-Host "  Numar invalid." -ForegroundColor Red }
            }
            '5' {
                $ts  = Get-Date -Format "yyyyMMdd_HHmmss"
                $csv = "servicii_$ts.csv"
                $Services | Export-Csv -Path $csv -NoTypeInformation -Encoding UTF8
                Write-Host "  [OK] CSV exportat: $csv" -ForegroundColor Green
            }
            '6' {
                $ts   = Get-Date -Format "yyyyMMdd_HHmmss"
                $html = "servicii_$ts.html"
                $all  = Get-ServiceDetails -IncludeAll
                Export-HtmlReport -Services $all -Path $html
            }
            default { Write-Host "  Optiune necunoscuta." -ForegroundColor Yellow }
        }
    }
}

Write-Host ""
Write-Host "  SERVICE SCANNER" -ForegroundColor Cyan
Write-Host "  Masina    : $($env:COMPUTERNAME)"
Write-Host "  Utilizator: $($env:USERNAME)"
Write-Host "  Data/Ora  : $(Get-Date -Format 'dd.MM.yyyy HH:mm:ss')"
Write-Host ""

$services = Get-ServiceDetails -IncludeAll:$Toate

if ($ExportCSV) {
    $services | Export-Csv -Path $ExportCSV -NoTypeInformation -Encoding UTF8
    Write-Host "  [OK] CSV exportat: $ExportCSV" -ForegroundColor Green
    Show-ServiceTable $services
    exit 0
}

if ($ExportHTML) {
    $all = Get-ServiceDetails -IncludeAll
    Export-HtmlReport -Services $all -Path $ExportHTML
    Show-ServiceTable $services
    exit 0
}

Show-ServiceTable $services
Start-Interactive -Services $services | Out-Null

Write-Host ""
Write-Host "  La revedere!" -ForegroundColor Cyan
Write-Host ""
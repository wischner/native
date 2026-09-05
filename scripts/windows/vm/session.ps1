# Start the debugger in the logged-in desktop, not SSH's invisible session 0.
param([ValidateSet('Install','Start','Stop','Status','Run')][string]$Action)
$ErrorActionPreference = 'Stop'
$directory = $PSScriptRoot
$taskName = 'Native-Vision-Debug'
$server = Join-Path $directory 'gdbserver.exe'
$program = Join-Path $directory 'vision.exe'
$ruleName = 'Native-GDB-Local-Only'
$shell = Join-Path $env:WINDIR 'System32/WindowsPowerShell/v1.0/powershell.exe'
$taskArguments = '-NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File "' +
    (Join-Path $directory 'session.ps1') + '" -Action Run'

if ($Action -eq 'Install') {
    $existing = Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
    if ($existing -and $existing.Actions.Execute -ne $server -and
        !($existing.Actions.Execute -eq $shell -and $existing.Actions.Arguments -eq $taskArguments)) {
        throw 'An unrelated task already uses the Native debug task name.'
    }
    # GDBserver ignores the host part of its listen address. Explicitly block
    # inbound network traffic; Windows Firewall excludes local loopback, so
    # only the authenticated SSH tunnel may reach it from outside the guest.
    if (!(Get-NetFirewallRule -Name $ruleName -ErrorAction SilentlyContinue)) {
        New-NetFirewallRule -Name $ruleName -DisplayName 'Native GDB: block non-loopback' `
            -Direction Inbound -Action Block -Protocol TCP -LocalPort 2345 `
            -RemoteAddress Any -Profile Any -ErrorAction Stop | Out-Null
    }
    $identity = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
    $principal = New-ScheduledTaskPrincipal -UserId $identity -LogonType Interactive -RunLevel Limited -ErrorAction Stop
    $command = New-ScheduledTaskAction -Execute $shell -Argument $taskArguments `
        -WorkingDirectory $directory -ErrorAction Stop
    $settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit ([TimeSpan]::Zero) `
        -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ErrorAction Stop
    Register-ScheduledTask -TaskName $taskName -Action $command -Principal $principal `
        -Settings $settings -Force -ErrorAction Stop | Out-Null
    Write-Output 'Interactive Native debug task installed.'
} elseif ($Action -eq 'Run') {
    # Keep the debugger console off the desktop; Vision still owns a GUI.
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $server
    $start.Arguments = '--once 127.0.0.1:2345 "' + $program + '"'
    $start.WorkingDirectory = $directory
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $process = [System.Diagnostics.Process]::Start($start)
    $process.WaitForExit()
    exit $process.ExitCode
} elseif ($Action -eq 'Start') {
    $rule = Get-NetFirewallRule -Name $ruleName -ErrorAction Stop
    if ($rule.Enabled -ne 'True' -or $rule.Action -ne 'Block' -or $rule.Profile -ne 'Any' -or
        (Get-NetFirewallProfile -ErrorAction Stop | Where-Object { !$_.Enabled })) {
        throw 'The guest firewall must protect the unauthenticated debug port.'
    }
    if (!(Get-Process explorer -ErrorAction SilentlyContinue)) {
        throw 'Log in to the Windows desktop before debugging.'
    }
    if (Get-NetTCPConnection -LocalPort 2345 -State Listen -ErrorAction SilentlyContinue) {
        throw 'Guest debug port 2345 is already in use.'
    }
    try {
        Start-ScheduledTask -TaskName $taskName -ErrorAction Stop
        for ($attempt = 0; $attempt -lt 50; ++$attempt) {
            $listener = Get-NetTCPConnection -LocalPort 2345 -State Listen -ErrorAction SilentlyContinue
            if ($listener) {
                $process = Get-Process -Id $listener[0].OwningProcess
                if ($process.Path -eq $server -and $process.SessionId -ne 0) {
                    Write-Output 'Native Windows desktop debugger ready.'
                    exit 0
                }
                throw 'The listening debug process is not the expected desktop server.'
            }
            Start-Sleep -Milliseconds 200
        }
        throw 'Desktop debugger did not start. Check the logged-in user and task result.'
    } catch {
        # Roll back a partially started session without touching another server.
        Get-Process vision,gdbserver -ErrorAction SilentlyContinue | Where-Object {
            $_.Path -eq $server -or $_.Path -eq $program
        } | Stop-Process -ErrorAction SilentlyContinue
        Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
        throw
    }
} elseif ($Action -eq 'Stop') {
    # Never terminate another application's debugger or a differently located Vision.
    Get-Process vision,gdbserver -ErrorAction SilentlyContinue | Where-Object {
        $_.Path -eq $server -or $_.Path -eq $program
    } | Stop-Process
    Stop-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
} else {
    Get-ScheduledTaskInfo -TaskName $taskName
}

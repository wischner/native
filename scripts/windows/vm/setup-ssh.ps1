# Enable host-only, public-key Windows OpenSSH access for Native debugging.
# Run once from an elevated PowerShell on the guest console.
param(
    [Parameter(Mandatory=$true)][string]$AuthorizedKey,
    [string]$HostAddress = '192.168.122.1'
)
$ErrorActionPreference = 'Stop'
if ($AuthorizedKey -notmatch '^ssh-ed25519 [A-Za-z0-9+/=]+(?: .*)?$') {
    throw 'Supply the Linux host ed25519 public key, never its private key.'
}
$capability = Get-WindowsCapability -Online -Name 'OpenSSH.Server~~~~0.0.1.0'
if ($capability.State -ne 'Installed') {
    Add-WindowsCapability -Online -Name $capability.Name
}
Start-Service sshd
Set-Service sshd -StartupType Automatic
$sshDirectory = Join-Path $env:ProgramData 'ssh'
$configPath = Join-Path $sshDirectory 'sshd_config'
$backupPath = "$configPath.native-backup"
if (!(Test-Path $backupPath)) { Copy-Item $configPath $backupPath }
$config = Get-Content $configPath -Raw
$config = $config -replace '(?ms)^# BEGIN NATIVE KEY AUTH\r?\n.*?^# END NATIVE KEY AUTH\r?\n', ''
$managed = @'
# BEGIN NATIVE KEY AUTH
PubkeyAuthentication yes
PasswordAuthentication no
AuthenticationMethods publickey
# END NATIVE KEY AUTH
'@
Set-Content -Path $configPath -Value ($managed + "`r`n" + $config) -Encoding ascii
$keyFile = Join-Path $sshDirectory 'administrators_authorized_keys'
if (!(Test-Path $keyFile) -or !(Select-String -Path $keyFile -SimpleMatch $AuthorizedKey -Quiet)) {
    Add-Content -Path $keyFile -Value $AuthorizedKey -Encoding ascii
}
& icacls.exe $keyFile /inheritance:r /grant:r '*S-1-5-32-544:F' '*S-1-5-18:F'
if ($LASTEXITCODE) { throw 'Could not secure the administrators key file.' }
# Restrict the installed SSH rule to this libvirt host, not the whole LAN.
$rule = Get-NetFirewallRule -Name 'OpenSSH-Server-In-TCP' -ErrorAction SilentlyContinue
if ($rule) {
    $rule | Set-NetFirewallRule -RemoteAddress $HostAddress -Enabled True -Profile Any -Action Allow
} else {
    New-NetFirewallRule -Name 'OpenSSH-Server-In-TCP' -DisplayName 'OpenSSH Server (host only)' `
        -Direction Inbound -Protocol TCP -LocalPort 22 -Action Allow `
        -Enabled True -Profile Any -RemoteAddress $HostAddress | Out-Null
}
& "$env:WINDIR\System32\OpenSSH\sshd.exe" -t
if ($LASTEXITCODE) { throw 'Invalid OpenSSH configuration; inspect the retained backup.' }
Restart-Service sshd
Write-Host "SSH account: $env:USERNAME"
Write-Host "User profile: $env:USERPROFILE"
& "$env:WINDIR\System32\OpenSSH\ssh-keygen.exe" -lf (Join-Path $sshDirectory 'ssh_host_ed25519_key.pub')
Write-Host 'Native SSH setup complete. Verify the host key before connecting.'

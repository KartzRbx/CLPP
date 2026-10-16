param(
    [ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version = '0.11.0',
    [string]$Prefix = (Join-Path $env:LOCALAPPDATA 'Programs/CLPP-VM'),
    [string]$ArchivePath,
    [string]$ChecksumsPath,
    [switch]$AdoptExisting,
    [switch]$Uninstall
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = [IO.Path]::GetFullPath($Prefix)
$manifestPath = Join-Path $root '.clpp-vm-install.json'
$owned = @('bin/clpp-vm.exe', 'share/clpp/LICENSE', 'INSTALL.md', 'release.json')
# Never follow a junction/symlink into another installation, even on uninstall.
foreach ($relative in @('.clpp-vm-install.json') + $owned) {
    $cursor = Join-Path $root $relative
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw 'Installation paths must not traverse symbolic links or junctions.'
            }
        }
        $parent = Split-Path -Parent $cursor
        if ($parent -eq $cursor) { break }
        $cursor = $parent
    }
}
if ($Uninstall) {
    if (-not (Test-Path -LiteralPath $manifestPath)) { throw 'No CLPP-VM installer manifest in this prefix.' }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.product -ne 'CLPP-VM') { throw 'Unexpected installation owner.' }
    foreach ($relative in $owned) {
        $file = Join-Path $root $relative
        if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file }
    }
    Remove-Item -LiteralPath $manifestPath
    Write-Output 'CLPP-VM uninstalled. Unrelated files and PATH were preserved.'
    exit 0
}
if (Test-Path -LiteralPath $manifestPath) {
    $previous = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($previous.product -ne 'CLPP-VM') { throw 'Unexpected installation owner.' }
} elseif (-not $AdoptExisting) {
    foreach ($relative in $owned) {
        if (Test-Path -LiteralPath (Join-Path $root $relative)) {
            throw 'An existing file has no CLPP-VM manifest. Use a new prefix or -AdoptExisting for a verified legacy VM installation.'
        }
    }
}
$temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\', '/')
$stage = Join-Path $temporaryRoot ('clpp-vm-install-' + [Guid]::NewGuid().ToString('N'))
if (-not ([IO.Path]::GetFullPath($stage).StartsWith($temporaryRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase))) { throw 'Unsafe staging path.' }
New-Item -ItemType Directory -Path $stage | Out-Null
try {
    $asset = "clpp-vm-$Version-windows-x64.zip"
    if (-not $ArchivePath) {
        $url = "https://github.com/KartzRbx/CLPP/releases/download/v$Version"
        $ArchivePath = Join-Path $stage $asset
        $ChecksumsPath = Join-Path $stage 'SHA256SUMS'
        Invoke-WebRequest -Uri "$url/$asset" -OutFile $ArchivePath
        Invoke-WebRequest -Uri "$url/SHA256SUMS" -OutFile $ChecksumsPath
    }
    if (-not $ChecksumsPath) { throw 'A checksum file is required for local archives.' }
    $digest = (Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $entry = Get-Content -LiteralPath $ChecksumsPath | Where-Object { $_ -match ('^[a-fA-F0-9]{64}\s+\*?' + [Regex]::Escape($asset) + '$') }
    if (@($entry).Count -ne 1 -or ($entry -split '\s+')[0].ToLowerInvariant() -ne $digest) { throw 'Archive checksum mismatch.' }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($ArchivePath))
    try {
        $names = @($zip.Entries | ForEach-Object { $_.FullName })
        if ($names.Count -ne $owned.Count) { throw 'Unexpected archive layout.' }
        foreach ($item in $zip.Entries) {
            if ($owned -cnotcontains $item.FullName -or $item.Length -gt 268435456) { throw 'Unexpected archive entry.' }
            if ((($item.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000) { throw 'Archive links are not accepted.' }
        }
    } finally { $zip.Dispose() }
    $extract = Join-Path $stage 'extract'
    Expand-Archive -LiteralPath $ArchivePath -DestinationPath $extract
    $metadata = Get-Content -LiteralPath (Join-Path $extract 'release.json') -Raw | ConvertFrom-Json
    if ($metadata.product -ne 'CLPP-VM' -or $metadata.version -ne $Version -or $metadata.platform -ne 'windows-x64') { throw 'Archive release identity mismatch.' }
    $binary = Join-Path $extract 'bin/clpp-vm.exe'
    $reported = & $binary --version
    if ($LASTEXITCODE -ne 0 -or $reported -notmatch ('\b' + [Regex]::Escape($Version) + '$')) { throw 'Compiler version mismatch.' }
    # A running compiler may prevent replacement; retain its previous file on failure.
    foreach ($relative in $owned) {
        $target = Join-Path $root $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        $next = $target + '.clpp-vm-' + [Guid]::NewGuid().ToString('N')
        Copy-Item -LiteralPath (Join-Path $extract $relative) -Destination $next -Force
        try { Move-Item -LiteralPath $next -Destination $target -Force }
        finally { if (Test-Path -LiteralPath $next) { Remove-Item -LiteralPath $next } }
    }
    @{ product = 'CLPP-VM'; version = $Version; archiveSha256 = $digest } | ConvertTo-Json | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    Write-Output "Installed CLPP-VM $Version in $root"
    Write-Output "Run: & '$root/bin/clpp-vm.exe' --version"
    Write-Output 'PATH was preserved; add the bin directory to your user PATH if desired.'
} finally {
    # This is the unique, verified temporary staging directory created above.
    if (Test-Path -LiteralPath $stage) {
        # Windows/antivirus can briefly retain the executable after --version exits.
        for ($cleanupAttempt = 0; $cleanupAttempt -lt 5; $cleanupAttempt++) {
            try { Remove-Item -LiteralPath $stage -Recurse -Force; break }
            catch {
                if ($cleanupAttempt -eq 4) { throw }
                Start-Sleep -Milliseconds (100 * ($cleanupAttempt + 1))
            }
        }
    }
}

param([string]$WasiSdkPath = $env:WASI_SDK_PATH)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (-not $WasiSdkPath) { throw 'Pass -WasiSdkPath pointing to the official wasi-sdk directory.' }
Push-Location "$root/grammar"
try {
    & npm.cmd ci
    if ($LASTEXITCODE) { throw 'npm ci failed' }
    & npm.cmd run generate
    if ($LASTEXITCODE) { throw 'Tree-sitter generation failed' }
} finally { Pop-Location }
& cargo build --manifest-path "$root/Cargo.toml" --locked --target wasm32-wasip2 --release
if ($LASTEXITCODE) { throw 'Rust extension build failed' }
$package = Join-Path $root '../../build/zed-extension/clpp'
New-Item -ItemType Directory -Force "$package/grammars" | Out-Null
Copy-Item -LiteralPath "$root/extension.toml" -Destination "$package/extension.toml"
Copy-Item -LiteralPath "$root/target/wasm32-wasip2/release/clpp_zed.wasm" -Destination "$package/extension.wasm"
Copy-Item -LiteralPath "$root/languages" -Destination $package -Recurse -Force
Copy-Item -LiteralPath "$root/icons" -Destination $package -Recurse -Force
Copy-Item -LiteralPath "$root/icon_themes" -Destination $package -Recurse -Force
& "$WasiSdkPath/bin/clang.exe" -fPIC -shared -Os '-Wl,--export=tree_sitter_clpp' -o "$package/grammars/clpp.wasm" -I "$root/grammar/src" "$root/grammar/src/parser.c" "$root/grammar/src/scanner.c"
if ($LASTEXITCODE) { throw 'Grammar WebAssembly build failed' }
Write-Output "Built extension: $package"

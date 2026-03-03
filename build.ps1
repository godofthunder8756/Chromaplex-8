<# 
.SYNOPSIS
    Build script for Chromaplex 8 Fantasy Game Console
.DESCRIPTION
    Uses w64devkit (portable GCC), SDL2 MinGW, and Lua 5.4.
    All dependencies live in deps\.
.EXAMPLE
    .\build.ps1
    .\build.ps1 -Clean
    .\build.ps1 -Run -Cart "carts\hello.lua"
#>

param(
    [switch]$Clean,
    [switch]$Run,
    [string]$Cart = "carts\hello.lua"
)

$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot
$Build = Join-Path $Root "build"
$Deps  = Join-Path $Root "deps"
$Src   = Join-Path $Root "src"

$GccDir  = Join-Path $Deps "w64devkit\bin"
$SDL2Ver = "2.30.11"
$LuaVer  = "5.4.7"
$SDL2Dir = Join-Path $Deps "SDL2-$SDL2Ver\x86_64-w64-mingw32"
$LuaDir  = Join-Path $Deps "lua-$LuaVer"

Write-Host ""
Write-Host "  ╔═══════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "  ║       CHROMAPLEX 8 — BUILD SYSTEM         ║" -ForegroundColor Cyan
Write-Host "  ╚═══════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# ─── Check GCC ───────────────────────────────────────────────
$gcc = Join-Path $GccDir "gcc.exe"
if (-not (Test-Path $gcc)) {
    Write-Host "[ERROR] GCC not found at $GccDir" -ForegroundColor Red
    Write-Host "  Download w64devkit from:" -ForegroundColor Yellow
    Write-Host "  https://github.com/skeeto/w64devkit/releases" -ForegroundColor Yellow
    Write-Host "  Extract to $Deps\w64devkit\" -ForegroundColor Yellow
    exit 1
}
$env:PATH = "$GccDir;$env:PATH"
Write-Host "[OK] GCC found" -ForegroundColor Green

# ─── Check SDL2 ──────────────────────────────────────────────
$sdlHeader = Join-Path $SDL2Dir "include\SDL2\SDL.h"
if (-not (Test-Path $sdlHeader)) {
    Write-Host "[ERROR] SDL2 not found at $SDL2Dir" -ForegroundColor Red
    Write-Host "  Download SDL2-devel-$SDL2Ver-mingw.tar.gz from:" -ForegroundColor Yellow
    Write-Host "  https://www.libsdl.org/release/" -ForegroundColor Yellow
    exit 1
}
Write-Host "[OK] SDL2 found" -ForegroundColor Green

# ─── Build Lua if needed ─────────────────────────────────────
$luaLib = Join-Path $LuaDir "src\liblua.a"
if (-not (Test-Path $luaLib)) {
    $luaH = Join-Path $LuaDir "src\lua.h"
    if (-not (Test-Path $luaH)) {
        Write-Host "[ERROR] Lua source not found at $LuaDir" -ForegroundColor Red
        Write-Host "  Download from https://www.lua.org/ftp/lua-$LuaVer.tar.gz" -ForegroundColor Yellow
        exit 1
    }
    Write-Host "[BUILD] Compiling Lua $LuaVer..." -ForegroundColor Yellow
    Push-Location (Join-Path $LuaDir "src")
    $luaSrcs = @("lapi","lcode","lctype","ldebug","ldo","ldump","lfunc","lgc","llex",
        "lmem","lobject","lopcodes","lparser","lstate","lstring","ltable","ltm",
        "lundump","lvm","lzio","lauxlib","lbaselib","lcorolib","ldblib",
        "liolib","lmathlib","loadlib","loslib","lstrlib","ltablib","lutf8lib","linit")
    foreach ($s in $luaSrcs) {
        & $gcc -std=c11 -O2 -Wall -DLUA_COMPAT_5_3 -c "$s.c"
    }
    $objs = $luaSrcs | ForEach-Object { "$_.o" }
    & (Join-Path $GccDir "ar.exe") rcs liblua.a @objs
    Pop-Location
    Write-Host "[OK] Lua compiled" -ForegroundColor Green
} else {
    Write-Host "[OK] Lua library found" -ForegroundColor Green
}

# ─── Clean ────────────────────────────────────────────────────
if ($Clean -and (Test-Path $Build)) {
    Remove-Item $Build -Recurse -Force
    Write-Host "[OK] Build directory cleaned" -ForegroundColor Yellow
}

# ─── Build ────────────────────────────────────────────────────
New-Item $Build -ItemType Directory -Force | Out-Null
New-Item (Join-Path $Build "carts") -ItemType Directory -Force | Out-Null

Write-Host "[BUILD] Compiling Chromaplex 8..." -ForegroundColor Yellow

$sources = Get-ChildItem -Path $Src -Filter "*.c" | ForEach-Object { $_.FullName }

& $gcc -std=c11 -O2 -Wall -Wno-unused-result `
    "-I$SDL2Dir\include\SDL2" `
    "-I$LuaDir\src" `
    "-I$Src" `
    @sources `
    "-L$SDL2Dir\lib" `
    "-L$LuaDir\src" `
    -lmingw32 -lSDL2main -lSDL2 -llua -lm `
    -o (Join-Path $Build "chromaplex8.exe")

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed!" -ForegroundColor Red
    exit 1
}

# ─── Copy runtime files ──────────────────────────────────────
Copy-Item (Join-Path $SDL2Dir "bin\SDL2.dll") $Build -Force
Copy-Item (Join-Path $Root "carts\*") (Join-Path $Build "carts") -Force

Write-Host ""
Write-Host "  ╔═══════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "  ║            BUILD SUCCESSFUL!              ║" -ForegroundColor Green
Write-Host "  ╠═══════════════════════════════════════════╣" -ForegroundColor Green
Write-Host "  ║  Output: build\chromaplex8.exe            ║" -ForegroundColor Green
Write-Host "  ║                                           ║" -ForegroundColor Green
Write-Host "  ║  Run:  cd build                           ║" -ForegroundColor Green
Write-Host "  ║        chromaplex8.exe carts\hello.lua     ║" -ForegroundColor Green
Write-Host "  ╚═══════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""

# ─── Run if requested ────────────────────────────────────────
if ($Run) {
    $exe = Join-Path $Build "chromaplex8.exe"
    $cartPath = Join-Path $Build $Cart
    Write-Host "Running: $exe $cartPath" -ForegroundColor Cyan
    & $exe $cartPath
}
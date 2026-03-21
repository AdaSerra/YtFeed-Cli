#!/usr/bin/env pwsh

$srcCpp = @("ytf.cpp", "sqll.cpp", "types.cpp","util.cpp", "console.cpp")
$srcC   = @("./dep/sqlite3.c", "./dep/utf8proc.c")
$includes = "./include"
#$libs = "./lib"
$outBase = "ytf"

# Detect OS
$onWindows = $IsWindows
$onLinux   = $IsLinux
$onMac     = $IsMacOS

Write-Host "Detected platform: $($PSVersionTable.OS)"

if ($onWindows) {
    # -------------------------
    # WINDOWS → MSVC (cl.exe)
    # -------------------------
    $allSrc = $srcCpp + $srcC
    $clFlags = @(
        "/std:c++20",
        "/EHsc",
        "/O2",
        "/GL",
        "/MD",
        "/DUTF8PROC_STATIC", 
        "/D_CRT_SECURE_NO_WARNINGS",
        "/Dssize_t=SSIZE_T",
        "/I$includes",
        "/I."
    )
    $linkFlags = @("/LTCG", "/OPT:REF", "/OPT:ICF")#"/LIBPATH:$libs")
    $outFile = "$outBase.exe"

    Write-Host "Compiling with MSVC..."
    cl $allSrc $clFlags /Fe$outFile /link $linkFlags
    Remove-Item *.obj -ErrorAction SilentlyContinue
}
else {
    # -------------------------
    # LINUX / MAC → g++ or clang++
    # -------------------------
    $cppCompiler = if (Get-Command clang++ -ErrorAction SilentlyContinue) { "clang++" } else { "g++" }
    $cCompiler   = if (Get-Command clang -ErrorAction SilentlyContinue) { "clang" } else { "gcc" }

    $commonFlags = @("-O2", "-Wall", "-Wextra", "-I$includes", "-I.", "-DUTF8PROC_STATIC")
    $cxxFlags = $commonFlags + @("-std=c++20")
    $cFlags   = $commonFlags

    $objs = @()

    Write-Host "Compiling C dependencies with $cCompiler..."
    foreach ($file in $srcC) {
        $obj = $file.Replace(".c", ".o")
        & $cCompiler -c $file -o $obj $cFlags
        if ($LASTEXITCODE -ne 0) { throw "Error compiling $file" }
        $objs += $obj
    }

    Write-Host "Compiling C++ sources with $cppCompiler..."
    foreach ($file in $srcCpp) {
        $obj = $file.Replace(".cpp", ".o")
        & $cppCompiler -c $file -o $obj $cxxFlags
        if ($LASTEXITCODE -ne 0) { throw "Error compiling $file" }
        $objs += $obj
    }

    Write-Host "Linking..."
   
    $ldFlags = @("-L$libs", "-lcurl", "-lpthread", "-ldl", "-lm")
    & $cppCompiler $objs -o $outBase $ldFlags

    Write-Host "Cleaning object files..."
    $objs | ForEach-Object { Remove-Item $_ -ErrorAction SilentlyContinue }
}

if ($LASTEXITCODE -eq 0) {
    $finalName = if ($onWindows) { "$outBase.exe" } else { $outBase }
    Write-Host "Build completed: $finalName" -ForegroundColor Green
} else {
    Write-Host "Build failed with error: $LASTEXITCODE" -ForegroundColor Red
}
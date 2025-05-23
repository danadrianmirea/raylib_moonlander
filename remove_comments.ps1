$files = @("src/game.cpp", "src/game.h", "src/globals.cpp", "src/globals.h", "src/main.cpp")

foreach ($file in $files) {
    $content = Get-Content $file -Raw
    
    # Remove single-line comments but preserve #defines and #includes
    $content = $content -replace "(?<!\#include.*)(?<!\#define.*)//.*", ""
    
    # Clean up empty lines that result from comment removal
    $content = $content -replace "\n\s*\n\s*\n", "`n`n"
    
    Set-Content $file $content
    Write-Host "Processed $file"
}

Write-Host "Done removing comments from all files"

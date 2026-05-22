# Arduino CLI 编译脚本
$arduinoCLI = "C:\Users\liuyong\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$sketchPath = $PSScriptRoot
$board = "esp32:esp32:esp32s3"
$buildPath = Join-Path $sketchPath "build\esp32.esp32.esp32s3"

if (-not (Test-Path $buildPath)) {
    New-Item -ItemType Directory -Path $buildPath -Force | Out-Null
}

Write-Host "编译项目: $sketchPath"
Write-Host "目标板: $board"
Write-Host "构建目录: $buildPath"

# 使用固定构建目录并强制 clean，避免 Windows 临时目录中的对象文件占用问题
& $arduinoCLI compile --fqbn $board --build-path $buildPath --clean --build-property "compiler.cpp.extra_flags=-DBOARD_HAS_PSRAM" $sketchPath

if ($LASTEXITCODE -eq 0) {
    Write-Host "编译成功!" -ForegroundColor Green
} else {
    Write-Host "编译失败!" -ForegroundColor Red
}

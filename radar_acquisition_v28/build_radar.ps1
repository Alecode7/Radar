$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
$sdkRoot=Join-Path $taskRoot 'sdk'
$buildDir=Join-Path $sdkRoot 'project\mmwave\radar_analysis_sleep\cdk_6130_1812_xip_cpus_sleep'
New-Item -ItemType Directory -Force -Path (Join-Path $buildDir 'Obj'),(Join-Path $buildDir 'Lst'),(Join-Path $taskRoot 'build-logs'),(Join-Path $taskRoot 'release') | Out-Null
$toolchain='D:\C-Sky\CDKRepo\Toolchain\XTGccElfNewlib\V3.1.0\R\bin'
$cdkRoot='D:\C-Sky\CDK'
$mingwBin=Join-Path $cdkRoot 'CSKY\MinGW\bin'
$msysBin=Join-Path $cdkRoot 'CSKY\MinGW\msys\1.0\bin'
$env:Path=@($toolchain,$msysBin,$mingwBin,(Join-Path $cdkRoot 'CSKY\FlashProgrammer\Bins'),$cdkRoot,$env:Path) -join ';'
$env:SDK_ROOT_DIR=$sdkRoot
$env:CURRENT_PRJ_PATH=$buildDir
$env:PYTHONDONTWRITEBYTECODE='1'
$env:PYTHONIOENCODING='utf-8'
$pythonExe='D:\MRS6130\RS6x_7x_mmWave_sdk_V2.0.6\Software_Kit\02_SDK\psdf_sdk\tools\python\WPy64-31321b1\python\python.exe'
Push-Location $buildDir
try {
    $ErrorActionPreference='Continue'
    & (Join-Path $mingwBin 'make.exe') -r -B -f (Join-Path $taskRoot 'build-safe.mk') -o Obj/src_hal_board.o -o Obj/utils_version.o -j 16 *> (Join-Path $taskRoot 'build-logs\make.log')
    $makeResult=$LASTEXITCODE
    $ErrorActionPreference='Stop'
    if($makeResult -ne 0) {throw 'Build failed; see build-logs/make.log'}
    & (Join-Path $toolchain 'riscv64-unknown-elf-nm.exe') -S (Join-Path $buildDir 'Obj\psdf.elf') | Out-File -LiteralPath (Join-Path $buildDir 'Obj\psdf.symbols') -Encoding ascii
    if($LASTEXITCODE -ne 0) {throw 'Symbol export failed'}
    $ErrorActionPreference='Continue'
    & $pythonExe -B (Join-Path $taskRoot 'package_safe.py') *> (Join-Path $taskRoot 'build-logs\package-build.log')
    $packageResult=$LASTEXITCODE
    $ErrorActionPreference='Stop'
    if($packageResult -ne 0) {throw 'Packaging failed; see package-build.log'}
    Copy-Item -LiteralPath (Join-Path $buildDir 'output\mrs6130_p1812.img') -Destination (Join-Path $taskRoot 'release\RS6130_V28_IQ_STREAM_20261119.img')
    Write-Output 'V28_IQ_STREAM_BUILD_OK_NOT_HARDWARE_VERIFIED'
} finally {Pop-Location}

param (
    [string]$repoUrl = "git@github.com:LiteLDev/LeviLamina.git",
    [string]$branch = "header"
)

function Test-Command($command) {
    $result = Get-Command $command -ErrorAction SilentlyContinue
    return $null -ne $result
}

function Write-Info($message, [bool]$newLine = $false) {
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    if ($newLine) {
        Write-Host "`n$timestamp - INFO - $message"
    } else {
        Write-Host "$timestamp - INFO - $message"
    }
}

function Get-File($url, $outputPath) {
    $response = Invoke-WebRequest -Uri $url -UseBasicParsing
    if ($response.StatusCode -ne 200) {
        Write-Error "Failed to download $url"
        return
    }

    $totalSize = $response.Headers.'Content-Length'
    $blockSize = 1024
    $downloadedSize = 0
    $startTime = Get-Date

    $stream = [System.IO.File]::OpenWrite($outputPath)
    $buffer = New-Object byte[] $blockSize

    while ($true) {
        $read = $response.RawContentStream.Read($buffer, 0, $blockSize)
        if ($read -le 0) { break }
        $stream.Write($buffer, 0, $read)
        $downloadedSize += $read
        $done = [math]::Round(50 * $downloadedSize / $totalSize)
        $elapsedTime = (Get-Date) - $startTime
        $speed = $downloadedSize / (1024 * $elapsedTime.TotalSeconds)
        Write-Host -NoNewline "`r[$('=' * $done) $(' ' * (50 - $done))] $([math]::Round($downloadedSize / 1MB, 2)) MB / $([math]::Round($totalSize / 1MB, 2)) MB ($([math]::Round($speed, 2)) KB/s)"
    }

    $stream.Close()
    Write-Info "Downloaded $outputPath" $true
}

if (-not (Test-Command 'git')) {
    Write-Error "Git not found, please install Git."
    exit 1
}

if (-not (Test-Command 'xmake')) {
    Write-Error "xmake not found, exiting..."
    exit 1
}

Write-Info "Cloning the repository..."
git clone -b $branch $repoUrl > $null 2>&1
Set-Location -Path "LeviLamina"

Write-Info "Running xmake project..."
xmake project -k compile_commands build > $null 2>&1

$downloads = @(
    @("https://github.com/bdsmodding/Troll/releases/latest/download/Troll.exe", "LeviLamina/Troll.exe"),
    @("https://github.com/LiteLDev/PreLoader/releases/latest/download/PreLoader.dll", "LeviLamina/PreLoader.dll"),
    @("https://github.com/LiteLDev/bedrock-runtime-data/releases/latest/download/bedrock-runtime-data-windows-x64.zip", "LeviLamina/bedrock-runtime-data-windows-x64.zip")
)

foreach ($download in $downloads) {
    Get-File -url $download[0] -outputPath $download[1]
}

Write-Info "Extracting bedrock-runtime-data-windows-x64.zip..."
Expand-Archive -Path "bedrock-runtime-data-windows-x64.zip" -DestinationPath "." -Force
Remove-Item -Path "bedrock-runtime-data-windows-x64.zip"

Write-Info "Running Troll.exe..."
Start-Process -FilePath "Troll.exe" -ArgumentList "build", "test/include_all.h" -Wait

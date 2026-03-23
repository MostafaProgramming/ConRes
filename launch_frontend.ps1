<#
    Build the native simulation, run it once to refresh the replay dataset,
    then start the local frontend server for the dashboard and shared-file API.
#>
powershell -ExecutionPolicy Bypass -File .\build.ps1
.\main.exe
Write-Host ""
Write-Host "ConRes frontend ready at http://localhost:8000" -ForegroundColor Cyan
Write-Host "Press Ctrl+C to stop the local server." -ForegroundColor DarkGray
python .\frontend_server.py

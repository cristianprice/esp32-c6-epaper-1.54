# Windows wrapper: .\scripts\build.ps1 [--idf-path DIR] [options]  (see scripts/build.py)
python "$PSScriptRoot\build.py" @args
exit $LASTEXITCODE

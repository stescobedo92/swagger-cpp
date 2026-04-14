$ErrorActionPreference = "Stop"

& cmd.exe /c "`"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat`" && `"$PSScriptRoot\..\build\default\swaggercpp-benchmarks.exe`" --benchmark_min_time=0.02s"

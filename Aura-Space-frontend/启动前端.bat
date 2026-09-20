@echo off
chcp 65001 >nul
cd /d "%~dp0"

set "BUNDLED_NODE=C:\Users\86136\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe"

where node >nul 2>nul
if %errorlevel%==0 (
  node server.mjs
) else if exist "%BUNDLED_NODE%" (
  "%BUNDLED_NODE%" server.mjs
) else (
  echo [错误] 未找到 Node.js。
  echo 请安装 Node.js 后再运行，或在 Codex 中启动本项目。
  pause
)

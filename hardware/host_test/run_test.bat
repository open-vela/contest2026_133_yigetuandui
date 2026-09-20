@echo off
rem ============================================================
rem  Aura-Space 硬件逻辑宿主仿真测试（Windows 一键运行，无需开发板）
rem  验证内容：手势识别准确率 + 判定/结算/五维存储/Glicko-2 全链路
rem ============================================================
setlocal
cd /d "%~dp0"

set GCC=gcc
where gcc >nul 2>nul
if errorlevel 1 set GCC=C:\mingw64\bin\gcc.exe

set INC=-I../aura_hw -I../aura_hw/engine -Istub
set CORE=../aura_hw/event_bus.c ../aura_hw/imu_service.c ../aura_hw/gesture_recognizer.c ../aura_hw/lsm6ds3.c
set GAME=../aura_hw/game_port.c ../aura_hw/radar_store.c ../aura_hw/glicko2.c ../aura_hw/vib_motor.c ../aura_hw/engine/beatmap_parser.c ../aura_hw/engine/judge_system.c ../aura_hw/engine/cJSON.c

echo [1/2] 编译并运行 手势识别注入测试...
%GCC% -O2 -Wall %INC% host_inject_test.c %CORE% -o host_inject_test.exe -lm
if errorlevel 1 goto :fail
host_inject_test.exe
if errorlevel 1 goto :fail

echo.
echo [2/2] 编译并运行 全链路仿真测试（约 12 秒，模拟一局 8 音符）...
%GCC% -O2 -Wall %INC% host_full_test.c %CORE% %GAME% -o host_full_test.exe -lm
if errorlevel 1 goto :fail
host_full_test.exe
if errorlevel 1 goto :fail

echo.
echo ============ 全部测试通过 ============
goto :end

:fail
echo.
echo ============ 测试失败，请检查上方输出 ============

:end
pause

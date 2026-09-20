#!/bin/sh
# Aura-Space 硬件逻辑宿主仿真测试（Linux/macOS/Git Bash）
# 验证内容：手势识别准确率 + 判定/结算/五维存储/Glicko-2 全链路
set -e
cd "$(dirname "$0")"

INC="-I../aura_hw -I../aura_hw/engine -Istub"
CORE="../aura_hw/event_bus.c ../aura_hw/imu_service.c ../aura_hw/gesture_recognizer.c ../aura_hw/lsm6ds3.c"
GAME="../aura_hw/game_port.c ../aura_hw/radar_store.c ../aura_hw/glicko2.c ../aura_hw/vib_motor.c ../aura_hw/engine/beatmap_parser.c ../aura_hw/engine/judge_system.c ../aura_hw/engine/cJSON.c"

echo "[1/2] 手势识别注入测试（100 组合成波形，目标 >=90%）..."
gcc -O2 -Wall $INC host_inject_test.c $CORE -o host_inject_test -lm
./host_inject_test

echo ""
echo "[2/2] 全链路仿真测试（约 12 秒，模拟一局 8 音符）..."
gcc -O2 -Wall $INC host_full_test.c $CORE $GAME -o host_full_test -lm
./host_full_test

echo ""
echo "============ 全部测试通过 ============"

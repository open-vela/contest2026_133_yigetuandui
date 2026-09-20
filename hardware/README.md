# Aura-Space 黄山派（SF32LB52）硬件代码

本目录是《心流节拍项目方案报告》的板端实现，运行在 openvela（NuttX 内核）上：

- **方案 3.3（体感交互）**：`lsm6ds3.c`（寄存器级 I2C 驱动）、`imu_service.c`（200Hz 采样 + 互补滤波 + 降级）、`gesture_recognizer.c`（滑窗 + 双阈值 + FSM + 误触过滤）、`vib_motor.c`（5 模式 PWM 振动 + 优先级抢占）
- **方案 3.4（系统架构）**：`event_bus.c`（静态订阅表）、`radar_store.c`（五维模型 + KV/文件持久化）、`imu_service.c` 内的故障检测与安全模式
- **方案 3.1（游戏引擎）**：`engine/` 从 `Aura_Space_Backend` 原样同步（谱面解析 + 判定 + 计分），`game_port.c` 负责把手势事件接进判定系统并补齐 EventBus 广播（后端代码里的两个 TODO）
- `glicko2.c`：方案承诺但此前缺失的 Glicko-2 动态难度（固定 σ 变式）

## 目录结构

```
hardware/
├── README.md                 ← 本文件
└── aura_hw/                  ← openvela 应用（放入 openvela apps/ 下使用）
    ├── Kconfig / Make.defs / Makefile
    ├── aura_main.c           ← NSH 入口：demo / imu / gesture / vib / inject
    ├── event_bus.[ch]
    ├── lsm6ds3.[ch]
    ├── imu_service.[ch]
    ├── gesture_recognizer.[ch]
    ├── vib_motor.[ch]
    ├── radar_store.[ch]
    ├── glicko2.[ch]
    ├── game_port.[ch]
    └── engine/               ← 后端 C 引擎同步副本（改动须同步回后端仓库）
        ├── game_engine.h / beatmap_parser.c / judge_system.c / cJSON.[ch]
```

## 前置条件

1. openvela 源码的 `nuttx` 与 `vendor_sifli` 都必须切到 **`dev-ai-contest-2026`** 分支（trunk/dev 缺少芯片层依赖，无法编译）：
   https://github.com/open-vela/vendor_sifli/blob/dev-ai-contest-2026/boards/sf32lb52/lckfb_huangshan_pi/README_zh-cn.md
2. 工具链：arm-none-eabi-gcc ≥ 10.3、cmake ≥ 3.22、ninja ≥ 1.10、python3 ≥ 3.10
3. 在 `lckfb_huangshan_pi/configs/nsh` defconfig 基础上确认/追加：
   - `CONFIG_I2C_DRIVER=y`（I2C 字符设备，`/dev/i2cN`）
   - `CONFIG_PWM=y` + 马达对应 PWM 通道使能（`/dev/pwmN`）
   - `CONFIG_PTHREAD=y`（openvela 默认一般已开）
   - `CONFIG_LIBC_FLOATINGPOINT=y`（printf 支持 %f）
   - `CONFIG_KVDB=y`（可选；不开则五维数据降级写 `/data/aura_radar.json`）
4. 两个硬件参数的默认值**已按 vendor 板级源码核实**（`sifli_ap.c`）：
   - `EXAMPLES_AURA_HW_IMU_PATH` 默认 `/dev/i2c1`：黄山派 IMU 接线为 PA39=I2C2 SDA、PA40=I2C2 SCL，I2C2 注册为 `/dev/i2c1`（触摸 I2C1 是 `/dev/i2c0`）；传感器 LDO（PA30）与 pinmux 由板级初始化完成
   - `EXAMPLES_AURA_HW_PWM_PATH` 默认 `/dev/pwm0`：板级当前只注册该节点（映射 GPTIM1，引脚 PA32=RGB LED）；**马达驱动引脚 vendor 树中未配置，需按立创原理图确认后补 pinmux**（见"已知待办"）

## 集成步骤

```bash
# 1. 放入 openvela apps 树
cp -r aura_hw <openvela>/apps/

# 2. 在 <openvela>/apps/Kconfig 末尾追加一行：
#    source "$APPSDIR/aura_hw/Kconfig"

# 3. 配置并编译（沿用黄山派官方构建方式）
cmake -B cmake_out/lckfb_huangshan_pi -S "$PWD/nuttx" -GNinja \
  -DBOARD_CONFIG=../vendor/sifli/boards/sf32lb52/lckfb_huangshan_pi/configs/nsh \
  -DEXTRA_FLAGS="-Wno-cpp -Wno-deprecated-declarations"
# menuconfig 中启用 EXAMPLES_AURA_HW 后：
cmake --build cmake_out/lckfb_huangshan_pi

# 4. 烧录 cmake_out/lckfb_huangshan_pi/nuttx.bin 到内置 NOR 偏移 0x12010000
#    （工具见立创黄山派 Wiki / SiFli 烧录工具）
```

## 零依赖宿主仿真（无开发板也能验证）

`host_test/` 内的测试把板端代码中的硬件无关部分（手势识别、判定引擎、五维存储、Glicko-2）直接编译成 PC 可执行文件，NuttX 头文件用 `stub/` 里的桩替代：

```bash
# Windows：双击 run_test.bat；Linux/macOS/Git Bash：
sh run_test.sh
```

预期输出：

```
[1/2] 手势识别注入测试（100 组合成波形，目标 >=90%）
up 34/34  left 33/33  right 33/33
total accuracy 100/100 = 100.0%
[2/2] 全链路仿真测试（约 12 秒）
[judge] note#0 -> Perfect (delta=20) ... note#7 -> Miss
[result] score=45 combo=3 ... | glicko2 mu=1522 next_density=3
============ 全部测试通过 ============
```

## NSH 验证流程（建议按序执行）

| 命令 | 验证内容 | 对应验收标准 |
|---|---|---|
| `aura imu` | 10Hz 打印融合后 IMU 数据，甩腕观察数值范围 | 200Hz 采样稳定 |
| `aura vib 4` | 播放模式 4（长振），依次试 1~5 | 五种振动模式时序正确 |
| `aura gesture` | 打印手势事件与置信度，每 10s 打印采样抖动 | 抖动均值 <0.5ms / 最大 <2ms |
| `aura inject` | 注入 100 组合成波形，输出分类型与总准确率 | **识别准确率 ≥90%**（无需真机手势，可在模拟器跑） |
| `aura demo` | 全链路：IMU→手势→判定→振动→五维存储，每局打印结算与 Glicko-2 评分 | 手势→判定→反馈端到端打通 |

## 与前后端契约（docs/INTEGRATION.md）的对应关系

| 契约（浏览器侧） | 板端实现 |
|---|---|
| `gesture:detected {type, timestamp, confidence}` | `AURA_EVENT_GESTURE` / `gesture_event_t`（type 取值 0/1/2 与引擎 NoteType 一致） |
| `game:judgement {noteId, result, deltaMs}` | `AURA_EVENT_JUDGE` / `aura_judge_info_t` |
| `haptic:request {pattern}` | `vib_request(pattern, prio)` |
| `POST /api/game_result` | `AURA_EVENT_GAME_RESULT` → `radar_store_apply_game_result()` |
| `GET /api/dashboard` 五维 | `radar_store_get()` |

浏览器前端继续用于交互原型与 AI 看板演示；真机 UI 需按黄山派 nsh defconfig 自带的 LVGL 重写渲染层（PlatformAdapter / FocusEngine 两层与 DOM 无关，可移植）。

## 已知待办（按优先级）

1. **判定窗口定稿**：后端 `WINDOW_PERFECT/GOOD`（100/500ms）与方案 1.2/2.1（±50/±100ms）不一致，全队定稿后改 `engine/game_engine.h` 两行宏并同步回后端仓库——详见根目录《软件部分合理性评估.md》
2. 手势阈值（`gesture_recognizer.c` 顶部宏）需按真机采集波形标定：先跑 `aura gesture` 采集，再调参
3. **马达 pinmux**：vendor 树中马达驱动引脚未配置（PWM2=GPTIM1 映射到 PA32=RGB LED），需按立创原理图确认马达引脚后在 `bsp_pinmux.c` 补一行 `HAL_PIN_Set` 并把 `EXAMPLES_AURA_HW_PWM_PATH` 指向对应节点
4. LVGL UI、音频解码（板载 Class-D PA + I2S）、BLE 广播同步、N-Back C 状态机尚未实现
5. 备选：板级已启用 NuttX `lsm6dsl` 内核驱动（`/dev/lsm6dsl0`），但其上报的角速度是 int16 mdps 会溢出（>±32°/s 即回绕），不满足甩腕手势量程，故本项目坚持直调 I2C 方案；该内核驱动保持被动未打开状态，不冲突

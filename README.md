# Aura-Space 心流节拍

> 基于律动音游的注意力训练系统 —— 北京邮电大学《智能嵌入式系统综合挑战实践 II》项目（第三组）
> 运行于 openvela 实时操作系统 · 立创·黄山派 SF32LB52 开发板

[![Platform](https://img.shields.io/badge/platform-openvela%20%2B%20SF32LB52-blue)]()
[![UI](https://img.shields.io/badge/UI-LVGL%209.2-green)]()
[![License](https://img.shields.io/badge/license-Apache--2.0-lightgrey)]()

## 一、项目简介

Aura-Space 是面向碎片化场景的智能手表认知增强与专注力训练工具，将**体感音游、工作记忆训练、呼吸引导与 AI 数据分析**有机结合，形成"动作输入 → 游戏反馈 → 数据分析 → 个性化训练"的完整闭环。

系统由三部分组成：

| 组成 | 目录 | 说明 |
|---|---|---|
| 板端固件 | `app/aura_hw/` | openvela/NuttX 应用，四大模块全部运行于黄山派开发板 |
| 前端 | `Aura-Space-frontend/` | 浏览器版交互原型（纯静态，Mock/API 双模），用于 UI 评审与契约定义 |
| 后端 | `Aura_Space_Backend/` | 可移植 C 游戏引擎（已直接编译进固件）+ FastAPI 开发桩 |

## 二、功能特性

### 四大训练模块（手表端，`aura app` 启动）

| 模块 | 功能 | 输入方式 |
|---|---|---|
| **BeatFlicks** 体感音游 | 三轨道音符下落判定（Perfect/Good/Miss）、连击/评级、Glicko-2 动态难度、节拍音乐引擎（音频缺失时降级为边框节拍脉冲） | 甩腕手势 + 触屏点按轨道（双输入） |
| **Synapse-N** 记忆矩阵 | Dual N-Back 2×2 色点训练，30 轮一局，正确率驱动 N 值自适应（1~4），成绩写入五维 | 触屏 MATCH 作答 |
| **Aura-Focus** 心流呼吸 | 吸气 4s / 呼气 6s 圆环引导动画，4 分钟计时，心流时长写入五维 | 触屏 START |
| **AI Coach** 数据教练 | 五维雷达条形图、Focus Score、本地规则引擎生成训练建议（云端 AI 的降级路径） | 触屏查看 |

### 底层能力

- **IMU 体感链路**：LSM6DS3TR-C 六轴 IMU 208Hz 采样（寄存器级 I2C 驱动），零偏高通 + 加速度低通 + 互补滤波（α=0.98），滑动窗口手势识别（上抛/左甩/右甩），注入测试准确率 100%（验收线 ≥90%）
- **判定引擎**：JSON 谱面解析（cJSON）、三档时间窗判定、连击倍率、S~D 评级
- **数据层**：五维雷达数据模型（反应/专注/记忆/压力/心流），EMA 聚合，KV/文件双后端持久化
- **Glicko-2 动态难度**：固定 σ 变式，输出 1~5 档谱面密度
- **故障韧性**：I2C 连续失败自动降级 100Hz → 安全模式；AI 云端不可用降级本地规则
- **交互**：390×450 AMOLED + LVGL 9.2 界面，FT6146 触屏，**整屏右滑返回主页**

## 三、目录结构

```
contest2026_133_yigetuandui/
├── README.md                    # 本文件
├── 软件使用说明.md               # 终端用户使用手册
├── 软件部分合理性评估.md          # 软件评审报告（问题清单与优先级）
├── 心流节拍项目方案报告 第三组(1).docx
├── contest2026_133_yigetuandui.xml  # 队伍 manifest（linkfile 映射见下）
│
├── app/
│   └── aura_hw/                 # 板端固件（核心交付，见下）
│                                #   经 manifest linkfile 映射为
│                                #   packages/demos/aura_hw
│
├── Aura-Space-frontend/         # 浏览器交互原型（node server.mjs → :4173）
│   ├── src/                     #   模块：beatflicks/synapse-n/focus-engine/event-bus
│   ├── docs/INTEGRATION.md      #   前后端接口契约
│   └── scripts/smoke.mjs        #   npm run check 自检
│
├── Aura_Space_Backend/          # 后端
│   ├── src/                     #   C 游戏引擎（谱面解析/判定/计分）+ api/server.py
│   └── inc/                     #   game_engine.h / cJSON.h
│
└── hardware/                    # 固件配套文档与无板测试
    ├── README.md                #   固件集成与构建说明
    ├── 真机调测指南.md           #   连接/烧录/调测/排查（附录 A：构建补丁全集）
    └── host_test/               #   无板仿真测试（run_test.bat / run_test.sh）

app/aura_hw/
├── aura_main.c                  # NSH 入口：app/demo/imu/gesture/calib/vib/inject
├── aura_app.c                   # 应用外壳：主屏 + 四模块导航 + 右滑返回
├── aura_ui.c                    # BeatFlicks 模块（LVGL + 双输入 + 音乐钩子）
├── nback.c / focus.c / coach.c  # 其余三模块
├── music.c                      # 节拍音乐合成器（PCM 16kHz，后端可插拔）
├── lsm6ds3.c / imu_service.c    # IMU 驱动与 200Hz 采样服务（滤波/降级/注入）
├── gesture_recognizer.c         # 滑窗 + 峰值/均值双阈值 + 主方向判定 + 误触过滤
├── game_port.c                  # 手势→判定→反馈→结算 胶水层
├── vib_motor.c                  # PWM 振动 5 模式 + 优先级抢占（马达 pinmux 待原理图）
├── radar_store.c                # 五维数据 + KV/文件持久化
├── glicko2.c                    # Glicko-2 动态难度
├── event_bus.c                  # C 版 EventBus（静态订阅表）
└── engine/                      # 从后端同步的可移植 C 引擎（含 cJSON）
```

## 四、快速开始

### 4.1 环境要求

| 用途 | 环境 |
|---|---|
| 前端原型 | Node.js ≥ 16（纯静态，无依赖安装） |
| 后端 Mock | Python ≥ 3.10 + `pip install fastapi uvicorn` |
| 固件编译 | WSL2 Ubuntu 22.04：arm-none-eabi-gcc ≥10.3 / cmake ≥3.22 / ninja / python3.10（+ pip: kconfiglib pyelftools cxxfilt） |
| 烧录 | Windows：sftool 0.2.5（OpenSiFli/sftool releases）+ CH340 串口驱动 |

> ⚠️ 国内网络注意：openvela 源码用 gitee 镜像（`gitee.com/open-vela/*`），GitHub release 资产走 `api.github.com` octet-stream 下载。全部已验证的补丁与镜像方案见 `hardware/真机调测指南.md` 附录 A。

### 4.2 前端原型（30 秒）

```bash
cd Aura-Space-frontend
node server.mjs          # → http://127.0.0.1:4173
npm run check            # 自检（语法+静态资源+核心模块）
```

键盘 `A/W/D` 模拟左甩/上抛/右甩；`M` 键切换 mock/api 数据模式。

### 4.3 后端（可选，开发桩）

```bash
cd Aura_Space_Backend
# C 引擎 Mock 测试（谱面→判定→结算）：由 src/ 现场编译
gcc src/main.c src/beatmap_parser.c src/judge_system.c src/cJSON.c -Iinc -lm -o engine_test
./engine_test
pip install fastapi uvicorn
uvicorn api.server:app --port 8000   # API 桩（/api/beatmap /api/game_result ...）
```

### 4.4 固件编译与烧录

完整步骤见 `hardware/真机调测指南.md`，核心命令：

```bash
# ① 集成：代码已通过本仓 manifest 的 linkfile 自动映射到
#    packages/demos/aura_hw，无需手动复制（生产仓库零改动）
#    映射条目见 contest2026_133_yigetuandui.xml：
#    <linkfile src="app/aura_hw" dest="packages/demos/aura_hw"/>

# ② 编译（BOARD_CONFIG 必须 dev-ai-contest-2026 分支）
cmake -B cmake_out/lckfb_huangshan_pi -S "$PWD/nuttx" -GNinja \
  -DBOARD_CONFIG=../vendor/sifli/boards/sf32lb52/lckfb_huangshan_pi/configs/nsh \
  -DEXTRA_FLAGS="-Wno-cpp -Wno-deprecated-declarations -fpermissive"
cmake --build cmake_out/lckfb_huangshan_pi     # 产物 nuttx.bin

# ③ 烧录（板子 Type-C 连电脑，COMx 以设备管理器为准）
sftool -c SF32LB52 -p COM6 -b 1000000 --before default_reset --after soft_reset \
       write_flash nuttx.bin@0x12010000

# ④ 串口控制台：PuTTY 打开 COMx @ 1000000（注意：不是 115200）
nsh> aura app
```

### 4.5 无板验证

```bash
cd hardware/host_test
sh run_test.sh     # 或 Windows 双击 run_test.bat
# 预期：手势注入 100/100 = 100%；全链路判定序列正确；FULL-CHAIN TEST PASS
```

## 五、验证命令对照表（真机）

| NSH 命令 | 用途 | 通过标准 |
|---|---|---|
| `aura app` | 四模块主应用 | 主屏四入口，右滑可返回 |
| `aura imu` | IMU 数据 10Hz 打印 | 甩腕 gyro 显著变化，accel 模长 ≈1g |
| `aura gesture` | 手势事件实时打印 | 三种手势可识别，附 10s 抖动统计 |
| `aura calib` | 250ms 窗口峰值打印 | 用于阈值标定（采真实波形） |
| `aura inject` | 100 组合成波形注入 | 总准确率 ≥90%（当前 100%） |
| `aura vib <1-5>` | 振动模式 | 待马达 pinmux（见已知限制） |
| `aura demo` | 控制台全链路 | 判定序列 + 结算 + Glicko-2 评分 |

## 六、文档索引

| 文档 | 内容 |
|---|---|
| `软件使用说明.md` | 终端用户操作手册（各模块玩法、右滑导航、FAQ） |
| `hardware/README.md` | 固件结构、集成步骤、前后端契约对照 |
| `hardware/真机调测指南.md` | 连接/驱动/编译/烧录/NSH 调测/排查表 + 附录 A 构建补丁全集 |
| `软件部分合理性评估.md` | 前后端代码评审（判定窗口等 P0 问题清单） |
| `Aura-Space-frontend/docs/INTEGRATION.md` | 前后端接口契约 v1 |
| `交接说明.md` | 前端 Demo 边界与联调入口 |

## 七、已知限制与路线图

| 项 | 状态 | 说明 |
|---|---|---|
| 振动马达 | ⚠️ 待办 | 马达驱动引脚在 vendor 树中未配置，需按立创原理图补一行 pinmux，代码已就绪（`vib_motor.c` 五种模式） |
| 音乐放音 | ⚠️ 待办 | 板载 audcodec 无 openvela 驱动（需移植）且喇叭为外接件；节拍合成引擎已完成，驱动就绪即插即用，当前降级为屏幕边框节拍脉冲 |
| 中文界面 | 待办 | montserrat 无中文字形，当前全英文 UI，需编译 CJK 字体 |
| 手势阈值 | 持续标定 | 已按真机实测下调（120/80/120 dps），`aura calib` 可继续采集个体数据 |
| 判定窗口统一 | 待后端定稿 | 后端引擎 ±100/±500ms 与方案 ±50/±100ms 未定稿（见评估报告 P0） |
| BLE 跨端同步 | 路线图 | 五维摘要广播（7 字节压缩包 + CRC8）方案已设计 |
| AI 云端对接 | 路线图 | 本地规则引擎已实现降级路径 |

## 八、团队

北京邮电大学 未来学院 · 计算机专业（第三组）

| 成员 | 分工 |
|---|---|
| 詹诗涵 | 体感交互：IMU 驱动、手势识别、振动反馈 |
| 牛睿涵 | 游戏引擎：谱面/判定/连击/自适应难度 |
| 秦家依 | 前端与 AI：UI 渲染、数据分析、训练建议 |
| 隆彦洁 | 系统架构：EventBus、数据层、BLE、低功耗、集成测试 |

指导教师：修佳鹏、刘健培、寇菲菲

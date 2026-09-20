# Aura-Space 心流节拍前端（秦嘉一模块）

这是一个可独立运行、零第三方依赖的前端 Demo，覆盖项目材料中分配给秦嘉一的核心内容：

- BeatFlicks：Canvas 三轨音符渲染、对象池、绝对时间驱动、Perfect/Good/Miss 反馈；
- Synapse-N：2×2 四象限、150/500/200ms 刺激动画、视觉与振动反馈；
- Aura-Focus：4/6 呼吸节奏动画和四分钟训练计时；
- AI 数据教练：五维个体基线归一化、动态目标权重、正向训练建议、对话界面；
- 后台任务策略：训练 200Hz、亮屏 50Hz、后台 10Hz、低电量 1Hz/30s 的状态模型；
- 团队对接：Mock/API 一键切换、EventBus、统一数据适配层和接口契约。

## 运行

电脑已经安装 Node.js 时，在本目录执行：

```powershell
npm start
```

如果 `npm` 不在环境变量中，也可以直接执行：

```powershell
node server.mjs
```

浏览器打开 `http://127.0.0.1:4173`。项目不需要 `npm install`，不会联网下载依赖。

自检：

```powershell
npm run check
```

## 演示顺序

1. 首页说明五维专注分、训练入口和一周趋势；
2. 进入 BeatFlicks，用 `A / W / D` 或页面按钮模拟左甩、上抛、右甩；
3. 进入 Synapse-N，观察两个刺激后判断“位置/颜色/都相同/都不同”；
4. 进入 Aura-Focus，启动 4/6 呼吸动画；
5. 进入 AI 教练，点击“今天怎么练”“提升记忆”或输入问题；
6. 在桌面左侧将数据模式从 Mock 切到真实 API，说明后端接入只替换适配层。

## 模块边界

当前代码中的谱面与 N-Back 状态只属于“前端联调 Mock”，不是代替牛睿涵的正式后端实现。正式联调时：

- 牛睿涵提供谱面、判定、分数和 N-Back 状态；
- 詹诗涵、隆彦洁提供 IMU 手势事件、振动能力、KV/BLE 数据；
- 秦嘉一模块继续负责视图、动画、交互反馈、专注分析和 AI 助手；
- `src/services/platform-adapter.js` 由 Mock 切换为 API，不改页面代码。

接口详见 [docs/INTEGRATION.md](./docs/INTEGRATION.md)。

## openvela 迁移说明

本工程首先用于浏览器验收和无硬件演示。openvela 官方提供模拟器、原生应用示例和快应用开发路径，后续可按团队最终选定的真机 UI 技术栈迁移表现层；核心算法、接口契约、状态机和视觉令牌可直接复用。

- openvela 官方文档：https://doc.openvela.com/
- openvela 官方仓库：https://github.com/open-vela
- AI Agent 框架仓库：https://github.com/open-vela/packages_ai_agent

注意：浏览器 Demo 本身不是可直接烧录到 SF32LB52 的固件，也不是 RPK 包；硬件工程、开发板 SDK 与最终 UI Runtime 确定后，再实现 `PlatformAdapter` 的设备版本。

## 目录

```text
aura-space-frontend/
├─ index.html                    # 设备模拟器与项目调试面板
├─ server.mjs                   # 零依赖静态服务器
├─ src/
│  ├─ app.js                    # 页面路由与模块组装
│  ├─ styles.css                # UI 视觉系统与动效
│  ├─ core/event-bus.js         # 松耦合事件总线
│  ├─ modules/beatflicks.js     # 音游 Canvas 渲染器
│  ├─ modules/synapse-n.js      # N-Back 前端交互控制器
│  └─ services/
│     ├─ focus-engine.js        # 五维分析与建议规则引擎
│     └─ platform-adapter.js    # Mock / API / 后台采样适配层
├─ docs/INTEGRATION.md          # 团队接口契约
└─ scripts/smoke.mjs            # 最小自动化自检
```

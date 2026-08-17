# Aura-Space 心流节拍（秦家依前端模块）

本目录是 2026 openvela AI 硬件开发者大赛「手表应用创新赛道」的 Vela 快应用源码，映射到 `packages/apps/contest2026_133_aura_space`。

## 已完成内容

- 腕上首页：今日专注分、反应/记忆指标、四个训练入口；
- BeatFlicks：三方向节奏交互、分数、连击、Perfect/Good/Miss 反馈；
- Synapse-N：2-Back 刺激矩阵、相同/不同判断、正确率与振动语义；
- Aura-Focus：4 秒吸气/6 秒呼气、四分钟倒计时、开始/暂停/重置；
- AI 数据教练：通过官方 `@system.velaclaw` 调用端侧 AI，失败时自动使用本地规则建议；
- 手表 UI 优化：466 设计宽度、圆/方屏安全边距、大触控区、深色低功耗视觉。

负责人：**秦家依（前端、UI/UX 优化、AI 助手交互）**。

## 运行与验证

1. 使用 AIoT-IDE 打开本目录；
2. 创建并选择 `vela-watch-5` 模拟器；
3. 点击「调试」验证完整交互；
4. 参赛提交时点击「发布」，生成 `dist/*.release.rpk`；
5. 在 openvela 模拟器中运行时，先启动 `ai_agent &`，再执行：

```text
vapp hap://app/com.openvela.contest2026.team133.auraspace
```

无需 AIoT-IDE 的源码自检：

```text
npm run check
```

> 生产模式 `release.rpk` 需要在安装 AIoT-IDE 后生成。本仓已提交全部源码；生成后应将产物一并放入本目录的 `dist/`。

## 团队接口边界

当前版本内置 Mock 状态，可在后端和硬件尚未接入时独立演示。整合阶段只需替换数据/事件适配层：

- 牛睿涵：提供正式谱面、判定、分数和 N-Back 状态；
- 詹诗涵、隆彦洁：提供 IMU 手势、振动与设备数据；
- 秦家依：维护快应用页面、视觉动效、交互反馈、五维分析和 AI 教练。

具体字段见 [docs/INTEGRATION.md](docs/INTEGRATION.md)。

## AI Agent 配置

快应用调用 `@system.velaclaw`。openvela 构建配置需开启：

```text
CONFIG_EXAMPLES_AI_AGENT_VELA=y
CONFIG_FEATURE_SYSTEM_VELACLAW=y
CONFIG_MQ_MAXMSGSIZE=4096
```

所有密钥只在设备/模拟器端通过 `set_llm` 配置，源码中不保存 API Key。

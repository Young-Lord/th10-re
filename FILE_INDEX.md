# 文件索引与维护说明（TH10 RE）

本文件是 `th10-re` 仓库的文件地图与维护约定。它回答两个问题：

1. 每个文件/目录负责什么；
2. 什么时候、由谁、按什么规则更新它。

维护目标：任何一次改动后，`config/` 与 `docs/evidence/` 都能反映源码的真实状态，
不存在只存在于某人工作区、或者互相矛盾的口径。

## 0. 权威优先级

出现冲突时，按下面的顺序裁决，序号小的覆盖序号大的：

1. 目标二进制 `resources/th10.exe`（唯一事实来源，SHA-256 锁定在 `config/target.toml`）。
2. `config/function-status.csv` 与 `config/crt-lib-map.csv`（可审计账本与库边界）。
3. `src/` 里的语义 C++（逻辑与副作用的实际实现）。
4. `docs/evidence/` 里的逐函数证据。
5. `TH10_PROGRESS_METRIC.md` / `TH10_RE_METHODOLOGY.md`（口径与方法）。
6. `HANDOUT.md`（历史交接记录，仅作线索，不作为状态来源）。

规则：**任何"进度数字"必须以 2 为准，任何"当前状态"不得引用 6。**

## 1. 顶层文件

| 文件 | 作用 | 维护时机 | 备注 |
| --- | --- | --- | --- |
| `.gitignore` | 排除 `resources/*`、`build/`、`*.obj/*.exe/*.map`、Ghidra 工程 | 新增本地/生成物类型时 | 原版 EXE 与工具绝不能入库 |
| `TH10_PROGRESS_METRIC.md` | 完成度度量口径与当前基线快照 | 每次重要提交后刷新快照 | 权威进度文档，报告必须注明分析器 |
| `TH10_RE_METHODOLOGY.md` | 方法论：原则、验证闭环、工具链、G0–G7 门槛 | 流程本身变化时 | 立项期章节（仓建/队列）已成历史 |
| `HANDOUT.md` | 最早的交接说明 + 8/28–9/5 逐日 session log | 仅追加历史线索 | **不是状态来源**，见第 7 节遗留项 |
| `FILE_INDEX.md` | 本文件：文件地图与维护约定 | 目录结构或约定变化时 | — |
| `objdiff.yml` | objdiff 比较单元清单（src→obj） | **由 `scripts/objdiff-baseline.sh` 重新生成** | 不要手改 |
| `reccmp-project.yml` | reccmp 目标配置（哈希/编码/数据源） | 新增数据源 CSV 时 | 引用的 `config/reccmp-*.csv` 目前仅表头 |
| `config/` `docs/` `scripts/` `src/` | 见下文分节 | — | — |
| `references/` `resources/` `build/` | 外部参考 / 本地输入 / 生成物 | — | 见第 6 节，均不应入库（`references/*` 各自带 `.git`） |

## 2. `config/` — 可审计账本与元数据

| 文件 | 作用 | 维护时机 | 生成方式 |
| --- | --- | --- | --- |
| `target.toml` | 锁定目标 EXE 的文件名/哈希/大小/基址/入口 | 目标变更时（几乎不变） | 手工 |
| `function-status.csv` | **重建账本**：`address,name,object,status,match_percent,owner,evidence,notes`；当前 626 行 | 每实现/改名一个函数 | 手工 + 证据文档同步 |
| `crt-lib-map.csv` | 库/游戏/thunk 边界，2108 行 `address,size,kind,name` | 重新导入/加 FLIRT 签名后 | IDA + FLIRT 导出 |
| `ghidra_ns_to_obj.csv` | Ghidra 命名空间 → 稳定对象名映射 | 新增比较对象组时 | 手工 |
| `timeline-opcode-jumptable.csv` | 时间线 opcode 跳转表（`address,handler`） | 跳转表被重新确认时 | Ghidra MCP 导出 |
| `reccmp-functions/globals/strings/floats.csv` | reccmp 数据面输入 | **待填充（当前仅表头，数据面≈0）** | Ghidra/IDA 导出 |

`function-status.csv` 的 `status` 取值（强度递增）：
`unknown` → `typed` → `stubbed` → `implemented` → `object-matched` → `matched`，
另有 `boundary`（原生 ABI 薄 thunk，逻辑在语义体内）。**不得因为函数有名字就标
`implemented`。**

## 3. `docs/` — 证据与覆盖率

| 路径 | 作用 | 维护时机 | 备注 |
| --- | --- | --- | --- |
| `docs/evidence/*.md` | **逐函数/逐簇证据**（157 篇）：地址、ABI、布局、原生怪异行为、边界 | 每次重建后同步新增/更新 | 与 `function-status.csv` 的 `evidence` 列一一对应 |
| `docs/coverage-gap-shortlist.md` | 覆盖率缺口排查的收尾记录（9/6 声明完成） | 缺口数字变化时 | 其中 592 已被 metric 的 596 刷新 |
| `docs/coverage-gap.csv` | 上述排查的 136 行明细快照 | 一般只作历史 | — |

evidence 文档按子系统前缀成组，便于检索：

| 前缀 | 篇数 | 主题 |
| --- | --- | --- |
| `ascii-*` | 15 | ASCII 字形/HUD/overlay/投影/场景遍历 |
| `main-chain-*` | 19 | 主链生命周期、线程、渲染、输入、注册钩子 |
| `player-*` | 11 | 玩家移动/射击/道具/死亡/近接淡出 |
| `title-*` | 10 | 标题画面演算/绘制/状态构造与析构 |
| `ecl-*` | 9 | ECL 脚本库/VM/名称表/选择菜单 |
| `manager-*` | 7 | 管理器创建/释放/工作推进 |
| `global-*` | 7 | 全局生命周期与回调 |
| `replay-*` | 7 | 回放录制/保存/视图加载 |
| `score-*` | 6 | 分数文件与分数画面 |
| `stage-*` | 5 | 关卡宿主/对象 vtable/文本特效 |
| `ending-*` | 4 | 结局 MIDI/overlay |
| `result-screen-*` | 4 | 结算画面 |
| `transition-*` | 4 | 转场音效适配器/生命周期 |
| `generated-*` | 3 | 生成字体表/表面 blit |
| `registration-*` | 3 | 注册/绘制所有者与时序 |
| `scene-trigger-*` | 3 | 场景触发器 |
| 其余单篇/双篇 | 42 | `bgm-*`、`pause-*`、`hint-*`、`entity-*`、`vm-leaf-helpers`、`zunmath`、`vendor-libraries`、`target-baseline` 等 |

## 4. `scripts/` — 构建、导出与验证

| 文件 | 作用 | 维护时机 | 备注 |
| --- | --- | --- | --- |
| `compile-main-chain-cpp.sh` | 逐文件 MSVC 严格构建，产出 `build/cpp/*.obj` | 新增 `.cpp` 模块时 | 需显式登记文件，**不要用 glob 决定顺序** |
| `objdiff-baseline.sh` | 生成 objdiff 基线：编译 → `build/ours` + `build/reference` → 重写 `objdiff.yml` | 每次新增模块后 | 新对比流程入口 |
| `extract_reference_functions.py` | 从原版 EXE 抽取逐函数参考反汇编/字节到 `build/reference/` | 由上一脚本调用 | — |
| `verify-target.sh` | 校验 `resources/th10.exe` 哈希/大小/PE 字段 | 怀疑目标被替换时 | 会比对 `config/target.toml` |
| `ghidra/ExportTh10Delinker.java` | 合成重定位并导出可重定位 COFF | 对象分组变化时 | 用法：`<mapping.csv> <out-dir>` |

第一代 `compare-*.sh` 已删除，被 `objdiff-baseline.sh` 取代；`config/mapping.csv`、
`config/implemented.csv` 两个立项期 stub 同日删除。

## 5. `src/` — 语义 C++ 重建

约定：

- 每个重建簇一个 `.cpp/.hpp` 对，文件名用 `PascalCase` 描述该簇（如
  `EclScriptVm.cpp`、`TitleScreenDrawPasses.cpp`）。
- 原生寄存器 ABI 只留在 `.asm` 薄 thunk 边界，语义体使用正常函数签名。
- 每个函数体带 `// FUNCTION: TH10 0x004xxxxx` 注释，与账本地址对应。
- 定宽类型、Win32/D3D9 声明集中在 `Th10Types.hpp` / `Th10Platform.hpp`。

模块族（按前缀，共 328 个文件）：

| 族 | 代表文件 | 负责 |
| --- | --- | --- |
| `Ascii*` | `AsciiHudRenderer`、`AsciiOverlayCallbacks`、`AsciiSceneObjectRenderer` | ASCII 字形/HUD/overlay/投影渲染管线 |
| `MainChain*` | `MainChainContext`、`MainChainResourceThread`、`MainChain*Thread` | 主链状态机、线程、渲染适配、输入、关停 |
| `Timeline*` | `TimelineRecordInterpreter`、`TimelineRenderObjects`、`TimelineAudioActions` | 时间线 opcode 解释器与对象/音频/文本动作 |
| `Ecl*` | `EclScriptVm`、`EclScriptLibrary`、`EclSelectMenu` | ECL 脚本对象、VM、选择菜单 |
| `Global*` / `LargeRenderOwner*` | `GlobalLifecycleManager`、`LargeRenderOwnerCallbacks` | 全局生命周期、大渲染所有者 |
| `Player*` | `PlayerMovement`、`PlayerShotHoming`、`PlayerDamageOutput` | 玩家移动/射击/伤害/道具 |
| `Title*` | `TitleScreenDrawPasses`、`TitleGameManagerLifecycle` | 标题画面 |
| `Result*` / `Score*` | `ResultScreenUpdate`、`ScoreFileLoad`、`ScoreSave` | 结算与分数 |
| `Replay*` | `ReplaySave`、`ReplayPackedCodec`、`ReplayViewLoad` | 回放录制/保存/加载 |
| `Stage*` / `SceneTrigger*` | `StageObjectVtable`、`SceneTriggerFeatures` | 关卡与场景触发 |
| `Ending*` / `Transition*` / `Bgm*` | `EndingMidiSequencer`、`TransitionSoundAdapter` | 结局、转场、BGM |
| `Hint*` / `KeyConfig*` / `Joystick*` / `Pause*` | `KeyConfigScreens`、`HintTextLoader` | 配置与菜单界面 |
| 工具族 | `ZunMath`、`VmLeafHelpers`、`EntityHelpers`、`ThreadControl` | 数学/叶子 helper/实体/线程 |
| `*.asm` | `ZunMath.asm`、`MainChainDrawCallbacks.asm` 等 12 个 | 原生 ABI thunk 层 |

## 6. 本地输入与生成物（不应入库）

| 路径 | 内容 | 说明 |
| --- | --- | --- |
| `resources/` | `th10.exe`（锁定目标）、`th10.exe.i64`、`th10_ident.exe(.i64)` | IDA 数据库与变体，仅本地 |
| `references/` | `th06`、`th07`、`th08` 三个第三方重建仓库 | 各自带 `.git`，只作方法参考 |
| `build/` | `cpp/`、`strict/`、`ours/`、`reference/`、`objdiff/`、Ghidra 工程 | 全部可重新生成 |

## 7. 维护工作流与检查表

每次重建（一个函数簇）完成后，按顺序更新：

1. `src/`：实现语义 C++（正常签名，`// FUNCTION:` 注释）。
2. `docs/evidence/<cluster>.md`：记录地址、ABI、布局、怪异行为、边界。
3. `config/function-status.csv`：更新对应行的 `status` / `evidence` / `notes`。
4. Ghidra：维护函数名与注释（对未 outline 的地址用原始反汇编）。
5. 运行基线：

```sh
scripts/compile-main-chain-cpp.sh
g++ -m32 -std=c++98 -fsyntax-only -I src src/*.cpp
git diff --check
```

6. 新增 `.cpp` 后重跑 `scripts/objdiff-baseline.sh` 刷新 `objdiff.yml`。
7. 重要提交后刷新 `TH10_PROGRESS_METRIC.md` 的快照。

提交前自检（承接 `TH10_RE_METHODOLOGY.md` 第 13 节）：

- 改动针对的是 `config/target.toml` 锁定的哈希吗？
- 地址/类型/全局引用是否已进入 `config/` 与 `docs/evidence/`？
- `function-status.csv` 是否消除了重复地址（`0x40b940`、`0x40c4d0` 等一对多）？
- 原版二进制与生成物是否仍被 Git 排除？

## 8. 已知遗留项

1. `docs/evidence/ascii-manager.md` 仍引用已删除的 `scripts/compare-ascii-manager.sh`；
   属历史陈述，可改为"该脚本已随第一代流程移除"。
2. `config/reccmp-*.csv` 四张表仍为空表头，是数据面覆盖 ≈0 的直接原因，应由
   Ghidra/IDA 导出填充。
3. `HANDOUT.md` 是 40KB+ 的追加式历史日志，与账本存在重复；新读者应只把它
   当作线索，状态一律以 `config/function-status.csv` 为准。
4. `TH10_RE_METHODOLOGY.md` 的立项期章节（仓库结构/首批队列）已落地，建议在
   文件头标注"已完成/历史"，并把工具链路径统一为 `resources/th10.exe`。

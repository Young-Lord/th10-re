# 第二桶采纳计划（ADOPT_EXACT + IMPORT_EXACT）

- 日期：2026-10-04
- 输入：`config/merge-ledger.csv`（v2）× N0zoM1z0/th10 台账 + match-units.toml
- 产出：`config/adopt-plan.csv`（252 函数）、`build/adopt-staging/src/`（他们
  55 个源文件的暂存副本，build/ 不入 git）、`scripts/compare-obj-function.py`
  （重定位感知比对器，试点已验证）

## 清单概要

- **252 个函数**：ADOPT_EXACT 237（我方 implemented + 他们 authored exact）+
  IMPORT_EXACT 15（仅他们有的 authored exact），覆盖 28 个他们的源文件，
  **252/252 在 match-units.toml 有验证配方**（编译档/链接档/重定位清单齐全）。
- 分布前列：AnmManager.cpp 91、Enemy.cpp 20、EclVm.cpp 14、ZWave.cpp 12、
  BulletManager.cpp 11、EnemyEclDispatcher.cpp 11、PbgFile.cpp 11 …
- 注意：IMPORT_LEAF 的 299 个（owner 空白的 leaf accessor）不在本清单，
  后续走批量脚本另行处理。

## 试点（已通过）

`Lzss.cpp` → `Lzss::InitEncoderState @ 0x435FD0`（42B）：

1. `th10_sp1_cl /nologo /c /TP /MT /O2 /Gy /GF /Oi /DNDEBUG /Isrc` 编译干净，
   `obj-compid.py` 确认 comp.id `0x60178e`（6030）。
2. `compare-obj-function.py` 字节比对：**非重定位字节逐字节一致**；仅 3 处
   DIR32 重定位点（obj 占位零 → 镜像已解析值 0x48F868 / 0x47785C），属
   obj-vs-image 预期形态。

## 暂存与复现

```sh
git clone https://github.com/N0zoM1z0/th10 /tmp/th10-n0zom1z0   # 他们的仓库
cp -r /tmp/th10-n0zom1z0/src build/adopt-staging/src
source /home/niko/.local/share/th10-re/env.sh                    # th10_sp1_* 链
python3 scripts/compare-obj-function.py build/adopt-staging/Lzss.obj \
        '?InitEncoderState@Lzss@@SAXXZ' 0x435FD0
```

## 排期（按依赖从少到多）

1. **无依赖纯算法**：Lzss(6)、RandomMath(4)、ResFile(8)、PbgFile(11)、
   PbgArchive(8) —— 共 ~37 个函数，逐单元编译+比对。
2. **中依赖**：AnmVmId(6)、ZWave(12)、BulletManager(11)、ItemManager(10)、
   EnemyEclDispatcher(11)、EnemyMotion(1)、EnemyLaser(1)、BulletRuntime(1)、
   PlayerCollision(1)、Midi(1)、Sound(1)、FileSystem(1)、Chain(2)、
   GameErrorContext(2)、GameManager(1)、Gui(2)、Player(5)、Main(8) —— 需
   他们头文件的类型定义，搬运时连头一起带、命名锚点保留。
3. **大头**：AnmManager(91) + EclVm(14) —— 最后做，需与他们 EclVm.hpp/
   AnmManager.hpp 的结构布局逐字段对我方 IDB 类型库（75 结构体）仲裁。

## 已知坑

- **地址格式不一致**：他们台账部分地址无零填充（`0x44be20`），join 必须
  按 int 归一（compare-obj-function.py 已按 int 处理）。
- **reference 语料缺口**：`scripts/extract_reference_functions.py` 只认
  `config/function-status.csv`，采纳地址（如 0x435FD0）不在其中——比对器
  已改为直读 th10.exe，但批量验证前应扩展提取器输入或补录台账。
- **比对器归属**：`scripts/compare-obj-function.py` 由主线会话实现并试点
  验证；并行 subagent 也在写同名工具（改写 asm→C++ 任务），落地时二选一、
  以试点验证过的为准，避免两套口径。
- 法务口径不变：他们源码是 ZUN 代码衍生物（灰区）；采纳单元必须过我方
  6030 管线复验后才在 function-status.csv 记账，状态沿用其 exact 前提是
  我方比对复现。

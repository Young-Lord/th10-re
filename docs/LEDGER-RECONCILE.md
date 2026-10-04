# 台账对账报告

- 日期：2026-10-03（接手会话补全）
- 输入：本仓库 `config/function-status.csv` + `config/crt-lib-map.csv`（经
  `scripts/audit-crt-map.py` 审计增补）× N0zoM1z0/th10 `config/functions.csv`
- 生成：`scripts/reconcile-ledgers.py` → `config/merge-ledger.csv`（2,339 行）
- 背景：见 `docs/HANDOFF-2026-10-03.md`

## 分母（审计后）

`audit-crt-map.py` 对 crt-lib-map 做了两项证据驱动的修正：

1. **尾部表归属（`owned_size` 列）**：IDA 的函数 size 止于最后一条指令，不
   含编译器追加的对齐 sled + switch 跳转表 + 字节选择表。检测器：函数体后
   ≤7 字节对齐内出现 ≥3 个连续指回函数体的小端 dword。**交叉验证：双方
   size 不同的 24 个函数上，检测器逐一复现 N0zoM1z0 独立审查的 extent，
   24/24**（本次会话独立复算确认）。
2. **来源重分类（46 个 game→library）**：N0zoM1z0 台账把 46 个我方
   `kind=game` 行命名为 CRT 内部（`_realloc`、`__output` 等）。结构佐证：
   这些行 91% 与已知 library 函数相邻，对照组（同地址段其他 game 函数）
   仅 7%。记录在 `config/crt-origin-corrections.csv`，`--apply` 幂等。

审计后游戏函数分母：**1,369 个 / 277,829 字节**（比审计前少 48 个/约 16KB
划给库，多了尾部表字节）。

## 游戏函数分桶（字节为 owned_size 口径）

| 桶 | 函数 | 字节 | 占比 | 含义 |
| --- | ---: | ---: | ---: | --- |
| BOTH_VERIFIED | 7 | 373 | 0.1% | 双方均已验证，无需动作 |
| ADOPT_EXACT | 237 | 34,951 | 12.6% | 我方 implemented + 他们 authored exact，直接采纳其单元 |
| VERIFY_OURS | 668 | 236,202 | **85.0%** | 我方 implemented，需自行推到 object-matched（含他们从未台账的地址） |
| COMPILER_EMITTED | 7 | 316 | 0.1% | 编译器从正确源码自动生成（标量删除析构等），不手写 |
| HEADER_INSTANTIATED | 7 | 1,051 | 0.4% | STL 模板实例化（std::string 成员），靠正确源码形状+编译器 |
| IMPORT_EXACT | 15 | 241 | 0.1% | 仅他们有的 authored exact |
| IMPORT_LEAF | 299 | 1,539 | 0.6% | 仅他们 exact、origin 未分类——leaf accessor，便宜但验证价值低 |
| THEIR_DRAFT_ONLY | 0 | — | — | （本轮并入其他桶） |
| UNSOURCED | 129 | 3,156 | 1.1% | 双方都无源码 |

非 game 行：LIB_LINKED=739、UNSOURCED=204、NO_IDA_ENTRY=27（在台账但非
IDA 函数入口的别名/跳板）。

**读法**：采纳桶（ADOPT_EXACT+IMPORT_EXACT，252 个单元/35KB）是合并的
直接回报；主战场 VERIFY_OURS 668 个/236KB 占游戏代码 85%，与工期评估一致。

## 差异仲裁（residual 54 项）

- 25 项：尾部表我方检出、他们台账未记——我方 `owned_size` 更可信
  （检测器 24/24 交叉验证过）。
- 25 项 library：我方 COFF 函数体 vs 他们连续片段口径差，无实质冲突。
- 3 项 library 未解释（`_fast_error_exit` +3、`_abort` +1、`__fptrap` +2，
  差 1-3 字节，疑其含尾部 padding 口径，低风险）。
- **1 项 game 已裁决：`0x4386b0`（MenuItemStringAssign）我方 322B 正确，
  他们 115B 是截断**。证据：函数唯一 `ret`（`retn 8`）位于偏移 0x10F
  （0x4387bf），其后的 SEH 落点、`__CxxThrowException` 调用路径（0x4387c2-ff）
  在函数控制流内，且 3 个 code xref（0x4382c4/0x438490/0x4385d0）都指向
  函数头。他们 115B 截断在 `retn 8` 之前 27 字节处，中间无边界特征。
  **台账记账用我方 322B。**

## 遗留

- 3 项 library 微差未逐字节归因（合计 ±6 字节，不阻塞任何桶）。
- `IMPORT_LEAF` 299 个 owner 空白的 exact：采纳时按 leaf accessor 批量走，
  不占用第二桶的主力评审带宽。
- 工具链 6030 落地 + 重放 16 个 object-matched（见 HANDOFF 待办 4）。

# TH10 完成度度量方法

本文定义如何量化《东方风神录》(`TH10`) 匹配式逆向重建的完成度。它是
`TH10_RE_METHODOLOGY.md` 的配套文件：方法论定义"怎么做"，本文定义"怎么量"。

核心结论：**不存在单一的完成度百分比**。同一份代码库，按不同验收等级和权重
计算，结果可以从约 1% 到约 90%。任何宣称单一数字的报告都是口径未声明的。

## 1. 度量原则

1. 分母必须锚定到一份确定的目标二进制（`config/target.toml` 中的 SHA-256）。
2. 分子必须来自可审计的账本（`config/function-status.csv`），不能是源码目录里
   存在同名文件就算数。
3. 必须区分"验收等级"。`TH10_RE_METHODOLOGY.md:125-126` 定义了等级序列
   `unknown` → `typed` → `stubbed` → `implemented` → `object-matched` → `matched`；
   语义正确(`implemented`)与逐字节相等(`object-matched`/`matched`)相差一个数量级。
4. 必须区分"游戏代码"与"CRT/静态库"，两者比例差异极大。
5. 函数只是一部分；全局、字符串、浮点、vtable 等数据单元是同等交付物
   （见 `TH10_RE_METHODOLOGY.md` 第 9 节）。
6. 报告区间与口径，而不是孤立的点值。

## 2. 分母：目标二进制的可还原单元

### 2.1 权威来源

`config/reccmp-functions.csv`、`reccmp-globals.csv`、`reccmp-strings.csv`、
`reccmp-floats.csv` 当前均为**仅表头**，`config/mapping.csv` 仅 16 行。也就是说
仓库目前**没有提交权威函数清单**，分母必须每次从二进制现算。

游戏/CRT 边界已经作为派生产物提交：`config/crt-lib-map.csv`，每行
`address,size,kind,name`，`kind ∈ {game, library, thunk}`，共 2108 行。它由
IDA + FLIRT 生成（见 2.4），可直接用来从分母中扣除库代码。

### 2.2 用 Ghidra headless 现算

前置：`source /home/niko/.local/share/th10-re/env.sh` 提供的 `TH10_GHIDRA_HOME`
与 `TH10_GHIDRA_JDK`。

统计脚本 `CountFunctions.java`（放在 `-scriptPath` 指向的目录）：

```java
import java.io.FileWriter;
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.FunctionManager;

public class CountFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        FunctionManager functionManager = currentProgram.getFunctionManager();
        FunctionIterator iterator = functionManager.getFunctions(true);

        int totalFunctions = 0;
        int thunkFunctions = 0;
        long totalBodyBytes = 0;

        PrintWriter writer = new PrintWriter(new FileWriter("/tmp/th10_functions.tsv"));
        writer.println("address\tsize\tis_thunk\tname");

        while (iterator.hasNext() && !monitor.isCancelled()) {
            Function function = iterator.next();
            if (function.isExternal()) {
                continue;
            }
            totalFunctions++;
            if (function.isThunk()) {
                thunkFunctions++;
            }
            long size = (function.getBody() == null) ? 0 : function.getBody().getNumAddresses();
            totalBodyBytes += size;

            Address entry = function.getEntryPoint();
            writer.println(String.format("0x%08x\t%d\t%d\t%s",
                    entry.getOffset(), size, function.isThunk() ? 1 : 0, function.getName()));
        }
        writer.close();

        println("TH10_TOTAL_FUNCTIONS=" + totalFunctions);
        println("TH10_THUNK_FUNCTIONS=" + thunkFunctions);
        println("TH10_TOTAL_BODY_BYTES=" + totalBodyBytes);
    }
}
```

运行（全新导入、跑完整自动分析）：

```sh
export JAVA_HOME=/home/niko/.local/share/th10-re/jdk-21.0.12.1+1
GHIDRA_HOME=/home/niko/.local/share/th10-re/ghidra/ghidra_11.2_DEV
PROJECT_DIR=/tmp/ghidra_th10_proj
rm -rf "$PROJECT_DIR"; mkdir -p "$PROJECT_DIR"
"$GHIDRA_HOME/support/analyzeHeadless" "$PROJECT_DIR" TH10 \
  -import "$PWD/resources/th10.exe" \
  -scriptPath /tmp/ghidrascripts \
  -postScript CountFunctions.java \
  -deleteProject
```

输出 `TH10_TOTAL_FUNCTIONS` / `TH10_TOTAL_BODY_BYTES`，并把逐函数表写到
`/tmp/th10_functions.tsv`。

### 2.3 已知偏差（必须写进报告）

- Ghidra 的自动分析**会漏切函数**。经验证，账本中有 91 个地址 Ghidra 未 outline
  （例如 `0x401510`、`0x40ba70`、`0x40c3a0`、`0x417c70`、`0x43bd40`）。因此函数
  总数是**下界**，真实值约高 5%–10%。
- Ghidra 也会把极小的函数合并进邻居。因此 `0x420000` 段会出现"账本地址数 >
  Ghidra 函数数"的现象，逐函数归属本身带噪声。
- 结论：函数计数用作分母时，误差约 ±10%，报告应给区间。

### 2.4 首选：IDA + FLIRT 自动定界

Ghidra 会漏切函数（1205 vs IDA 的 2108），因此**首选 IDA 做分母与库边界**。
IDA 自带公开的 MSVC 运行时签名，`sig/pc/autoload.cfg` 已注册 `vc32rtf`、
`vc32mfc`、`vcextra` 等；载入时会自动应用，也可在脚本里显式应用。

headless 运行（`-A` 自主模式，`-S` 指定 IDAPython 脚本）：

```sh
/opt/ida-pro-9.4/idat -A -L/tmp/ida_work/ida.log \
  -S/tmp/ida_scripts/dump.py /tmp/ida_work/th10.exe
```

脚本要点：`ida_auto.auto_wait()` 等分析结束，用
`idc.plan_to_apply_idasgn(<sig>)` 显式应用签名（返回值非 0 表示排队成功），
再用 `ida_funcs.get_func(ea).flags & ida_funcs.FUNC_LIB` 判定库函数，导出
`is_library` / `is_thunk` / `name`。

实测的增量（同一个 `th10.exe`，干净目录）：

| 步骤 | 函数总数 | 库函数 |
| --- | --- | --- |
| 仅自动加载 | 2045 | 410 |
| + 公开 `vc32rtf.sig` | 2097 | 482 |
| + 自建 `libcmt.sig` | 2099 | 490 |
| + 自建 `libcpmt.sig` | 2108 | 496 |

结论：公开签名已覆盖约 97% 的库函数，自建签名只多认 14 个（主要是 C++ 标准库）。

### 2.5 可选：用锁定工具链自建精确签名（FLAIR）

TH10 是静态 CRT（导入表无 `msvcrt.dll`），因此目标库是 VC Toolkit 2003 自带的
`libcmt.lib` / `libcpmt.lib`。自建流程：

```sh
VCLIB="$TH10_RE_ROOT/wineprefix/drive_c/Program Files (x86)/Microsoft Visual C++ Toolkit 2003/lib"
FLAIR=/opt/ida-pro-9.4/tools/flair
$FLAIR/pcf "$VCLIB/libcmt.lib" libcmt.pat
$FLAIR/sigmake -nlibcmt libcmt.pat libcmt.sig     # 首次会因冲突失败并写出 libcmt.exc
# 删掉 .exc 第一行的 "delete these lines" 标记，保留其余（= 排除全部歧义模块）
$FLAIR/sigmake -nlibcmt libcmt.pat libcmt.sig
```

`libcmt`/`libcpmt` 各有 19 / 83 组冲突；采用"排除全部歧义模块"的保守策略，
避免误报。收益有限，只有在需要压榨最后几个百分点时才值得做。

## 3. 分子：重建账本

```sh
# 状态分布
awk -F',' 'NR>1{print $4}' config/function-status.csv | sort | uniq -c | sort -rn
```

状态语义（按验收强度递增）：

| 状态 | 含义 | 计入"已重建" |
| --- | --- | --- |
| `unknown` / `typed` | 仅命名或标注类型 | 否 |
| `stubbed` | 可链接占位，非还原 | 否 |
| `implemented` | 有语义 C++ + 证据文档，未逐字节验证 | 是（语义口径） |
| `object-matched` | objdiff 对象组 100% 通过 | 是（精确口径） |
| `matched` | objdiff + reccmp + stackcmp + 数据均通过 | 是（最强口径） |
| `boundary` | 原生 ABI 薄 thunk，逻辑在各语义体 | 单独统计 |

## 4. 交叉核对与覆盖率计算

把分析器导出的函数表与账本比对，按 `config/crt-lib-map.csv` 的 `kind` 分组，
按状态、按字节分别求和：

```python
#!/usr/bin/env python3
import csv
from collections import Counter

FUNCTIONS_TSV = "/tmp/ida_work2/functions.tsv"   # IDA 导出（address,size,is_library,is_thunk,name）
STATUS_CSV = "config/function-status.csv"

functions = []
with open(FUNCTIONS_TSV) as fh:
    next(fh)
    for line in fh:
        p = line.rstrip("\n").split("\t")
        if len(p) < 5:
            continue
        functions.append({"address": int(p[0], 16), "size": int(p[1]),
                          "library": int(p[2]), "thunk": int(p[3]), "name": p[4]})

rows = list(csv.DictReader(open(STATUS_CSV)))
print("status rows:", dict(Counter(r["status"] for r in rows)))

semantic = {int(r["address"], 16) for r in rows
            if r["status"] in ("implemented", "object-matched")}
exact = {int(r["address"], 16) for r in rows if r["status"] == "object-matched"}

def report(label, subset, keep):
    covered = [f for f in subset if f["address"] in keep]
    tb = sum(f["size"] for f in subset)
    cb = sum(f["size"] for f in covered)
    print("%-28s %5d/%-5d = %5.1f%% count; %6d/%-6d = %5.1f%% bytes" % (
        label, len(covered), len(subset), 100.0 * len(covered) / max(1, len(subset)),
        cb, tb, 100.0 * cb / max(1, tb)))

game = [f for f in functions if not f["library"]]
report("semantic, whole binary", functions, semantic)
report("semantic, game code", game, semantic)
report("byte-exact, whole binary", functions, exact)
report("byte-exact, game code", game, exact)
```

## 5. 口径 A：分层覆盖率

### 5.1 当前基线快照

锁定二进制：`th10.exe`，SHA-256
`2f14760b6fbbf57549541583283badb9a19a4222b90f0a146d5aa17f01dc9040`，
入口点 `0x4537dc`，`.text` 约 402 KiB。快照日期 2026-09-14。

分母（IDA + FLIRT，见 2.4；Ghidra 仅作对照，说明分析器选择会显著改变分母）：

| 项目 | IDA + FLIRT | Ghidra 对照 |
| --- | --- | --- |
| 函数总数 | **2108** | 1205（下界） |
| 库函数（FLIRT） | 496（23.5%） | 未识别 |
| thunk | 195 | 23 |
| 函数体字节 | 349,271 | 311,445 |
| 库字节 | 54,369（15.6%） | — |
| 游戏代码（非库） | **1612 函数 / 294,902 字节** | 692 函数 / 247,254 字节 |

地址分布（IDA，bucket `0x10000`）：

| bucket | game | library |
| --- | --- | --- |
| `0x400000` | 359 | 0 |
| `0x410000` | 255 | 0 |
| `0x420000` | 228 | 0 |
| `0x430000` | 228 | 5 |
| `0x440000` | 251 | 1 |
| `0x450000` | 196 | 312 |
| `0x460000` | 95 | 178 |

分子（`config/function-status.csv`，626 行 / 592 唯一地址；568 个能对上 IDA 入口）：

| 状态 | 行数 | 唯一地址 | 命中的 IDA 入口 | 字节 |
| --- | --- | --- | --- | --- |
| `implemented` | 596 | 572 | 552 | 239,493 |
| `object-matched` | 16 | 16 | 13 | 1,088 |
| `boundary` | 14 | 14 | 13 | 1,863 |

注：不同状态的行可能指向同一地址，所以分状态之和（565）大于去重后的语义集合
（561）；另有 24 个账本地址不是 IDA 函数入口（别名或 Gadget）。

覆盖率（语义 = `implemented` + `object-matched`）：

| 口径 | 按函数数 | 按代码字节 |
| --- | --- | --- |
| 语义重建，全二进制 | 561 / 2108 = **26.6%** | 240,193 / 349,271 = **68.8%** |
| 语义重建，仅游戏代码（非库） | 560 / 1612 = **34.7%** | 240,149 / 294,902 = **81.4%** |
| 语义重建，游戏代码且排除 thunk | 560 / 1417 = **39.5%** | 240,149 / 293,738 = **81.8%** |
| 逐字节精确匹配，全二进制 | 13 / 2108 = **0.6%** | 1,088 / 349,271 = **0.3%** |
| 逐字节精确匹配，游戏代码 | 13 / 1612 = **0.8%** | — |
| 完整 `matched` | 0 / 2108 = **0%** | — |

### 5.2 边界来自 FLIRT，不再是启发式

- 库函数绝大多数在 `0x450000` 以上，但两段都有混入：`0x430000`–`0x440000` 有 6 个
  库函数，`0x450000` 以上仍有 291 个非库函数（多为 FLIRT 未命中的 `sub_XXXXXX`）。
- 因此**不要用固定地址切点**，直接读 `config/crt-lib-map.csv` 的 `kind` 字段。
  `0x450000` 只作为快速目视参考，每次刷新后应重画直方图确认。
- 分析器选择会显著改变分母：Ghidra 1205 vs IDA 2108。同一份账本，用 Ghidra 的
  分母算出的"游戏代码函数覆盖"是 70.7%，用 IDA 的则是 34.7%。**必须在报告里
  写明用的是哪一个。**

### 5.3 为什么字节覆盖率远高于函数覆盖率

当前 68.8% vs 26.6%（游戏区 81.4% vs 34.7%），说明重建集中在"大而核心"的函数，
叶函数和小工具尚未铺开。只报其中一个维度会误导，两者都要给。

## 6. 口径 B：复合加权完成度

把三类交付物合成一个可用于趋势跟踪的指数：

```text
总完成度 = 0.6 × F + 0.3 × D + 0.1 × B

F（函数，按字节加权） = Σ(函数体字节 × 等级权重) / Σ(函数体字节)
    等级权重：matched = 1.0, object-matched = 1.0,
             implemented = 0.8, stubbed = 0.2, typed/unknown = 0
D（数据） = 已验证的 全局/字符串/浮点/vtable 单元 / 对应分母
    当前 reccmp-*.csv 为空 → D ≈ 0
B（可构建可运行） = 严格构建通过 + 受控运行验证
    当前严格构建通过（scripts/compile-main-chain-cpp.sh）→ B 部分计入
```

权重是显式的可调约定，不是客观真理。改动权重必须在提交信息里说明。

## 7. 报告规范

一次完成度报告必须包含：

1. 目标二进制 SHA-256 与快照日期。
2. 分母的函数总数与总字节，**注明分析器**（优先 IDA + FLIRT；Ghidra 为下界）与
   库函数扣除依据（`config/crt-lib-map.csv`）。
3. 分子按状态拆分的行数/唯一地址/命中入口数。
4. 覆盖率表（见 5.1），区分游戏代码与 CRT，并同时给函数数与字节两个维度。
5. 明确说明当前验证等级上限（现在最高到 `object-matched`，`matched` 为 0）。
6. 数据面覆盖（当前 ≈ 0）。
7. 方法论门槛进度。`TH10_RE_METHODOLOGY.md:315-326` 定义 G0–G7：
   当前 G0–G5 已过，G6 进行中，G7 未达成。

禁止：把 `implemented` 行数直接除以函数总数当作"完成度"，或把 CRT 计入游戏
逻辑分母。

## 8. 当前综合估计（2026-09-14）

| 口径 | 估计 |
| --- | --- |
| 逐字节可验证（全二进制 / 游戏代码） | ≈ 0.6% / ≈ 0.8% |
| 语义重建（游戏代码，按函数数） | ≈ 35–40%（点值 34.7%） |
| 语义重建（游戏代码，按字节） | ≈ 80–85%（点值 81.4%） |
| 语义重建（全二进制，按字节） | ≈ 69% |
| 计入数据/全局/字符串/浮点（≈0）与可运行验收（G7 未达） | 综合保守 ≈ 30–40% |

注意：如果误用 Ghidra 的 1205 作分母，游戏代码函数覆盖会显示成 70.7%，比
真实值高一倍。这是本文件要求"注明分析器"的直接原因。

## 9. 让数字可信的必要动作

1. 在仓库提交**全量** `mapping.csv` 与 `reccmp-functions/globals/strings/floats.csv`
   （由 Ghidra/IDA 导出），作为固定分母；否则每次现算的分母都会漂移。
   `config/crt-lib-map.csv`（2108 行，game/library/thunk 标记）已经提交，可用于
   扣除库代码。
2. 给 `config/function-status.csv` 的每一行补上明确的验证等级，并消除重复地址
   （当前 626 行 / 592 地址，`0x40b940`、`0x40c4d0` 等出现一对多）。
3. 把上述 Java 与 Python 脚本纳入 `scripts/`，由统一入口运行，保证全员同口径。
4. 每次重要提交后刷新一次快照并记录趋势，而不是只记单点。

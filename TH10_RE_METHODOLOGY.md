# TH10 逆向还原方法论

本文定义《东方风神录》(`TH10`) 的匹配式反编译还原流程。它吸收本地
`references/th06`、`references/th07`、`references/th08` 的实践，但以当前
TH10 目标二进制为唯一事实来源。

目标不是写出“看起来能运行”的 C++，而是重建可读、可审计，并能持续与
一份确定原版 EXE 验证的源码。移植、修复和方便运行的补丁应当是独立产物，
不能混入匹配度统计。

## 1. 目标二进制基线

当前锁定的候选原版为 `/home/niko/Games/th10/th10.exe`。

| 项目 | 值 |
| --- | --- |
| 内置版本字符串 | `Mountain of Faith. ver 1.00a` |
| SHA-256 | `2f14760b6fbbf57549541583283badb9a19a4222b90f0a146d5aa17f01dc9040` |
| 文件大小 | 487,936 bytes |
| 格式 | PE32 GUI, x86, 4 sections |
| PE 时间戳 | 2007-08-03 04:21:36 |
| 映像基址 | `0x00400000` |
| 入口点 | `0x004537dc` |
| 链接器版本 | 7.10 |
| 图形依赖 | `d3d9.dll`, `d3dx9_31.dll` |

下列文件不可作为匹配基准：

- `th10chs.exe`：汉化版，哈希不同。
- `th10 (thprac1).exe`：练习补丁版，PE 布局不同且有 5 个区段。
- `vpatch.exe`、`vpatch_th10.dll`：运行时补丁。

在将来 TH10 仓库中，只复制此目标到被忽略的 `resources/th10.exe`，并将
哈希写入已提交的清单。所有差异工具默认拒绝哈希不符的文件。

```sh
sha256sum resources/th10.exe
file resources/th10.exe
objdump -p resources/th10.exe
```

原版 EXE、DAT、提取资源及工具链安装包均不得提交进 Git。

## 2. 核心原则

1. 原始二进制是规范，反编译器输出只是证据之一。
2. 行为相同不等于匹配。函数还必须在 ABI、数据布局、代码生成、重定位和
   放置位置上达到当前验收等级。
3. 分离三类内容：Ghidra 中的分析事实、重建的源码、自动导出的比较元数据。
   不应直接手改自动导出物而不保留对应的 Ghidra 变更。
4. 严格匹配构建与功能构建/移植构建必须分开。Detours、替换库、优化和修复
   可以用于可玩性，但不构成匹配证据。
5. 不确定的名称、类型和对象归属必须标为假设，并附上地址或交叉引用证据。

## 3. 三个参考项目的用途

| 项目 | 可复用的方法 | 不能直接继承的假设 |
| --- | --- | --- |
| TH06 | 函数映射、自动 stub、Ghidra 到 COFF 导出、objdiff 工作流 | DX8/MSVC 7.0 环境和对象分组 |
| TH07 | `reccmp`、`stackcmp`、控制流恢复、编译器临时变量和栈布局分析 | 编译参数和源码布局 |
| TH08 | 函数/全局/字符串/浮点导出、源码到对象映射、按源文件的编译参数 | MSVC 7.0/DX8 工具链和硬编码库边界 |

关键参考文件：

- `references/th06/scripts/export_ghidra_objs.py`
- `references/th06/scripts/ghidra/ExportDelinker.java`
- `references/th06/scripts/generate_stubs.py`
- `references/th07/CONTRIBUTING.md`
- `references/th08/scripts/ghidra/ExportGhidraToReccmp.java`
- `references/th08/scripts/configure.py`

TH10 不能直接从 TH08 复制构建环境。它的 PE 元数据是 linker 7.10，且依赖
D3D9/D3DX9_31，而 TH06--TH08 的参考构建使用 MSVC 7.0 与 DX8。应把
MSVC 7.1 / Visual Studio .NET 2003 和相应 DX9 SDK 视为起始假设，并用
实际函数编译实验验证，不能直接当作结论。

## 4. 建议的仓库结构

```text
th10-re/
  config/
    target.toml
    mapping.csv
    implemented.csv
    ghidra_ns_to_obj.csv
    reccmp-functions.csv
    reccmp-globals.csv
    reccmp-strings.csv
    reccmp-floats.csv
    function-status.csv
  docs/evidence/
  resources/                 # Git ignored：原版 EXE 与本地资源
  scripts/ghidra/
  src/
  tests/
  build/                     # Git ignored
  reccmp-project.yml
  objdiff.json
```

建议的 `config/target.toml`：

```toml
[original]
filename = "th10.exe"
sha256 = "2f14760b6fbbf57549541583283badb9a19a4222b90f0a146d5aa17f01dc9040"
size = 487936
image_base = 0x00400000
entry_point = 0x004537dc
version = "1.00a"
```

这里需要特别注意：上面 `sha256` 的值必须是本文件第 1 节的完整哈希。创建
真实清单时应重新执行 `sha256sum`，不要手工抄写。

`mapping.csv` 是 Ghidra 导出的详细函数清单，沿用 TH06 字段：全限定名、地址、
大小、调用约定、是否变参、返回类型和参数类型。

`function-status.csv` 是人工维护的队列，建议格式为：

```text
address,name,object,status,match_percent,owner,evidence,notes
```

状态使用 `unknown`、`typed`、`stubbed`、`implemented`、`object-matched`、
`matched`。不要仅因为函数已有名称就把它标记为已实现。

## 5. 先建立分析数据库

### 5.1 导入并保存事实

1. 为精确的 `resources/th10.exe` 创建独立 Ghidra 项目。
2. 以 `x86:LE:32:default` 导入，保留 PE 的 `0x00400000` 映像基址。
3. 先完成标准分析并保留导入表、字符串、数据定义、函数边界和交叉引用，
   再做大规模重命名。
4. 人工创建的符号和类型应标记为 user-defined；不确定项写入证据或注释。
5. 定期备份/导出 Ghidra 数据库。提交的 CSV 是可评审接口，不是数据库替代品。

目标二进制包含以下有价值的源码路径线索：

```text
c:\cygwin\home\zun\prog\th10\src\core\zunlib.h
c:\cygwin\home\zun\prog\th10\src\core\sprtlib.h
c:\cygwin\home\zun\prog\th10\src\game\replay.h
```

它们可作为初始的模块划分和命名假设，但一个路径字符串不能证明某个函数一定
来自对应源码文件。

### 5.2 导出可重复的元数据

从 TH08 的 `ExportGhidraToReccmp.java` 改造出 TH10 专用脚本，自动生成：

| 文件 | 用途 |
| --- | --- |
| `reccmp-functions.csv` | 可执行文件报告中的函数名和地址 |
| `reccmp-globals.csv` | 用户定义的全局和虚表 |
| `reccmp-strings.csv` | 字符串位置与转义内容 |
| `reccmp-floats.csv` | 浮点常量 |
| `mapping.csv` | 详细签名、调用约定和函数大小 |

不要复制 TH08 脚本中硬编码的 `0x476ce0` 库函数边界。TH10 的库/用户代码边界
必须由 Ghidra 证据推导，并写入 TH10 专用配置。

每次重要分析变更后，重新导出 CSV，审阅 diff，并将元数据变更和受影响源码
一起提交。

### 5.3 将代码分组为重建对象

原始 EXE 不含可直接恢复的原始 `.obj`。objdiff 使用的对象，是分析者选择的一组
地址范围，目的是对应一个重建出的源翻译单元。

1. 建立 `ghidra_ns_to_obj.csv`，把 Ghidra 命名空间或函数映射到稳定对象名，
   例如 `zunlib`、`sprtlib`、`replay`。
2. 采用 TH06 的 `ExportDelinker.java`：先合成重定位表，再把选中的地址集导出
   为可重定位 COFF。
3. 源翻译单元边界尽量与比较对象边界一致。把函数挪到另一个 `.cpp` 会改变
   符号顺序、COMDAT、常量归属和地址。
4. 对象分组不合理是分析问题，应修正映射，而不是用报告排除项掩盖。

## 6. 恢复构建环境

### 6.1 先指纹化，再选择编译器和参数

TH10 的 linker 7.10 与 VS .NET 2003 世代相符，但不能由此推断全部编译选项。
用候选编译器写小型测试，挑选原版中的简单函数作比较，并从以下证据确认或
推翻工具链假设：

- 函数序言/尾声和栈帧；
- C++ 符号修饰、RTTI、异常处理；
- x87/浮点代码；
- 导入调用约定；
- 内联、优化与库函数代码形状。

严格构建必须使用对应 D3D9/D3DX9_31 的导入库，不能因为早期项目使用 DX8 就
继续链接 DX8。

### 6.2 构建细节也是被还原的对象

以下项目都属于匹配规范的一部分：

- 编译器和链接器版本、C/C++ 运行时选择；
- 优化、内联、intrinsic、异常、调用约定等编译参数；
- 按源文件的参数覆盖；
- include 顺序和自动生成头文件内容；
- 传给链接器的对象和库顺序；
- 子系统、区段/文件对齐、资源和链接参数。

TH08 的 `configure.py` 表明不能只使用一个全局优化级别：不同翻译单元分别用到
`/Od`、`/Oi`、`/Os` 和 `/Ob1`。TH10 应从第一天起把参数建模为按翻译单元配置。

使用确定性的构建生成器（建议 Ninja），在显式列表中固定源文件和链接顺序；
绝不能用文件 glob 决定顺序。

### 6.3 先达到可链接基线

1. 根据证据定义定宽类型、Win32/D3D9 声明、导入和数据布局。
2. 参考 TH06 的 `generate_stubs.py`，由 `mapping.csv` 生成初始声明与 stub。
3. 严格保留每个函数的调用约定、返回类型、参数顺序、成员归属和变参属性。
4. 先让严格构建链接成功，再实现大型子系统。
5. 生成 linker map，在解读总体匹配率前先检查区段和地址布局。

Stub 只是脚手架。每个 stub 都应出现在状态清单中，绝不可被计入“已还原”。

## 7. 验证闭环

同时使用三个层级的比较：

| 层级 | 工具 | 回答的问题 |
| --- | --- | --- |
| 翻译单元 | objdiff + Ghidra 导出的 COFF | 哪个对象/函数开始偏离？ |
| 完整 EXE | reccmp | 地址、函数、全局、字符串和整体输出是否收敛？ |
| 栈布局 | stackcmp | 局部变量、`this` 和编译器临时量是否落在正确槽位？ |

初始 `reccmp-project.yml` 可采用以下结构：

```yaml
targets:
  TH10:
    filename: th10.exe
    source_root: src
    hash:
      sha256: 2f14760b6fbbf57549541583283badb9a19a4222b90f0a146d5aa17f01dc9040
    encoding: shift_jis
    data_sources:
      - config/reccmp-functions.csv
      - config/reccmp-globals.csv
      - config/reccmp-strings.csv
      - config/reccmp-floats.csv
```

只有原版和首个重建 EXE 都存在后才初始化 reccmp。全局 HTML 报告用于分流；
具体实现必须切换到按地址的报告。把相关命令包进项目构建脚本，保证每位协作者
使用同一目标、参数和路径。

## 8. 单函数匹配流程

按“证据到源码”的顺序工作：

1. 在 Ghidra 中确认函数边界、调用者/被调用者、调用约定和返回行为。
2. 从字段访问和交叉引用推导参数、对象及全局布局；已验证的大小和偏移可用
   `static_assert` 固化。
3. 并排阅读原始汇编和重编译汇编。
4. 按跳转和共享错误路径恢复控制流。Ghidra 常把共享 cleanup 路径错误地变成
   巨大的嵌套条件；原始代码可能是早退或 `goto cleanup`。
5. 先匹配可观察语义，再调整代码生成：表达式形状、有符号性、cast、内联包装、
   临时变量、作用域和局部声明顺序。
6. 在主要结构差异消除后再使用 `stackcmp`。控制流仍严重不匹配时，它的结果
   不可靠。
7. 仅在“局部变量槽位顺序已被证实”且候选编译器支持时，使用 `var_order`。
   它是最后一公里的代码生成工具，不能代替正确类型和控制流。
8. 在 `function-status.csv` 更新状态，添加
   `// FUNCTION: TH10 0x004xxxxx` 注释，并把汇编/报告证据随代码提交。

常见差异的首查方向：

| 现象 | 常见原因 | 首先检查 |
| --- | --- | --- |
| `ret N` 或栈清理不同 | 调用约定/签名错误 | 参数类型、`__thiscall`、`__stdcall` |
| 序言、尾声或栈帧不同 | 参数、局部变量、临时量或编译参数错误 | 单文件参数、stackcmp |
| 操作相同但跳转不同 | 控制流被写得过于高层 | 条件极性、早退、cleanup label |
| 常量或数据地址不同 | 类型、全局放置、字符串或链接顺序错误 | CSV 元数据、对象/链接顺序 |
| 整个对象偏移很大 | 翻译单元边界或此前对象大小变化 | `ghidra_ns_to_obj.csv`、固定对象顺序 |
| 语义正确但残留小 diff | 内联包装、intrinsic、cast 或局部顺序 | 第一处汇编不匹配附近 |

## 9. 数据、类型和全局状态

数据还原与函数还原同等重要：

- 通过重复字段访问、构造/析构、虚调用和分配大小构建结构体。
- 只有偏移和大小有证据后才添加 `sizeof`/offset 断言。
- 对每个全局跟踪地址、大小、对齐、初始化器和所属模块。
- 虚表、字符串池、浮点常量和数组应作为独立比较输入；函数逻辑正确仍可能因为
  数据顺序或对齐失败。
- 使用单一权威声明，避免散落的 `extern`。
- Shift-JIS 字符串必须保持字节稳定，编码同时写入构建和 reccmp 配置。

## 10. 功能构建和测试

严格匹配构建在很长时间内可能无法运行完整游戏。可以维护独立的功能构建，使用
stub、Detours 或原版整合层，做法可参考 TH06/TH08；它必须有独立 build type，
且绝不进入严格匹配指标。

行为验证使用可控输入：

- 固定回放文件；
- 固定配置和可观察的 RNG seed；
- 逐帧截图或状态采集；
- PBG/压缩、数学、回放、分数、配置等小模块测试；
- 每次源码改动前后的 objdiff/reccmp 报告。

初期不要从渲染和完整游戏循环开始。优先处理叶函数、解析器、数学、内存工具和
小型构造函数，它们以较低成本稳定类型并验证工具链。

## 11. 阶段门槛

| Gate | 验收证据 |
| --- | --- |
| G0: 目标锁定 | 清单哈希匹配 `resources/th10.exe`；汉化/补丁版明确排除 |
| G1: 工具链候选 | 小函数验证或否定编译器、链接器和 DX9 假设 |
| G2: 分析导出 | 可重复生成函数、全局、字符串、浮点和详细映射 |
| G3: 基线链接 | 确定性严格构建、linker map、显式 stub 清单 |
| G4: 对象差异 | 至少一个稳定对象组可由 Ghidra 导出并用 objdiff 比较 |
| G5: 首个精确函数 | 一个非平凡函数通过 reccmp，必要时也通过 stackcmp |
| G6: 子系统 | 一个模块有已验证类型、数据、测试和函数级进度 |
| G7: 完整程序 | 审阅完整 reccmp、数据比较和受控运行结果 |

## 12. 首批工作队列

1. 创建 TH10 仓库、`.gitignore` 和 `config/target.toml`；只把合法本地目标复制到
   被忽略的 `resources/th10.exe`。
2. 将精确目标导入 Ghidra，导出初始 CSV 清单。
3. 在 `scripts/ghidra/` 下改造 TH08 的 reccmp 导出与 TH06 的详细映射/对象导出。
4. 用小编译实验确定真实的 VS .NET 2003 世代编译器和 DX9 库输入，并把证据写到
   `docs/evidence/`。
5. 建立显式的源文件、对象和链接顺序，得到最小严格构建。
6. 从一个小而明确的模块建立第一个对象组。
7. 选择一个叶函数，从汇编证据实现，并用 objdiff + reccmp 跑通第一个完整闭环。

## 13. 提交前检查表

1. 改动针对的是锁定的 SHA-256 吗？
2. 地址、符号、类型和全局引用是否已进入提交的元数据？
3. 翻译单元与链接位置是否有意保持稳定？
4. objdiff/reccmp 是否改善或至少未退化？
5. 编译器 workaround 是否有汇编或 stackcmp 证据？
6. 原版二进制和资源仍被 Git 排除吗？
7. 是否有范围明确的测试或报告支撑本次结论？

该流程刻意优先选择缓慢但可审计的收敛，而不是快速生成“合理”的 C++。这正是
可读移植版与匹配式反编译还原之间的区别。

## 附录 A：当前 Arch Linux 工具链基线

工具安装在 `/home/niko/.local/share/th10-re`，与游戏目录分离。环境入口是
`/home/niko/.local/share/th10-re/env.sh`；在 shell 中执行
`source /home/niko/.local/share/th10-re/env.sh` 后可使用 `th10_cl`、
`th10_link` 和 `th10_rc`。不要用宿主的 MinGW、WineGCC 或 Linux 链接器替代
这些命令。

| 组件 | 锁定版本/来源 | 已验证用途 |
| --- | --- | --- |
| 编译器/链接器 | Visual C++ Toolkit 2003，`cl/link 13.10/7.10.3077` | 与目标 PE 的 linker 7.10 世代一致 |
| Win32 SDK | Windows Server 2003 R2 Platform SDK | `Windows.h`、x86 系统库与 `RC.Exe` |
| DirectX SDK | October 2006 的 `Include`、`Lib/x86` | `d3dx9.lib` 导入 `d3dx9_31.dll` |
| Ghidra | 11.2_DEV + delinker extension 11.1 | 分析、合成重定位和导出 COFF |
| Java | Temurin JDK 21.0.12.1+1 | Ghidra 运行时，已在 `launch.properties` 固定 |
| 比较工具 | reccmp 0.1.6、objdiff-cli 2.0.0-beta.6 | EXE/栈/对象比较 |

DirectX SDK April 2007 不可用作严格构建的链接输入：它的 `d3dx9.lib` 导入
`d3dx9_33.dll`。当前采用的 October 2006 `d3dx9.lib` 已通过字符串和实际链接
验证，生成的 32 位 PE 同时导入 `d3d9.dll` 与 `d3dx9_31.dll`。

Ghidra/delinker 已在 JDK 21 下完成 headless 导入测试，并成功加载和执行
TH06 的 `ExportDelinker.java`；该脚本只因其 `askFile`/`askDirectory` 设计而不能
无参数 headless 运行。实际导出时在 Ghidra GUI 运行，或将该脚本改造成接受
headless properties 的 TH10 专用版本。

首次建立 TH10 项目时，复制 `references/th06/scripts/ghidra/ExportDelinker.java`
到新仓库的 `scripts/ghidra/`，再把对象映射配置和输出目录作为项目资产提交。
IDA 可以继续作为主分析器；Ghidra 在这里仅承担 delinker 所需的重定位/COFF 导出，
两者不竞争同一份标注数据库。

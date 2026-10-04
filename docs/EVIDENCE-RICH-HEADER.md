# Rich Header 工具链鉴定证据

- 日期：2026-10-03
- 目标：`resources/th10.exe`，SHA-256 `2f14760b…dc9040`（487,936 字节，已校验）
- 方法：`scripts/rich_header_fingerprint.py`（本仓库独立实现，未使用外部代码）
- 交叉证据：N0zoM1z0/th10 `config/tools.lock.toml` 的 `compiler_banner` /
  `linker_banner`（`13.10.6030` / `7.10.6030`），及其 `KNOWLEDGE_BASE.md`
  TOOLCHAIN-001 行。

## 背景

`config/target.toml` 原钉 `compiler = "Visual C++ Toolkit 2003 13.10"` /
`linker = "7.10.3077"`。VC++ Toolkit 2003 是免费版 VC7.1，编译器为
`13.10.3077`（RTM）。N0zoM1z0/th10 项目声称原版由 SP1（`13.10.6030`）编译。
若钉错编译器版本，部分函数永远无法做到 object-matched——SP1 相对 RTM 有实际
codegen 差异。本次鉴定用于独立裁决。

## 解码结果

DOS stub 中 Rich header（XOR key `0x3152a748`，DanS @ 0x80，Rich @ 0x108）
完整解码如下（"cl" 含 prodid 0x5F/0x60/0x64 三个 cl 家族标记）：

| prodid | 名称 | build | 对象数 | 说明 |
| --- | --- | --- | ---: | --- |
| 0x0001 | implib | 0 | 187 | 导入库输入 |
| 0x005F | cl | 6030 | 131 | C 输入 |
| 0x0064 | cl-ltcg | 6030 | 52 | LTCG C++ 输入（`/GL`） |
| 0x000F | idl | 6030 | 33 | cl 附带 IDL 标记 |
| 0x0060 | cl | 6030 | 15 | C++ 输入 |
| 0x005D | cvtres | 4035/2179/2067 | 19 | 资源转换器 DLL |
| 0x005A | masm | 6030 | 1 | MASM 输入 |
| 其余 | 链接期杂项 | 2179/4035/3052/9178 | 8 | 链接器邻近 DLL |

**cl 家族标记合计 232 个对象，其中 229 个 build=6030，5 个杂项（2179×2、
4035×3，为链接期 DLL 输入，非编译主体）；build=3077 的对象为 0 个。**

## 裁决

1. **编译器 = cl 13.10.6030（VC7.1 SP1）**，与 N0zoM1z0/th10 的断言完全一致
   （131 C / 15 C++ / 52 LTCG 三项数字亦逐一吻合）。
2. **原 `target.toml` 钉的 13.10.3077（Toolkit 2003 RTM）是错的**——二进制中
   不存在任何 3077 指纹。继续用 Toolkit 2003 编译会在部分函数上出现无法
   消除的 codegen 差异。
3. `target.toml` 的 `[toolchain]` 已按本证据更新为 13.10.6030 / link 7.10.6030。

## 对重建的直接影响

- 工具链获取目标从 VC++ Toolkit 2003 改为 VC7.1 SP1 完整版（非公开下载，
  N0zoM1z0/th10 的 `archaic-msvc/msvc710_sp1` 提交 `cf62606…` 是已验证来源，
  其 `verify-toolchain.py --execute` 提供横幅/散列 + 无头 COFF/LTCG 冒烟验证）。
- 已有的 16 个 object-matched 结果需复核：若当年是拿 3077 编出来的，其有效性
  存疑，应改用 6030 重放一遍。

## 工具链落地（2026-10-04 复核）

**获取与校验**

- 来源：`https://github.com/archaic-msvc/msvc710_sp1.git` @ `cf62606`，装至
  `~/.local/share/th10-re/tools/msvc710_sp1`，并复制进 wine 前缀
  `drive_c/MSVC710SP1/`。
- 8 个关键二进制（cl/c1/c1xx/c2/link/mspdb71/rc/cvtres）SHA-256 与
  N0zoM1z0 `config/tools.lock.toml` **逐一相符**。
- wine（11.17，wow64 模式运行 win32 工具）横幅验证：
  `cl` = `Version 13.10.6030 for 80x86`，`link` = `Version 7.10.6030`。
- env.sh 新增 `th10_sp1_cl` / `th10_sp1_link` / `th10_sp1_rc`（与旧
  `th10_cl` 3077 链并存，仅作 A/B 用；新编译一律走 `th10_sp1_*`）。
- 冒烟：`scripts/obj-compid.py` 读取 COFF `@comp.id`——6030 链产物
  `0x60178e`、3077 链产物 `0x600c05`，与 Rich header 判据一致。

**"重放 16 个 object-matched" 一项撤销**

- 16 个 object-matched 函数的宿主对象**全部是手写 .asm**
  （ZunMath.asm、GlobalBufferRelease.asm、MainChain*.asm、
  GlobalManager*.asm、AsciiManagerStrings.asm），经汇编器（ml/NASM）产出，
  **与 cl 版本无关**，无需重验。3077 风险只影响 C++ 管线。

**3077 → 6030 codegen A/B（PlayerMovement.cpp）**

| flags | 指令流差异 |
| --- | --- |
| `/c /TP`（现管线默认） | 0 行 |
| `/c /TP /O2 /Os /Og` | 0 行 |

两个版本在当前代码库的代表性 TU 上生成**逐指令相同**的代码（obj 文件仅
`@comp.id`/时间戳元数据不同）。单样本不能证明全库等价，但切换风险判定为
低；后续新 C++ 单元直接用 6030 链编译，历史 build/cpp/ 基线无需重编。

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

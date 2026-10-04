# 16 个 object-matched 单元归属结论（2026-10-04 终版）

## 结论

**16 个函数全部属于原版构建中的非标准 ABI 代码区——最可能是 ZUN 的单一
手写 MASM 模块（Rich header: 恰好 1 个 MASM 对象, build 6030）。
`.asm` 即忠实重建，"C++ 化"目标方法论上不成立，撤销。**

## 证据

1. **cl 13.10.6030 无法从任何 C++ 源形状/旗标组合复现本区的代码形状**。
   已测矩阵（ZunMath 0x41F800 为样本）：
   - `/O2 /Os /Og /Oi /Gy`、`+ /Op`、`sin(f)/cos(f)` 双版本、源码顺序两变体
     → 均输出 `fld;fcos;fmul;fstp;fld;fsin;...`（非合并）+ ECX 直接使用；
   - `/GL` 单函数 + `/LTCG` + data-anchor 链接（`__fltused` 锚点）→ 同上；
   - **全程序 /GL 复现**（helper + 8 调用点 + 取地址引用，整链 LTCG）→ 仍同上。
   目标形状是 `fld;fsincos;fmul;fstp;fmul;fstp` + `push ecx/mov [esp],ecx/
   mov eax,[esp]` 参数落栈槽。
2. **自定义寄存器 ABI 遍布本区**：
   - `0x447810`（释放助手）从 **EDI** 取参（`mov eax,[edi+108h]`），26 个
     调用方（0x401260/0x40B7B0/0x41F930/0x42BC30…）全部"EDI 装值直接 call"，
     无 ECX 搬运；
   - `0x449AE0/0x449B70`（链注册助手）EDI+ESI 双寄存器传参；
   - `FUN_00401530`（AsciiManager 字符串 worker）**ECX+EAX** 双寄存器传参。
   cl 的任何标准约定（cdecl/stdcall/fastcall/thiscall）都不使用
   EDI/ESI/EAX 传参；这些函数彼此调用、成片分布于 0x4012xx-0x4501B0。
3. **反例核查**：14 处 fsincos 中其余 6 处为大函数内联位置（如 0x4436ED，
   弹幕函数内的 cl 风格 x87 比较 `fucompp/fnstsw/test ah,44h`），属编译器
   正常合并——说明 fsincos 本身不是禁区，**助手函数的特殊性在自定义 ABI
   与落栈槽习惯，而非指令本身**。
4. **旁证**：本区函数与编译器生成区风格断裂（手工回调注册、原始 vtable
   调用、`mov esi,ecx` 手工寄存器搬移、无 SEH/cookie 习惯差异）。

## 处置

| 单元 | 函数 | 处置 |
| --- | --- | --- |
| ZunMath / ZunMathBounds | 2 | 保留 .asm（fsincos/x87 族） |
| GlobalBufferRelease | 1 | 保留 .asm（0x447810 EDI 族） |
| GlobalManager×5 | 6 | 保留 .asm（同族） |
| MainChainDrawInitialize/Callbacks/Registration | 4 | 保留 .asm（0x4215A0/0x449xxx 族） |
| AsciiManagerStrings | 3 | 保留 .asm（FUN_00401530 ECX+EAX 族；0x401630/1690 含 GS cookie 需后续单独裁决） |

- ZunMath 的 `__fastcall` ABI 修正保留（调用方 ECX 传参与原版一致）。
- 已验证设施：`compare-obj-function.py`（重定位感知比对）、
  `verify-zunmath.py`、LTCG data-anchor 链接流程（`/GL + /LTCG +
  /nodefaultlib + __fltused 锚点 + /map` 提取）——后续 VERIFY_OURS/
  adopt 工作直接复用。

## 战略含义

C++ 字节匹配的真实战场在**编译器生成区**：VERIFY_OURS 668 个 +
adopt 252 个（标准 ABI，Lzss 试点已证明管线可用）。本区 16 个以 .asm
为最终形态，与原版的构建方式一致——这正是 matching decomp 的本义。

# fsincos 助手函数族结论：保留 .asm（2026-10-04）

## 对象

原 `src/ZunMath.asm` 等手写汇编覆盖的 ECX-ABI 向量数学助手，特征序列
`51 89 0c 24 8b 04 24`（push ecx; mov [esp],ecx; mov eax,[esp]——ECX 参数
落栈槽）+ `d9 fb`（fsincos）。全库命中 8 处函数入口：

```
0x408750 0x413270 0x41BEB0 0x41F800(ZunMath) 0x441EF0 0x4458B0 0x44C5D0 0x4501B0
```

另 14 处 `d9 fb`（fsincos）中的其余 6 处位于大函数内联位置
（如 0x4436ED，弹幕函数内联 trig），属编译器合并产物，与本族无关。

## 证据链

1. **cl 13.10.6030 在已测旗标组合下均不从小助手源码生成 fsincos**：
   `/O2 /Os /Og /Oi /Gy`、加 `/Op`（退化为 59B + CRT 调用，更糟）、
   `sinf/cosf` 与 `sin/cos`（double）两种源码、`/GL + /LTCG`（整链
   data-anchor 链接后提取，仍是非合并 fcos/fsin 序列）。sinf/cosf 的
   调用顺序（V0/V1 变体）不影响产物。
2. **Rich header：MASM 输入恰好 1 个对象**（prodid 0x005A, build 6030,
   count 1）。8 个助手函数共享同一 28 字节形状、同一手写风格的 ECX 落栈
   槽（该形状在 cl 生成的代码里无 C++ 动机），与"单一手写 MASM 模块"自洽。
3. 结论：这 8 个函数在 ZUN 原始构建里就是**手写汇编模块**（一个 .asm 编译
   单元）。忠实的重建载体就是我们的 .asm（现状 object-matched 正确），
   **不应也不会被 C++ 替换**。

## 对项目的影响

- 撤销"16 个 object-matched 全部 C++ 化"中涉及本族的 8 个函数的目标；
  剩余可 C++ 化的是 GlobalBufferRelease、GlobalManager×5、
  MainChainDraw×3、AsciiManagerStrings（非 fsincos 族）。
- `ZunMath.hpp/cpp` 的 `__fastcall` ABI 修正**保留**：原版调用点就是
  ECX 传参（10 个调用方在 StageObjectVtable.cpp 等），语义 C++ 声明与
  原始 ABI 对齐后调用方代码形状更接近原版。
- 验证设施：`scripts/compare-obj-function.py`（重定位感知比对器）、
  `scripts/verify-zunmath.py`（本族专用验证）——两者修复了 COFF 符号表
  解析 bug（NumberOfAuxSymbols 在 +17，此前误读 +16 的 StorageClass）。

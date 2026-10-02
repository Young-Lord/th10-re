# AsciiHudOwner / TitleScreenState 结构体化(第 6 轮)

## AsciiHudOwner — TH10 DAT_0047770c,0x9ed0 字节(src/AsciiHudOwner.hpp)

- 0x9ed0 = 0x10 头 + 43 × 0x3ac + 0xdc 尾,精确分解自 ctor 0x413810 六个
  eh-vector 数组计数 10/10/9/4/2/7(slot 0x10/0x24c8/0x4980/0x6a8c/0x793c/
  0x8094)+ 辅助 VM 0x9a48(其 +0x35c/+0x384/+0x2ff/+0x304/+0x340 别名
  0x9da4/0x9dcc/0x9d47/0x9d4c/0x9d88 证明整条 VmRecord)。
- 不是 LargeRenderOwner 的缩水版:头不同(flags + 两个调度元素 @8/@0xc),
  池为固定数组而非游标池,无 work_slots/D3D 状态缓存/顶点区;所有 HUD 绘制
  经 DispatchAsciiAnimationVmRenderMode 打入 0x491c10 LRO。
- 尾部:8 个结算数字句柄@0x9df4、双横幅句柄@0x9e14/18、boss 句柄@0x9e24、
  10 个 bench 子句柄@0x9e28、script-102@0x9e50、背景 VM id@0x9e58/5c、
  TimerNode@0x9e60、best_score@0x9e74、displayed_score@0x9e78(与
  OpenSceneScriptResource 的场景子计时器副本共用)、rate@0x9e7c、
  stage_script_work@0x9e80、boss HP 三联@0x9e84-8c、bench_child_count@0x9e90、
  spell_bars[4]@0x9e94({value,color}×8 字节;0x9e9c 开启脚本句柄与
  0x9ea8 结算表脚本 id 为重叠视图,保留裸访问)、hud_mode_flags@0x9eb4、
  result_script_state@0x9eb8、result_script_blob@0x9ebc、spell_countdown
  @0x9ec0、last@0x9ec4、front_anm_work@0x9ec8、render_mode_counter@0x9ecc。
- 已知瑕疵(保留):ManagerCreation.cpp 的 ctor 死存储块基址 0x9a28 与
  AsciiHudOwnerLifecycle 的 0x9a48 读法不一致(0x9f60 越界写);全部被随后的
  0x9ed0 wipe 覆盖,不可观察,维持原样并记录。dtor 旧注释(+0x9da4/+0x9e90/
  +0x9e88/+0x9d60)已按真实字段改正。

## TitleScreenState — 0x2b64 字节(DAT_004776e4 primary / e8 secondary)

- 大小裁决:0x2a78 是旧注释错误;0x402640 `operator new(0x2b64)`、
  0x402160 wipe 0xad9 dwords、相机快照 0x2a4c+0x118 恰好到 0x2b64 三重证据。
  相关注释已全部改为 0x2b64。
- 布局:调度元素@8/c、脚本缓冲/指针表/基址@0x10-0x1c、wait_timer@0x38、
  script_cursor@0x4c、interp_a/b 双插值块 + color_track 块 0x50-0x174、
  anm_manager_work@0x178、vm_heap_array@0x17c、background_vms[8]@0x180、
  background_fade/idle_latch/modulation_color/aux_vm_arm_latch@0x1ee0-0x1eec、
  aux_vms[3]@0x1f08、scene/op 计数器@0x2a0c-14、master_flags@0x2a18、
  score_anim_timer@0x2a1c、draw_pass1_element@0x2a40、file_buffer/size
  @0x2a44/48、camera_snapshot(MainChainCameraWork)@0x2a4c(与背景脚本的
  活动 vec3 写入重叠,保留裸访问)。

## 语义修复(均经反编译/反汇编核实)

1. TeardownTitleScreenStackAbi(0x00417c80)status-13 分支:原生调用
   0x418a90 = AdvanceTitleMenuItemIndexEaxAbi(帧状态块 +0x50 菜单索引 ++,
   clamp 9),源码误调 ResetMainChainFrameStateBlockEax(0x418b80 函数体)。
   已改正并移除 704 行的陈旧 extern;0x418b80 的真身 ResetMainChainFrameStateBlock
   不受影响。
2. VmRecord +0x34/+0x38 悬案裁决:setup opcode 0x35 写三联旋转角速度、
   epilogue 积分进 +0x24/+0x28/+0x2c(行为证据);0x40c960 "scale pair" 标签
   错误(缩放在 +0x3c/+0x40,由 scale_rate 输出)。VmRecord.hpp 注释已更新;
   width@0x4c 的错误注释一并修正。
3. AsciiOwnerTraversal 子指针双解引用:原生 0x403a90-96
   `mov edx,[ebp+14h]; mov esi,[edx+ecx*4]` 经 +0x14 指针二次解引用,旧源码
   单次解引用;随字段化一并修正。

## IDB 迁移(/tmp/th10ref.exe.i64,session th10ref)

- 声明:AsciiHudSpellBarEntry(8)、AsciiHudOwner(0x9ed0)、
  TitleScreenState(0x2b64);绑定 0x47770c → AsciiHudOwner*、
  0x4776e4/0x4776e8 → TitleScreenState*;三个全局注释已写。

## 本轮改写

- HUD owner 侧 20 文件(重 7 + 轻 13,约 240 处);title state 侧 6 文件
  (约 120 处)。全量构建 exit 0(229 obj)。
- 保留的刻意裸访问:ctor 死存储块、0x9e9c/0x9ea8 重叠视图、相机快照重叠区、
  未命名 gap、TickPointerRateTimer 等以原始指针工作的计时器辅助调用。

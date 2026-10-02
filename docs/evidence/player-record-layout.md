# PlayerRecord / MainChainContext 结构体化(第 5 轮)

## PlayerRecord — TH10 DAT_00477834,0x4478 字节(src/PlayerRecord.hpp)

- 总大小证据:`::operator new(0x4478)`(ManagerCreation.cpp CreatePlayerStateBlock)、
  `memset(bytes,0,0x4478)`(PlayerRecordHelpers.cpp / ManagerCreation.cpp)、
  就地析构 0x00424ed0(ManagerReleaseWrappers.cpp)。
- 内嵌 `VmRecord anim_vm` 占 +0x14..0x3c0(`memset(bytes+0x14,0,0x3ac)`,
  PlayerRecordHelpers.cpp;kFlagOffsets 清位表 {0x6c,0xb0,0xfc,0x128,0x174,0x1b0,
  0x1fc,0x228,0x378} 即 VmRecord 插值块旗标偏移表,+0x398 u16=0xffff =
  anim_vm.sprite_entry_id)。
- 子结构:
  - `PlayerShotRecord` 128 × 0x5c @ +0x49c:0x00427e90(生成)以 `a2+295` 个
    float 定位(=+0x49c)、步长 23 float(0x5c)、128 槽;0x00428280(更新);
    0x00428630(伤害 pass)游标为 `a2+312` 个 int(=+0x4e0 = record+0x44)。
    字段:timer{prev@0(NaN 初始化),count@4,accum@8,rate@c,latch@0x10},
    position[3]@0x14,velocity[2]@0x20,speed@0x2c,angle@0x30,
    angle_delta[2]@0x34,angle_flags@0x3c(bit0 积分角差),state@0x40
    {0 空,1 活动,2 命中/淡出},entity_id@0x44,secondary_entity_id@0x48,
    homing_target@0x4c,hit_flag@0x50,magnet_latch@0x54,descriptor@0x58。
    重置走查 `&= ~1` @+0x4ac 步长 0x5c ×128 = record+0x10 timer_latch,与基址
    +0x49c 互证。
  - `PlayerOptionRecord` 4 × 0x98 @ +0x32a0(0x00426f70 重建;字段表见
    PlayerOptionRecords.cpp 头注释)。
  - `PlayerSubEffectRecord` 32 × 0x6c @ +0x350c(0x00427b50 生成;0x00428630
    伤害扫描:angle@8,width@0x10,height@0x14,pos@0x18/1c,timer prev/count
    @0x44/0x48,value@0x58,accum@0x5c,limit@0x60,period@0x64,flags@0x68;
    重置走查 `&= ~1` @+0x3560 步长 0x6c ×33 = record+0x54 timer_latch)。
  - TimerNode 复用:+0x460 autocollect、+0x474 frame、+0x488 move_gate、
    +0x430c deathbomb(+0x470/0x484/0x498/0x431c 的 `&= ~1` 即 flags@+0x10)。
- PlayerDamageOutput.cpp 修正(0x00428630 反编译对照):旧 pass-1 视图整体错位
  ——旧基址 player+0x4a0、字段 position@0/+4、state@0x2c、entity@0x30、
  magnet_latch@0x40、descriptor@0x44 应为基址 +0x49c 的 +0x14、+0x40、+0x44、
  +0x54、+0x58;fade(×0.125)与 0.1f step 两处旧代码还写到了错误字段(应为
  speed@0x2c ×0.125、position[2]@0x1c = 0.1f)。hit 回调与 IsTimerFrameMultiple
  现传当前记录基址(原生 v10/v47 随循环推进),handle 辅助函数
  (0x40c480/0x449450/0x449630)统一传 &record.entity_id(原生 v12 = B+0x44)。

## MainChainContext 扩展 — TH10 0x491c28,0x784 字节(src/MainChainContext.hpp)

- 0x784 总大小来源:0x00421f00 `memset(bytes,0,0x784)`(MainChainStateHelpers.cpp)
  与原 assert。
- camera_work_bank[2](MainChainCameraWork 0x118)@+0x154:GateVmSlots.cpp /
  TitleScreenDrawPasses.cpp 的 `slot+0x154+index*0x118` 索引;DrawInitialize
  固定 index 1。
- ThreadControl @+0x62c(marker/handle/id/stop/active/entry = 0x62c/630/634/
  638/63c/644;+0x648 = update_status_001c,MainChainUpdate 读作状态字)。
- state_locks[7] @+0x64c、state_update_depths[7] @+0x6f4(原生 0x00421420/
  0x00421450 循环 0..6;0x00420ea0 用第 7 把锁 +0x6dc/深度 +0x6fa)。
- background_vm_latch @+0x6fc(GameManagerGateVms 一次性生成闩;DAT_00492324
  为其绝对地址镜像)。
- anm_manager_work @+0x3c8(三只背景 VM 的脚本绑定 ECX)。
- 快照暂存区 +0x50c..0x62c:snapshot_busy、packed SnapshotBmpFileHeader
  ('BM'@0x510,size@0x512,data_offset@0x51a,+0x51c u16=0 为 data_offset 高半
  的重叠写)、bmp_info@0x520、pixel_buffer@0x524、snapshot_path[0x104]@0x528。

## IDB 迁移(/tmp/th10ref.exe.i64,session th10ref)

- 声明:D3DVector3/D3DMatrix/D3DViewport/MainChainCameraWork(0x118)/
  ThreadControl(0x20)/Win32CriticalSection(0x18)/SnapshotBmpFileHeader(0x10)/
  PlayerShotRecord(0x5c)/PlayerOptionRecord(0x98)/PlayerSubEffectRecord(0x6c)/
  PlayerRecord(0x4478);重声明 MainChainContext(0x784,字段扩展)。
- 绑定:0x477834 → PlayerRecord*;0x491c28 维持 MainChainContext*。
- 注释:0x477834/0x491c28/0x492324。

## 本轮改写

- MainChainContext 侧 8 文件(GameManagerGateVms/ScreenshotWriter/
  MainChainStateHelpers/TitleGameManagerLifecycle/GateVmSlots/
  TitleScreenDrawPasses/MainChainConfiguration/MainChainDrawInitialize)。
- PlayerRecord 侧 24 文件(核心 11 + 外围 13;OptionTrailUpdate.cpp 为死代码
  有意不动)。全量构建 exit 0,229 obj。
- 保留的刻意裸访问:kFlagOffsets 动画旗标表走查、0x88 步长四元组写
  (+0x334c 族,跨越 option 记录边界的混叠)、33 项 sub-effect 走查的第 33 项
  (落在 [32] 数组之后的未知区,与原生一致)、未命名 gap 访问。

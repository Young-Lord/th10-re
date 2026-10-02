# GameManager / TitleScreen 结构体化(第 7 轮)

## GameManager — TH10 DAT_0047784c,0x5acc 字节(src/GameManagerObject.hpp)

- 大小三重证据:AllocateMainChainObject(0x5acc)、ctor 0x0042c920
  `memset(manager,0,0x5acc)`(REP STOSD ECX=0x16b3)、尾部 ThreadControl
  thread_entry@+0x5ac8 恰好收尾。
- 布局:flags@4(bit1)、调度元素@0xc/0x10、title/title_v ANM 工作记录
  @0x14/0x18、state@0x1c / sub_state@0x20(调度键)、ManagerCursorRecord
  0xd8@0x24(value/prev/max/step_values[16]/step_maxima[16]/step_count/
  disabled_rows[16]/wrap/disabled_count;+0xfc 三元组为第二游标)、
  teardown_entity_id@0x174、state-B 页游标@0x1d4-0x1dc、state_row_counter
  @0x2ac、frame_timer(TimerNode)@0x2b0、script_entity_handles[180]@0x2c4
  (0x2c4+4*script_id;slot 182 越界访问保留裸)、ascii_work_handle@0x5d0、
  state_b_option_handles[10]@0x5d4、name entry 区@0x58dc-0x590c、
  selected_replay_slot@0x59c4、result_stats[5](u16)@0x59cc、
  selected_replay_index@0x59dc、replay_parse_handles[50]@0x59e4(dtor 走查)、
  owned_buffer@0x5aac、worker ThreadControl 前缀 0x1c@0x5ab0(0x5acc 截断,
  update_status dword 在对象之外)。
- 归属未决(保留裸):StageEffectHost.cpp EnterGameModeSetupEsiAbi 的
  +0x34/+0x114/+0x1ec/+0x1f4(疑为 0x688 stage-host 对象)、MenuStateHelpers
  的向量 tween +0x70..0xb8(疑为 0x3ac 池 VM 记录)。

## TitleScreen — TH10 DAT_00477810,0x60 字节(src/TitleScreenObject.hpp)

- CreateTitleScreen 0x4180e0 分配 0x60;mode_record@4、调度元素@8/0xc、
  TimerNode timer@0x10(count 即帧计数器,驱动 0/30/2940/3000 帧门槛)、
  0x34 字节子对象@0x24(sub+0x30 = +0x54 有符号字节读)、flags@0x58
  (0x1 拆除/0x2 运行/0x4 引导/0x8 关机帧/0x10 暂停/0x20|0x40 暂停菜单模式/
  0x80 待关机/0x200 声音门/0x400 场景活动/0x800 分数动画)、mode@0x5c。
- 解引用裁决:0x477810 全局直接持有对象指针(单次间接)。原源码
  PlayerFrameworkHelpers RunGameOverPathBStackAbi 的双重解引用是错的,已修。

## 语义修复(经 0x004231d0 反编译核实)

RunGameOverPathBStackAbi:原生顺序为 spawn 脚本 0 → id 存 +0x1d8 →
CreateGameOverOverlay(render owner, 第一个 id, 32,16,384,448) →
front_anm_work 存 +0x2c4 → spawn 脚本 0x80 → id 存 +0x1d4。旧源码把 `param`
(函数参数)而非生成的实体 id 存进 +0x1d4/0x1d8,并把 (manager, param)
传给 CreateGameOverOverlay(其它调用点均为 (render owner, id))——已按
原生重排并修正。

## IDB 迁移(/tmp/th10ref.exe.i64,session th10ref)

- 声明:ManagerCursorRecord(0xd8)、GameManagerWorkerControl(0x1c)、
  GameManager(0x5acc)、TitleScreen(0x60);绑定 0x47784c → GameManager*、
  0x477810 → TitleScreen*;两处全局注释已写。

## 本轮改写

- game manager 侧 15 文件(重 3 + 轻 6 + 已含在重文件内的 controller);
  TitleScreen 侧 16 文件。全量构建 exit 0(232 obj)。
- 保留的刻意裸访问:memset/vtable 存储、slot ≥180 的脚本句柄算术、
  state-C parse-hook 区(0x59d4 起,与 result_stats 建模重叠)、
  归属未决的两个文件、sub_object 内部(+0x54 字节除外)、相机/其它对象。

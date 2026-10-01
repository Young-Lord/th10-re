# 全局变量语义账本（globals ledger）

来源：src/*.cpp / *.hpp 中带地址注释的 `extern g_*` 声明汇总。
冲突地址按源代码中出现频次取主名；全部主名已写入 IDB（/tmp/th10ref.exe.i64，th10ref 会话）。

- 已登记地址：220
- 存在命名冲突的地址：75（见末节）
- 0x004660ac：Sleep 导入槽（__imp_Sleep），源码别名 g_LoadWaitFrameYield，未改导入名。
- IDB 命名待补（item 非 dword_ 头或位于函数体内，共 16 个）：0x447708=g_RegistrationDrawOwner, 0x474cb4=g_CurrentStageSceneId, 0x474cc0=g_PracticeStageValue2, 0x474cc8=g_PracticeStageValue, 0x474cd0=g_ReplayPathFlags, 0x474e38=g_ManagerConfirmFlagsSecond, 0x47773c=g_HudFragmentSlotOwner, 0x477748=g_SceneConfigTablePointer, 0x477783=g_BgmQueueFlagsBase, 0x477784=g_StageRecordSlot, 0x491c40=g_TipEntityManager, 0x492274=g_CallbackSchedulerLock, 0x4924f0=g_MainChainWindow, 0x49250c=g_MainChainPerformanceFrequencyHi, 0x49251c=g_ScreenSaverWasActive, 0x4977b0=g_MainChainSoundWorkerId

## 主映射

| 地址 | 主名 | 备注 |
|---|---|---|
| 0x447708 | g_RegistrationDrawOwner |  |
| 0x4660ac | g_LoadWaitFrameYield |  |
| 0x466704 | g_MainChainControllerDataFormat |  |
| 0x46ef10 | g_ExtraUnlockCodeSequence |  |
| 0x46f288 | g_TextureFormatTable |  |
| 0x46f2a0 | g_TextureBppTable |  |
| 0x46f810 | g_MidiTimerBaseVtable |  |
| 0x470b08 | g_AsciiBarHeight |  |
| 0x470b48 | g_AsciiObjectScrollY |  |
| 0x470b4c | g_AsciiObjectScrollX |  |
| 0x470be8 | g_AsciiBarTopAdjust |  |
| 0x470c64 | g_AsciiHudEntryOffset |  |
| 0x470c90 | g_MainChainFrameStep |  |
| 0x470cb4 | g_AsciiBarTopAnchor |  |
| 0x470cb8 | g_AsciiHudBarScale |  |
| 0x470cbc | g_AsciiBarLeftAnchor |  |
| 0x470d24 | g_AsciiHudProgressOffset |  |
| 0x470d28 | g_AsciiHudProgressScale |  |
| 0x470d2c | g_AsciiHudBarOffset |  |
| 0x474170 | g_TriggerVmScriptTable |  |
| 0x4746d8 | g_MenuCharset |  |
| 0x474788 | g_TitleTransitionRecords | 冲突: g_StageRecordTable/g_TitleTransitionRecords |
| 0x474908 | g_FileOpenModeDefault |  |
| 0x474b40 | g_MainChainWaveNames |  |
| 0x474c38 | g_ReplayFileHandle |  |
| 0x474c40 | g_ScoreBlock | 冲突: g_HighScoreValue/g_MainChainFrameStateBlock/g_MaximumScore/g_ScoreBlock/g_StageFramePeak |
| 0x474c44 | g_CurrentRunScore | 冲突: g_CurrentRunScore/g_CurrentRunScoreValue/g_CurrentScore/g_SceneSubTimer/g_StageFrameCount |
| 0x474c48 | g_PlayerPowerGauge | 冲突: g_PlayerLivesCounter/g_PlayerPowerGauge/g_PlayerPowerGaugeDword/g_PlayerPowerGaugeWord/g_SceneWord48 |
| 0x474c4c | g_PlayerPowerPool | 冲突: g_PlayerPowerPool/g_ScoreBonusBaseDword/g_ScorePool |
| 0x474c50 | g_BossDefeatedFlag | 冲突: g_BossDefeatedFlag/g_SceneSelector50 |
| 0x474c58 | g_AsciiHudBarValue |  |
| 0x474c68 | g_PlayerCharacter | 冲突: g_PlayerCharacter/g_PlayerCharacterDamage/g_PopupStepSelector/g_RunChara/g_SceneStageIndexA/g_StageTextSprites |
| 0x474c6c | g_PlayerShotType | 冲突: g_PlayerShotType/g_RunCharaSlot/g_SceneStageIndexB |
| 0x474c70 | g_PlayerLivesRemaining | 冲突: g_PlayerLivesRemaining/g_PracticeStartIndex |
| 0x474c74 | g_CurrentDifficulty | 冲突: g_CurrentDifficulty/g_PlayerDifficulty/g_RunShot/g_StageScoreSelector/g_TimelinePhase |
| 0x474c78 | g_PlayerDifficultyWord |  |
| 0x474c7c | g_ActiveTextLayer | 冲突: g_ActiveTextLayer/g_LastTitleTransitionIndex/g_PlayerShotMode/g_RunStage/g_SceneModeSelector/g_StageIndex/g_StageSelectorIndex |
| 0x474c80 | g_TitleTransitionIndex | 冲突: g_SceneModeSelectorMirror/g_StageIndexMirror/g_StageSelectorCurrent/g_TitleTransitionIndex |
| 0x474c84 | g_TextStyleFlag | 冲突: g_ResultScriptSlotCount/g_SceneFlag84/g_StageSubState/g_TextStyleFlag |
| 0x474c88 | g_TextStyleDefault | 冲突: g_PlayerContinueMode/g_SceneFlag88/g_TextStyleDefault |
| 0x474c8c | g_TextStyleAlt | 冲突: g_ResultScriptSlotCache/g_SceneFlag8c/g_TextStyleAlt |
| 0x474c90 | g_PracticeScoreSeed |  |
| 0x474c94 | g_SceneVoiceId |  |
| 0x474c98 | g_ScorePenaltyCounter | 冲突: g_ResultScore/g_RunRankValue/g_SceneRankValue/g_ScorePenaltyCounter |
| 0x474c9c | g_ScenePlayCountSeed | 冲突: g_ResultPower/g_ScenePlayCountSeed |
| 0x474ca0 | g_GlobalModeFlags | 冲突: g_GameModeFlags/g_GameRunFlags/g_GlobalModeFlags/g_ManagerModeFlags/g_ScoreNameStage/g_StagePauseGate |
| 0x474ca4 | g_DemoWaitCounter | 冲突: g_DemoWaitCounter/g_ResultPiv |
| 0x474ca8 | g_DemoNameIndex |  |
| 0x474cac | g_CurrentStageIndex | 冲突: g_ClearFlagScan/g_CurrentStageIndex/g_PracticeStartRequest |
| 0x474cb0 | g_GeneratedSurface |  |
| 0x474cb4 | g_CurrentStageSceneId |  |
| 0x474cc0 | g_PracticeStageValue2 |  |
| 0x474cc8 | g_PracticeStageValue |  |
| 0x474cd0 | g_ReplayPathFlags |  |
| 0x474dd4 | g_MainChainBackgroundControl |  |
| 0x474e30 | g_MainChainPresentDiagnosticOptions |  |
| 0x474e34 | g_MenuInputFlagsByte |  |
| 0x474e36 | g_ManagerSubGateFlags | 冲突: g_MainChainPresentDiagnosticOptions/g_ManagerSubGateFlags/g_ScoreNameTrigger/g_StagePauseTrigger |
| 0x474e38 | g_ManagerConfirmFlagsSecond |  |
| 0x474e5c | g_InputMask | 冲突: g_InputMask/g_InputMaskWord/g_SceneGateFlags |
| 0x474e62 | g_ResultGateFlags |  |
| 0x474e88 | g_MainChainInputBindings | 冲突: g_MainChainInputBindings/g_TitleGeometryDefaults |
| 0x474e98 | g_TitleGeometryWord |  |
| 0x474f70 | g_EndingMidiErrorConsole | 冲突: g_MainChainErrorBuffer/g_MainChainErrorReceiver |
| 0x476f70 | g_MainChainErrorCursor |  |
| 0x476f74 | g_MainChainErrorShowDialog |  |
| 0x476f78 | g_FrameTimeScale | 冲突: g_AsciiOverlayInitialRate/g_FrameTimeScale/g_MainChainStartupScale/g_SceneFadeScale |
| 0x476fa0 | g_PlayerSpeedTable |  |
| 0x476fa8 | g_PlayerItemBoxHalfTable |  |
| 0x476fb0 | g_PlayerGrazeHalfTable |  |
| 0x476fb8 | g_PlayerHitHalfTable |  |
| 0x476fc0 | g_PlayerShotEntryTable |  |
| 0x4776e0 | g_AsciiManagerHost | 冲突: g_AsciiManager/g_AsciiManagerHost/g_LoadingScreenContext |
| 0x4776e4 | g_TitleScreenStatePrimary |  |
| 0x4776e8 | g_TitleScreenStateSecondary | 冲突: g_SpellBannerFlagOwner/g_TitleScreenStateSecondary |
| 0x4776ec | g_GameContext | 冲突: g_GameContext/g_GameContextObject/g_StageNode |
| 0x4776f0 | g_EffectManagerRoot | 冲突: g_EffectManagerRoot/g_EnemyArrayBase/g_SceneCommandManager/g_StageRecordHolder |
| 0x4776f4 | g_SpellBulletBase | 冲突: g_SpellBulletBase/g_SpellBulletGate/g_StageState |
| 0x4776f8 | g_StageHostObject |  |
| 0x4776fc | g_MainChainObject6fc |  |
| 0x477700 | g_TransitionObject | 冲突: g_TimelineGateState/g_TransitionObject |
| 0x477704 | g_AsciiHudConditionalState | 冲突: g_AsciiHudConditionalState/g_AsciiHudConditionalStateDamage/g_BossBattleState |
| 0x477708 | g_SlowRateStats | 冲突: g_RegistrationDrawOwner/g_SceneTimeSource/g_SlowRateStats |
| 0x47770c | g_AsciiHudOwner | 冲突: g_AsciiHudOverlayState/g_AsciiHudOwner |
| 0x477710 | g_SceneSingletons | 冲突: g_SceneNameBuffer/g_SceneSingletons |
| 0x47773c | g_HudFragmentSlotOwner |  |
| 0x477748 | g_SceneConfigTablePointer |  |
| 0x477783 | g_BgmQueueFlagsBase |  |
| 0x477784 | g_StageRecordSlot |  |
| 0x477810 | g_TitleScreen | 冲突: g_GameModeRecord/g_MainChainContext/g_ManagerObject810/g_SoundGateOwner/g_TitleScreen |
| 0x477814 | g_TextLayerManagerSlot | 冲突: g_PublishedHintState/g_TextLayerManagerSlot/g_TitleStateLists |
| 0x477818 | g_BulletManagerSlot | 冲突: g_BulletManagerSlot/g_EffectPoolManager/g_ExplosionManager |
| 0x47781c | g_BulletListRoot | 冲突: g_BulletListRoot/g_BulletListRootSlot/g_StageObjectManager |
| 0x477820 | g_GlobalLifecycleManager | 冲突: g_GlobalLifecycleManager/g_StageEntityRecord |
| 0x477824 | g_ManagerBackgroundVmIdA |  |
| 0x477828 | g_ManagerBackgroundVmIdB |  |
| 0x47782c | g_ManagerBackgroundVmIdC |  |
| 0x477830 | g_GameStateManager | 冲突: g_GameStateManager/g_ScoreRecordOwner |
| 0x477834 | g_OptionPositionBase | 冲突: g_OptionPositionBase/g_OptionPositionManager/g_PlayerRecord/g_PlayerStateBlock/g_PlayerStateBlock834/g_ScreenTargetBlock |
| 0x477838 | g_GameModeObject | 冲突: g_GameModeObject/g_UnknownMainChainObject |
| 0x47783c | g_SpellPracticeRecords | 冲突: g_ScoreSaveState/g_TimelineAudioFlags/g_TitleScoreSaveRecord |
| 0x477840 | g_MainChainObject840 | 冲突: g_MainChainObject840/g_PointItemDigitState/g_TextEffectOwnerSlot |
| 0x477844 | g_ScriptTestManagerSlot |  |
| 0x477848 | g_PublishedModeRecord | 冲突: g_ModeRecordTablePointer/g_PublishedModeRecord/g_TitleTransitionRecord |
| 0x47784c | g_GameManager |  |
| 0x477850 | g_LoadedArchiveCount |  |
| 0x477858 | g_ArchiveSlotBase |  |
| 0x48f860 | g_ArchiveSlotIndex |  |
| 0x4918a0 | g_GdiFontHandles |  |
| 0x4918a4 | g_AsciiOverlayUpdateSuspended | 冲突: g_AsciiOverlayUpdateSuspended/g_MainChainManagerGate/g_SceneWord4918a4 |
| 0x4918a8 | g_MainChainStartupTickLow1 |  |
| 0x4918b0 | g_TimelinePrngStateB | 冲突: g_AsciiOverlayRandomState/g_GeneratedRandomState/g_MainChainStartupTickLow0/g_ScorePrngState/g_TimelinePrngStateB |
| 0x4918b4 | g_GeneratedRandomCounter |  |
| 0x4918b8 | g_MainChainJoystickCaps |  |
| 0x491be0 | g_MainChainPointerTable |  |
| 0x491be4 | g_CallbackScheduler | 冲突: g_CallbackScheduler/g_SchedulerHeap/g_SchedulerRoot |
| 0x491be8 | g_PreopenedScriptHandle |  |
| 0x491bec | g_ManagerSlotAuxFlag |  |
| 0x491bf0 | g_PlayerShotEntryCache |  |
| 0x491c00 | g_UnknownTransitionStatus | 冲突: g_MainChainManagerFlow/g_UnknownTransitionStatus |
| 0x491c08 | g_StageSelectMemory |  |
| 0x491c10 | g_MainChainRenderOwner | 冲突: g_EntityPoolManager/g_MainChainRenderOwner/g_ManagerWorkOwner/g_RenderOwner/g_RenderOwner910/g_StageScriptWorkSlot |
| 0x491c14 | g_GameScheduler | 冲突: g_GameScheduler/g_TimeWords |
| 0x491c28 | g_MainChainContext | 冲突: g_GameManagerSlot/g_MainChainContext/g_MainChainContextSlot |
| 0x491c2c | g_Direct3D9 |  |
| 0x491c30 | g_MainChainD3D9Device | 冲突: g_D3D9ClearDevice/g_MainChainD3D9Device/g_MainChainD3DDevice/g_ManagerWorkD3DDevice/g_TitleOwnerHookTarget |
| 0x491c38 | g_InputAxisByteA |  |
| 0x491c3c | g_InputAxisByteB |  |
| 0x491c40 | g_TipEntityManager | 冲突: g_EntityManager/g_TipEntityManager |
| 0x491c70 | g_MainChainD3DWindow |  |
| 0x491c74 | g_MainChainInitialView |  |
| 0x491cb4 | g_MainChainInitialProjection |  |
| 0x491cf4 | g_MainChainInitialViewport | 冲突: g_AsciiOverlayViewport/g_MainChainInitialViewport |
| 0x491d0c | g_D3D9PresentationParameters |  |
| 0x491d14 | g_MainChainBackBufferFormat |  |
| 0x491d44 | g_MainChainTimerObject |  |
| 0x491d48 | g_MainChainConfigBytes | 冲突: g_MainChainConfigBytes/g_MainChainConfiguration/g_TitleStateDefaults |
| 0x491d4c | g_ResultStatusSnapshot |  |
| 0x491d62 | g_MainChainBackBufferFormatOption |  |
| 0x491d65 | g_MainChainFallbackWindowMode |  |
| 0x491d66 | g_RegistrationFrameIncrement |  |
| 0x491d68 | g_BgmVolumeInputConfig |  |
| 0x491d69 | g_BgmVolumeEnabledConfig |  |
| 0x491d6a | g_ExtraSaveSelector | 冲突: g_ExtraSaveSelector/g_HintFileGate |
| 0x491d78 | g_MainChainRuntimeOptions | 冲突: g_BgmModeFlags/g_GlyphFormatGateByte/g_MainChainRuntimeOptions/g_ManagerWorkFormatRemapFlag/g_ReplayOverwriteGate/g_SoundStopFlags/g_TextureQualityGate |
| 0x491d7c | g_AsciiCameraWork | 冲突: g_AsciiCameraWork/g_AsciiFogPosition |
| 0x491e64 | g_AsciiOverlayRenderOffsetX | 冲突: g_AsciiOverlayRenderOffsetX/g_MainChainDrawFinalizeField0 |
| 0x491e68 | g_AsciiOverlayRenderOffsetY | 冲突: g_AsciiOverlayRenderOffsetY/g_MainChainDrawFinalizeField1 |
| 0x491e78 | g_AsciiFogNearDistance |  |
| 0x491e7c | g_AsciiFogCutoffDistance |  |
| 0x491e80 | g_AsciiFogBlue |  |
| 0x491e84 | g_AsciiFogGreen |  |
| 0x491e88 | g_AsciiFogRed |  |
| 0x491e90 | g_AsciiFogFixedRgb |  |
| 0x491fac | g_MainChainActiveCameraWork | 冲突: g_MainChainActiveCameraWork/g_MainChainFrameViewportBlock |
| 0x491fb0 | g_AsciiActiveViewIsDefault | 冲突: g_AsciiActiveViewIsDefault/g_MainChainActiveView |
| 0x491fb4 | g_MainChainRegistrationState |  |
| 0x491fb8 | g_MainChainSharedStatus | 冲突: g_MainChainSharedStatus/g_PostGameOverState/g_SharedStatusGate |
| 0x491fc0 | g_MainChainRegistrationField |  |
| 0x491fc4 | g_GameActiveFlag |  |
| 0x491fd4 | g_MainChainDeviceRecoveryState |  |
| 0x491fdc | g_MainChainDeviceFallbackMode |  |
| 0x491fe0 | g_MainChainDeviceRefreshFallback |  |
| 0x491fe4 | g_MainChainDeviceInitState |  |
| 0x491fe8 | g_GlobalMidiOutput | 冲突: g_GlobalMidiOutput/g_MainChainMidiOutput |
| 0x491ff4 | g_MainChainRuntimeFlags | 冲突: g_InputModeFlags/g_MainChainInputStateWord/g_MainChainRuntimeFlags/g_ReplayModeFlags/g_ScoreNamePending/g_StagePausePending |
| 0x491ff8 | g_MainChainStartupTick |  |
| 0x492000 | g_MainChainD3DCaps |  |
| 0x492254 | g_MainChainSecondaryControl |  |
| 0x492260 | g_ManagerGateRun |  |
| 0x492264 | g_ManagerGateReady |  |
| 0x492274 | g_CallbackSchedulerLock |  |
| 0x4922a4 | g_ResourceLoaderLock |  |
| 0x4922ec | g_MainChainFrameClockLock |  |
| 0x49231c | g_CallbackSchedulerActivityDepth |  |
| 0x49231e | g_ResourceLoaderNesting | 冲突: g_ResourceLoaderActivityDepth/g_ResourceLoaderNesting |
| 0x492321 | g_MainChainFrameClockDepth |  |
| 0x492378 | g_AsciiFogEnableCache | 冲突: g_AsciiFogEnableCache/g_MainChainPresentColor |
| 0x492380 | g_MainChainExecutableChecksum |  |
| 0x492384 | g_MainChainExecutableSize |  |
| 0x492388 | g_VersionDataSize |  |
| 0x49238c | g_VersionData |  |
| 0x4923a0 | g_MainChainFrameOverrun |  |
| 0x4923a8 | g_MainChainClearColor |  |
| 0x4923b0 | g_LoadedArchiveTable |  |
| 0x4924f0 | g_MainChainWindow | 冲突: g_MainChainFrameState/g_MainChainWindow |
| 0x4924f4 | g_MainChainExitRequested |  |
| 0x4924f8 | g_MainChainApplicationInstance |  |
| 0x4924fc | g_MainChainWindowInputEnabled |  |
| 0x492500 | g_MainChainWindowMouseState |  |
| 0x492508 | g_MainChainPerformanceFrequency | 冲突: g_MainChainPerformanceFrequency/g_MainChainPerformanceFrequencyLo |
| 0x49250c | g_MainChainPerformanceFrequencyHi |  |
| 0x492510 | g_MainChainPerformanceBase |  |
| 0x492518 | g_MainChainAlternateLaunchPath |  |
| 0x49251c | g_ScreenSaverWasActive | 冲突: g_MainChainScreenSaverWasActive/g_ScreenSaverWasActive |
| 0x492520 | g_LowPowerWasActive | 冲突: g_LowPowerWasActive/g_MainChainLowPowerWasActive |
| 0x492524 | g_PowerOffWasActive | 冲突: g_MainChainPowerOffWasActive/g_PowerOffWasActive |
| 0x492528 | g_MainChainFrameCalculationTick |  |
| 0x492530 | g_MainChainFrameDrawTick |  |
| 0x492538 | g_MainChainFramePreviousTick |  |
| 0x492540 | g_MainChainFrameClockEpoch |  |
| 0x492590 | g_TransitionRoot | 冲突: g_SoundGateContext/g_TransitionRoot |
| 0x494518 | g_ModeWorkerBusy |  |
| 0x4977a8 | g_MainChainSoundWorker |  |
| 0x4977ac | g_MainChainResourceThread |  |
| 0x4977b0 | g_MainChainSoundWorkerId |  |
| 0x4977b4 | g_MainChainResourceGate |  |
| 0x4977b8 | g_MainChainSoundWorkerWindow |  |
| 0x4977bc | g_MainChainSoundWorkerFinished |  |
| 0x497854 | g_TransitionVolumeScale |  |
| 0x497858 | g_BgmSoundSequenceVolumeScale |  |
| 0x497990 | g_PackedArchive |  |
| 0x4979a0 | g_ExtraCodeIdleTimer |  |
| 0x4979a4 | g_ExtraCodeSequenceIndex |  |
| 0x4979a8 | g_KeyboardPressedEdge |  |
| 0x497ba8 | g_MainChainInstanceMutex |  |
| 0x497d80 | g_TitlePlayfieldOrigin |  |
| 0x497d8c | g_TitleOriginGuard |  |
| 0x497d90 | g_KeyboardStatePrevious |  |
| 0x497e90 | g_KeyboardStateCurrent |  |

## 常量账本

### 浮点常量（38 个，均已 set_type(float/double) 并按源码注释命名）

| 地址 | 名称 | 值 | 备注 |
|---|---|---|---|
| 0x470afc | kTimerStep | 1.0f |  |
| 0x470b04 | kBenchXHigh | 0.0f |  |
| 0x470b08 | kTwo (别名) | FloatFromBits(1073741824U) | 保留 g_AsciiBarHeight（语义更强）；kTwo=2.0f 为局部别名 |
| 0x470b0c | kHalf | FloatFromBits(1056964608U) |  |
| 0x470b14 | kTwoPi | 6.2831855f |  |
| 0x470b18 | kPi | 3.14159265f |  |
| 0x470b3c | kBossOffHigh | 192.0f |  |
| 0x470b40 | kBossOffLow | -192.0f |  |
| 0x470b48 | kSpawnOffsetY (别名) | 16.0f | 保留 g_AsciiObjectScrollY；kSpawnOffsetY=16.0f 为局部别名 |
| 0x470b4c | kBossBaseX (别名) | 224.0f | 保留 g_AsciiObjectScrollX；kBossBaseX=224.0f 为局部别名 |
| 0x470b50 | kClampLow | -990.0f |  |
| 0x470b5c | kBenchXLow | -64.0f |  |
| 0x470b60 | kMinusOne | -1.0f |  |
| 0x470b64 | kTimerHigh | 1.01f |  |
| 0x470b68 | kTimerLow | 0.99f |  |
| 0x470bc8 | kBenchYLow | 64.0f |  |
| 0x470bdc | kEnterY | 416.0f |  |
| 0x470be0 | kTerminalX | FloatFromBits(1139539968U) |  |
| 0x470bf4 | kPhaseBLine | FloatFromBits(1124073472U) |  |
| 0x470c28 | kBenchYHigh | 80.0f |  |
| 0x470c38 | kAccelStep | FloatFromBits(1045220557U) |  |
| 0x470c40 | kAnchorScale | 4.0f |  |
| 0x470c80 | kPhaseALine | FloatFromBits(1119879168U) |  |
| 0x470c84 | kFollowScale | FloatFromBits(1028443341U) |  |
| 0x470cc4 | kRetargetScale | FloatFromBits(1051372203U) |  |
| 0x470cc8 | kFallbackYLow | -999.0f |  |
| 0x470ccc | kClampHigh | 990.0f |  |
| 0x470cd0 | kBonusScale | FloatFromBits(995595318U) |  |
| 0x470cd4 | kBonusLineY | FloatFromBits(1125122048U) |  |
| 0x470cd8 | kAccelGate | FloatFromBits(1094713344U) |  |
| 0x470cdc | kGravityStep | FloatFromBits(1022739087U) |  |
| 0x470ce0 | kBonusBias | FloatFromBits(1167867904U) |  |
| 0x470ce8 | kBossAlphaScale | -2.984375f |  |
| 0x470cf0 | kBossNearX | 64.0 |  |
| 0x470cf8 | kHpFillStep | 0.025f |  |
| 0x470d00 | kLeaveX | -112.0 |  |
| 0x470d08 | kLeaveY | 400.0f |  |
| 0x470d10 | kEnterX | -128.0 |  |

### 字符串账本（游戏专用串，共 175 条，全量见 find_regex 结果；按用途分组）

- **归档/脚本**: th10.dat (0x46e000), thbgm.dat (0x46dbe0), scoreth10.dat (0x46e848), th10.cfg (0x46f770), ../../bgm/thbgm.fmt (0x46dc14), ../../data/*.ecl (0x46cea8)
- **关卡资源表** (0x46e9b0-0x46ecdc): stage00-07 每关五元组 stageNN.ecl / stageNN.std / stgenmNN.anm / stNNlogo.anm / bgm/th10_NN.wav（含 13/15/16/17 等散布副本）
- **玩家/标题**: pl00.anm (0x46e42c), pl01.anm (0x46e420), title.anm (0x46f0ac), title_v.anm (0x46f0a0), capture/text/ascii/bullet/front/sig.anm
- **音效** (0x46fe80-0x4700b4): se_*.wav 34 个（se_pldead00/se_ok00/se_cancel00/se_graze/se_extend/se_cardget 等）
- **replay/score 格式**: th10_%.2d.rpy (0x46e0c8), th10_ud????.rpy (0x46efec), th10_%.4x%c.ver (0x46dff0), replay/%s (0x46e644), 排名行格式 (0x46e0e8, 0x46ee50)
- **ZUN 调试断言串**（揭示原始内部名，与本项目模块命名互证）: PlayerInf, ReplayInf, ScoreInf, SmollScoreInf, StageFileHeaderInf, ReplayStageDataHeaderInf, ReplayFrameDataInf, ScoreFileHeaderInf, ScoreChunkInf；路径前缀 c:\cygwin\home\zun\prog\th10\src\...
- **BGM 状态日志** (0x46faa4-0x46fd24): "Sound : Play Stage"/"Streming BGM *"/"error : bgmfile is not find %s"

## 冲突仲裁（75 个多义地址）

原则：同一 dword 在不同模块以不同角色访问时，主名取「跨模块语义」；每个地址保留别名清单。
对 37 个核心地址逐一点取了 xref 引用函数做证据；其余以源码模块上下文为准。

| 地址 | 主名（IDB 现名） | 别名 | 证据/裁决 |
|---|---|---|---|
| 0x474788 | g_StageRecordTable | g_TitleTransitionRecords | xref: SelectStageConfigTable/SelectStageRecordSlot/RunManagerStateBody9/C —— 是关卡配置表而非标题过渡记录 |
| 0x474c40 | g_ScoreBlock | g_MainChainFrameStateBlock, g_MaximumScore, g_HighScoreValue, g_StageFramePeak | xref 52 处：ECL setup/敌死序列/结果屏 —— 分数/状态快照块 |
| 0x474c44 | g_CurrentRunScore | g_SceneSubTimer, g_CurrentRunScoreValue, g_CurrentScore, g_StageFrameCount | 源码模块上下文 |
| 0x474c48 | g_PlayerPowerGauge | g_PlayerLivesCounter, g_PlayerPowerGaugeDword, g_PlayerPowerGaugeWord, g_SceneWord48 | 源码模块上下文 |
| 0x474c4c | g_PlayerPowerPool | g_ScoreBonusBaseDword, g_ScorePool | xref 16 处：符卡练习/脚本名请求/子弹管理 |
| 0x474c50 | g_BossDefeatedFlag | g_SceneSelector50 | xref 2 处：ECL setup、SetupGameSceneFromTitle |
| 0x474c68 | g_PlayerCharacter | g_StageTextSprites, g_SceneStageIndexA, g_PlayerCharacterDamage, g_RunChara, g_PopupStepSelector | 源码模块上下文 |
| 0x474c6c | g_PlayerShotType | g_RunCharaSlot, g_SceneStageIndexB | 源码模块上下文 |
| 0x474c70 | g_PlayerLivesRemaining | g_PracticeStartIndex | xref 20 处：HUD 重置/结果屏/开场 —— 残机数 |
| 0x474c74 | g_CurrentDifficulty | g_TimelinePhase, g_StageScoreSelector, g_PlayerDifficulty, g_RunShot | xref 79 处：Timeline gate/ECL —— 难度选择 |
| 0x474c7c | g_ActiveTextLayer | g_LastTitleTransitionIndex, g_StageSelectorIndex, g_PlayerShotMode, g_StageIndex, g_SceneModeSelector, g_RunStage | 源码模块上下文 |
| 0x474c80 | g_TitleTransitionIndex | g_StageSelectorCurrent, g_SceneModeSelectorMirror, g_StageIndexMirror | 源码模块上下文 |
| 0x474c84 | g_TextStyleFlag | g_StageSubState, g_SceneFlag84, g_ResultScriptSlotCount | xref 16 处：符卡练习/HUD/结果屏槽位 |
| 0x474c88 | g_TextStyleDefault | g_PlayerContinueMode, g_SceneFlag88 | xref 6 处：标题 calc —— 继续模式/文本样式 |
| 0x474c8c | g_TextStyleAlt | g_SceneFlag8c, g_ResultScriptSlotCache | 源码模块上下文 |
| 0x474c98 | g_ScorePenaltyCounter | g_ResultScore, g_RunRankValue, g_SceneRankValue | 源码模块上下文 |
| 0x474c9c | g_ExtendRequirementIndex | g_ResultPower, g_ScenePlayCountSeed | xref: UpdateInGameScoreDisplay 用作 dword_474474/474488 延命表下标且奖命后自增 |
| 0x474ca0 | g_GlobalModeFlags | g_GameModeFlags, g_GameRunFlags, g_ManagerModeFlags, g_StagePauseGate, g_ScoreNameStage | xref 52 处：标题销毁/HUD —— 全局模式标志 |
| 0x474ca4 | g_DemoWaitCounter | g_ResultPiv | 源码模块上下文 |
| 0x474cac | g_CurrentStageIndex | g_PracticeStartRequest, g_ClearFlagScan | 源码模块上下文 |
| 0x474e36 | g_ManagerSubGateFlags | g_MainChainPresentDiagnosticOptions, g_StagePauseTrigger, g_ScoreNameTrigger | xref 82 处：ECL 选择菜单为主 |
| 0x474e5c | g_InputMask | g_SceneGateFlags, g_InputMaskWord | xref 20 处：ECL 菜单/结果屏/子弹管理 —— 输入掩码 |
| 0x474e88 | g_MainChainInputBindings | g_TitleGeometryDefaults | 源码模块上下文 |
| 0x474f70 | g_EndingMidiErrorConsole | g_MainChainErrorBuffer, g_MainChainErrorReceiver | 源码模块上下文 |
| 0x476f78 | g_FrameTimeScale | g_MainChainStartupScale, g_AsciiOverlayInitialRate, g_SceneFadeScale | xref 168 处，值 0.5f —— 帧时间比例 |
| 0x4776e0 | g_AsciiManagerHost | g_AsciiManager, g_LoadingScreenContext | xref 108 处：AsciiManager 构造/析构为主 |
| 0x4776e8 | g_TitleScreenStateSecondary | g_SpellBannerFlagOwner | 源码模块上下文 |
| 0x4776ec | g_GameContext | g_GameContextObject, g_StageNode | 源码模块上下文 |
| 0x4776f0 | g_EffectManagerRoot | g_EnemyArrayBase, g_SceneCommandManager, g_StageRecordHolder | xref 48 处：特效管理器根生命周期 |
| 0x4776f4 | g_SpellBulletBase | g_StageState, g_SpellBulletGate | xref 22 处：场景触发/炸弹伤害常量 |
| 0x477700 | g_TransitionObject | g_TimelineGateState | 源码模块上下文 |
| 0x477704 | g_AsciiHudConditionalState | g_AsciiHudConditionalStateDamage, g_BossBattleState | 源码模块上下文 |
| 0x477708 | g_RegistrationDrawOwner | g_SlowRateStats, g_SceneTimeSource | xref: CreateRegistrationDrawOwner/DestroyRegistrationDrawOwner —— 原 g_SlowRateStats 判定错误 |
| 0x47770c | g_AsciiHudOwner | g_AsciiHudOverlayState | xref 57 处：HUD/符卡故事状态 |
| 0x477710 | g_SceneSingletons | g_SceneNameBuffer | 源码模块上下文 |
| 0x477810 | g_TitleScreen | g_SoundGateOwner, g_GameModeRecord, g_ManagerObject810, g_MainChainContext | 源码模块上下文 |
| 0x477814 | g_TextLayerManagerSlot | g_PublishedHintState, g_TitleStateLists | xref 6 处：标题状态列表 + 提示状态 |
| 0x477818 | g_BulletManagerSlot | g_ExplosionManager, g_EffectPoolManager | xref 25 处：开场扫描/清弹/舞台宿主 |
| 0x47781c | g_BulletListRoot | g_BulletListRootSlot, g_StageObjectManager | 源码模块上下文 |
| 0x477820 | g_GlobalLifecycleManager | g_StageEntityRecord | 源码模块上下文 |
| 0x477830 | g_GameStateManager | g_ScoreRecordOwner | xref 10 处：游戏状态管理器对象 |
| 0x477834 | g_OptionPositionBase | g_ScreenTargetBlock, g_OptionPositionManager, g_PlayerStateBlock834, g_PlayerStateBlock, g_PlayerRecord | xref 71 处：场景触发/选项位置 —— 玩家记录块 |
| 0x477838 | g_GameModeObject | g_UnknownMainChainObject | 源码模块上下文 |
| 0x47783c | g_SpellPracticeRecords | g_TimelineAudioFlags, g_TitleScoreSaveRecord, g_ScoreSaveState | 源码模块上下文 |
| 0x477840 | g_MainChainObject840 | g_PointItemDigitState, g_TextEffectOwnerSlot | xref 15 处：标题/子弹/舞台宿主 |
| 0x477848 | g_PublishedModeRecord | g_TitleTransitionRecord, g_ModeRecordTablePointer | xref 26 处：场景脚本资源/关卡表 |
| 0x4918a4 | g_AsciiOverlayUpdateSuspended | g_MainChainManagerGate, g_SceneWord4918a4 | 源码模块上下文 |
| 0x4918b0 | g_TimelinePrngStateB | g_MainChainStartupTickLow0, g_GeneratedRandomState, g_AsciiOverlayRandomState, g_ScorePrngState | 源码模块上下文 |
| 0x491be4 | g_CallbackScheduler | g_SchedulerHeap, g_SchedulerRoot | xref 135 处：回调调度器 |
| 0x491c00 | g_UnknownTransitionStatus | g_MainChainManagerFlow | 源码模块上下文 |
| 0x491c10 | g_MainChainRenderOwner | g_ManagerWorkOwner, g_RenderOwner, g_EntityPoolManager, g_RenderOwner910, g_StageScriptWorkSlot | 源码模块上下文 |
| 0x491c14 | g_GameScheduler | g_TimeWords | 源码模块上下文 |
| 0x491c28 | g_MainChainContext | g_MainChainContextSlot, g_GameManagerSlot | xref 24 处：主链上下文（结构体断言证实） |
| 0x491c30 | g_MainChainD3D9Device | g_D3D9ClearDevice, g_ManagerWorkD3DDevice, g_MainChainD3DDevice, g_TitleOwnerHookTarget | xref 216 处：D3D9 设备 |
| 0x491c40 | g_TipEntityManager | g_EntityManager | 源码模块上下文 |
| 0x491cf4 | g_MainChainInitialViewport | g_AsciiOverlayViewport | 源码模块上下文 |
| 0x491d48 | g_MainChainConfiguration | g_MainChainConfigBytes, g_TitleStateDefaults | xref: LoadMainChainConfiguration 5 处访问 |
| 0x491d6a | g_ExtraSaveSelector | g_HintFileGate | xref 12 处：提示回调安装/释放 + ManagerBody4 |
| 0x491d78 | g_MainChainRuntimeOptions | g_ManagerWorkFormatRemapFlag, g_SoundStopFlags, g_GlyphFormatGateByte, g_BgmModeFlags, g_ReplayOverwriteGate, g_TextureQualityGate | 源码模块上下文 |
| 0x491d7c | g_AsciiCameraWork | g_AsciiFogPosition | 源码模块上下文 |
| 0x491e64 | g_AsciiOverlayRenderOffsetX | g_MainChainDrawFinalizeField0 | xref 10 处：标题绘制 pass/DrawFinalize/overlay |
| 0x491e68 | g_AsciiOverlayRenderOffsetY | g_MainChainDrawFinalizeField1 | xref 9 处：同上（Y） |
| 0x491fac | g_MainChainActiveCameraWork | g_MainChainFrameViewportBlock | 源码模块上下文 |
| 0x491fb0 | g_AsciiActiveViewIsDefault | g_MainChainActiveView | 源码模块上下文 |
| 0x491fb8 | g_MainChainSharedStatus | g_PostGameOverState, g_SharedStatusGate | xref 24 处：共享状态门 |
| 0x491fe8 | g_GlobalMidiOutput | g_MainChainMidiOutput | xref 4 处：WinMain/窗口过程 —— MIDI 输出 |
| 0x491ff4 | g_MainChainRuntimeFlags | g_ReplayModeFlags, g_InputModeFlags, g_StagePausePending, g_ScoreNamePending, g_MainChainInputStateWord | 源码模块上下文 |
| 0x49231e | g_ResourceLoaderNesting | g_ResourceLoaderActivityDepth | 源码模块上下文 |
| 0x492378 | g_AsciiFogEnableCache | g_MainChainPresentColor | xref 18 处：标题绘制 pass —— 雾开关缓存 |
| 0x4924f0 | g_MainChainWindow | g_MainChainFrameState | 源码模块上下文 |
| 0x492508 | g_MainChainPerformanceFrequency | g_MainChainPerformanceFrequencyLo | 源码模块上下文 |
| 0x49251c | g_ScreenSaverWasActive | g_MainChainScreenSaverWasActive | 源码模块上下文 |
| 0x492520 | g_LowPowerWasActive | g_MainChainLowPowerWasActive | xref 3 处：WinMain/系统设置 —— 低电量标志 |
| 0x492524 | g_PowerOffWasActive | g_MainChainPowerOffWasActive | xref 3 处：同上 —— 断电标志 |
| 0x492590 | g_TransitionRoot | g_SoundGateContext | 源码模块上下文 |

## IDB 命名待补（item 非独立头，共 15 个）

这些地址的类型已正确设置，但字节位于相邻更大 item 内部，rename 工具按名寻址失败。
语义名以本账本为准，源码 extern 注释一致。后续可用 IDA 手工拆分 item 后命名。

- 0x474cb4 = g_CurrentStageSceneId
- 0x474cc0 = g_PracticeStageValue2
- 0x474cc8 = g_PracticeStageValue
- 0x474cd0 = g_ReplayPathFlags
- 0x474e38 = g_ManagerConfirmFlagsSecond
- 0x47773c = g_HudFragmentSlotOwner
- 0x477748 = g_SceneConfigTablePointer
- 0x477783 = g_BgmQueueFlagsBase
- 0x477784 = g_StageRecordSlot
- 0x491c40 = g_TipEntityManager
- 0x492274 = g_CallbackSchedulerLock
- 0x4924f0 = g_MainChainWindow
- 0x49250c = g_MainChainPerformanceFrequencyHi
- 0x49251c = g_ScreenSaverWasActive
- 0x4977b0 = g_MainChainSoundWorkerId

- 0x447708 原登记 g_RegistrationDrawOwner 判定为**错误标注**：该地址位于 .text 内（函数 PollManagerWorkQueuesStackAbi 之后）；真正的 g_RegistrationDrawOwner = 0x477708（本轮已改判并写入 IDB）。

## 结构体类型迁移（本轮新增）

从 src/*.hpp 的 offsetof 断言迁移进 IDA 类型库（declare_type），尺寸全部吻合：

| 结构体 | 尺寸 | 绑定 |
|---|---|---|
| ChainLink/ChainElem/CallbackScheduler | 0x48 | `g_CallbackScheduler` (0x491BE4) → `struct CallbackScheduler *` |
| TimelineGateStatePartial | 0x28 | `g_TimelineGateState` (0x477700) → `struct TimelineGateStatePartial *`；主名由 g_TransitionObject 改判 |
| BgmCommandPartial (0x10C) + TransitionControlPartial (0x20) + TransitionRootPartial | 0x52D0 | `g_TransitionRoot` (0x492590) → `struct TransitionRootPartial *` |
| MainChainContext | 0x784 | `g_MainChainContext` (0x491C28) → `struct MainChainContext *` |

函数签名（__usercall 寄存器参数定型，反编译已出现 context->requested_state / state_locks[5] / transition_color 等命名字段）：
- `0x4218D0 MainChainAdvanceState@<eax>(struct MainChainContext *context@<eax>)`
- `0x421E00 AdvanceTransitionEaxAbi@<eax>(struct TransitionRootPartial *root@<eax>)`（同址 TickBgmFadeSequencerEaxAbi 自动获得 root-> 字段）
- `0x41FF80` 新命名 MainChainContextUpdate，thiscall 自动携带 MainChainContext*

TransitionRootPartial 布局同时裁决了旧别名：0x497854=bgm_volume_input、0x497858=bgm_volume_enabled、0x49785C=bgm_volume（原 g_TransitionVolumeScale/g_BgmSoundSequenceVolumeScale 判定为误名）；0x4977A8/AC/B0 = bgm_control_thread0/1/thread_id（与 g_MainChainSoundWorker/ResourceThread/SoundWorkerId 同址同义）。

## 待补命名收尾

通过调用点反编译发现容器头并完成命名 4 个：
- 0x4924F0 `hWnd` → g_MainChainWindow
- 0x4977B0 `ThreadId` → g_MainChainSoundWorkerId
- 0x49251C `pvParam` → g_ScreenSaverWasActive
- 0x492274 `CriticalSection` → g_MainChainCriticalSections（7 个 CRITICAL_SECTION 数组头；g_CallbackSchedulerLock 为首元素别名）

其余 11 个确认无法按名寻址（位于相邻更大 item 内部，且部分地址无独立代码引用，如 0x474CB4 xref=0，疑为 g_GeneratedSurface 等记录的内部字段）：
0x474CB4, 0x474CC0, 0x474CC8, 0x474CD0, 0x474E38, 0x47773C, 0x477748, 0x477783, 0x477784, 0x491C40, 0x49250C（QWORD 高半部，头在 0x492508）。
语义名以本账本与源码 extern 注释为准。

## 结构体迁移第二轮（2026-10-01，本会话）

### 新增 IDB struct（尺寸全部经 type_inspect 验证 + MSVC 编译期断言复核）

| struct | 尺寸 | 源码权威定义 |
|---|---|---|
| VmRecord | 0x3AC (940) | src/VmRecord.hpp（本轮新建；此前 0x3AC 布局散落在 20+ 文件注释里） |
| Vec3InterpBlock | 0x4C | src/VmRecord.hpp |
| AlphaInterpBlock | 0x2C | src/VmRecord.hpp |
| ScaleInterpBlock | 0x3C | src/VmRecord.hpp |
| GameContext | 0x48 (72) | src/GameContext.hpp（本轮新建） |
| EclInstruction | 0x12 (18) | src/EclScriptVm.hpp |
| EclRunContext | 0x1024 (4132) | src/EclScriptVm.hpp |
| EclContextNode | 0xC | src/EclScriptVm.hpp |
| LargeRenderOwnerLayout | 0x732460 (7545952) | src/LargeRenderOwner.cpp（骨架）+ MainChainRenderAdapters/LargeRenderOwnerFrameLoop 字段级证据 |
| CachedSurfaceDescriptor | 0x1C (28) | src/MainChainRender.hpp |
| AsciiManager | 0x89AC (35244) | src/AsciiManager.hpp |
| AsciiManagerString | 0x68 (104) | src/AsciiManager.hpp |

修正过程记录：初版 VmRecord 934 字节，两处错误——AlphaInterpBlock 少 4 字节（timer 应为 4 个 dword）与 bind_frame_stamp@0x380 为 dword 不能与 gap0382 并存；LargeRenderOwnerLayout 初版 +60 字节（gap3ad130 应为 856、vertex_arena 应为 3670024 的笔误）。均以二分法定位后修正。

### 新增全局指针绑定

| 地址 | 名称 | 类型 |
|---|---|---|
| 0x4776EC | g_GameContext | `struct GameContext *` |
| 0x491C10 | g_MainChainRenderOwner | `struct LargeRenderOwnerLayout *` |
| 0x4776E0 | g_AsciiManagerHost | `struct AsciiManager *`（SpellBulletVtable.cpp LoadPointerAt(0x4776e0) 证实持指针） |
| 0x491C14 | g_GameScheduler | `struct CallbackScheduler *` |

### 新增函数签名（39 个，寄存器绑定均以 prologue 反汇编逐个核实）

ECL VM 族（ctx 均为 struct EclRunContext *）：
- 0x44FF00 EclVmEvalIntArg@<eax>(ctx@<edx>, arg_index, raw_value@<eax>)
- 0x44FF80 EclVmEvalFloatArg@<st0>(ctx@<eax>, arg_index@<cl>, fallback)
- 0x44FE40 EclVmEvalFloatArgFromIns@<st0>(ctx@<eax>, arg_index@<ecx>)
- 0x44FDB0 EclVmEvalIntArgFromIns@<eax>(ctx@<edx>, arg_index)
- 0x450030 EclVmResolvePointerArg@<eax>(ctx@<esi>, arg_index@<cl>)
- 0x450070 EclVmResolveStringArg@<eax>(ctx@<eax>, arg_index@<cl>)
- 0x450160 FindEclContextNodeById@<eax>(manager@<eax>, script_id@<ecx>)
- 0x44DF70 BeginEclSubFrame@<eax>(ctx, initial_value, parent, first_arg_index)
- 0x44FD10 RunEclContextListEdiStackAbi@<eax>(manager@<edi>, delta)
- 0x44E1A0 ExecuteEclInstruction@<eax>(ctx@<eax>, delta) —— 整个 opcode 解释器反编译已全字段化（ctx->ins/stack_cursor/chunk、ins->opcode/variable_mask/rank_mask）

实体/VM 记录族（owner 均为 struct LargeRenderOwnerLayout *，vm 为 struct VmRecord *）：
- 0x4491C0 FindEntityEdxStackAbi@<eax>(owner@<edx>, id) → struct VmRecord *
- 0x4492A0 ReleaseEntityById@<eax>(owner@<edx>, id)
- 0x449470 SetEntityStateWordEaxEsiAbi@<eax>(id_slot@<eax>, value@<esi>)
- 0x449950 AllocatePoolVmEsiAbi@<eax>(owner@<esi>)（反编译已显示 owner->pool_cursor/pooled_nodes[]/pooled_node_active[] + operator new(0x3AC)）
- 0x449870 AssignPoolVmScriptEcxEaxAbi@<eax>(vm, script_id)
- 0x4489D0/0x448A50 LinkEntity(Front)AndAssignIdEaxEsiAbi@<eax>(out_id@<eax>, entity@<ebx>, owner@<edx>)
- 0x409E50 ExpireEntityHandleEaxAbi@<eax>(handle@<eax>)；0x40C4D0 FireEntityHandleEaxAbi@<eax>(handle@<eax>)
- 0x4493E0 ReleaseEntitiesUsingResourceEaxEdxAbi@<eax>(owner@<eax>, resource@<edx>)

GameContext 族：
- 0x405750 UpdateSceneTriggerPopupEdiAbi@<eax>(ctx@<edi>)
- 0x405860 TickRespawnDeathEffectStackAbi@<eax>(ctx)
- 0x4059F0 ComputeBombAreaDamageThisAbi（thiscall this=struct GameContext *, position）
- 0x4055C0 RegisterGameContextSchedulerRecordsEbxAbi@<eax>(ctx@<ebx>)

RenderOwner 族：
- 0x442F50 FlushRenderOwnerPendingVerticesEsiAbi@<eax>(owner@<esi>)
- 0x438A30 ReleaseResetSensitiveRenderSlots@<eax>(owner@<eax>)
- 0x445900 ConstructLargeRenderOwner@<eax>(owner)
- 0x446220 DestroyLargeRenderOwnerInPlace@<eax>(owner)

ABI 语法经验：__usercall 必须带返回位置注解（@<eax>），否则报 "Not a function type"；double 返回用 @<st0>。

### 整表常量

| 地址 | 名称 | 内容 | 证据 |
|---|---|---|---|
| 0x474474 | g_ExtendScoreTableA | int[5] {2000000, 4000000, 8000000, 15000000, 1000000000} | UpdateInGameScoreDisplayEsiAbi 0x417040 延命表（下标 0x474C9C），title-calc-cluster.md |
| 0x474488 | g_ExtendScoreTableB | int[3] {3000000, 10000000, 1000000000} | 同上；rank 门 bit 0x20 清零且 DAT_00474C74==4 时选用 |

两表已 set_type 为 int 数组并加重复注释。

## 裸偏移→struct 字段改写（第三轮，2026-10-01）

源码侧把 0x3AC VmRecord 与 0x48 GameContext 的裸 `+ 0x...` 访问改写为命名字段访问。33 个 cpp、约 550 处访问转换，全部单文件编译 + 全量脚本验证（227 obj，仅 SceneTriggerUpdate.cpp 两个预存 C4700 警告）。

- GameContext 族（7 文件）：PlayerStageHelpers（含新提取的 TimerNode 视图，ResetTimerNode 同时服务 ctx+0x14 与 enemy+0x3f8）、SceneTriggerPopup、PlayerDamageOutput、PlayerModeDispatcher（deathbomb 门）、GameContextLifecycle、ManagerReleaseWrappers（calc_record/draw_record）、EclScriptLibrary（g_StageNode 门）。
- VmRecord 族（26 文件）：实体/效果池族、ASCII 渲染族（Mode8/Mode9/Glyph/Projected/Traversal 等 10 文件）、时间轴/舞台族（TimelineRenderObjectSetup 约 230 处、StageObjectVtable、SpellBulletVtable 等 9 文件）。所有转换经函数级 `VmRecord &vm = *reinterpret_cast<VmRecord *>(...)` 视图完成，ABI 注释与语句顺序原样保留。
- 结构修正（本轮发现并修入两处）：Vec3InterpBlock 的 timer 实为 4 个 dword（证据：三个块 flags 均在 base+0x40，即 0xb0/0xfc），此前误作 timer[3]+field_0048；GameContext 内嵌 TimerNode（源码与 IDB 同步）。0x6c/0xb0/0xfc/0x128/0x174/0x1b0/0x1fc/0x228/0x378 旗标表独立印证了块内偏移。
- 刻意保留的裸访问类别：子 dword 颜色字节（0x2ff/0x303）、u32 位模式写入 float 字段（0x3c/0x40/0x2c/0x34/0x38，字段赋值会改变语义）、数组驱动的批量清零/旗标循环、块指针参数化的插值器内部、以及上下文不足以归属对象类型的访问。

剩余裸偏移热点（下一批候选）：RenderOwner 侧 +0x3ada60 族与 +0x72dad4 族（源码已有 LargeRenderOwnerLayout 骨架但未统一引用）、GameManager/管理器槽位、player 对象。

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

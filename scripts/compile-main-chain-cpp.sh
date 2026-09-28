#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/.." && pwd)
source /home/niko/.local/share/th10-re/env.sh

mkdir -p "$repo_root/build/cpp"
cd "$repo_root"
th10_cl /nologo /c /TP /I src src/MainChainContext.cpp /Fobuild\\cpp\\MainChainContext.obj
th10_cl /nologo /c /TP /I src src/AsciiManagerLifecycle.cpp /Fobuild\\cpp\\AsciiManagerLifecycle.obj
th10_cl /nologo /c /TP /I src src/AsciiAnimationVm.cpp /Fobuild\\cpp\\AsciiAnimationVm.obj
th10_cl /nologo /c /TP /I src src/AsciiGlyphRenderer.cpp /Fobuild\\cpp\\AsciiGlyphRenderer.obj
th10_cl /nologo /c /TP /I src src/AsciiGlyphSubmission.cpp /Fobuild\\cpp\\AsciiGlyphSubmission.obj
th10_cl /nologo /c /TP /I src src/AsciiProjectedRenderer.cpp /Fobuild\\cpp\\AsciiProjectedRenderer.obj
th10_cl /nologo /c /TP /I src src/AsciiMode8Renderer.cpp /Fobuild\\cpp\\AsciiMode8Renderer.obj
th10_cl /nologo /c /TP /I src src/AsciiMode9Renderer.cpp /Fobuild\\cpp\\AsciiMode9Renderer.obj
th10_cl /nologo /c /TP /I src src/AsciiRenderModeDispatcher.cpp /Fobuild\\cpp\\AsciiRenderModeDispatcher.obj
th10_cl /nologo /c /TP /I src src/AsciiOwnerTraversal.cpp /Fobuild\\cpp\\AsciiOwnerTraversal.obj
th10_cl /nologo /c /TP /I src src/AsciiHudRenderer.cpp /Fobuild\\cpp\\AsciiHudRenderer.obj
th10_cl /nologo /c /TP /I src src/AsciiOverlayCallbacks.cpp /Fobuild\\cpp\\AsciiOverlayCallbacks.obj
th10_cl /nologo /c /TP /I src src/AsciiOverlayFactory.cpp /Fobuild\\cpp\\AsciiOverlayFactory.obj
th10_cl /nologo /c /TP /I src src/AsciiSceneObjectRenderer.cpp /Fobuild\\cpp\\AsciiSceneObjectRenderer.obj
th10_cl /nologo /c /TP /I src src/AsciiHudOverlayUpdate.cpp /Fobuild\\cpp\\AsciiHudOverlayUpdate.obj
th10_cl /nologo /c /TP /I src src/PlayerObjectLifecycle.cpp /Fobuild\\cpp\\PlayerObjectLifecycle.obj
th10_cl /nologo /c /TP /I src src/PlayerModeDispatcher.cpp /Fobuild\\cpp\\PlayerModeDispatcher.obj
th10_cl /nologo /c /TP /I src src/PlayerOptionRecords.cpp /Fobuild\\cpp\\PlayerOptionRecords.obj
th10_cl /nologo /c /TP /I src src/PlayerMovement.cpp /Fobuild\\cpp\\PlayerMovement.obj
th10_cl /nologo /c /TP /I src src/PlayerDeathProcessor.cpp /Fobuild\\cpp\\PlayerDeathProcessor.obj
th10_cl /nologo /c /TP /I src src/PlayerShotData.cpp /Fobuild\\cpp\\PlayerShotData.obj
th10_cl /nologo /c /TP /I src src/PlayerOptionCallbacks.cpp /Fobuild\\cpp\\PlayerOptionCallbacks.obj
th10_cl /nologo /c /TP /I src src/PlayerTimerHelpers.cpp /Fobuild\\cpp\\PlayerTimerHelpers.obj
th10_cl /nologo /c /TP /I src src/PlayerMotionHelpers.cpp /Fobuild\\cpp\\PlayerMotionHelpers.obj
th10_cl /nologo /c /TP /I src src/PlayerShotSpawner.cpp /Fobuild\\cpp\\PlayerShotSpawner.obj
th10_cl /nologo /c /TP /I src src/EntityHelpers.cpp /Fobuild\\cpp\\EntityHelpers.obj
th10_cl /nologo /c /TP /I src src/PlayerStageHelpers.cpp /Fobuild\\cpp\\PlayerStageHelpers.obj
th10_cl /nologo /c /TP /I src src/PlayerFrameworkHelpers.cpp /Fobuild\\cpp\\PlayerFrameworkHelpers.obj
th10_cl /nologo /c /TP /I src src/StageEffectHelpers.cpp /Fobuild\\cpp\\StageEffectHelpers.obj
th10_cl /nologo /c /TP /I src src/VmLeafHelpers.cpp /Fobuild\\cpp\\VmLeafHelpers.obj
th10_cl /nologo /c /TP /I src src/PlayerItemMagnet.cpp /Fobuild\\cpp\\PlayerItemMagnet.obj
th10_cl /nologo /c /TP /I src src/PlayerProjectileManager.cpp /Fobuild\\cpp\\PlayerProjectileManager.obj
th10_cl /nologo /c /TP /I src src/PlayerOptionCallbacks.cpp /Fobuild\\cpp\\PlayerOptionCallbacks.obj
th10_cl /nologo /c /TP /I src src/TimelineGateState.cpp /Fobuild\\cpp\\TimelineGateState.obj
th10_cl /nologo /c /TP /I src src/TimelineGateLifecycle.cpp /Fobuild\\cpp\\TimelineGateLifecycle.obj
th10_cl /nologo /c /TP /I src src/TimelineStreamLoader.cpp /Fobuild\\cpp\\TimelineStreamLoader.obj
th10_cl /nologo /c /TP /I src src/TimelineTextSubmission.cpp /Fobuild\\cpp\\TimelineTextSubmission.obj
th10_cl /nologo /c /TP /I src src/TimelineRenderObjectSetup.cpp /Fobuild\\cpp\\TimelineRenderObjectSetup.obj
th10_cl /nologo /c /TP /I src src/TimelineRenderObjects.cpp /Fobuild\\cpp\\TimelineRenderObjects.obj
th10_cl /nologo /c /TP /I src src/TimelineContinuation.cpp /Fobuild\\cpp\\TimelineContinuation.obj
th10_cl /nologo /c /TP /I src src/TimelineAudioActions.cpp /Fobuild\\cpp\\TimelineAudioActions.obj
th10_cl /nologo /c /TP /I src src/TimelineRecordInterpreter.cpp /Fobuild\\cpp\\TimelineRecordInterpreter.obj
th10_cl /nologo /c /TP /I src src/MainChainDrawContinue.cpp /Fobuild\\cpp\\MainChainDrawContinue.obj
th10_cl /nologo /c /TP /I src src/MainChainRegistration.cpp /Fobuild\\cpp\\MainChainRegistration.obj
th10_cl /nologo /c /TP /I src src/MainChainUpdate.cpp /Fobuild\\cpp\\MainChainUpdate.obj
th10_cl /nologo /c /TP /I src src/MainChainDrawFinalize.cpp /Fobuild\\cpp\\MainChainDrawFinalize.obj
th10_cl /nologo /c /TP /I src src/GlobalLifecycleManager.cpp /Fobuild\\cpp\\GlobalLifecycleManager.obj
th10_cl /nologo /c /TP /I src src/GlobalLifecycleStartup.cpp /Fobuild\\cpp\\GlobalLifecycleStartup.obj
th10_cl /nologo /c /TP /I src src/BgmRuntime.cpp /Fobuild\\cpp\\BgmRuntime.obj
th10_cl /nologo /c /TP /I src src/BgmUpdateLoop.cpp /Fobuild\\cpp\\BgmUpdateLoop.obj
th10_cl /nologo /c /TP /I src src/MainChainDrawInitialize.cpp /Fobuild\\cpp\\MainChainDrawInitialize.obj
th10_cl /nologo /c /TP /I src src/MainChainRenderAdapters.cpp /Fobuild\\cpp\\MainChainRenderAdapters.obj
th10_cl /nologo /c /TP /I src src/CallbackScheduler.cpp /Fobuild\\cpp\\CallbackScheduler.obj
th10_cl /nologo /c /TP /I src src/TransitionTick.cpp /Fobuild\\cpp\\TransitionTick.obj
th10_cl /nologo /c /TP /I src src/ThreadControl.cpp /Fobuild\\cpp\\ThreadControl.obj
th10_cl /nologo /c /TP /I src src/MainChainRegistrationHook.cpp /Fobuild\\cpp\\MainChainRegistrationHook.obj
th10_cl /nologo /c /TP /I src src/MainChainStartupGlobals.cpp /Fobuild\\cpp\\MainChainStartupGlobals.obj
th10_cl /nologo /c /TP /I src src/RegistrationDrawOwner.cpp /Fobuild\\cpp\\RegistrationDrawOwner.obj
th10_cl /nologo /c /TP /I src src/AsciiRenderAdapter.cpp /Fobuild\\cpp\\AsciiRenderAdapter.obj
th10_cl /nologo /c /TP /I src src/ManagerWork.cpp /Fobuild\\cpp\\ManagerWork.obj
th10_cl /nologo /c /TP /I src src/ManagerWorkStageAdapters.cpp /Fobuild\\cpp\\ManagerWorkStageAdapters.obj
th10_cl /nologo /c /TP /I src src/GameModeTeardown.cpp /Fobuild\\cpp\\GameModeTeardown.obj
th10_cl /nologo /c /TP /I src src/TransitionSoundAdapter.cpp /Fobuild\\cpp\\TransitionSoundAdapter.obj
th10_cl /nologo /c /TP /I src src/MainChainGlobalTeardown.cpp /Fobuild\\cpp\\MainChainGlobalTeardown.obj
th10_cl /nologo /c /TP /I src src/MainChainShutdown.cpp /Fobuild\\cpp\\MainChainShutdown.obj
th10_cl /nologo /c /TP /I src src/MidiTimer.cpp /Fobuild\\cpp\\MidiTimer.obj
th10_cl /nologo /c /TP /I src src/MainChainPlatformCleanup.cpp /Fobuild\\cpp\\MainChainPlatformCleanup.obj
th10_cl /nologo /c /TP /I src src/MainApplicationFinalCleanup.cpp /Fobuild\\cpp\\MainApplicationFinalCleanup.obj
th10_cl /nologo /c /TP /I src src/LargeRenderOwner.cpp /Fobuild\\cpp\\LargeRenderOwner.obj
th10_cl /nologo /c /TP /I src src/LargeRenderOwnerFrameLoop.cpp /Fobuild\\cpp\\LargeRenderOwnerFrameLoop.obj
th10_cl /nologo /c /TP /I src src/LargeRenderOwnerCallbacks.cpp /Fobuild\\cpp\\LargeRenderOwnerCallbacks.obj
th10_cl /nologo /c /TP /I src src/MainApplicationRuntime.cpp /Fobuild\\cpp\\MainApplicationRuntime.obj
th10_cl /nologo /c /TP /I src src/MainApplicationFrame.cpp /Fobuild\\cpp\\MainApplicationFrame.obj
th10_cl /nologo /c /TP /I src src/MainChainFrameTime.cpp /Fobuild\\cpp\\MainChainFrameTime.obj
th10_cl /nologo /c /TP /I src src/MainChainSystemSettings.cpp /Fobuild\\cpp\\MainChainSystemSettings.obj
th10_cl /nologo /c /TP /I src src/MainChainHostEnvironment.cpp /Fobuild\\cpp\\MainChainHostEnvironment.obj
th10_cl /nologo /c /TP /I src src/MainChainConfiguration.cpp /Fobuild\\cpp\\MainChainConfiguration.obj
th10_cl /nologo /c /TP /I src src/MainChainWindow.cpp /Fobuild\\cpp\\MainChainWindow.obj
th10_cl /nologo /c /TP /I src src/MainChainExecutableChecksum.cpp /Fobuild\\cpp\\MainChainExecutableChecksum.obj
th10_cl /nologo /c /TP /I src src/MainChainD3DDevice.cpp /Fobuild\\cpp\\MainChainD3DDevice.obj
th10_cl /nologo /c /TP /I src src/MainChainStartupInputState.cpp /Fobuild\\cpp\\MainChainStartupInputState.obj
th10_cl /nologo /c /TP /I src src/MainChainFileProbe.cpp /Fobuild\\cpp\\MainChainFileProbe.obj
th10_cl /nologo /c /TP /I src src/TitleGameManagerLifecycle.cpp /Fobuild\\cpp\\TitleGameManagerLifecycle.obj
th10_cl /nologo /c /TP /I src src/TitleCalcCluster.cpp /Fobuild\\cpp\\TitleCalcCluster.obj
th10_cl /nologo /c /TP /I src src/TitleScreenCalcBody.cpp /Fobuild\\cpp\\TitleScreenCalcBody.obj
th10_cl /nologo /c /TP /I src src/GameManagerState.cpp /Fobuild\\cpp\\GameManagerState.obj
th10_cl /nologo /c /TP /I src src/ReplaySave.cpp /Fobuild\\cpp\\ReplaySave.obj
th10_cl /nologo /c /TP /I src src/ScoreScreenUpdate.cpp /Fobuild\\cpp\\ScoreScreenUpdate.obj
th10_cl /nologo /c /TP /I src src/TitleSceneSetup.cpp /Fobuild\\cpp\\TitleSceneSetup.obj
th10_cl /nologo /c /TP /I src src/HintTextLoader.cpp /Fobuild\\cpp\\HintTextLoader.obj
th10_cl /nologo /c /TP /I src src/ScoreSave.cpp /Fobuild\\cpp\\ScoreSave.obj
th10_cl /nologo /c /TP /I src src/ReplayPackedCodec.cpp /Fobuild\\cpp\\ReplayPackedCodec.obj
th10_cl /nologo /c /TP /I src src/ScoreFileFormats.cpp /Fobuild\\cpp\\ScoreFileFormats.obj
th10_cl /nologo /c /TP /I src src/ScreenshotWriter.cpp /Fobuild\\cpp\\ScreenshotWriter.obj
th10_cl /nologo /c /TP /I src src/AsciiHudOwnerLifecycle.cpp /Fobuild\\cpp\\AsciiHudOwnerLifecycle.obj
th10_cl /nologo /c /TP /I src src/OptionTrailUpdate.cpp /Fobuild\\cpp\\OptionTrailUpdate.obj
th10_cl /nologo /c /TP /I src src/MenuItemObject.cpp /Fobuild\\cpp\\MenuItemObject.obj
th10_cl /nologo /c /TP /I src src/SceneTriggerUpdate.cpp /Fobuild\\cpp\\SceneTriggerUpdate.obj
th10_cl /nologo /c /TP /I src src/TitleBackgroundScript.cpp /Fobuild\\cpp\\TitleBackgroundScript.obj
th10_cl /nologo /c /TP /I src src/TitleBulletUpdate.cpp /Fobuild\\cpp\\TitleBulletUpdate.obj
th10_cl /nologo /c /TP /I src src/HintTextFile.cpp /Fobuild\\cpp\\HintTextFile.obj
th10_cl /nologo /c /TP /I src src/PostRunReplaySaveMenu.cpp /Fobuild\\cpp\\PostRunReplaySaveMenu.obj
th10_cl /nologo /c /TP /I src src/StagePracticeReplayContext.cpp /Fobuild\\cpp\\StagePracticeReplayContext.obj
th10_cl /nologo /c /TP /I src src/PlayerDamageOutput.cpp /Fobuild\\cpp\\PlayerDamageOutput.obj
th10_cl /nologo /c /TP /I src src/PauseMenuModes.cpp /Fobuild\\cpp\\PauseMenuModes.obj
th10_cl /nologo /c /TP /I src src/ReplayRecordScreens.cpp /Fobuild\\cpp\\ReplayRecordScreens.obj
th10_cl /nologo /c /TP /I src src/UnlockListRefresh.cpp /Fobuild\\cpp\\UnlockListRefresh.obj
th10_cl /nologo /c /TP /I src src/EndingMidiSequencer.cpp /Fobuild\\cpp\\EndingMidiSequencer.obj
th10_cl /nologo /c /TP /I src src/TitleScreenDrawPasses.cpp /Fobuild\\cpp\\TitleScreenDrawPasses.obj
th10_cl /nologo /c /TP /I src src/SpriteViewDebugText.cpp /Fobuild\\cpp\\SpriteViewDebugText.obj
th10_cl /nologo /c /TP /I src src/ScoreFileLoad.cpp /Fobuild\\cpp\\ScoreFileLoad.obj
th10_cl /nologo /c /TP /I src src/ScriptTestMenu.cpp /Fobuild\\cpp\\ScriptTestMenu.obj
th10_cl /nologo /c /TP /I src src/StageTextEffects.cpp /Fobuild\\cpp\\StageTextEffects.obj
th10_cl /nologo /c /TP /I src src/ReplayContextTimerTick.cpp /Fobuild\\cpp\\ReplayContextTimerTick.obj
th10_cl /nologo /c /TP /I src src/EclScriptNameTable.cpp /Fobuild\\cpp\\EclScriptNameTable.obj
th10_cl /nologo /c /TP /I src src/PlayerProximityFade.cpp /Fobuild\\cpp\\PlayerProximityFade.obj
th10_cl /nologo /c /TP /I src src/GdiOutlinedText.cpp /Fobuild\\cpp\\GdiOutlinedText.obj
th10_cl /nologo /c /TP /I src src/EndingMidiBlockLoader.cpp /Fobuild\\cpp\\EndingMidiBlockLoader.obj
th10_cl /nologo /c /TP /I src src/SceneTriggerFeatures.cpp /Fobuild\\cpp\\SceneTriggerFeatures.obj
th10_cl /nologo /c /TP /I src src/SpellBulletVtable.cpp /Fobuild\\cpp\\SpellBulletVtable.obj
th10_cl /nologo /c /TP /I src src/StageObjectVtable.cpp /Fobuild\\cpp\\StageObjectVtable.obj
th10_cl /nologo /c /TP /I src src/ResultScreenState.cpp /Fobuild\\cpp\\ResultScreenState.obj
th10_cl /nologo /c /TP /I src src/EndingMidiPlayer.cpp /Fobuild\\cpp\\EndingMidiPlayer.obj
th10_cl /nologo /c /TP /I src src/EndingOverlayQuad.cpp /Fobuild\\cpp\\EndingOverlayQuad.obj
th10_cl /nologo /c /TP /I src src/GeneratedSurfaceBlit.cpp /Fobuild\\cpp\\GeneratedSurfaceBlit.obj
th10_cl /nologo /c /TP /I src src/RenderOwnerSurfaceLoading.cpp /Fobuild\\cpp\\RenderOwnerSurfaceLoading.obj
th10_cl /nologo /c /TP /I src src/AsciiProjectedQuadSubmit.cpp /Fobuild\\cpp\\AsciiProjectedQuadSubmit.obj
th10_cl /nologo /c /TP /I src src/EntityRenderStateApply.cpp /Fobuild\\cpp\\EntityRenderStateApply.obj
th10_cl /nologo /c /TP /I src src/ConditionalStateSubrecords.cpp /Fobuild\\cpp\\ConditionalStateSubrecords.obj
th10_cl /nologo /c /TP /I src src/EclScriptObjectTeardown.cpp /Fobuild\\cpp\\EclScriptObjectTeardown.obj
th10_cl /nologo /c /TP /I src src/StageHostTeardown.cpp /Fobuild\\cpp\\StageHostTeardown.obj
th10_cl /nologo /c /TP /I src src/TitleScreenStateCtor.cpp /Fobuild\\cpp\\TitleScreenStateCtor.obj
th10_cl /nologo /c /TP /I src src/PauseEnterSetup.cpp /Fobuild\\cpp\\PauseEnterSetup.obj
th10_cl /nologo /c /TP /I src src/SaveRunHighScoreEntry.cpp /Fobuild\\cpp\\SaveRunHighScoreEntry.obj
th10_cl /nologo /c /TP /I src src/ReplaySceneReuse.cpp /Fobuild\\cpp\\ReplaySceneReuse.obj
th10_cl /nologo /c /TP /I src src/GameManagerGateVms.cpp /Fobuild\\cpp\\GameManagerGateVms.obj
th10_cl /nologo /c /TP /I src src/EffectPoolEntityUpdate.cpp /Fobuild\\cpp\\EffectPoolEntityUpdate.obj
th10_cl /nologo /c /TP /I src src/PlayerShotHoming.cpp /Fobuild\\cpp\\PlayerShotHoming.obj
th10_cl /nologo /c /TP /I src src/EclScriptVm.cpp /Fobuild\\cpp\\EclScriptVm.obj
th10_cl /nologo /c /TP /I src src/EnemyDeathEffects.cpp /Fobuild\\cpp\\EnemyDeathEffects.obj
th10_cl /nologo /c /TP /I src src/ScoreScreenRenderers.cpp /Fobuild\\cpp\\ScoreScreenRenderers.obj
th10_cl /nologo /c /TP /I src src/EclEasedTransforms.cpp /Fobuild\\cpp\\EclEasedTransforms.obj
th10_cl /nologo /c /TP /I src src/SceneTriggerObject.cpp /Fobuild\\cpp\\SceneTriggerObject.obj
th10_cl /nologo /c /TP /I src src/ReplayViewLoad.cpp /Fobuild\\cpp\\ReplayViewLoad.obj
th10_cl /nologo /c /TP /I src src/StageScriptOpen.cpp /Fobuild\\cpp\\StageScriptOpen.obj
th10_cl /nologo /c /TP /I src src/GameManagerStateBodies.cpp /Fobuild\\cpp\\GameManagerStateBodies.obj
th10_cl /nologo /c /TP /I src src/MainChainRenderRecovery.cpp /Fobuild\\cpp\\MainChainRenderRecovery.obj
th10_cl /nologo /c /TP /I src src/GeneratedFontTable.cpp /Fobuild\\cpp\\GeneratedFontTable.obj
th10_cl /nologo /c /TP /I src src/GeneratedSurfaceText.cpp /Fobuild\\cpp\\GeneratedSurfaceText.obj
th10_cl /nologo /c /TP /I src src/VersionData.cpp /Fobuild\\cpp\\VersionData.obj
th10_cl /nologo /c /TP /I src src/GeneratedSurface.cpp /Fobuild\\cpp\\GeneratedSurface.obj
th10_cl /nologo /c /TP /I src src/MainChainBackgroundThread.cpp /Fobuild\\cpp\\MainChainBackgroundThread.obj
th10_cl /nologo /c /TP /I src src/MainChainThreadCreation.cpp /Fobuild\\cpp\\MainChainThreadCreation.obj
th10_cl /nologo /c /TP /I src src/MainChainSoundWorker.cpp /Fobuild\\cpp\\MainChainSoundWorker.obj
th10_cl /nologo /c /TP /I src src/MainChainInput.cpp /Fobuild\\cpp\\MainChainInput.obj
th10_cl /nologo /c /TP /I src src/JoystickConfigPoll.cpp /Fobuild\\cpp\\JoystickConfigPoll.obj
th10_cl /nologo /c /TP /I src src/KeyConfigScreens.cpp /Fobuild\\cpp\\KeyConfigScreens.obj
th10_cl /nologo /c /TP /I src src/EclScriptLibrary.cpp /Fobuild\\cpp\\EclScriptLibrary.obj
th10_cl /nologo /c /TP /I src src/TextureDilateFilter.cpp /Fobuild\\cpp\\TextureDilateFilter.obj
th10_cl /nologo /c /TP /I src src/ResultScreenDigits.cpp /Fobuild\\cpp\\ResultScreenDigits.obj
th10_cl /nologo /c /TP /I src src/EclSelectMenu.cpp /Fobuild\\cpp\\EclSelectMenu.obj
th10_cl /nologo /c /TP /I src src/ManagerReleaseWrappers.cpp /Fobuild\\cpp\\ManagerReleaseWrappers.obj
th10_cl /nologo /c /TP /I src src/TitleScoreAnimTriggers.cpp /Fobuild\\cpp\\TitleScoreAnimTriggers.obj
th10_cl /nologo /c /TP /I src src/ManagerCreation.cpp /Fobuild\\cpp\\ManagerCreation.obj
th10_cl /nologo /c /TP /I src src/MainChainDirectInputAdapter.cpp /Fobuild\\cpp\\MainChainDirectInputAdapter.obj
th10_cl /nologo /c /TP /I src src/MainChainResourceThread.cpp /Fobuild\\cpp\\MainChainResourceThread.obj
th10_cl /nologo /c /TP /I src src/MainChainSoundLoading.cpp /Fobuild\\cpp\\MainChainSoundLoading.obj
th10_cl /nologo /c /TP /I src src/PackedArchive.cpp /Fobuild\\cpp\\PackedArchive.obj
th10_cl /nologo /c /TP /I src src/ResultScreenScript.cpp /Fobuild\\cpp\\ResultScreenScript.obj
th10_cl /nologo /c /TP /I src src/ResultScreenUpdate.cpp /Fobuild\\cpp\\ResultScreenUpdate.obj
th10_cl /nologo /c /TP /I src src/ArchiveFileReaders.cpp /Fobuild\\cpp\\ArchiveFileReaders.obj
th10_cl /nologo /c /TP /I src src/AsciiHudGameplayUpdate.cpp /Fobuild\\cpp\\AsciiHudGameplayUpdate.obj
th10_cl /nologo /c /TP /I src src/AsciiManager.cpp /Fobuild\\cpp\\AsciiManager.obj
th10_cl /nologo /c /TP /I src src/AsciiSecondaryStrings.cpp /Fobuild\\cpp\\AsciiSecondaryStrings.obj
th10_cl /nologo /c /TP /I src src/BgmSupportMisc.cpp /Fobuild\\cpp\\BgmSupportMisc.obj
th10_cl /nologo /c /TP /I src src/ColorAndVecHelpers.cpp /Fobuild\\cpp\\ColorAndVecHelpers.obj
th10_cl /nologo /c /TP /I src src/EclInstructionHelpers.cpp /Fobuild\\cpp\\EclInstructionHelpers.obj
th10_cl /nologo /c /TP /I src src/EclObjectLifecycle.cpp /Fobuild\\cpp\\EclObjectLifecycle.obj
th10_cl /nologo /c /TP /I src src/EclVmChunkWriter.cpp /Fobuild\\cpp\\EclVmChunkWriter.obj
th10_cl /nologo /c /TP /I src src/EffectPoolLifecycle.cpp /Fobuild\\cpp\\EffectPoolLifecycle.obj
th10_cl /nologo /c /TP /I src src/EffectScheduler.cpp /Fobuild\\cpp\\EffectScheduler.obj
th10_cl /nologo /c /TP /I src src/EndingMidiEdge.cpp /Fobuild\\cpp\\EndingMidiEdge.obj
th10_cl /nologo /c /TP /I src src/EnemyScriptVars.cpp /Fobuild\\cpp\\EnemyScriptVars.obj
th10_cl /nologo /c /TP /I src src/EntityFieldSetters.cpp /Fobuild\\cpp\\EntityFieldSetters.obj
th10_cl /nologo /c /TP /I src src/EntitySlotAccessors.cpp /Fobuild\\cpp\\EntitySlotAccessors.obj
th10_cl /nologo /c /TP /I src src/GameContextLifecycle.cpp /Fobuild\\cpp\\GameContextLifecycle.obj
th10_cl /nologo /c /TP /I src src/GateVmSlots.cpp /Fobuild\\cpp\\GateVmSlots.obj
th10_cl /nologo /c /TP /I src src/GdiTextSupport.cpp /Fobuild\\cpp\\GdiTextSupport.obj
th10_cl /nologo /c /TP /I src src/GlobalLifecycleHelpers.cpp /Fobuild\\cpp\\GlobalLifecycleHelpers.obj
th10_cl /nologo /c /TP /I src src/JoystickKeyLatch.cpp /Fobuild\\cpp\\JoystickKeyLatch.obj
th10_cl /nologo /c /TP /I src src/MainChainErrorLog.cpp /Fobuild\\cpp\\MainChainErrorLog.obj
th10_cl /nologo /c /TP /I src src/MainChainObject6fcAndGate.cpp /Fobuild\\cpp\\MainChainObject6fcAndGate.obj
th10_cl /nologo /c /TP /I src src/MainChainStateHelpers.cpp /Fobuild\\cpp\\MainChainStateHelpers.obj
th10_cl /nologo /c /TP /I src src/ManagerWorkPipeline.cpp /Fobuild\\cpp\\ManagerWorkPipeline.obj
th10_cl /nologo /c /TP /I src src/MenuRecordHelpers.cpp /Fobuild\\cpp\\MenuRecordHelpers.obj
th10_cl /nologo /c /TP /I src src/MenuStateHelpers.cpp /Fobuild\\cpp\\MenuStateHelpers.obj
th10_cl /nologo /c /TP /I src src/PlayerRecordHelpers.cpp /Fobuild\\cpp\\PlayerRecordHelpers.obj
th10_cl /nologo /c /TP /I src src/RegistrationStageOpen.cpp /Fobuild\\cpp\\RegistrationStageOpen.obj
th10_cl /nologo /c /TP /I src src/RenderOwnerClearColor.cpp /Fobuild\\cpp\\RenderOwnerClearColor.obj
th10_cl /nologo /c /TP /I src src/RenderOwnerDrawHelpers.cpp /Fobuild\\cpp\\RenderOwnerDrawHelpers.obj
th10_cl /nologo /c /TP /I src src/ReplayContextHelpers.cpp /Fobuild\\cpp\\ReplayContextHelpers.obj
th10_cl /nologo /c /TP /I src src/ReplayFileReader.cpp /Fobuild\\cpp\\ReplayFileReader.obj
th10_cl /nologo /c /TP /I src src/ResultScreenStateAccessors.cpp /Fobuild\\cpp\\ResultScreenStateAccessors.obj
th10_cl /nologo /c /TP /I src src/SceneTriggerManager.cpp /Fobuild\\cpp\\SceneTriggerManager.obj
th10_cl /nologo /c /TP /I src src/SceneTriggerPopup.cpp /Fobuild\\cpp\\SceneTriggerPopup.obj
th10_cl /nologo /c /TP /I src src/ScriptMapLoader.cpp /Fobuild\\cpp\\ScriptMapLoader.obj
th10_cl /nologo /c /TP /I src src/SpellBulletManagerLifecycle.cpp /Fobuild\\cpp\\SpellBulletManagerLifecycle.obj
th10_cl /nologo /c /TP /I src src/StageConditionalState.cpp /Fobuild\\cpp\\StageConditionalState.obj
th10_cl /nologo /c /TP /I src src/StageEffectHost.cpp /Fobuild\\cpp\\StageEffectHost.obj
th10_cl /nologo /c /TP /I src src/StageObjectManager.cpp /Fobuild\\cpp\\StageObjectManager.obj
th10_cl /nologo /c /TP /I src src/StdStringCrt.cpp /Fobuild\\cpp\\StdStringCrt.obj
th10_cl /nologo /c /TP /I src src/SystemUiSupport.cpp /Fobuild\\cpp\\SystemUiSupport.obj
th10_cl /nologo /c /TP /I src src/TextScanHelpers.cpp /Fobuild\\cpp\\TextScanHelpers.obj
th10_cl /nologo /c /TP /I src src/TextureStageLoading.cpp /Fobuild\\cpp\\TextureStageLoading.obj
th10_cl /nologo /c /TP /I src src/TimelinePrngDraw.cpp /Fobuild\\cpp\\TimelinePrngDraw.obj
th10_cl /nologo /c /TP /I src src/TimelineSpawnControl.cpp /Fobuild\\cpp\\TimelineSpawnControl.obj
th10_cl /nologo /c /TP /I src src/TitleScreenVmSlots.cpp /Fobuild\\cpp\\TitleScreenVmSlots.obj
th10_cl /nologo /c /TP /I src src/TitleStateAccessors.cpp /Fobuild\\cpp\\TitleStateAccessors.obj
th10_cl /nologo /c /TP /I src src/ZunMath.cpp /Fobuild\\cpp\\ZunMath.obj

printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainContext.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/AsciiManagerLifecycle.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/AsciiAnimationVm.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/AsciiGlyphRenderer.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/AsciiGlyphSubmission.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/AsciiProjectedRenderer.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/AsciiMode8Renderer.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/AsciiMode9Renderer.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/AsciiRenderModeDispatcher.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/AsciiOwnerTraversal.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/AsciiHudRenderer.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/AsciiOverlayCallbacks.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/AsciiOverlayFactory.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/TimelineGateState.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/TimelineGateLifecycle.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/TimelineStreamLoader.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/TimelineTextSubmission.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/TimelineRenderObjectSetup.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/TimelineRenderObjects.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/TimelineContinuation.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/TimelineAudioActions.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/TimelineRecordInterpreter.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainDrawContinue.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainRegistration.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainUpdate.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainDrawFinalize.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/GlobalLifecycleManager.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/GlobalLifecycleStartup.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/BgmRuntime.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainDrawInitialize.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainRenderAdapters.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/CallbackScheduler.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/TransitionTick.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/ThreadControl.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainRegistrationHook.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainStartupGlobals.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/RegistrationDrawOwner.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/AsciiRenderAdapter.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/ManagerWork.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/ManagerWorkStageAdapters.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/TransitionSoundAdapter.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainGlobalTeardown.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainShutdown.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MidiTimer.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainPlatformCleanup.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainApplicationFinalCleanup.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/LargeRenderOwner.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/LargeRenderOwnerFrameLoop.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/LargeRenderOwnerCallbacks.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainApplicationRuntime.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainApplicationFrame.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainFrameTime.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainSystemSettings.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainHostEnvironment.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainConfiguration.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainWindow.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainExecutableChecksum.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainD3DDevice.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainStartupInputState.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainFileProbe.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/GameManagerState.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/GameManagerStateBodies.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainRenderRecovery.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/GeneratedFontTable.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/GeneratedSurfaceText.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/VersionData.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/GeneratedSurface.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainBackgroundThread.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainThreadCreation.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainSoundWorker.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainInput.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainDirectInputAdapter.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainResourceThread.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/MainChainSoundLoading.obj"
printf 'Wrote %s\n' "$repo_root/build/cpp/PackedArchive.obj"
th10_cl /nologo /c /TP /I src src/ResultScreenScript.cpp /Fobuild\cpp\ResultScreenScript.obj

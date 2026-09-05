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
th10_cl /nologo /c /TP /I src src/GameManagerState.cpp /Fobuild\\cpp\\GameManagerState.obj
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
th10_cl /nologo /c /TP /I src src/MainChainDirectInputAdapter.cpp /Fobuild\\cpp\\MainChainDirectInputAdapter.obj
th10_cl /nologo /c /TP /I src src/MainChainResourceThread.cpp /Fobuild\\cpp\\MainChainResourceThread.obj
th10_cl /nologo /c /TP /I src src/MainChainSoundLoading.cpp /Fobuild\\cpp\\MainChainSoundLoading.obj
th10_cl /nologo /c /TP /I src src/PackedArchive.cpp /Fobuild\\cpp\\PackedArchive.obj
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

#pragma once

#include "MainChainContext.hpp"

namespace th10 {

i32 CreateMainChainDirectInputAdapter(void *instance, void **out_root);
i32 CreateMainChainKeyboardAdapter(void *root, void **out_keyboard);
i32 SetMainChainKeyboardDataFormatAdapter(void *keyboard);
i32 SetMainChainKeyboardCooperativeLevelAdapter(void *keyboard, void *window);
void AcquireMainChainKeyboardAdapter(void *keyboard);
void EnumerateMainChainControllersAdapter(void *root);
void ConfigureMainChainControllerAdapter(void *controller, void *window);
void UnacquireMainChainInputInterfaceAdapter(void *object);
void ReleaseMainChainInputInterfaceAdapter(void *object);

i32 TH10_STDCALL CreateFirstMainChainController(const void *instance,
                                                 void *unused_context);
i32 TH10_STDCALL ConfigureMainChainControllerObject(const void *object,
                                                     void *unused_context);

} // namespace th10

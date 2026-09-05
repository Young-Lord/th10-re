#include "MainChainDirectInputAdapter.hpp"

namespace th10 {

namespace {

struct InputGuid {
    u8 bytes[16];
};

struct DirectInputDeviceInstancePartial {
    u8 unknown_0000[4];
    InputGuid instance_guid;
};

struct DirectInputDeviceObjectInstancePartial {
    u8 unknown_0000[0x18];
    u32 type;
};

struct DirectInputPropertyRangePartial {
    u32 size;
    u32 header_size;
    u32 object;
    u32 how;
    i32 minimum;
    i32 maximum;
};

typedef char AssertDirectInputInstanceGuidOffset[
    offsetof(DirectInputDeviceInstancePartial, instance_guid) == 0x4 ? 1 : -1];
typedef char AssertDirectInputObjectTypeOffset[
    offsetof(DirectInputDeviceObjectInstancePartial, type) == 0x18 ? 1 : -1];
typedef char AssertDirectInputPropertyRangeSize[
    sizeof(DirectInputPropertyRangePartial) == 0x18 ? 1 : -1];

typedef i32 (TH10_STDCALL *InputReleaseFn)(void *self);
typedef i32 (TH10_STDCALL *InputCreateDeviceFn)(void *self,
                                                 const InputGuid *guid,
                                                 void **out_device,
                                                 void *outer_unknown);
typedef i32 (TH10_STDCALL *InputEnumDevicesFn)(void *self, u32 device_class,
                                                void *callback, void *context,
                                                u32 flags);
typedef i32 (TH10_STDCALL *InputSetPropertyFn)(void *self, u32 property,
                                                const void *header);
typedef i32 (TH10_STDCALL *InputEnumObjectsFn)(void *self, void *callback,
                                                void *context, u32 flags);
typedef i32 (TH10_STDCALL *InputAcquireFn)(void *self);
typedef i32 (TH10_STDCALL *InputSetDataFormatFn)(void *self,
                                                  const void *format);
typedef i32 (TH10_STDCALL *InputSetCooperativeLevelFn)(void *self,
                                                        void *window,
                                                        u32 flags);

extern MainChainContext g_MainChainContext; // TH10 DAT_00491c28
extern const InputGuid g_DirectInput8Aiid;
extern const InputGuid g_SystemKeyboardGuid;
extern const void *g_MainChainKeyboardDataFormat; // c_dfDIKeyboard
extern const void *g_MainChainControllerDataFormat; // 0x00466704
extern const void *PrepareMainChainControllerProperty2c(); // 0x00491c44
extern i32 DirectInput8CreateAdapter(void *instance, u32 version,
                                     const InputGuid *iid, void **out_root);

void *GetVtableSlot(void *object, u32 index)
{
    return static_cast<void **>(static_cast<void **>(object)[0])[index];
}

i32 CreateDevice(void *root, const InputGuid *guid, void **out_device)
{
    return reinterpret_cast<InputCreateDeviceFn>(GetVtableSlot(root, 3))(
        root, guid, out_device, 0);
}

} // namespace

i32 CreateMainChainDirectInputAdapter(void *instance, void **out_root)
{
    return DirectInput8CreateAdapter(instance, 0x800, &g_DirectInput8Aiid,
                                     out_root);
}

i32 CreateMainChainKeyboardAdapter(void *root, void **out_keyboard)
{
    return CreateDevice(root, &g_SystemKeyboardGuid, out_keyboard);
}

i32 SetMainChainKeyboardDataFormatAdapter(void *keyboard)
{
    return reinterpret_cast<InputSetDataFormatFn>(GetVtableSlot(keyboard, 11))(
        keyboard, g_MainChainKeyboardDataFormat);
}

i32 SetMainChainKeyboardCooperativeLevelAdapter(void *keyboard, void *window)
{
    return reinterpret_cast<InputSetCooperativeLevelFn>(GetVtableSlot(keyboard, 13))(
        keyboard, window, 0x16);
}

void AcquireMainChainKeyboardAdapter(void *keyboard)
{
    (void)reinterpret_cast<InputAcquireFn>(GetVtableSlot(keyboard, 7))(keyboard);
}

void EnumerateMainChainControllersAdapter(void *root)
{
    (void)reinterpret_cast<InputEnumDevicesFn>(GetVtableSlot(root, 4))(
        root, 4, reinterpret_cast<void *>(CreateFirstMainChainController), 0, 1);
}

void ConfigureMainChainControllerAdapter(void *controller, void *window)
{
    (void)reinterpret_cast<InputSetDataFormatFn>(GetVtableSlot(controller, 11))(
        controller, g_MainChainControllerDataFormat);
    (void)reinterpret_cast<InputSetCooperativeLevelFn>(GetVtableSlot(controller, 13))(
        controller, window, 0x0a);
    (void)reinterpret_cast<InputSetPropertyFn>(GetVtableSlot(controller, 3))(
        controller, 0x2c, PrepareMainChainControllerProperty2c());
    (void)reinterpret_cast<InputEnumObjectsFn>(GetVtableSlot(controller, 4))(
        controller, reinterpret_cast<void *>(ConfigureMainChainControllerObject),
        0, 0);
}

void UnacquireMainChainInputInterfaceAdapter(void *object)
{
    (void)reinterpret_cast<InputReleaseFn>(GetVtableSlot(object, 8))(object);
}

void ReleaseMainChainInputInterfaceAdapter(void *object)
{
    (void)reinterpret_cast<InputReleaseFn>(GetVtableSlot(object, 2))(object);
}

i32 TH10_STDCALL CreateFirstMainChainController(const void *instance,
                                                 void *)
{
    if (g_MainChainContext.input_controller_0014 != 0)
        return 0;

    const DirectInputDeviceInstancePartial *const device =
        static_cast<const DirectInputDeviceInstancePartial *>(instance);
    return CreateDevice(g_MainChainContext.input_root_000c, &device->instance_guid,
                        &g_MainChainContext.input_controller_0014) < 0 ? 1 : 0;
}

i32 TH10_STDCALL ConfigureMainChainControllerObject(const void *object,
                                                     void *)
{
    const DirectInputDeviceObjectInstancePartial *const device_object =
        static_cast<const DirectInputDeviceObjectInstancePartial *>(object);
    if ((device_object->type & 3U) == 0)
        return 1;

    DirectInputPropertyRangePartial range;
    range.size = sizeof(range);
    range.header_size = 0x10;
    range.object = device_object->type;
    range.how = 2;
    range.minimum = -1000;
    range.maximum = 1000;
    return reinterpret_cast<InputSetPropertyFn>(GetVtableSlot(
        g_MainChainContext.input_controller_0014, 3))(
        g_MainChainContext.input_controller_0014, 4, &range) < 0 ? 0 : 1;
}

} // namespace th10

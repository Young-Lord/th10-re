#include <string.h>

#include "MainChainContext.hpp"

namespace th10 {

namespace {

struct MainChainConfiguration {
    u32 format_magic;
    u16 input_bindings[9];
    u16 field_0016;
    u16 field_0018;
    u8 option_001a;
    u8 option_001b;
    u8 option_001c;
    u8 windowed_mode;
    u8 option_001e;
    u8 option_001f;
    u8 field_0020;
    u8 field_0021;
    u8 field_0022;
    u8 unknown_0023[13];
    u32 runtime_options;
};

typedef char AssertMainChainConfigurationSize[
    sizeof(MainChainConfiguration) == 0x34 ? 1 : -1];

extern MainChainConfiguration g_MainChainConfiguration; // DAT_00491d48
extern u8 g_MainChainInputBindings[18]; // DAT_00474e88
extern i32 g_MainChainDeviceFallbackMode; // DAT_00491fdc

extern void *LoadMainChainFile(const char *path, u32 *file_size,
                               i32 filesystem_mode); // TH10 0x0044b360
extern void FreeMainChainFileBuffer(void *buffer); // TH10 0x00452422
extern i32 WriteMainChainFile(const char *path, const void *data,
                              u32 size); // TH10 0x0044b540
extern void AppendMissingMainChainConfigurationMessage();
extern void AppendInvalidMainChainConfigurationMessage();
extern void AppendMainChainConfigurationWriteFailureMessage(const char *path);
extern void AppendMainChainConfigurationWriteFailureDetail();
extern void AppendMainChainStartupInputFlagMessage(u32 flag);
extern void AppendMainChainStartupInputStateMessage();

void InitializeMainChainConfigurationDefaults(MainChainConfiguration *config)
{
    memset(config, 0, sizeof(*config));
    config->field_0016 = 600;
    config->field_0018 = 600;
    config->option_001b = 1;
    config->option_001c = 1;
    config->runtime_options |= 0x100U;
    config->option_001a = 0;
    config->windowed_mode = 0;
    config->option_001e = 0;
    memcpy(config->input_bindings, g_MainChainInputBindings,
        sizeof(config->input_bindings));
    config->option_001f = 2;
    config->field_0022 = 0;
    config->field_0020 = 100;
    config->field_0021 = 80;
}

bool IsValidMainChainConfiguration(const MainChainConfiguration *config,
                                   u32 file_size)
{
    return config->option_001a < 2 && config->option_001b < 3 &&
        config->option_001c < 2 && config->windowed_mode < 2 &&
        config->option_001e < 3 && config->option_001f < 3 &&
        config->format_magic == 0x00100003U && file_size == sizeof(*config);
}

} // namespace

i32 LoadMainChainConfiguration(MainChainContext *context, const char *path)
{
    InitializeMainChainConfigurationDefaults(&g_MainChainConfiguration);

    u32 file_size;
    void *const file_buffer = LoadMainChainFile(path, &file_size, 1);
    if (file_buffer == 0) {
        AppendMissingMainChainConfigurationMessage();
        InitializeMainChainConfigurationDefaults(&g_MainChainConfiguration);
    } else {
        memcpy(&g_MainChainConfiguration, file_buffer,
            sizeof(g_MainChainConfiguration));
        FreeMainChainFileBuffer(file_buffer);
        if (IsValidMainChainConfiguration(&g_MainChainConfiguration, file_size)) {
            memcpy(g_MainChainInputBindings,
                g_MainChainConfiguration.input_bindings,
                sizeof(g_MainChainInputBindings));
        } else {
            AppendInvalidMainChainConfigurationMessage();
            InitializeMainChainConfigurationDefaults(&g_MainChainConfiguration);
        }
    }

    *reinterpret_cast<u32 *>(reinterpret_cast<u8 *>(context) + 0x3b4) = 0;
    const u8 input_flags = context->input_setup_flags_0150;
    if ((input_flags & 0x4) != 0)
        AppendMainChainStartupInputFlagMessage(0x4);
    if ((input_flags & 0x1) != 0)
        AppendMainChainStartupInputFlagMessage(0x1);
    if (*(reinterpret_cast<u8 *>(context) + 0x13d) != 0)
        AppendMainChainStartupInputStateMessage();
    if ((input_flags & 0x2) != 0)
        AppendMainChainStartupInputFlagMessage(0x2);
    if ((input_flags & 0x8) != 0)
        AppendMainChainStartupInputFlagMessage(0x8);
    if ((input_flags & 0x10) != 0)
        AppendMainChainStartupInputFlagMessage(0x10);
    if ((input_flags & 0x20) != 0) {
        AppendMainChainStartupInputFlagMessage(0x20);
        g_MainChainDeviceFallbackMode = 1;
    }
    if ((input_flags & 0x40) != 0)
        AppendMainChainStartupInputFlagMessage(0x40);

    if (WriteMainChainFile(path, &g_MainChainConfiguration,
            sizeof(g_MainChainConfiguration)) != 0) {
        AppendMainChainConfigurationWriteFailureMessage(path);
        AppendMainChainConfigurationWriteFailureDetail();
        return -1;
    }
    return 0;
}

} // namespace th10

/*
 * SPEmulator — Platform Abstraction
 */
#include "platform/platform.h"

const char *platform_name(void) {
#if defined(PLATFORM_MACOS)
    return "macOS";
#elif defined(PLATFORM_WINDOWS)
    return "Windows";
#elif defined(PLATFORM_LINUX)
    return "Linux";
#else
    return "Unknown";
#endif
}

const char *platform_default_config_path(void) {
    return "spemulator.ini";
}

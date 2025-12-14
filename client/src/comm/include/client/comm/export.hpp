#pragma once

// Export/import macros for shared library symbols.
#ifdef CLIENT_PLATFORM_WINDOWS
#if defined(CLIENT_COMM_BUILDING_SHARED)
#define CLIENT_COMM_API __declspec(dllexport)
#elif defined(CLIENT_COMM_USING_SHARED)
#define CLIENT_COMM_API __declspec(dllimport)
#else
#define CLIENT_COMM_API
#endif
#else
#if defined(CLIENT_COMM_BUILDING_SHARED)
#define CLIENT_COMM_API __attribute__((visibility("default")))
#else
#define CLIENT_COMM_API
#endif
#endif

// Macro for local symbols (hidden visibility on non-Windows).
#ifdef CLIENT_PLATFORM_WINDOWS
#define CLIENT_COMM_LOCAL
#else
#define CLIENT_COMM_LOCAL __attribute__((visibility("hidden")))
#endif

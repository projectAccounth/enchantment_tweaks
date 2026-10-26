// miniz_export.h - simplified for our build
#pragma once

#if defined(_WIN32) && defined(BUILD_MINIZ_DLL)
#define MINIZ_EXPORT __declspec(dllexport)
#elif defined(_WIN32) && defined(USE_MINIZ_DLL)
#define MINIZ_EXPORT __declspec(dllimport)
#else
#define MINIZ_EXPORT
#endif
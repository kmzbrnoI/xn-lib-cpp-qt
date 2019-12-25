#ifndef LIB_API_COMMON_DEF_H
#define LIB_API_COMMON_DEF_H

#if defined(XN_SHARED_LIBRARY)
#define XN_SHARED_EXPORT Q_DECL_EXPORT
#else
#define XN_SHARED_EXPORT Q_DECL_IMPORT
#endif

#ifdef Q_OS_WIN
#define CALL_CONV __stdcall
#else
#define CALL_CONV
#endif

#endif

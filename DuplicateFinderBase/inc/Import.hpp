#pragma once

#if _WIN32
#define DUP_FIN_BASE_WINDOWS
#else
#define DUP_FIN_BASE_LINUX
#endif

#if defined DUP_FIN_BASE_LINUX
#define DUP_FIN_BASE_API
#define DUP_FIN_BASE_POST
#define DUP_FIN_BASE_TEMPLATE
#elif defined DUP_FIN_BASE_WINDOWS && defined( DUP_FIN_BASE_STATIC )
#define DUP_FIN_BASE_API
#define DUP_FIN_BASE_POST
#define DUP_FIN_BASE_TEMPLATE
#else
#define DUP_FIN_BASE_API __declspec( dllexport )
#define DUP_FIN_BASE_POST __cdecl
#define DUP_FIN_BASE_TEMPLATE extern
#endif

#if _MSC_VER
#if defined( _M_X64 ) || defined( __amd64__ )
#pragma warning( disable : 4820 )
#endif
#endif
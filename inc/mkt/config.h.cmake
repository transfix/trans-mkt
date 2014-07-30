#ifndef CONFIG_H
#define CONFIG_H

#define MKT_VERSION "@MKT_VERSION@"
#cmakedefine MKT_USING_XMLRPC

#define NOMINMAX

#ifdef __WINDOWS__
#include <WinSock2.h>
#endif

#endif // CONFIG_H

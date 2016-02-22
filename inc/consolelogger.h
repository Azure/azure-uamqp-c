// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef CONSOLELOGGER_H
#define CONSOLELOGGER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "xlogging.h"

	extern void consolelogger_log(unsigned int options, char* format, ...);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CONSOLELOGGER_H */

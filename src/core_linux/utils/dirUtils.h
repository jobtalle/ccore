#pragma once

#include "../../core/utils/dirUtils.h"
#include "../../core/utils/charUtils.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#ifndef CC_USER_LOCATION
#define CC_USER_LOCATION "~/.config/"
#endif

#ifndef CC_TEMP_LOCATION
#define CC_TEMP_LOCATION "/tmp/"
#endif
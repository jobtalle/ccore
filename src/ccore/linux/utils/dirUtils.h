#pragma once

#include <ccore/dirUtils.h>
#include <ccore/charUtils.h>

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
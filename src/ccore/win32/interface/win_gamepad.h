#pragma once

#include <Windows.h>
#include <hidsdi.h>

#include <ccore/gamepad.h>

#include <ccore/assert.h>

#include "win_window.h"

#define GAMEPAD_MAXBUTTONS 128

ccGamepadEvent _generateGamepadEvent(RAWINPUT *raw);
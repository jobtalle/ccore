#pragma once

#include <dirent.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <linux/joystick.h>

#include <ccore/gamepad.h>
#include <ccore/assert.h>
#include <ccore/print.h>

typedef struct {
	int id, fd;
} ccGamepad_lin;

typedef struct {
	int fd, watch;
} ccGamepads_lin;

#define GAMEPAD_DATA(gamepad) ((ccGamepad_lin*)(gamepad)->data)
#define GAMEPADS_DATA() ((ccGamepads_lin*)(_gamepads)->data)

ccGamepadEvent ccGamepadEventPoll(void);

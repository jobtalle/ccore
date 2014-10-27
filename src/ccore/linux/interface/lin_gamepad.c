#if defined CC_USE_ALL || defined CC_USE_GAMEPAD

#include "lin_gamepad.h"

#define _CC_TEST_BIT(nr, addr) \
	(((1UL << ((nr) & 31)) & (((const unsigned int *) addr)[(nr) >> 5])) != 0)

static int openGamepadDescriptor(char *locName)
{
	char dirName[30];
	int fd;

	snprintf(dirName, 30, "/dev/input/%s", locName);
	fd = open(dirName, O_RDONLY | O_NONBLOCK, 0);

	return fd;
}

static ccReturn initHaptic(int joyId, char *locName)
{
	DIR *d;
	struct dirent *dir;
	char dirName[30];
	int fd;
	unsigned long features[1 + FF_MAX / sizeof(unsigned long)];	
	struct ff_effect effect;

	snprintf(dirName, 30, "/sys/class/input/%s/device", locName);

	d = opendir(dirName);
	fd = -1;
	// Check for the haptic device (event<x>)
	// TODO support multiple motors on one gamepad
	while((dir = readdir(d)) != NULL){
		if(*dir->d_name == 'e'){
			snprintf(dirName, 30, "/dev/input/%s", dir->d_name);
			fd = open(dirName, O_RDWR, 0);
			if(!fd){
				continue;
			}

			if(ioctl(fd, EVIOCGBIT(EV_FF, sizeof(features)), features) < 0){
				fd = -1;
				close(fd);
				continue;
			}

			if(!_CC_TEST_BIT(FF_RUMBLE, features)){
				fd = -1;
				close(fd);
				continue;
			}

			break;
		}	
	}
	closedir(d);

	if(fd < 0){
		return CC_FAIL;
	}

	effect.type = FF_RUMBLE;
	effect.u.rumble.strong_magnitude = 65535;
	effect.u.rumble.weak_magnitude = 65535;
	effect.replay.length = USHRT_MAX;
	effect.replay.delay = 0;
	effect.id = -1;

	if(ioctl(fd, EVIOCSFF, &effect) < 0){
		close(fd);
		return CC_FAIL;
	}

	GAMEPAD_DATA(_ccGamepads->gamepad + joyId)->fffd = fd;
	GAMEPAD_DATA(_ccGamepads->gamepad + joyId)->ffid = effect.id;

	_ccGamepads->gamepad[joyId].outputAmount++;

	return CC_SUCCESS;
}

static ccReturn createGamepad(char *locName, int i)
{
	char buf[64];
	int fd;

	fd = openGamepadDescriptor(locName);
	if(fd < 0){
		if(errno != EACCES){
			ccErrorPush(CC_ERROR_GAMEPAD_DATA);
			return CC_FAIL;
		}else{
			return CC_FAIL;
		}
	}

	if(i == 0){
		ccMalloc(_ccGamepads->gamepad, sizeof(ccGamepad));
	}else{
		ccRealloc(_ccGamepads->gamepad, (i + 1) * sizeof(ccGamepad));
	}
	ccMalloc((_ccGamepads->gamepad + i)->data, sizeof(ccGamepad_lin));

	// Clear gamepad buffer
	while(read(fd, buf, 64) > 0);

	_ccGamepads->gamepad[i].plugged = true;
	_ccGamepads->gamepad[i].axisAmount = 
		_ccGamepads->gamepad[i].buttonAmount = 
		_ccGamepads->gamepad[i].outputAmount = 0;
	ccMalloc(_ccGamepads->gamepad[i].name, 80);

	ioctl(fd, JSIOCGAXES, &_ccGamepads->gamepad[i].axisAmount);
	ioctl(fd, JSIOCGBUTTONS, &_ccGamepads->gamepad[i].buttonAmount);
	ioctl(fd, JSIOCGNAME(80), _ccGamepads->gamepad[i].name);

	ccCalloc(_ccGamepads->gamepad[i].axis, _ccGamepads->gamepad[i].axisAmount, sizeof(int));
	ccCalloc(_ccGamepads->gamepad[i].button, _ccGamepads->gamepad[i].buttonAmount, sizeof(char));

	GAMEPAD_DATA(_ccGamepads->gamepad + i)->fd = fd;
	GAMEPAD_DATA(_ccGamepads->gamepad + i)->fffd = -1;
	GAMEPAD_DATA(_ccGamepads->gamepad + i)->id = atoi(locName + 2);

	initHaptic(i, locName);

	return CC_SUCCESS;
}

static bool canReadINotify(void)
{
	fd_set set;
	struct timeval timeout;

	FD_ZERO(&set);
	FD_SET(GAMEPADS_DATA()->fd, &set);
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	return select(GAMEPADS_DATA()->fd + 1, &set, NULL, NULL, &timeout) > 0 && 
		FD_ISSET(GAMEPADS_DATA()->fd, &set);
}

ccGamepadEvent ccGamepadEventPoll(void)
{
	struct js_event js;
	struct inotify_event ne;
	ccGamepadEvent event;
	int i, id;

	while(canReadINotify()){
		if(CC_LIKELY(read(GAMEPADS_DATA()->fd, &ne, sizeof(struct inotify_event) + 16) >= 0)){
			if(*ne.name != 'j'){
				continue;
			}

			// Find the matching gamepad
			event.id = -1;
			id = atoi(ne.name + 2);	
			for(i = 0; i < _ccGamepads->amount; i++){
				if(GAMEPAD_DATA(_ccGamepads->gamepad + i)->id == id){
					event.id = i;
					break;
				}
			}

			if(ne.mask & IN_DELETE){
				if(event.id != -1){
					_ccGamepads->gamepad[event.id].plugged = false;
					close(GAMEPAD_DATA(_ccGamepads->gamepad + event.id)->fd);

					// Close haptic when available
					if(GAMEPAD_DATA(_ccGamepads->gamepad + event.id)->fffd > 0){
						ioctl(GAMEPAD_DATA(_ccGamepads->gamepad + event.id)->fffd, 
								EVIOCRMFF, GAMEPAD_DATA(_ccGamepads->gamepad + event.id)->ffid);
						close(GAMEPAD_DATA(_ccGamepads->gamepad + event.id)->fffd);
					}
				}

				event.type = CC_GAMEPAD_DISCONNECT;
				return event;
			}else if(ne.mask & IN_ATTRIB){
				if(event.id != -1){
					if(_ccGamepads->gamepad[event.id].plugged){
						continue;
					}
					_ccGamepads->gamepad[event.id].plugged = true;
					GAMEPAD_DATA(_ccGamepads + event.id)->fd = openGamepadDescriptor(ne.name);
					if(GAMEPAD_DATA(_ccGamepads + event.id)->fd < 0){
						_ccGamepads->gamepad[event.id].plugged = false;
						GAMEPAD_DATA(_ccGamepads + event.id)->fd = 0;
					}
					event.type = CC_GAMEPAD_CONNECT;
					return event;
				}else{
					if(createGamepad(ne.name, _ccGamepads->amount) == CC_SUCCESS){
						event.id = _ccGamepads->amount;
						_ccGamepads->amount++;
						event.type = CC_GAMEPAD_CONNECT;
						return event;
					}
				}
			}
		}
	}

	event.type = CC_GAMEPAD_UNHANDLED;
	for(i = 0; i < _ccGamepads->amount; i++){
		if(!_ccGamepads->gamepad[i].plugged){
			continue;
		}
		if(CC_LIKELY(read(GAMEPAD_DATA(_ccGamepads->gamepad + i)->fd, &js, sizeof(struct js_event)) > 0)){
			event.id = i;
			event.type = CC_GAMEPAD_UNHANDLED;

			switch(js.type & ~JS_EVENT_INIT){
				case JS_EVENT_AXIS:
					if(_ccGamepads->gamepad[i].axis[js.number] != js.value){
						event.axisId = js.number;

						event.type = CC_GAMEPAD_AXIS_MOVE;

						_ccGamepads->gamepad[i].axis[js.number] = js.value;
						return event;
					}
				case JS_EVENT_BUTTON:
					if(_ccGamepads->gamepad[i].button[js.number] != (js.value != 0)){
						event.buttonId = js.number;

						if(_ccGamepads->gamepad[i].button[js.number] == 0){
							event.type = CC_GAMEPAD_BUTTON_DOWN;
						}else{
							event.type = CC_GAMEPAD_BUTTON_UP;
						}

						_ccGamepads->gamepad[i].button[js.number] = js.value != 0;
						return event;
					}
			}
		}
	}

	return event;
}

ccReturn ccGamepadInitialize(void)
{
	DIR *d;
	struct dirent *dir;
	int fd, watch;

	ccGamepadFree();

	// Attach notifications to check if a device connects/disconnects
	fd = inotify_init();
	if(CC_UNLIKELY(fd < 0)){
		ccErrorPush(CC_ERROR_GAMEPAD_DATA);
		goto error;
	}

	watch = inotify_add_watch(fd, "/dev/input", IN_DELETE | IN_ATTRIB);
	if(CC_UNLIKELY(watch < 0)){
		ccErrorPush(CC_ERROR_GAMEPAD_DATA);
		goto error;
	}

	if(CC_LIKELY(_ccGamepads == NULL)){
		ccMalloc(_ccGamepads, sizeof(ccGamepads));
		ccMalloc(_ccGamepads->data, sizeof(ccGamepads_lin));
	}
	_ccGamepads->amount = 0;

	GAMEPADS_DATA()->fd = fd;
	GAMEPADS_DATA()->watch = watch;

	d = opendir("/dev/input");
	// Check for gamepads (js<x>)
	while((dir = readdir(d)) != NULL){
		if(*dir->d_name == 'j'){
			if(CC_UNLIKELY(createGamepad(dir->d_name, _ccGamepads->amount) == CC_FAIL)){
				goto error;
			}
			_ccGamepads->amount++;
		}
	}
	closedir(d);

	if(_ccGamepads->amount == 0){
		ccErrorPush(CC_ERROR_GAMEPAD_NONE);
	}else{
		return CC_SUCCESS;
	}

error:
	free(_ccGamepads->data);
	free(_ccGamepads);
	closedir(d);
	close(fd);
	close(watch);
	return CC_FAIL;
}

ccReturn ccGamepadOutputSet(ccGamepad *gamepad, int outputIndex, int force)
{
	struct input_event ffev;

	if(GAMEPAD_DATA(gamepad)->fffd < 0){
		return CC_FAIL;
	}	

	ffev.type = EV_FF;
	ffev.code = GAMEPAD_DATA(gamepad)->ffid; 
	if(force < CC_GAMEPAD_OUTPUT_VALUE_MIN){
		ffev.value = 0;
	}else{
		ffev.value = 1;
	}

	write(GAMEPAD_DATA(gamepad)->fffd, (const void*)&ffev, sizeof(ffev));

	return CC_SUCCESS;
}

ccReturn ccGamepadFree(void)
{
	int i;

	if(CC_UNLIKELY(_ccGamepads == NULL)){
		return CC_SUCCESS;
	}

	inotify_rm_watch(GAMEPADS_DATA()->fd, GAMEPADS_DATA()->watch);
	close(GAMEPADS_DATA()->fd);

	if(_ccGamepads->amount != 0){
		for(i = 0; i < _ccGamepads->amount; i++){
			if(_ccGamepads->gamepad[i].plugged){
				close(GAMEPAD_DATA(_ccGamepads->gamepad + i)->fd);

				if(GAMEPAD_DATA(_ccGamepads->gamepad + i)->fffd > 0){
					ioctl(GAMEPAD_DATA(_ccGamepads->gamepad + i)->fffd, 
							EVIOCRMFF, GAMEPAD_DATA(_ccGamepads->gamepad + i)->ffid);
					close(GAMEPAD_DATA(_ccGamepads->gamepad +i)->fffd);
				}
			}

			free(_ccGamepads->gamepad[i].name);
			free(_ccGamepads->gamepad[i].data);
			free(_ccGamepads->gamepad[i].button);
			free(_ccGamepads->gamepad[i].axis);
		}
		free(_ccGamepads->gamepad);
	}
	free(_ccGamepads->data);
	free(_ccGamepads);

	_ccGamepads = NULL;

	return CC_SUCCESS;
}

#endif

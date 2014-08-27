#include "win_gamepad.h"

ccError ccGamepadInitialize(void)
{
	ccAssert(_window != NULL);

	ccMalloc(_gamepads, sizeof(ccGamepads));
	ccMalloc(_gamepads->data, sizeof(ccGamepads_win));
	_gamepads->amount = 0;
	_gamepads->gamepad = NULL;
	
	WINDOW_DATA->queryXinput = true;

	WINDOW_DATA->rid[RAWINPUT_GAMEPAD].usUsagePage = 1;
	WINDOW_DATA->rid[RAWINPUT_GAMEPAD].usUsage = 4;
	WINDOW_DATA->rid[RAWINPUT_GAMEPAD].dwFlags = 0;
	WINDOW_DATA->rid[RAWINPUT_GAMEPAD].hwndTarget = WINDOW_DATA->winHandle;

	return RegisterRawInputDevices(&WINDOW_DATA->rid[RAWINPUT_GAMEPAD], 1, sizeof(RAWINPUTDEVICE)) == TRUE?CC_ERROR_NONE:CC_ERROR_NOGAMEPAD;
}

void ccGamepadFree(void)
{
	ccAssert(_gamepads != NULL);
	
	WINDOW_DATA->queryXinput = false;

	WINDOW_DATA->rid[RAWINPUT_GAMEPAD].dwFlags = RIDEV_REMOVE;
	WINDOW_DATA->rid[RAWINPUT_GAMEPAD].hwndTarget = NULL;
	
	RegisterRawInputDevices(&WINDOW_DATA->rid[RAWINPUT_GAMEPAD], 1, sizeof(RAWINPUTDEVICE));

	if(ccGamepadCount() != 0) {
		int i;
		for(i = 0; i < ccGamepadCount(); i++) {
			if(((ccGamepad_win*)_gamepads->gamepad[i].data)->inputType == CC_GAMEPAD_INPUT_RAW) {
				free(((ccGamepad_win*)_gamepads->gamepad[i].data)->raw.buttonCaps);
				free(((ccGamepad_win*)_gamepads->gamepad[i].data)->raw.valueCaps);
				free(((ccGamepad_win*)_gamepads->gamepad[i].data)->raw.axisFactor);
				free(((ccGamepad_win*)_gamepads->gamepad[i].data)->raw.axisNegativeComponent);
				free(((ccGamepad_win*)_gamepads->gamepad[i].data)->raw.preparsedData);
			}
			free(_gamepads->gamepad[i].data);
			free(_gamepads->gamepad[i].button);
			free(_gamepads->gamepad[i].axis);
		}
		free(_gamepads->gamepad);
	}

	free(_gamepads->data);
	free(_gamepads);
}

void _queryXinput()
{
	int i;

	for(i = 0; i < XUSER_MAX_COUNT; i++) {
		XINPUT_STATE state;
		DWORD result;

		ZeroMemory(&state, sizeof(XINPUT_STATE));
		result = XInputGetState(i, &state);

		//if(result == ERROR_SUCCESS) printf("connected %d\n", i);
	}
}

void _generateGamepadEvents(RAWINPUT *raw)
{
	ccEvent event;
	ULONG usageLength;
	int i, j;

	int newInt;
	
	// Find the current gamepad or create it
	ccGamepad *currentGamepad = NULL;

	event.type = CC_EVENT_GAMEPAD;

	for(i = 0; i < ccGamepadCount(); i++) {
		if(((ccGamepad_win*)(_gamepads->gamepad[i].data))->raw.handle == raw->header.hDevice) {
			currentGamepad = &_gamepads->gamepad[i];
			event.gamepadEvent.id = i;
			break;
		}
	}
	if(currentGamepad == NULL) {
		USHORT capsLength;

		_gamepads->amount++;
		_gamepads->gamepad = realloc(_gamepads->gamepad, ccGamepadCount() * sizeof(ccGamepad));

		currentGamepad = &_gamepads->gamepad[ccGamepadCount() - 1];
		event.gamepadEvent.id = ccGamepadCount() - 1;

		// Initialize current gamepad
		currentGamepad->data = malloc(sizeof(ccGamepad_win));

		GAMEPAD_DATA->inputType = CC_GAMEPAD_INPUT_RAW;

		GetRawInputDeviceInfo(raw->header.hDevice, RIDI_PREPARSEDDATA, NULL, &GAMEPAD_DATA->raw.preparsedDataSize);
		GAMEPAD_DATA->raw.preparsedData = malloc(GAMEPAD_DATA->raw.preparsedDataSize);
		GetRawInputDeviceInfo(raw->header.hDevice, RIDI_PREPARSEDDATA, GAMEPAD_DATA->raw.preparsedData, &GAMEPAD_DATA->raw.preparsedDataSize);

		currentGamepad->name = "Gamepad"; //TODO: can I fetch this?
		currentGamepad->plugged = true; //TODO: use this properly
		currentGamepad->supportsVibration = false;
		GAMEPAD_DATA->raw.handle = raw->header.hDevice;
		HidP_GetCaps(GAMEPAD_DATA->raw.preparsedData, &GAMEPAD_DATA->raw.caps);

		GAMEPAD_DATA->raw.buttonCaps = malloc(sizeof(HIDP_BUTTON_CAPS)* GAMEPAD_DATA->raw.caps.NumberInputButtonCaps);
		GAMEPAD_DATA->raw.valueCaps = malloc(sizeof(HIDP_VALUE_CAPS)* GAMEPAD_DATA->raw.caps.NumberInputValueCaps);

		capsLength = GAMEPAD_DATA->raw.caps.NumberInputButtonCaps;
		HidP_GetButtonCaps(HidP_Input, GAMEPAD_DATA->raw.buttonCaps, &capsLength, GAMEPAD_DATA->raw.preparsedData);
		capsLength = GAMEPAD_DATA->raw.caps.NumberInputValueCaps;
		HidP_GetValueCaps(HidP_Input, GAMEPAD_DATA->raw.valueCaps, &capsLength, GAMEPAD_DATA->raw.preparsedData);

		currentGamepad->buttonAmount = GAMEPAD_DATA->raw.buttonCaps->Range.UsageMax - GAMEPAD_DATA->raw.buttonCaps->Range.UsageMin + 1;
		currentGamepad->axisAmount = GAMEPAD_DATA->raw.caps.NumberInputValueCaps;
		
		currentGamepad->button = calloc(currentGamepad->buttonAmount, sizeof(bool));
		currentGamepad->axis = malloc(sizeof(int)* currentGamepad->axisAmount);
		GAMEPAD_DATA->raw.axisFactor = malloc(sizeof(double)* currentGamepad->axisAmount);
		GAMEPAD_DATA->raw.axisNegativeComponent = malloc(sizeof(int)* currentGamepad->axisAmount);

		for(i = 0; i < currentGamepad->axisAmount; i++) {
			GAMEPAD_DATA->raw.axisFactor[i] = (double)(GAMEPAD_AXIS_MAX - GAMEPAD_AXIS_MIN) / (GAMEPAD_DATA->raw.valueCaps[i].PhysicalMax - GAMEPAD_DATA->raw.valueCaps[i].PhysicalMin);
			GAMEPAD_DATA->raw.axisNegativeComponent[i] = ((GAMEPAD_DATA->raw.valueCaps[i].PhysicalMax - GAMEPAD_DATA->raw.valueCaps[i].PhysicalMin) >> 1) - GAMEPAD_DATA->raw.valueCaps[i].PhysicalMin;
		}

		event.gamepadEvent.type = CC_GAMEPAD_CONNECT;
		_ccEventStackPush(event);
	}
	
	// Get buttons
	usageLength = currentGamepad->buttonAmount;
	HidP_GetUsages(HidP_Input, GAMEPAD_DATA->raw.buttonCaps->UsagePage, 0, GAMEPADS_DATA->usage, &usageLength, GAMEPAD_DATA->raw.preparsedData, raw->data.hid.bRawData, raw->data.hid.dwSizeHid);
	
	for(i = 0; i < (int)usageLength; i++)
	{
		int index = GAMEPADS_DATA->usage[i] - GAMEPAD_DATA->raw.buttonCaps->Range.UsageMin;
		if(currentGamepad->button[index] == false) {
			currentGamepad->button[index] = true;
			
			event.gamepadEvent.type = CC_GAMEPAD_BUTTON_DOWN;
			event.gamepadEvent.buttonId = index;
			_ccEventStackPush(event);
		}
	}
	for(i = 0; i < currentGamepad->buttonAmount; i++) {
		if(currentGamepad->button[i] == true) {
			for(j = 0; j < (int)usageLength; j++) {
				if(currentGamepad->button[GAMEPADS_DATA->usage[j] - GAMEPAD_DATA->raw.buttonCaps->Range.UsageMin] == true) {
					goto pressed;
				}
			}
			currentGamepad->button[i] = false;

			event.gamepadEvent.type = CC_GAMEPAD_BUTTON_UP;
			event.gamepadEvent.buttonId = i;
			_ccEventStackPush(event);
		}
	pressed:;
	}

	// Get axes
	usageLength = currentGamepad->axisAmount;

	for(i = 0; i < currentGamepad->axisAmount; i++)
	{
		HidP_GetUsageValue(HidP_Input, GAMEPAD_DATA->raw.valueCaps[i].UsagePage, 0, GAMEPAD_DATA->raw.valueCaps[i].NotRange.Usage, &newInt, GAMEPAD_DATA->raw.preparsedData, raw->data.hid.bRawData, raw->data.hid.dwSizeHid);
		newInt = (int)((newInt - GAMEPAD_DATA->raw.axisNegativeComponent[i]) * GAMEPAD_DATA->raw.axisFactor[i]);
		if(newInt < GAMEPAD_AXIS_MIN) {
			newInt = GAMEPAD_AXIS_MIN;
		}
		else if(newInt > GAMEPAD_AXIS_MAX) {
			newInt = GAMEPAD_AXIS_MAX;
		}

		if(newInt != currentGamepad->axis[i]) {
			currentGamepad->axis[i] = newInt;
			event.gamepadEvent.type = CC_GAMEPAD_AXIS_MOVE;
			event.gamepadEvent.axisId = i;
			_ccEventStackPush(event);
		}
	}
}
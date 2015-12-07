// v_repExtXbox360Controller.cpp : Defines the exported functions for the DLL application.
//

/*
  Lua extensions:

boolean isConnected = simExtXbox360Controller_isConnected()
number port = simExtXbox360Controller_getPort() -- return integer x in [1, 4]
boolean refreshState = simExtXbox360Controller_refreshState()

boolean isStartPressed = simExtXbox360Controller_isStartPressed()
boolean isBackPressed = simExtXbox360Controller_isBackPressed()

boolean isAPressed = simExtXbox360Controller_isAPressed()
boolean isBPressed = simExtXbox360Controller_isBPressed()
boolean isXPressed = simExtXbox360Controller_isXPressed()
boolean isYPressed = simExtXbox360Controller_isYPressed()

boolean isDPadLeftPressed = simExtXbox360Controller_isDPadLeftPressed()
boolean isDPadRightPressed = simExtXbox360Controller_isDPadRightPressed()
boolean isDPadUpPressed = simExtXbox360Controller_isDPadUpPressed()
boolean isDPadDownPressed = simExtXbox360Controller_isDPadDownPressed()

boolean isLeftShoulderPressed = simExtXbox360Controller_isLeftShoulderPressed()
boolean isRightShoulderPressed = simExtXbox360Controller_isRightShoulderPressed()

boolean isLeftThumbStickPressed = simExtXbox360Controller_isLeftThumbStickPressed()
boolean isRightThumbStickPressed = simExtXbox360Controller_isRightThumbStickPressed()

table_2 leftThumbStickCoords = simExtXbox360Controller_getLeftThumbStickCoords() -- returns table_2 (x, y) . x, y in [-1, 1]
table_2 rightThumbStickCoords = simExtXbox360Controller_getRightThumbStickCoords() -- returns table_2 (x, y) . x, y in [-1, 1]

float leftTriggerPressure = simExtXbox360Controller_getLeftTriggerPressure() -- returns float x in [0, 1]
float rightTriggerPressure = simExtXbox360Controller_getRightTriggerPressure() -- returns float x in [0, 1]

*/



//#define DEBUG


#include "v_repExtXbox360Controller.h"

#include <Xinput.h>

#include <iostream>
#include <vector>

#include "luaFunctionData.h"
#include "v_repLib.h"


#ifdef _WIN32
#ifdef QT_COMPIL
#include <direct.h>
#else
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#endif
#endif /* _WIN32 */
#if defined (__linux) || defined (__APPLE__)
#include <unistd.h>
#define WIN_AFX_MANAGE_STATE
#endif /* __linux || __APPLE__ */

#define CONCAT(x,y,z) x y z
#define strConCat(x,y,z)	CONCAT(x,y,z)

LIBRARY vrepLib;



// --------------------------------------------------------------------------------------
// Gamepad class
//
// https://katyscode.wordpress.com/2013/08/30/xinput-tutorial-part-1-adding-gamepad-support-to-your-windows-game/
// --------------------------------------------------------------------------------------

class Gamepad
{
private:
	int _cId;
	XINPUT_STATE _state;

	float _deadzoneX;
	float _deadzoneY;

public:
	Gamepad() : _deadzoneX(0.05f), _deadzoneY(0.02f) {}
	Gamepad(float dzX, float dzY) : _deadzoneX(dzX), _deadzoneY(dzY) {}

	float leftStickX;
	float leftStickY;
	float rightStickX;
	float rightStickY;
	float leftTrigger;
	float rightTrigger;

	int  getPort();
	XINPUT_GAMEPAD* getState();
	bool checkConnection();
	bool refresh();
	bool isPressed(WORD);
};

int Gamepad::getPort()
{
	return _cId + 1;
}

XINPUT_GAMEPAD* Gamepad::getState()
{
	return &_state.Gamepad;
}

bool Gamepad::checkConnection()
{
	int controllerId = -1;

	for (DWORD i = 0; i < XUSER_MAX_COUNT && controllerId == -1; ++i)
	{
		XINPUT_STATE state;
		ZeroMemory(&state, sizeof(XINPUT_STATE));

		if (XInputGetState(i, &state) == ERROR_SUCCESS)
		{
			controllerId = i;
		}
	}

	_cId = controllerId;

	return (controllerId != -1);
}

// Returns false if the controller has been disconnected
bool Gamepad::refresh()
{
	if (_cId == -1)
	{
		checkConnection();
	}

	if (_cId != -1)
	{
		ZeroMemory(&_state, sizeof(XINPUT_STATE));
		if (XInputGetState(_cId, &_state) != ERROR_SUCCESS)
		{
			_cId = -1;
			return false;
		}

		float normLX = fmaxf(-1, (float)_state.Gamepad.sThumbLX / 32767);
		float normLY = fmaxf(-1, (float)_state.Gamepad.sThumbLY / 32767);

		leftStickX = (abs(normLX) < _deadzoneX ? 0 : (abs(normLX) - _deadzoneX) * (normLX / abs(normLX)));
		leftStickY = (abs(normLY) < _deadzoneY ? 0 : (abs(normLY) - _deadzoneY) * (normLY / abs(normLY)));

		if (_deadzoneX > 0) leftStickX *= 1 / (1 - _deadzoneX);
		if (_deadzoneY > 0) leftStickY *= 1 / (1 - _deadzoneY);

		float normRX = fmaxf(-1, (float)_state.Gamepad.sThumbRX / 32767);
		float normRY = fmaxf(-1, (float)_state.Gamepad.sThumbRY / 32767);

		rightStickX = (abs(normRX) < _deadzoneX ? 0 : (abs(normRX) - _deadzoneX) * (normRX / abs(normRX)));
		rightStickY = (abs(normRY) < _deadzoneY ? 0 : (abs(normRY) - _deadzoneY) * (normRY / abs(normRY)));

		if (_deadzoneX > 0) rightStickX *= 1 / (1 - _deadzoneX);
		if (_deadzoneY > 0) rightStickY *= 1 / (1 - _deadzoneY);

		leftTrigger = (float)_state.Gamepad.bLeftTrigger / 255;
		rightTrigger = (float)_state.Gamepad.bRightTrigger / 255;

		return true;
	}
	return false;
}

bool Gamepad::isPressed(WORD button)
{
	return ((_state.Gamepad.wButtons & button) != 0);
}


// internal XBOX 360 Controller representation
Gamepad gamepad;


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_isConnected
// --------------------------------------------------------------------------------------
#define LUA_IS_CONNECTED_COMMAND "simExtXbox360Controller_isConnected"

const int inArgs_IS_CONNECTED[] = {
	0,
};

void LUA_IS_CONNECTED_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_CONNECTED, inArgs_IS_CONNECTED[0], LUA_IS_CONNECTED_COMMAND))
	{
		// no inArgs to work with
	}

	bool isConnected = gamepad.checkConnection();

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: Controller " << (isConnected ? "CONNECTED" : "NOT connected") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(isConnected));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_getPort
//
// return integer x in [1, 4]
// --------------------------------------------------------------------------------------
#define LUA_GET_PORT_COMMAND "simExtXbox360Controller_getPort"

const int inArgs_GET_PORT[] = {
	0,
};

void LUA_GET_PORT_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_CONNECTED, inArgs_GET_PORT[0], LUA_GET_PORT_COMMAND))
	{
		// no inArgs to work with
	}

	int port = gamepad.getPort();

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: Controller connected to port ([1, 4]): " << port << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(gamepad.getPort()));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_refreshState
// --------------------------------------------------------------------------------------
#define LUA_REFRESH_STATE_COMMAND "simExtXbox360Controller_refreshState"

const int inArgs_REFRESH_STATE[] = {
	0,
};

void LUA_REFRESH_STATE_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_REFRESH_STATE, inArgs_REFRESH_STATE[0], LUA_REFRESH_STATE_COMMAND))
	{
		// no inArgs to work with
	}

	bool refreshStateResult = gamepad.refresh();

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: Refresh result " << (refreshStateResult ? "OK" : "KO") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(refreshStateResult));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_isStartPressed
// --------------------------------------------------------------------------------------
#define LUA_IS_START_PRESSED_COMMAND "simExtXbox360Controller_isStartPressed"

const int inArgs_IS_START_PRESSED[] = {
	0,
};

void LUA_IS_START_PRESSED_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_START_PRESSED, inArgs_IS_START_PRESSED[0], LUA_IS_START_PRESSED_COMMAND))
	{
		// no inArgs to work with
	}

	bool isStartPressed = gamepad.isPressed(XINPUT_GAMEPAD_START);

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: XINPUT_GAMEPAD_START " << (isStartPressed ? "IS pressed" : "is NOT pressed") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(isStartPressed));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_isBackPressed
// --------------------------------------------------------------------------------------
#define LUA_IS_BACK_PRESSED_COMMAND "simExtXbox360Controller_isBackPressed"

const int inArgs_IS_BACK_PRESSED[] = {
	0,
};

void LUA_IS_BACK_PRESSED_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_BACK_PRESSED, inArgs_IS_BACK_PRESSED[0], LUA_IS_BACK_PRESSED_COMMAND))
	{
		// no inArgs to work with
	}

	bool isBackPressed = gamepad.isPressed(XINPUT_GAMEPAD_BACK);

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: XINPUT_GAMEPAD_BACK " << (isBackPressed ? "IS pressed" : "is NOT pressed") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(isBackPressed));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_isAPressed
// --------------------------------------------------------------------------------------
#define LUA_IS_A_PRESSED_COMMAND "simExtXbox360Controller_isAPressed"

const int inArgs_IS_A_PRESSED[] = {
	0,
};

void LUA_IS_A_PRESSED_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_A_PRESSED, inArgs_IS_A_PRESSED[0], LUA_IS_A_PRESSED_COMMAND))
	{
		// no inArgs to work with
	}

	bool isAPressed = gamepad.isPressed(XINPUT_GAMEPAD_A);

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: XINPUT_GAMEPAD_A " << (isAPressed ? "IS pressed" : "is NOT pressed") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(isAPressed));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_isBPressed
// --------------------------------------------------------------------------------------
#define LUA_IS_B_PRESSED_COMMAND "simExtXbox360Controller_isBPressed"

const int inArgs_IS_B_PRESSED[] = {
	0,
};

void LUA_IS_B_PRESSED_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_B_PRESSED, inArgs_IS_B_PRESSED[0], LUA_IS_B_PRESSED_COMMAND))
	{
		// no inArgs to work with
	}

	bool isBPressed = gamepad.isPressed(XINPUT_GAMEPAD_B);

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: XINPUT_GAMEPAD_B " << (isBPressed ? "IS pressed" : "is NOT pressed") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(isBPressed));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_isXPressed
// --------------------------------------------------------------------------------------
#define LUA_IS_X_PRESSED_COMMAND "simExtXbox360Controller_isXPressed"

const int inArgs_IS_X_PRESSED[] = {
	0,
};

void LUA_IS_X_PRESSED_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_X_PRESSED, inArgs_IS_X_PRESSED[0], LUA_IS_X_PRESSED_COMMAND))
	{
		// no inArgs to work with
	}

	bool isXPressed = gamepad.isPressed(XINPUT_GAMEPAD_X);

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: XINPUT_GAMEPAD_X " << (isXPressed ? "IS pressed" : "is NOT pressed") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(isXPressed));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_isYPressed
// --------------------------------------------------------------------------------------
#define LUA_IS_Y_PRESSED_COMMAND "simExtXbox360Controller_isYPressed"

const int inArgs_IS_Y_PRESSED[] = {
	0,
};

void LUA_IS_Y_PRESSED_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_Y_PRESSED, inArgs_IS_Y_PRESSED[0], LUA_IS_Y_PRESSED_COMMAND))
	{
		// no inArgs to work with
	}

	bool isYPressed = gamepad.isPressed(XINPUT_GAMEPAD_Y);

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: XINPUT_GAMEPAD_Y " << (isYPressed ? "IS pressed" : "is NOT pressed") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(isYPressed));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_isDPadLeftPressed
// --------------------------------------------------------------------------------------
#define LUA_IS_DPAD_LEFT_PRESSED_COMMAND "simExtXbox360Controller_isDPadLeftPressed"

const int inArgs_IS_DPAD_LEFT_PRESSED[] = {
	0,
};

void LUA_IS_DPAD_LEFT_PRESSED_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_DPAD_LEFT_PRESSED, inArgs_IS_DPAD_LEFT_PRESSED[0], LUA_IS_DPAD_LEFT_PRESSED_COMMAND))
	{
		// no inArgs to work with
	}

	bool isDPadLeftPressed = gamepad.isPressed(XINPUT_GAMEPAD_DPAD_LEFT);

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: XINPUT_GAMEPAD_DPAD_LEFT " << (isDPadLeftPressed ? "IS pressed" : "is NOT pressed") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(isDPadLeftPressed));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_isDPadRightPressed
// --------------------------------------------------------------------------------------
#define LUA_IS_DPAD_RIGHT_PRESSED_COMMAND "simExtXbox360Controller_isDPadRightPressed"

const int inArgs_IS_DPAD_RIGHT_PRESSED[] = {
	0,
};

void LUA_IS_DPAD_RIGHT_PRESSED_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_DPAD_RIGHT_PRESSED, inArgs_IS_DPAD_RIGHT_PRESSED[0], LUA_IS_DPAD_RIGHT_PRESSED_COMMAND))
	{
		// no inArgs to work with
	}

	bool isDPadRightPressed = gamepad.isPressed(XINPUT_GAMEPAD_DPAD_RIGHT);

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: XINPUT_GAMEPAD_DPAD_RIGHT " << (isDPadRightPressed ? "IS pressed" : "is NOT pressed") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(isDPadRightPressed));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_isDPadUpPressed
// --------------------------------------------------------------------------------------
#define LUA_IS_DPAD_UP_PRESSED_COMMAND "simExtXbox360Controller_isDPadUpPressed"

const int inArgs_IS_DPAD_UP_PRESSED[] = {
	0,
};

void LUA_IS_DPAD_UP_PRESSED_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_DPAD_UP_PRESSED, inArgs_IS_DPAD_UP_PRESSED[0], LUA_IS_DPAD_UP_PRESSED_COMMAND))
	{
		// no inArgs to work with
	}

	bool isDPadUpPressed = gamepad.isPressed(XINPUT_GAMEPAD_DPAD_UP);

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: XINPUT_GAMEPAD_DPAD_UP " << (isDPadUpPressed ? "IS pressed" : "is NOT pressed") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(isDPadUpPressed));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_isDPadDownPressed
// --------------------------------------------------------------------------------------
#define LUA_IS_DPAD_DOWN_PRESSED_COMMAND "simExtXbox360Controller_isDPadDownPressed"

const int inArgs_IS_DPAD_DOWN_PRESSED[] = {
	0,
};

void LUA_IS_DPAD_DOWN_PRESSED_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_DPAD_DOWN_PRESSED, inArgs_IS_DPAD_DOWN_PRESSED[0], LUA_IS_DPAD_DOWN_PRESSED_COMMAND))
	{
		// no inArgs to work with
	}

	bool isDPadDownPressed = gamepad.isPressed(XINPUT_GAMEPAD_DPAD_DOWN);

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: XINPUT_GAMEPAD_DPAD_DOWN " << (isDPadDownPressed ? "IS pressed" : "is NOT pressed") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(isDPadDownPressed));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_isLeftShoulderPressed
// --------------------------------------------------------------------------------------
#define LUA_IS_LEFT_SHOULDER_PRESSED_COMMAND "simExtXbox360Controller_isLeftShoulderPressed"

const int inArgs_IS_LEFT_SHOULDER_PRESSED[] = {
	0,
};

void LUA_IS_LEFT_SHOULDER_PRESSED_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_LEFT_SHOULDER_PRESSED, inArgs_IS_LEFT_SHOULDER_PRESSED[0], LUA_IS_LEFT_SHOULDER_PRESSED_COMMAND))
	{
		// no inArgs to work with
	}

	bool isLeftShoulderPressed = gamepad.isPressed(XINPUT_GAMEPAD_LEFT_SHOULDER);

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: XINPUT_GAMEPAD_LEFT_SHOULDER " << (isLeftShoulderPressed ? "IS pressed" : "is NOT pressed") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(isLeftShoulderPressed));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_isRightShoulderPressed
// --------------------------------------------------------------------------------------
#define LUA_IS_RIGHT_SHOULDER_PRESSED_COMMAND "simExtXbox360Controller_isRightShoulderPressed"

const int inArgs_IS_RIGHT_SHOULDER_PRESSED[] = {
	0,
};

void LUA_IS_RIGHT_SHOULDER_PRESSED_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_RIGHT_SHOULDER_PRESSED, inArgs_IS_RIGHT_SHOULDER_PRESSED[0], LUA_IS_RIGHT_SHOULDER_PRESSED_COMMAND))
	{
		// no inArgs to work with
	}

	bool isRightShoulderPressed = gamepad.isPressed(XINPUT_GAMEPAD_RIGHT_SHOULDER);

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: XINPUT_GAMEPAD_RIGHT_SHOULDER " << (isRightShoulderPressed ? "IS pressed" : "is NOT pressed") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(isRightShoulderPressed));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_isLeftThumbStickPressed
// --------------------------------------------------------------------------------------
#define LUA_IS_LEFT_THUMB_STICK_PRESSED_COMMAND "simExtXbox360Controller_isLeftThumbStickPressed"

const int inArgs_IS_LEFT_THUMB_STICK_PRESSED[] = {
	0,
};

void LUA_IS_LEFT_THUMB_STICK_PRESSED_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_LEFT_THUMB_STICK_PRESSED, inArgs_IS_LEFT_THUMB_STICK_PRESSED[0], LUA_IS_LEFT_THUMB_STICK_PRESSED_COMMAND))
	{
		// no inArgs to work with
	}

	bool isLeftThumbStickPressed = gamepad.isPressed(XINPUT_GAMEPAD_LEFT_THUMB);

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: XINPUT_GAMEPAD_LEFT_THUMB " << (isLeftThumbStickPressed ? "IS pressed" : "is NOT pressed") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(isLeftThumbStickPressed));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_isRightThumbStickPressed
// --------------------------------------------------------------------------------------
#define LUA_IS_RIGHT_THUMB_STICK_PRESSED_COMMAND "simExtXbox360Controller_isRightThumbStickPressed"

const int inArgs_IS_RIGHT_THUMB_STICK_PRESSED[] = {
	0,
};

void LUA_IS_RIGHT_THUMB_STICK_PRESSED_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_IS_RIGHT_THUMB_STICK_PRESSED, inArgs_IS_RIGHT_THUMB_STICK_PRESSED[0], LUA_IS_RIGHT_THUMB_STICK_PRESSED_COMMAND))
	{
		// no inArgs to work with
	}

	bool isRightThumbStickPressed = gamepad.isPressed(XINPUT_GAMEPAD_RIGHT_THUMB);

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: XINPUT_GAMEPAD_RIGHT_THUMB " << (isRightThumbStickPressed ? "IS pressed" : "is NOT pressed") << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(isRightThumbStickPressed));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_getLeftThumbStickCoords
//
// returns table_2 (x, y) . x, y in [-1, 1]
// --------------------------------------------------------------------------------------
#define LUA_GET_LEFT_THUMB_STICK_COORDS_COMMAND "simExtXbox360Controller_getLeftThumbStickCoords"

const int inArgs_GET_LEFT_THUMB_STICK_COORDS[] = {
	0,
};

void LUA_GET_LEFT_THUMB_STICK_COORDS_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_GET_LEFT_THUMB_STICK_COORDS, inArgs_GET_LEFT_THUMB_STICK_COORDS[0], LUA_GET_LEFT_THUMB_STICK_COORDS_COMMAND))
	{
		// no inArgs to work with
	}

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: Left Thumb Stick Coords: (" << gamepad.leftStickX << ", " << gamepad.leftStickY << ")" << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(std::vector<float>({ gamepad.leftStickX, gamepad.leftStickY })));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_getRightThumbStickCoords
//
// returns table_2 (x, y) . x, y in [-1, 1]
// --------------------------------------------------------------------------------------
#define LUA_GET_RIGHT_THUMB_STICK_COORDS_COMMAND "simExtXbox360Controller_getRightThumbStickCoords"

const int inArgs_GET_RIGHT_THUMB_STICK_COORDS[] = {
	0,
};

void LUA_GET_RIGHT_THUMB_STICK_COORDS_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_GET_RIGHT_THUMB_STICK_COORDS, inArgs_GET_RIGHT_THUMB_STICK_COORDS[0], LUA_GET_RIGHT_THUMB_STICK_COORDS_COMMAND))
	{
		// no inArgs to work with
	}

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: Right Thumb Stick Coords: (" << gamepad.rightStickX << ", " << gamepad.rightStickY << ")" << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(std::vector<float>({ gamepad.rightStickX, gamepad.rightStickY })));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_getLeftTriggerPressure
//
// returns float x in [0, 1]
// --------------------------------------------------------------------------------------
#define LUA_GET_LEFT_TRIGGER_PRESSURE_COMMAND "simExtXbox360Controller_getLeftTriggerPressure"

const int inArgs_GET_LEFT_TRIGGER_PRESSURE[] = {
	0,
};

void LUA_GET_LEFT_TRIGGER_PRESSURE_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_GET_LEFT_TRIGGER_PRESSURE, inArgs_GET_LEFT_TRIGGER_PRESSURE[0], LUA_GET_LEFT_TRIGGER_PRESSURE_COMMAND))
	{
		// no inArgs to work with
	}

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: Left Trigger Pressure: " << gamepad.leftTrigger << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(gamepad.leftTrigger));

	D.writeDataToLua(p);
}


// --------------------------------------------------------------------------------------
// simExtXbox360Controller_getRightTriggerPressure
//
// returns float x in [0, 1]
// --------------------------------------------------------------------------------------
#define LUA_GET_RIGHT_TRIGGER_PRESSURE_COMMAND "simExtXbox360Controller_getRightTriggerPressure"

const int inArgs_GET_RIGHT_TRIGGER_PRESSURE[] = {
	0,
};

void LUA_GET_RIGHT_TRIGGER_PRESSURE_CALLBACK(SLuaCallBack* p)
{
	p->outputArgCount = 1;
	CLuaFunctionData D;

	if (D.readDataFromLua(p, inArgs_GET_RIGHT_TRIGGER_PRESSURE, inArgs_GET_RIGHT_TRIGGER_PRESSURE[0], LUA_GET_RIGHT_TRIGGER_PRESSURE_COMMAND))
	{
		// no inArgs to work with
	}

#ifdef DEBUG
	std::cout << "'Xbox360Controller' plugin: Right Trigger Pressure: " << gamepad.rightTrigger << std::endl;
#endif

	D.pushOutData(CLuaFunctionDataItem(gamepad.rightTrigger));

	D.writeDataToLua(p);
}



// --------------------------------------------------------------------------------------
// V-REP plugin callbacks
// --------------------------------------------------------------------------------------

// This is called just once, at the start of V-REP.
VREP_DLLEXPORT unsigned char v_repStart(void* reservedPointer, int reservedInt)
{
	// Dynamically load and bind V-REP functions:
	char curDirAndFile[1024];
#ifdef _WIN32
#ifdef QT_COMPIL
	_getcwd(curDirAndFile, sizeof(curDirAndFile));
#else
	GetModuleFileNameA(NULL, curDirAndFile, 1023);
	PathRemoveFileSpecA(curDirAndFile);
#endif
#elif defined (__linux) || defined (__APPLE__)
	getcwd(curDirAndFile, sizeof(curDirAndFile));
#endif

	std::string currentDirAndPath(curDirAndFile);
	std::string temp(currentDirAndPath);

#ifdef _WIN32
	temp += "\\v_rep.dll";
#elif defined (__linux)
	temp += "/libv_rep.so";
#elif defined (__APPLE__)
	temp += "/libv_rep.dylib";
#endif /* __linux || __APPLE__ */

	vrepLib = loadVrepLibrary(temp.c_str());
	if (vrepLib == NULL)
	{
		std::cout << "Error, could not find or correctly load v_rep.dll. Cannot start 'Xbox360Controller' plugin.\n";
		return(0); // Means error, V-REP will unload this plugin
	}
	if (getVrepProcAddresses(vrepLib) == 0)
	{
		std::cout << "Error, could not find all required functions in v_rep.dll. Cannot start 'Xbox360Controller' plugin.\n";
		unloadVrepLibrary(vrepLib);
		return(0); // Means error, V-REP will unload this plugin
	}

	// Check the V-REP version:
	int vrepVer;
	simGetIntegerParameter(sim_intparam_program_version, &vrepVer);
	if (vrepVer<30200) // if V-REP version is smaller than 3.02.00
	{
		std::cout << "Sorry, your V-REP copy is somewhat old, V-REP 3.2.0 or higher is required. Cannot start 'Xbox360Controller' plugin.\n";
		unloadVrepLibrary(vrepLib);
		return 0; // Means error, V-REP will unload this plugin
	}

	// Register new Lua commands:
	std::vector<int> inArgs;


	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_IS_CONNECTED, inArgs);
	simRegisterCustomLuaFunction(LUA_IS_CONNECTED_COMMAND,
		strConCat("boolean isConnected=", LUA_IS_CONNECTED_COMMAND, "()"), &inArgs[0], LUA_IS_CONNECTED_CALLBACK);

	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_GET_PORT, inArgs);
	simRegisterCustomLuaFunction(LUA_GET_PORT_COMMAND,
		strConCat("int port=", LUA_GET_PORT_COMMAND, "()"), &inArgs[0], LUA_GET_PORT_CALLBACK);

	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_REFRESH_STATE, inArgs);
	simRegisterCustomLuaFunction(LUA_REFRESH_STATE_COMMAND,
		strConCat("boolean refreshState=", LUA_REFRESH_STATE_COMMAND, "()"), &inArgs[0], LUA_REFRESH_STATE_CALLBACK);



	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_IS_START_PRESSED, inArgs);
	simRegisterCustomLuaFunction(LUA_IS_START_PRESSED_COMMAND,
		strConCat("boolean isStartPressed=", LUA_IS_START_PRESSED_COMMAND, "()"), &inArgs[0], LUA_IS_START_PRESSED_CALLBACK);

	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_IS_BACK_PRESSED, inArgs);
	simRegisterCustomLuaFunction(LUA_IS_BACK_PRESSED_COMMAND,
		strConCat("boolean isBackPressed=", LUA_IS_BACK_PRESSED_COMMAND, "()"), &inArgs[0], LUA_IS_BACK_PRESSED_CALLBACK);



	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_IS_A_PRESSED, inArgs);
	simRegisterCustomLuaFunction(LUA_IS_A_PRESSED_COMMAND,
		strConCat("boolean isAPressed=", LUA_IS_A_PRESSED_COMMAND, "()"), &inArgs[0], LUA_IS_A_PRESSED_CALLBACK);

	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_IS_B_PRESSED, inArgs);
	simRegisterCustomLuaFunction(LUA_IS_B_PRESSED_COMMAND,
		strConCat("boolean isBPressed=", LUA_IS_B_PRESSED_COMMAND, "()"), &inArgs[0], LUA_IS_B_PRESSED_CALLBACK);

	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_IS_X_PRESSED, inArgs);
	simRegisterCustomLuaFunction(LUA_IS_X_PRESSED_COMMAND,
		strConCat("boolean isXPressed=", LUA_IS_X_PRESSED_COMMAND, "()"), &inArgs[0], LUA_IS_X_PRESSED_CALLBACK);

	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_IS_Y_PRESSED, inArgs);
	simRegisterCustomLuaFunction(LUA_IS_Y_PRESSED_COMMAND,
		strConCat("boolean isYPressed=", LUA_IS_Y_PRESSED_COMMAND, "()"), &inArgs[0], LUA_IS_Y_PRESSED_CALLBACK);



	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_IS_DPAD_LEFT_PRESSED, inArgs);
	simRegisterCustomLuaFunction(LUA_IS_DPAD_LEFT_PRESSED_COMMAND,
		strConCat("boolean isDPadLeftPressed=", LUA_IS_DPAD_LEFT_PRESSED_COMMAND, "()"), &inArgs[0], LUA_IS_DPAD_LEFT_PRESSED_CALLBACK);

	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_IS_DPAD_RIGHT_PRESSED, inArgs);
	simRegisterCustomLuaFunction(LUA_IS_DPAD_RIGHT_PRESSED_COMMAND,
		strConCat("boolean isDPadRightPressed=", LUA_IS_DPAD_RIGHT_PRESSED_COMMAND, "()"), &inArgs[0], LUA_IS_DPAD_RIGHT_PRESSED_CALLBACK);

	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_IS_DPAD_UP_PRESSED, inArgs);
	simRegisterCustomLuaFunction(LUA_IS_DPAD_UP_PRESSED_COMMAND,
		strConCat("boolean isDPadUpPressed=", LUA_IS_DPAD_UP_PRESSED_COMMAND, "()"), &inArgs[0], LUA_IS_DPAD_UP_PRESSED_CALLBACK);

	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_IS_DPAD_DOWN_PRESSED, inArgs);
	simRegisterCustomLuaFunction(LUA_IS_DPAD_DOWN_PRESSED_COMMAND,
		strConCat("boolean isDPadDownPressed=", LUA_IS_DPAD_DOWN_PRESSED_COMMAND, "()"), &inArgs[0], LUA_IS_DPAD_DOWN_PRESSED_CALLBACK);



	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_IS_LEFT_SHOULDER_PRESSED, inArgs);
	simRegisterCustomLuaFunction(LUA_IS_LEFT_SHOULDER_PRESSED_COMMAND,
		strConCat("boolean isLeftShoulderPressed=", LUA_IS_LEFT_SHOULDER_PRESSED_COMMAND, "()"), &inArgs[0], LUA_IS_LEFT_SHOULDER_PRESSED_CALLBACK);

	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_IS_RIGHT_SHOULDER_PRESSED, inArgs);
	simRegisterCustomLuaFunction(LUA_IS_RIGHT_SHOULDER_PRESSED_COMMAND,
		strConCat("boolean isRightShoulderPressed=", LUA_IS_RIGHT_SHOULDER_PRESSED_COMMAND, "()"), &inArgs[0], LUA_IS_RIGHT_SHOULDER_PRESSED_CALLBACK);



	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_IS_LEFT_THUMB_STICK_PRESSED, inArgs);
	simRegisterCustomLuaFunction(LUA_IS_LEFT_THUMB_STICK_PRESSED_COMMAND,
		strConCat("boolean isLeftThumbStickPressed=", LUA_IS_LEFT_THUMB_STICK_PRESSED_COMMAND, "()"), &inArgs[0], LUA_IS_LEFT_THUMB_STICK_PRESSED_CALLBACK);

	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_IS_RIGHT_THUMB_STICK_PRESSED, inArgs);
	simRegisterCustomLuaFunction(LUA_IS_RIGHT_THUMB_STICK_PRESSED_COMMAND,
		strConCat("boolean isRightThumbStickPressed=", LUA_IS_RIGHT_THUMB_STICK_PRESSED_COMMAND, "()"), &inArgs[0], LUA_IS_RIGHT_THUMB_STICK_PRESSED_CALLBACK);



	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_GET_LEFT_THUMB_STICK_COORDS, inArgs);
	simRegisterCustomLuaFunction(LUA_GET_LEFT_THUMB_STICK_COORDS_COMMAND,
		strConCat("table_2 leftThumbStickCoords=", LUA_GET_LEFT_THUMB_STICK_COORDS_COMMAND, "()"), &inArgs[0], LUA_GET_LEFT_THUMB_STICK_COORDS_CALLBACK);

	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_GET_RIGHT_THUMB_STICK_COORDS, inArgs);
	simRegisterCustomLuaFunction(LUA_GET_RIGHT_THUMB_STICK_COORDS_COMMAND,
		strConCat("table_2 rightThumbStickCoords=", LUA_GET_RIGHT_THUMB_STICK_COORDS_COMMAND, "()"), &inArgs[0], LUA_GET_RIGHT_THUMB_STICK_COORDS_CALLBACK);



	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_GET_LEFT_TRIGGER_PRESSURE, inArgs);
	simRegisterCustomLuaFunction(LUA_GET_LEFT_TRIGGER_PRESSURE_COMMAND,
		strConCat("float leftTriggerPressure=", LUA_GET_LEFT_TRIGGER_PRESSURE_COMMAND, "()"), &inArgs[0], LUA_GET_LEFT_TRIGGER_PRESSURE_CALLBACK);

	CLuaFunctionData::getInputDataForFunctionRegistration(inArgs_GET_RIGHT_TRIGGER_PRESSURE, inArgs);
	simRegisterCustomLuaFunction(LUA_GET_RIGHT_TRIGGER_PRESSURE_COMMAND,
		strConCat("float rightTriggerPressure=", LUA_GET_RIGHT_TRIGGER_PRESSURE_COMMAND, "()"), &inArgs[0], LUA_GET_RIGHT_TRIGGER_PRESSURE_CALLBACK);


	return 1; // initialization went fine, we return the version number of this plugin (> 0)
}

// This is called just once, at the end of V-REP
VREP_DLLEXPORT void v_repEnd()
{
	unloadVrepLibrary(vrepLib); // release the library
}

// This is called quite often. Just watch out for messages/events you want to handle
VREP_DLLEXPORT void* v_repMessage(int message, int* auxiliaryData, void* customData, int* replyData)
{
	// This function should not generate any error messages:
	int errorModeSaved;
	simGetIntegerParameter(sim_intparam_error_report_mode, &errorModeSaved);
	simSetIntegerParameter(sim_intparam_error_report_mode, sim_api_errormessage_ignore);

	void* retVal = NULL;

	if (message == sim_message_eventcallback_modulehandle)
	{
		// is the command also meant for Xbox360Controller?
		if ((customData == NULL) || (std::string("Xbox360Controller").compare((char*)customData) == 0))
		{
			//std::cout << "'Xbox360Controller' plugin received a message!" << std::endl;
		}
	}

	if (message == sim_message_eventcallback_simulationended)
	{
		// simulation ended
		// ...
	}

	simSetIntegerParameter(sim_intparam_error_report_mode, errorModeSaved); // restore previous settings
	return(retVal);
}

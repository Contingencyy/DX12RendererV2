#include "Pch.h"
#include "Input.h"

namespace Input
{

	bool key_states[KeyCode_NumKeys];

	// TODO: Change this to a lookup/translation table
	KeyCode WParamToKeyCode(WPARAM wparam)
	{
		switch (wparam)
		{
		case VK_LBUTTON:
			return KeyCode_LeftMouse;
		case VK_RBUTTON:
			return KeyCode_RightMouse;
		case 0x57:
			return KeyCode_W;
		case 0x41:
			return KeyCode_A;
		case 0x53:
			return KeyCode_S;
		case 0x44:
			return KeyCode_D;
		case VK_SPACE:
			return KeyCode_Space;
		case VK_CONTROL:
			return KeyCode_LCTRL;
		default:
			return KeyCode_Invalid;
		}
	}

	void OnKeyPressed(WPARAM win_key_code)
	{
		key_states[WParamToKeyCode(win_key_code)] = true;
	}

	void OnKeyReleased(WPARAM win_key_code)
	{
		key_states[WParamToKeyCode(win_key_code)] = false;
	}
	
	bool IsKeyPressed(KeyCode key_code)
	{
		return key_states[key_code];
	}

	float GetAxis1D(KeyCode axis_pos, KeyCode axis_neg)
	{
		return (float)((int)key_states[axis_pos] + (-(int)key_states[axis_neg]));
	}

}

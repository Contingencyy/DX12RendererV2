#include "Pch.h"
#include "Input.h"
#include "Window.h"

#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

namespace Input
{

	static bool key_states[KeyCode_NumKeys];
	static bool mouse_captured;
	static POINT mouse_pos_cur, mouse_pos_prev;

	static KeyCode WParamToKeyCode(WPARAM wparam)
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
		case VK_SHIFT:
			return KeyCode_SHIFT;
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

	void UpdateMouseMove()
	{
		mouse_pos_prev = mouse_pos_cur;
		GetCursorPos(&mouse_pos_cur);

		if (mouse_captured)
		{
			Window::ResetMousePosition();
		}
	}

	void SetMouseCapture(bool capture)
	{
		mouse_captured = capture;
	}

	void GetMouseMoveRel(int* x, int* y)
	{
		if (mouse_captured)
		{
			uint32_t window_center_x, window_center_y;
			Window::GetWindowCenter(&window_center_x, &window_center_y);

			*x = mouse_pos_cur.x - window_center_x;
			*y = mouse_pos_cur.y - window_center_y;
		}
		else
		{
			*x = mouse_pos_cur.x - mouse_pos_prev.x;
			*y = mouse_pos_cur.y - mouse_pos_prev.y;
		}
	}

	bool IsMouseCaptured()
	{
		return mouse_captured;
	}

}

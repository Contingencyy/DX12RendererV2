#pragma once

namespace Input
{

	enum KeyCode
	{
		KeyCode_LeftMouse,
		KeyCode_RightMouse,
		KeyCode_W,
		KeyCode_A,
		KeyCode_S,
		KeyCode_D,
		KeyCode_Space,
		KeyCode_LCTRL,
		KeyCode_SHIFT,
		KeyCode_Invalid,
		KeyCode_NumKeys
	};

	void OnKeyPressed(WPARAM win_key_code);
	void OnKeyReleased(WPARAM win_key_code);

	bool IsKeyPressed(KeyCode key_code);
	float GetAxis1D(KeyCode axis_pos, KeyCode axis_neg);

	void UpdateMouseMove();
	void SetMouseCapture(bool capture);
	void GetMouseMoveRel(int* x, int* y);
	bool IsMouseCaptured();

}

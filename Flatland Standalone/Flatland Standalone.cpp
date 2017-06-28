// Flatland Standalone.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "Classes.h"
#include "Platform.h"
#include "Plugin.h"

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	// Run the app.

	return run_app(hInstance, nCmdShow, lpCmdLine);
}

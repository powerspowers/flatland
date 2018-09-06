//******************************************************************************
// Copyright (C) 2018 Flatland Online Inc., Philip Stephens, Michael Powers.
// This code is licensed under the MIT license (see LICENCE file for details).
//******************************************************************************

#include "stdafx.h"
#include <Shellapi.h>
#include "Classes.h"
#include "Platform.h"
#include "Plugin.h"

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	LPWSTR *argument_list;
	int arguments;
	char file_path[_MAX_PATH];

	// Parse the command line into arguments and extract the first argument as the spot file path.

	argument_list = CommandLineToArgvW(GetCommandLineW(), &arguments);
	if (arguments > 1) {
		WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argument_list[1], -1, file_path, _MAX_PATH, NULL, NULL);
	} else {
		*file_path = '\0';
	}
	LocalFree(argument_list);

	// Run the app.

	return run_app(hInstance, nCmdShow, file_path);
}

/*
DESCRIPTION
	<<<< LGTV Companion >>>>

	The application (UI and service) controls your WebOS (LG) Television.

	Using this application allows for your WebOS display to shut off and turn on
	in reponse to to the PC shutting down, rebooting, entering low power modes as well as
	and when user is afk (idle). Like a normal PC-monitor. Or, alternatively this application
	can be used as a command line tool to turn your displays on or off.

BACKGROUND
	With the rise in popularity of using OLED TVs as PC monitors, it is apparent that standard
	functionality of PC-monitors is missing. Particularly turning the display on or off in
	response to power events in windows. With OLED monitors this is particularly important to
	prevent "burn-in", or more accurately pixel-wear.

BUILD INSTRUCTIONS AND DEPENDENCIES
	First, to be able to build the projects (UI, Service, Daemon) in Visual Studio 2022 please 
	ensure that Vcpkg (https://github.com/microsoft/vcpkg) is installed. Vcpkg is a free 
	Library Manager for Windows. You should use Vcpkg in one of two ways:
	
	1) A Vcpkg manifest is included with the source code and the necessary dependencies
	will be automatically downloaded, configured and installed, if you choose to enable it.

	To enable the manifest please open the properties for each project in the solution, then 
	navigate to the vcpkg section and Select "Yes" for "Use Manifest". Do so for both the 
	"Debug" and "Release" project configurations.

	2) Alternatively, You can manually install the dependencies, with the following commands:

		vcpkg install nlohmann-json:x64-windows-static
		vcpkg install boost-asio:x64-windows-static
		vcpkg install boost-optional:x64-windows-static
		vcpkg install boost-utility:x64-windows-static
		vcpkg install boost-date-time:x64-windows-static
		vcpkg install boost-beast:x64-windows-static
		vcpkg install wintoast:x64-windows-static
		vcpkg install openssl:x64-windows-static
	
	Secondly, to be able to build the setup package please ensure that the WiX Toolset is 
	installed (https://wixtoolset.org/) and that the WiX Toolset Visual Studio Extension 
	(WiX v3	Visual Studio 2022 Extension) is installed.

INSTALLATION, USAGE ETC

	https://github.com/JPersson77/LGTVCompanion

CHANGELOG
	v 1.0.0             - Initial release

	v 1.2.0             - Update logic for managing power events.
						- Removal of redundant options
						- More useful logging and error messages
						- fixed proper command line interpretation of arguments enclosed in "quotes"
						- Minor bug fixes
						- Installer/upgrade bug fixed

	v 1.3.0             - Output Host IP in log (for debugging purposes) and additional logging
						- Display warning in UI when TV is configured on a subnet different from PC
						- Add service dependencies
						- Set service shutdown priority
						- Implementation of option to automatically check for new application version (off by default)
						- Bug fixes and optimisations

	v 1.4.0             - Additional device network options for powering on via WOL

	v 1.5.0             - Implementation of more options for controlling power on/off logic
						- Option to utilise capability for blanking TV (screen off)when user is idle
						- Extended command line parameters to include blanking of screen.

	v 1.6.0             - Implementation of option for setting HDMI input on system start / resume from low power mode
						- Command line parameter to set HDMI input added

	v 1.7.0             - More robust operations during RDP remote host
						- Option to power off devices during RDP
						- Some additional help texts in the options dialog
						- Bugfixes and optimisations

	v 1.8.0             - Option to conform to windows monitor topology (active display outputs only)

	v 1.9.0				- More granular user idle mode

	v 2.0.0				- Support for LG's newest firmware which changed the connection method (SSL)
						- Option to revert to legacy connection method for (presumably) older models
						- Optional support for NVIDIA Gamestream, Steam Link, Sunshine and RDP
						- Fixed a bug where the fullscreen detection for user idle mode was triggered by the NVIDIA GFE overlay sometimes
						- Improved logic for the monitor topology feature
						- More help texts

	v 2.1.0				- new command line options for setting/unsetting user idle mode (-idle, -unidle)
						- bugfix: sunshine remote host support
						- refactored code for common modules
						- minor bugfixes

	v 3.0.0				- Much extended command line support to control the app and managed devices


	TODO:
						- [ ] Feature to power on only when PC input s selected on TV (if possible)
						- [ ] Device on/off indicator
						- [ ] compatibility mode for topology
						- [ ] Build restart word phrases, see issue #132
LICENSE
	Copyright (c) 2021-2024 J�rgen Persson

	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
	files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,
	modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
	WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
	COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
	ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

COPYRIGHT
	Copyright (c) 2021-2024 J�rgen Persson
*/

#include "LGTV Companion UI.h"

namespace						common = jpersson77::common;
namespace						settings = jpersson77::settings;
namespace						ipc = jpersson77::ipc;

// Global Variables:
HINSTANCE                       hInstance;  // current instance
HWND                            hMainWnd = NULL;
HWND                            hDeviceWindow = NULL;
HWND                            hOptionsWindow = NULL;
HWND                            hTopologyWindow = NULL;
HWND                            hUserIdleConfWindow = NULL;
HWND							hWhitelistConfWindow = NULL;
HBRUSH                          hBackbrush;
HFONT                           hEditfont;
HFONT                           hEditMediumfont;
HFONT                           hEditSmallfont;
HFONT                           hEditMediumBoldfont;
HMENU                           hPopupMeuMain;
HICON                           hOptionsIcon;
HICON                           hTopologyIcon;
HICON                           hCogIcon;
WSADATA                         WSAData;
int								iTopConfPhase;
int								iTopConfDisplay;
std::vector<settings::PROCESSLIST>	WhitelistTemp;
std::vector<settings::PROCESSLIST>	ExclusionsTemp;
ipc::PipeClient*				pPipeClient;

settings::Preferences			Settings;

//Application entry point
int APIENTRY wWinMain(_In_ HINSTANCE Instance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(nCmdShow);

	hInstance = Instance;
	MSG msg;
	std::wstring WindowTitle;
	std::wstring CommandLineParameters;

	WindowTitle = APPNAME;
	WindowTitle += L" v";
	WindowTitle += APP_VERSION;

	if (lpCmdLine)
		CommandLineParameters = lpCmdLine;

	//make lowercase command line parameters
//	transform(CommandLineParameters.begin(), CommandLineParameters.end(), CommandLineParameters.begin(), ::tolower);

	// if commandline directed towards daemon
	if (MessageDaemon(CommandLineParameters))
		return false;

	// if the app is already running as another process, send the command line parameters to that process and exit
	if (MessageExistingProcess(CommandLineParameters))
		return false;

	// read the configuration file
	try {
		Settings.Initialize();
	}

	catch (std::exception const& e) {
		std::wstring s = L"Error when reading configuration.";
		s += common::widen(e.what());
		SvcReportEvent(EVENTLOG_ERROR_TYPE, s);
		MessageBox(NULL, L"Error when reading the configuration file.\n\nCheck the event log. Application terminated.", APPNAME, MB_OK | MB_ICONERROR);
		return false;
	}

	// tweak prefs conf
	bool bTop = false;
	for (auto m : Settings.Devices)
	{
		if (m.UniqueDeviceKey != "")
			bTop = true;
	}
	Settings.Prefs.AdhereTopology = bTop ? Settings.Prefs.AdhereTopology : false;

	// Initiate PipeClient IPC
	ipc::PipeClient PipeCl(PIPENAME, NamedPipeCallback);
	pPipeClient = &PipeCl;

	//parse and execute command line parameters when applicable and then exit
	if (Settings.Devices.size() > 0 && CommandLineParameters.size() > 0)
	{
		int max_wait = 1000;
		while (!PipeCl.isRunning() && max_wait > 0)
		{
			Sleep(25);
			max_wait -= 25;
		}
		CommunicateWithService(CommandLineParameters);
		PipeCl.Terminate();
		return false;
	}

	hBackbrush = CreateSolidBrush(0x00ffffff);
	hEditfont = CreateFont(26, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, TEXT("Calibri"));
	hEditMediumfont = CreateFont(20, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, TEXT("Calibri"));
	hEditSmallfont = CreateFont(18, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, TEXT("Calibri"));
	hEditMediumBoldfont = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, TEXT("Calibri"));
	hPopupMeuMain = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_BUTTONMENU));

	INITCOMMONCONTROLSEX icex;           // Structure for control initialization.
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	//   icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_TREEVIEW_CLASSES | ICC_UPDOWN_CLASS | ICC_USEREX_CLASSES | ICC_BAR_CLASSES ;
	icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_UPDOWN_CLASS | ICC_STANDARD_CLASSES | ICC_USEREX_CLASSES;
	InitCommonControlsEx(&icex);

	hOptionsIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, 25, 25, LR_DEFAULTCOLOR | LR_DEFAULTSIZE | LR_SHARED);
	hTopologyIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCE(IDI_ICON4), IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR | LR_DEFAULTSIZE | LR_SHARED);
	hCogIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCE(IDI_ICON3), IMAGE_ICON, 25, 25, LR_DEFAULTCOLOR | LR_DEFAULTSIZE | LR_SHARED);

	WSAStartup(MAKEWORD(2, 2), &WSAData);

	// create main window (dialog)
	hMainWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)WndProc);
	SetWindowText(hMainWnd, WindowTitle.c_str());
	ShowWindow(hMainWnd, SW_SHOW);
	UpdateWindow(hMainWnd);

	// spawn thread to check for updated version of the app.
	if (Settings.Prefs.AutoUpdate)
	{
		std::thread thread_obj(VersionCheckThread, hMainWnd);
		thread_obj.detach();
	}

	// message loop:
	// don't call DefWindowProc for modeless dialogs. Return true for handled messages or false otherwise
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsDialogMessage(hMainWnd, &msg) &&
			!IsDialogMessage(hDeviceWindow, &msg) &&
			!IsDialogMessage(hOptionsWindow, &msg) &&
			!IsDialogMessage(hTopologyWindow, &msg) &&
			!IsDialogMessage(hUserIdleConfWindow, &msg) &&
			!IsDialogMessage(hWhitelistConfWindow, &msg) &&
			!IsDialogMessage(hUserIdleConfWindow, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	PipeCl.Terminate();

	//clean up
	DeleteObject(hBackbrush);
	DeleteObject(hEditMediumBoldfont);
	DeleteObject(hEditfont);
	DeleteObject(hEditMediumfont);
	DeleteObject(hEditSmallfont);
	DestroyMenu(hPopupMeuMain);
	DestroyIcon(hOptionsIcon);
	DestroyIcon(hTopologyIcon);
	DestroyIcon(hCogIcon);

	WSACleanup();

	return (int)msg.wParam;
}

//   Process messages for the main window.
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	std::wstring str;
	switch (message)
	{
	case WM_INITDIALOG:
	{
		SendDlgItemMessage(hWnd, IDC_COMBO, WM_SETFONT, (WPARAM)hEditfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_CHECK_ENABLE, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_NEWVERSION, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_DONATE, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_SPLIT, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDOK, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_OPTIONS, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));

		SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)-1, (LPARAM)0);

		if (Settings.Devices.size() > 0)
		{
			for (const auto& item : Settings.Devices)
			{
				SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)common::widen(item.Name).c_str());
			}
			SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)0, (LPARAM)0);

			CheckDlgButton(hWnd, IDC_CHECK_ENABLE, Settings.Devices[0].Enabled ? BST_CHECKED : BST_UNCHECKED);
			EnableWindow(GetDlgItem(hWnd, IDC_COMBO), true);
			EnableWindow(GetDlgItem(hWnd, IDC_CHECK_ENABLE), true);

			SetDlgItemText(hWnd, IDC_SPLIT, L"C&onfigure");
		}
		else
		{
			EnableWindow(GetDlgItem(hWnd, IDC_COMBO), false);
			SetDlgItemText(hWnd, IDC_SPLIT, L"&Scan");
		}
		SendMessageW(GetDlgItem(hWnd, IDC_OPTIONS), BM_SETIMAGE, IMAGE_ICON, (LPARAM)hOptionsIcon);
	}break;
	case APP_NEW_VERSION:
	{
		ShowWindow(GetDlgItem(hWnd, IDC_NEWVERSION), SW_SHOW);
	}break;
	case APP_MESSAGE_ADD:
	{
		hDeviceWindow = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEVICE), hWnd, (DLGPROC)DeviceWndProc);
		SetWindowText(hDeviceWindow, DEVICEWINDOW_TITLE_ADD);
		SetWindowText(GetDlgItem(hDeviceWindow, IDOK), L"&Add");
		CheckDlgButton(hDeviceWindow, IDC_RADIO2, BST_CHECKED);
		EnableWindow(GetDlgItem(hDeviceWindow, IDC_SUBNET), false);
		SetWindowText(GetDlgItem(hDeviceWindow, IDC_SUBNET), WOL_DEFAULTSUBNET);

		CheckDlgButton(hDeviceWindow, IDC_HDMI_INPUT_NUMBER_CHECKBOX, BST_UNCHECKED);
		EnableWindow(GetDlgItem(hDeviceWindow, IDC_HDMI_INPUT_NUMBER), false);
		SetWindowText(GetDlgItem(hDeviceWindow, IDC_HDMI_INPUT_NUMBER), L"1");

		CheckDlgButton(hDeviceWindow, IDC_SET_HDMI_INPUT_CHECKBOX, BST_UNCHECKED);
		EnableWindow(GetDlgItem(hDeviceWindow, IDC_SET_HDMI_INPUT_NUMBER), false);
		SetWindowText(GetDlgItem(hDeviceWindow, IDC_SET_HDMI_INPUT_NUMBER), L"1");

		std::wstring s;
		SendMessage(GetDlgItem(hDeviceWindow, IDC_COMBO_SSL), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		s = L"Auto";
		SendMessage(GetDlgItem(hDeviceWindow, IDC_COMBO_SSL), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		s = L"Legacy";
		SendMessage(GetDlgItem(hDeviceWindow, IDC_COMBO_SSL), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		SendMessage(GetDlgItem(hDeviceWindow, IDC_COMBO_SSL), (UINT)CB_SETCURSEL, (WPARAM)0, (LPARAM)0);

		EnableWindow(GetDlgItem(hDeviceWindow, IDOK), false);

		EnableWindow(hWnd, false);
		ShowWindow(hDeviceWindow, SW_SHOW);
	}break;
	case APP_MESSAGE_MANAGE:
	{
		int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
		if (sel == CB_ERR)
			break;

		hDeviceWindow = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEVICE), hWnd, (DLGPROC)DeviceWndProc);
		SetWindowText(hDeviceWindow, DEVICEWINDOW_TITLE_MANAGE);
		SetWindowText(GetDlgItem(hDeviceWindow, IDOK), L"&Save");
		SetWindowText(GetDlgItem(hDeviceWindow, IDC_DEVICENAME), common::widen(Settings.Devices[sel].Name).c_str());
		SetWindowText(GetDlgItem(hDeviceWindow, IDC_DEVICEIP), common::widen(Settings.Devices[sel].IP).c_str());

		CheckDlgButton(hDeviceWindow, IDC_HDMI_INPUT_NUMBER_CHECKBOX, Settings.Devices[sel].HDMIinputcontrol);
		EnableWindow(GetDlgItem(hDeviceWindow, IDC_HDMI_INPUT_NUMBER), Settings.Devices[sel].HDMIinputcontrol ? true : false);
		SetWindowText(GetDlgItem(hDeviceWindow, IDC_HDMI_INPUT_NUMBER), common::widen(std::to_string(Settings.Devices[sel].OnlyTurnOffIfCurrentHDMIInputNumberIs)).c_str());
		SendDlgItemMessage(hDeviceWindow, IDC_HDMI_INPUT_NUMBER_SPIN, UDM_SETPOS, (WPARAM)NULL, (LPARAM)Settings.Devices[sel].OnlyTurnOffIfCurrentHDMIInputNumberIs);

		CheckDlgButton(hDeviceWindow, IDC_SET_HDMI_INPUT_CHECKBOX, Settings.Devices[sel].SetHDMIInputOnResume);
		EnableWindow(GetDlgItem(hDeviceWindow, IDC_SET_HDMI_INPUT_NUMBER), Settings.Devices[sel].SetHDMIInputOnResume ? true : false);
		SetWindowText(GetDlgItem(hDeviceWindow, IDC_SET_HDMI_INPUT_NUMBER), common::widen(std::to_string(Settings.Devices[sel].SetHDMIInputOnResumeToNumber)).c_str());
		SendDlgItemMessage(hDeviceWindow, IDC_SET_HDMI_INPUT_SPIN, UDM_SETPOS, (WPARAM)NULL, (LPARAM)Settings.Devices[sel].SetHDMIInputOnResumeToNumber);

		str = L"";
		for (const auto& item : Settings.Devices[sel].MAC)
		{
			str += common::widen(item);
			if (item != Settings.Devices[sel].MAC.back())
				str += L"\r\n";
		}

		std::wstring s;
		SendMessage(GetDlgItem(hDeviceWindow, IDC_COMBO_SSL), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		s = L"Auto";
		SendMessage(GetDlgItem(hDeviceWindow, IDC_COMBO_SSL), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		s = L"Legacy";
		SendMessage(GetDlgItem(hDeviceWindow, IDC_COMBO_SSL), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		SendMessage(GetDlgItem(hDeviceWindow, IDC_COMBO_SSL), (UINT)CB_SETCURSEL, (WPARAM)Settings.Devices[sel].SSL ? 0 : 1, (LPARAM)0);

		SetWindowText(GetDlgItem(hDeviceWindow, IDC_DEVICEMACS), str.c_str());
		EnableWindow(GetDlgItem(hDeviceWindow, IDOK), false);

		switch (Settings.Devices[sel].WOLtype)
		{
		case 1:
		{
			CheckDlgButton(hDeviceWindow, IDC_RADIO1, BST_CHECKED);
			EnableWindow(GetDlgItem(hDeviceWindow, IDC_SUBNET), false);
		}break;
		case 2:
		{
			CheckDlgButton(hDeviceWindow, IDC_RADIO2, BST_CHECKED);
			EnableWindow(GetDlgItem(hDeviceWindow, IDC_SUBNET), false);
		}break;
		case 3:
		{
			CheckDlgButton(hDeviceWindow, IDC_RADIO3, BST_CHECKED);
			EnableWindow(GetDlgItem(hDeviceWindow, IDC_SUBNET), true);
		}break;
		default:break;
		}
		if (Settings.Devices[sel].Subnet != "")
			SetWindowText(GetDlgItem(hDeviceWindow, IDC_SUBNET), common::widen(Settings.Devices[sel].Subnet).c_str());
		else
			SetWindowText(GetDlgItem(hDeviceWindow, IDC_SUBNET), WOL_DEFAULTSUBNET);

		EnableWindow(GetDlgItem(hDeviceWindow, IDOK), false);

		EnableWindow(hWnd, false);
		ShowWindow(hDeviceWindow, SW_SHOW);
	}break;
	case APP_MESSAGE_SCAN:
	{
		bool RemoveCurrentDevices = (wParam == 0) ? true : false;
		bool ChangesWereMade = false;
		int DevicesAdded = 0;

		HDEVINFO DeviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES);
		SP_DEVINFO_DATA DeviceInfoData;

		memset(&DeviceInfoData, 0, sizeof(SP_DEVINFO_DATA));
		DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		int DeviceIndex = 0;

		while (SetupDiEnumDeviceInfo(DeviceInfoSet, DeviceIndex, &DeviceInfoData))
		{
			PDEVPROPKEY     pDevPropKey;
			DEVPROPTYPE PropType;
			DWORD required_size = 0;
			DeviceIndex++;

			pDevPropKey = (PDEVPROPKEY)(&DEVPKEY_Device_FriendlyName);

			SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, nullptr, 0, &required_size, 0);
			if (required_size > 2)
			{
				std::vector<BYTE> unicode_buffer(required_size, 0);
				SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, unicode_buffer.data(), required_size, nullptr, 0);
				std::wstring FriendlyName;
				FriendlyName = ((PWCHAR)unicode_buffer.data()); //NAME
				if (FriendlyName.find(L"[LG]", 0) != std::wstring::npos)
				{
					pDevPropKey = (PDEVPROPKEY)(&PKEY_PNPX_IpAddress);
					SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, nullptr, 0, &required_size, 0);
					if (required_size > 2)
					{
						std::vector<BYTE> unicode_buffer2(required_size, 0);
						SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, unicode_buffer2.data(), required_size, nullptr, 0);
						std::wstring IP;
						IP = ((PWCHAR)unicode_buffer2.data()); //IP
						pDevPropKey = (PDEVPROPKEY)(&PKEY_PNPX_PhysicalAddress);
						SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, nullptr, 0, &required_size, 0);
						if (required_size >= 6)
						{
							std::vector<BYTE> unicode_buffer3(required_size, 0);
							SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, unicode_buffer3.data(), required_size, nullptr, 0);

							std::stringstream ss;
							std::string MAC;
							ss << std::hex << std::setfill('0');
							for (int i = 0; i < 6; i++) // TODO, check here what happens if more than one MAC in vector.
							{
								ss << std::setw(2) << static_cast<unsigned> (unicode_buffer3[i]);
								if (i < 5)
									ss << ":";
							}
							MAC = ss.str();
							transform(MAC.begin(), MAC.end(), MAC.begin(), ::toupper);
							if (RemoveCurrentDevices)
							{
								Settings.Devices.clear();
								RemoveCurrentDevices = false;
								ChangesWereMade = true;
							}
							bool DeviceExists = false;
							for (auto& item : Settings.Devices)
								for (auto& m : item.MAC)
									if (m == MAC)
									{
										if (common::narrow(IP) != item.IP)
										{
											item.IP = common::narrow(IP);
											ChangesWereMade = true;
										}
										DeviceExists = true;
									}

							if (!DeviceExists)
							{
								settings::DEVICE temp;
								temp.Name = common::narrow(FriendlyName);
								temp.IP = common::narrow(IP);
								temp.MAC.push_back(MAC);
								temp.Subnet = common::narrow(WOL_DEFAULTSUBNET);
								temp.WOLtype = WOL_IPSEND;
								Settings.Devices.push_back(temp);
								ChangesWereMade = true;
								DevicesAdded++;
							}
						}
					}
				}
			}
		}
		if (ChangesWereMade)
		{
			SendMessage(hWnd, (UINT)WM_INITDIALOG, (WPARAM)0, (LPARAM)0);
			EnableWindow(GetDlgItem(hWnd, IDOK), true);
		}
		if (DeviceInfoSet)
			SetupDiDestroyDeviceInfoList(DeviceInfoSet);

		std::wstringstream mess;
		mess << DevicesAdded;
		mess << L" new devices found.";
		MessageBox(hWnd, mess.str().c_str(), L"Scan results", MB_OK | MB_ICONINFORMATION);
	}break;
	case APP_MESSAGE_REMOVE:
	{
		int sel = CB_ERR;
		if (wParam == 1) //remove all
		{
			Settings.Devices.clear();
			CheckDlgButton(hWnd, IDC_CHECK_ENABLE, BST_UNCHECKED);
			EnableWindow(GetDlgItem(hWnd, IDC_CHECK_ENABLE), false);
			SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
			SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)-1, (LPARAM)0);
			EnableWindow(GetDlgItem(hWnd, IDC_COMBO), false);
			SetDlgItemText(hWnd, IDC_SPLIT, L"&Scan");
		}
		else
		{
			int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
			if (sel == CB_ERR)
				break;
			Settings.Devices.erase(Settings.Devices.begin() + sel);

			int ind = (int)SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_DELETESTRING, (WPARAM)sel, (LPARAM)0);
			if (ind > 0)
			{
				int j = sel < ind ? sel : sel - 1;
				SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)j, (LPARAM)0);
				CheckDlgButton(hWnd, IDC_CHECK_ENABLE, Settings.Devices[j].Enabled ? BST_CHECKED : BST_UNCHECKED);
				EnableWindow(GetDlgItem(hWnd, IDC_CHECK_ENABLE), true);
				EnableWindow(GetDlgItem(hWnd, IDC_COMBO), true);
				SetDlgItemText(hWnd, IDC_SPLIT, L"C&onfigure");
			}
			else
			{
				CheckDlgButton(hWnd, IDC_CHECK_ENABLE, BST_UNCHECKED);
				EnableWindow(GetDlgItem(hWnd, IDC_CHECK_ENABLE), false);
				SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)-1, (LPARAM)0);
				EnableWindow(GetDlgItem(hWnd, IDC_COMBO), false);
				SetDlgItemText(hWnd, IDC_SPLIT, L"&Scan");
			}
		}
		EnableWindow(GetDlgItem(hWnd, IDOK), true);
	}break;

	case APP_MESSAGE_TURNON:
	{
		int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
		if (sel == CB_ERR)
			break;
		std::wstring s = L"-poweron ";
		s += common::widen(Settings.Devices[sel].DeviceId);
		CommunicateWithService(s);
	}break;
	case APP_MESSAGE_TURNOFF:
	{
		int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
		if (sel == CB_ERR)
			break;
		std::wstring s = L"-poweroff ";
		s += common::widen(Settings.Devices[sel].DeviceId);
		CommunicateWithService(s);
	}break;
	case APP_MESSAGE_APPLY:
	{
		std::string logmsg = "Applying settings...";
		Log(logmsg);
		std::vector<std::string> IPs = common::GetOwnIP();
		if(IPs.size()>0)
		{
			{
				logmsg = "Enumerating network interfaces:";
				Log(logmsg);
				for (auto& IP : IPs)
				{
					Log(IP);
				}

			}
			for (auto& Dev : Settings.Devices)
			{
				bool bFound = false;
				for (auto& IP : IPs)
				{
					std::vector temp = common::stringsplit(IP, "/");
					std::string IP, CIRD;
					if (temp.size() > 1)
					{
						std::stringstream subnet;
						IP = temp[0];
						CIRD = temp[1];
						unsigned long mask = (0xFFFFFFFF << (32 - atoi(CIRD.c_str())) & 0xFFFFFFFF);
						subnet << (mask >> 24) << "." << ((mask >> 16) & 0xFF) << "." << ((mask >> 8) & 0xFF) << "." << (mask & 0xFF);
						if (isSameSubnet(Dev.IP.c_str(), IP.c_str(), subnet.str().c_str()))
							bFound = true;
					}
					else
						bFound = true;
				}
				if (!bFound)
				{
					std::string mess = Dev.DeviceId;
					mess += " with name \"";
					mess += Dev.Name;
					mess += "\" and IP ";
					mess += Dev.IP;
					mess += " is not on the same subnet as the PC. Please note that this might cause problems with waking "
						"up the TV. Please check the documentation and the configuration.\n\n Do you want to continue anyway?";
					int mb = MessageBox(hWnd, common::widen(mess).c_str(), L"Warning", MB_YESNO | MB_ICONEXCLAMATION);
					if (mb == IDNO)
					{
						EnableWindow(hWnd, true);
						return 0;
					}
				}
			}
		}

		Settings.WriteToDisk();

		//restart the service
		SERVICE_STATUS_PROCESS status;
		DWORD bytesNeeded;
		SC_HANDLE serviceDbHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
		if (serviceDbHandle)
		{
			SC_HANDLE serviceHandle = OpenService(serviceDbHandle, L"LGTVsvc", SERVICE_QUERY_STATUS | SERVICE_STOP | SERVICE_START);
			if (serviceHandle)
			{
				QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded);
				ControlService(serviceHandle, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&status);
				while (status.dwCurrentState != SERVICE_STOPPED)
				{
					Sleep(status.dwWaitHint);
					if (QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded))
					{
						if (status.dwCurrentState == SERVICE_STOPPED)
							break;
					}
				}

				StartService(serviceHandle, NULL, NULL);

				CloseServiceHandle(serviceHandle);
			}
			else
				MessageBox(hWnd, L"The LGTV Companion service is not installed. Please reinstall the application", L"Error", MB_OK | MB_ICONEXCLAMATION);
			CloseServiceHandle(serviceDbHandle);
		}
		//restart the daemon
		TCHAR buffer[MAX_PATH] = { 0 };
		GetModuleFileName(NULL, buffer, MAX_PATH);
		std::wstring path = buffer;
		std::wstring exe;
		std::wstring::size_type pos = path.find_last_of(L"\\/");
		path = path.substr(0, pos + 1);
		exe = path;
		exe += L"LGTVdaemon.exe";
		ShellExecute(NULL, L"open", exe.c_str(), L"-hide", path.c_str(), SW_SHOWNORMAL);
		pPipeClient->Init();
		EnableWindow(hWnd, true);
		EnableWindow(GetDlgItem(hWnd, IDOK), false);
	}break;
	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDC_CHECK_ENABLE:
			{
				int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
				if (sel == CB_ERR)
					break;
				Settings.Devices[sel].Enabled = IsDlgButtonChecked(hWnd, IDC_CHECK_ENABLE) == BST_CHECKED ? true : false;
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDOK:
			{
				EnableWindow(hWnd, false);
				//EnableWindow(GetDlgItem(hWnd, IDOK), false);
				PostMessage(hWnd, APP_MESSAGE_APPLY, (WPARAM)NULL, NULL);
			}break;
			case IDC_TEST:
			{
				PostMessage((HWND)-1, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
			}break;
			case IDC_SPLIT:
			{
				if (Settings.Devices.size() > 0)
					SendMessage(hWnd, APP_MESSAGE_MANAGE, NULL, NULL);
				else
				{
					int ms = MessageBoxW(hWnd, L"Scanning will discover and add any LG devices found.\n\nThe scan is performed locally by filtering the 'Digital Media Devices' category in the device manager for LG devices.\n\nClick OK to continue!", L"Scanning", MB_OKCANCEL | MB_ICONEXCLAMATION);
					if (ms == IDCANCEL)
						break;
					SendMessage(hWnd, APP_MESSAGE_SCAN, NULL, NULL);
				}
			}break;
			case IDC_OPTIONS:
			{
				hOptionsWindow = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_OPTIONS), hWnd, (DLGPROC)OptionsWndProc);
				SetWindowText(hOptionsWindow, L"Additional options");
				EnableWindow(hWnd, false);
				ShowWindow(hOptionsWindow, SW_SHOW);
			}break;
			default:break;
			}
		}break;
		case CBN_SELCHANGE:
		{
			if (Settings.Devices.size() > 0)
			{
				int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
				if (sel == CB_ERR)
					break;

				CheckDlgButton(hWnd, IDC_CHECK_ENABLE, Settings.Devices[sel].Enabled ? BST_CHECKED : BST_UNCHECKED);
				EnableWindow(GetDlgItem(hWnd, IDC_CHECK_ENABLE), true);
			}
		}break;

		default:break;
		}
	}break;

	case WM_NOTIFY:
	{
		switch (((NMHDR*)lParam)->code)
		{
		case NM_CLICK:
		{
			//download new version
			if (wParam == IDC_NEWVERSION)
			{
				ShellExecute(0, 0, NEWRELEASELINK, 0, 0, SW_SHOW);
			}
			//care to support your coder
			if (wParam == IDC_DONATE)
			{
				if (MessageBox(hWnd, L"This is free software, but your support is appreciated and there "
					"is a donation page set up over at PayPal. PayPal allows you to use a credit- or debit "
					"card or your PayPal balance to make a donation, even without a PayPal account.\n\n"
					"Click 'Yes' to continue to the PayPal donation web page (a PayPal account is not "
					"required to make a donation)!"
					"\n\nAlternatively you can go to the following URL in your web browser (a PayPal "
					"account is required for this link).\n\nhttps://paypal.me/jpersson77", L"Donate via PayPal?", MB_YESNO | MB_ICONQUESTION) == IDYES)
					ShellExecute(0, 0, DONATELINK, 0, 0, SW_SHOW);
			}
		}break;
		case BCN_DROPDOWN:
		{
			NMBCDROPDOWN* pDropDown = (NMBCDROPDOWN*)lParam;
			if (pDropDown->hdr.hwndFrom == GetDlgItem(hWnd, IDC_SPLIT))
			{
				POINT pt;
				MENUITEMINFO mi;
				pt.x = pDropDown->rcButton.right;
				pt.y = pDropDown->rcButton.bottom;
				ClientToScreen(pDropDown->hdr.hwndFrom, &pt);

				mi.cbSize = sizeof(MENUITEMINFO);
				mi.fMask = MIIM_STATE;
				if (Settings.Devices.size() > 0)
					mi.fState = MFS_ENABLED;
				else
					mi.fState = MFS_DISABLED;

				SetMenuItemInfo(hPopupMeuMain, ID_M_MANAGE, false, &mi);
				SetMenuItemInfo(hPopupMeuMain, ID_M_REMOVE, false, &mi);
				SetMenuItemInfo(hPopupMeuMain, ID_M_REMOVEALL, false, &mi);

				if (Settings.Devices.size() > 0 && !IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
					mi.fState = MFS_ENABLED;
				else
					mi.fState = MFS_DISABLED;

				SetMenuItemInfo(hPopupMeuMain, ID_M_TEST, false, &mi);
				SetMenuItemInfo(hPopupMeuMain, ID_M_TURNON, false, &mi);
				SetMenuItemInfo(hPopupMeuMain, ID_M_TURNOFF, false, &mi);

				switch (TrackPopupMenu(GetSubMenu(hPopupMeuMain, 0), TPM_TOPALIGN | TPM_RIGHTALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, NULL))
				{
				case ID_M_REMOVE:
				{
					if (Settings.Devices.size() > 0)
						if (MessageBox(hWnd, L"You are about to remove this device.\n\nDo you want to continue?", L"Remove device", MB_YESNO | MB_ICONQUESTION) == IDYES)
							SendMessage(hWnd, (UINT)APP_MESSAGE_REMOVE, (WPARAM)0, (LPARAM)lParam);
				}break;
				case ID_M_REMOVEALL:
				{
					if (Settings.Devices.size() > 0)
						if (MessageBox(hWnd, L"You are about to remove ALL devices.\n\nDo you want to continue?", L"Remove all devices", MB_YESNO | MB_ICONQUESTION) == IDYES)
							SendMessage(hWnd, (UINT)APP_MESSAGE_REMOVE, (WPARAM)1, (LPARAM)lParam);
				}break;

				case ID_M_ADD:
				{
					SendMessage(hWnd, (UINT)APP_MESSAGE_ADD, (WPARAM)wParam, (LPARAM)lParam);
				}break;
				case ID_M_MANAGE:
				{
					SendMessage(hWnd, (UINT)APP_MESSAGE_MANAGE, (WPARAM)wParam, (LPARAM)lParam);
				}break;
				case ID_M_SCAN:
				{
					if (Settings.Devices.size() > 0)
					{
						int ms = MessageBoxW(hWnd, L"Scanning will discover and add any LG devices found.\n\nThe scan is performed locally by filtering the 'Digital Media Devices' category in the device manager for LG devices.\n\nDo you want to replace the current devices with any discovered devices?\n\nYES = clear current devices before adding, \n\nNO = add to current list of devices.", L"Scanning", MB_YESNOCANCEL | MB_ICONEXCLAMATION);

						if (ms == IDCANCEL)
							break;

						SendMessage(hWnd, (UINT)APP_MESSAGE_SCAN, (WPARAM)ms == IDYES ? 0 : 1, (LPARAM)lParam);
					}
					else
					{
						int ms = MessageBoxW(hWnd, L"Scanning will discover and add any LG devices found.\n\nThe scan is performed locally by filtering the 'Digital Media Devices' category in the device manager for LG devices.\n\nClick OK to continue!", L"Scanning", MB_OKCANCEL| MB_ICONEXCLAMATION);
						if (ms == IDCANCEL)
							break;
						SendMessage(hWnd, (UINT)APP_MESSAGE_SCAN, (WPARAM)0, (LPARAM)lParam);
					}
				}break;
				case ID_M_TEST:
				{
					if (Settings.Devices.size() > 0)
					{
						if (IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
							if (MessageBox(hWnd, L"Please apply unsaved changes before attempting to control the device", L"Information", MB_OK | MB_ICONEXCLAMATION) == IDOK)
								break;
						int ms = MessageBoxW(hWnd, L"You are about to test the ability to control this device?\n\nPlease click YES to power off the device. Then wait about 5 seconds, or until you hear an internal relay of the TV clicking, and press ENTER on your keyboard to power on the device again.", L"Test device", MB_YESNO | MB_ICONQUESTION);
						switch (ms)
						{
						case IDYES:
							SendMessage(hWnd, (UINT)APP_MESSAGE_TURNOFF, (WPARAM)wParam, (LPARAM)lParam);
							MessageBoxW(hWnd, L"Please press ENTER on your keyboard to power on the device again.", L"Test device", MB_OK | MB_ICONEXCLAMATION);
							SendMessage(hWnd, (UINT)APP_MESSAGE_TURNON, (WPARAM)wParam, (LPARAM)lParam);
							break;
						default:break;
						}
					}
				}break;
				case ID_M_TURNON:
				{
					if (Settings.Devices.size() > 0)
					{
						if (IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
							if (MessageBox(hWnd, L"Please apply unsaved changes before attempting to control the device", L"Information", MB_OK | MB_ICONEXCLAMATION) == IDOK)
								break;
						SendMessage(hWnd, (UINT)APP_MESSAGE_TURNON, (WPARAM)wParam, (LPARAM)lParam);
					}
				}break;
				case ID_M_TURNOFF:
				{
					if (Settings.Devices.size() > 0)
					{
						if (IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
							if (MessageBox(hWnd, L"Please apply unsaved changes before attempting to control the device", L"Information", MB_OK | MB_ICONEXCLAMATION) == IDOK)
								break;
						SendMessage(hWnd, (UINT)APP_MESSAGE_TURNOFF, (WPARAM)wParam, (LPARAM)lParam);
					}
				}break;

				default:break;
				}
			}
		}	break;

		default:break;
		}
	}break;
	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		if ((HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_ENABLE))
		{
			SetBkMode(hdcStatic, TRANSPARENT);
		}
		return(INT_PTR)hBackbrush;
	}break;
	case WM_COPYDATA:
	{
		std::wstring CmdLineExternal;
		if (!lParam)
			return true;

		COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)lParam;

		if (pcds->dwData == NOTIFY_NEW_COMMANDLINE)
		{
			if (pcds->cbData == 0)
				return true;
			CmdLineExternal = (WCHAR*)pcds->lpData;
			if (CmdLineExternal.size() == 0)
				return true;
			CommunicateWithService(CmdLineExternal);
		}
		return true;
	}break;
	case WM_PAINT:
	{
		RECT rc = { 0 };
		GetClientRect(hWnd, &rc);
		PAINTSTRUCT ps;
		//		PAINTSTRUCT psPaint;
		InvalidateRect(hWnd, NULL, false);

		//		GetClientRect(hWnd, &rc);
		int width = rc.right;
		int height = rc.bottom;
		HDC hdc = BeginPaint(hWnd, &ps);
		HDC backbuffDC = CreateCompatibleDC(hdc);
		HBITMAP backbuffer = CreateCompatibleBitmap(hdc, width, height);
		int savedDC = SaveDC(backbuffDC);
		SelectObject(backbuffDC, backbuffer);

		FillRect(backbuffDC, &rc, (HBRUSH)hBackbrush);

		BitBlt(hdc, 0, 0, width, height, backbuffDC, 0, 0, SRCCOPY);
		RestoreDC(backbuffDC, savedDC);

		DeleteObject(backbuffer);
		DeleteDC(backbuffDC);

		ReleaseDC(hWnd, hdc);
		EndPaint(hWnd, &ps);
		return 0;
	}break;
	case WM_ERASEBKGND:
	{
		return true;
	}break;
	case WM_CLOSE:
	{
		if (IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
		{
			int mess = MessageBox(hWnd, L"You have made changes which are not yet applied.\n\nDo you want to apply the changes before exiting?", L"Unsaved configuration", MB_YESNOCANCEL | MB_ICONEXCLAMATION);
			if (mess == IDCANCEL)
				break;
			if (mess == IDYES)
				SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), NULL);
		}
		DestroyWindow(hWnd);
	}break;

	case WM_DESTROY:
	{
		PostQuitMessage(0);
		break;
	}
	default:
		return false;
	}
	return true;
}

//   Process messages for the add/manage devices window
LRESULT CALLBACK DeviceWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		SendDlgItemMessage(hWnd, IDC_DEVICENAME, WM_SETFONT, (WPARAM)hEditfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_DEVICEIP, WM_SETFONT, (WPARAM)hEditfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_HDMI_INPUT_NUMBER, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_HDMI_INPUT_NUMBER_SPIN, UDM_SETRANGE, (WPARAM)NULL, MAKELPARAM(4, 1));
		SendDlgItemMessage(hWnd, IDC_DEVICEMACS, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_SUBNET, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_SET_HDMI_INPUT_NUMBER, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_SET_HDMI_INPUT_SPIN, UDM_SETRANGE, (WPARAM)NULL, MAKELPARAM(4, 1));
		SendDlgItemMessage(hWnd, IDC_COMBO_SSL, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
	}break;
	case WM_NOTIFY:
	{
		switch (((NMHDR*)lParam)->code)
		{
		case NM_CLICK:
		{
			// explain the WOL options
			if (wParam == IDC_SYSLINK8)
			{
				MessageBox(hWnd,
					L"Name your devices so you can easily distinguish them.\n\n"
					"The IP-address and MAC-address are the device's unique network identifiers. If the automatic scan could not determine the "
					"correct values please enter the network settings "
					"of your  WebOS-device to determine the correct values.\n\n"
					"Please note that both MAC address and IP are mandatory to set.",
					L"Device information", MB_OK | MB_ICONINFORMATION);
			}
			// explain the WOL options
			else if (wParam == IDC_SYSLINK4)
			{
				MessageBox(hWnd,
					L"Devices are powered on by means of sending wake-on lan (WOL) magic packets. Depending on your network environment, device firmware and operating system you may need to adjust these settings.\n\n"
					"The \"Send to device IP-address\" method (no 2) sends unicast packets directly to the device IP. IP neighbor table overrides are used to make this work even if ARP doesn't. This is believed to be the most reliable option for most users.\n\n"
					"If the \"Send to device IP-address\" option doesn't work reliably please try the other options. You are invited to file an issue on GitHub so that we can get a better understanding of what works and what doesn't work in the wild.\n\n"
					"Anecdotal evidence seems to suggest broadcast-based approaches can intermittently fail to wake up the TV (see e.g. GitHub issue #19), which is why they are not recommended.\n\n"
					"Between the two broadcast options, the subnet approach is the most likely to work. The global network broadcast approach is prone to issues when multiple network interfaces are present (VPN, etc.), because the packet might be sent using the wrong interface.\n\n"
					"The current subnet mask of the subnet(s) of your PC can be found by using the \"IPCONFIG /all\" command in the command prompt.",
					L"Network options", MB_OK | MB_ICONINFORMATION);
			}
			// explain the WOL options
			else if (wParam == IDC_SYSLINK9)
			{
				MessageBox(hWnd,
					L"The option to process power and screen off events only when the HDMI display input is set to a specific input should be used to prevent the "
					"WebOS-device from powering off while you are not using it for your PC, i e when you are busy watching Netflix or gaming on your console.\n\n"
					"The option to set an HDMI input on startup or resume can be used to force the device to automatically switch to the input "
					" the PC is connected to when powering on.",
					L"Information", MB_OK | MB_ICONINFORMATION);
			}
			// explain the firmware options
			else if (wParam == IDC_SYSLINK10)
			{
				MessageBox(hWnd,
					L"In Q1 2023 a firmware upgrade was pushed by LG to many WebOS-devices which caused connectivity issues. A new method for connection was implemented "
					"and it is now recommended for most users to use the \"Auto\" option. The legacy option will force the usage of the original connection method.",
					L"Information", MB_OK | MB_ICONINFORMATION);
			}
		}break;
		}
	}break;
	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDC_HDMI_INPUT_NUMBER_CHECKBOX:
			{
				EnableWindow(GetDlgItem(hWnd, IDC_HDMI_INPUT_NUMBER), IsDlgButtonChecked(hWnd, IDC_HDMI_INPUT_NUMBER_CHECKBOX) == BST_CHECKED);
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDC_SET_HDMI_INPUT_CHECKBOX:
			{
				EnableWindow(GetDlgItem(hWnd, IDC_SET_HDMI_INPUT_NUMBER), IsDlgButtonChecked(hWnd, IDC_SET_HDMI_INPUT_CHECKBOX) == BST_CHECKED);
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDC_RADIO1:
			case IDC_RADIO2:
			{
				EnableWindow(GetDlgItem(hWnd, IDC_SUBNET), false);
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDC_RADIO3:
			{
				EnableWindow(GetDlgItem(hWnd, IDC_SUBNET), true);
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDOK:
			{
				HWND hParentWnd = GetParent(hWnd);
				if (hParentWnd)
				{
					std::vector<std::string> maclines;
					std::wstring edittext = common::GetWndText(GetDlgItem(hWnd, IDC_DEVICEMACS));

					//verify the user supplied information
					if (edittext != L"")
					{
						transform(edittext.begin(), edittext.end(), edittext.begin(), ::toupper);

						//remove unecessary characters
						char CharsToRemove[] = ":- ,;.\n";
						for (int i = 0; i < strlen(CharsToRemove); ++i)
							edittext.erase(remove(edittext.begin(), edittext.end(), CharsToRemove[i]), edittext.end());

						//check on HEX
						if (edittext.find_first_not_of(L"0123456789ABCDEF\r") != std::string::npos)
						{
							MessageBox(hWnd, L"One or several MAC addresses contain illegal caharcters.\n\nPlease correct before continuing.", L"Error", MB_OK | MB_ICONERROR);
							return 0;
						}
						//verify length of MACs
						maclines = common::stringsplit(common::narrow(edittext), "\r");
						for (auto& mac : maclines)
						{
							if (mac.length() != 12)
							{
								MessageBox(hWnd, L"One or several MAC addresses is incorrect.\n\nPlease correct before continuing.", L"Error", MB_OK | MB_ICONERROR);
								maclines.clear();
								return 0;
							}
							else
								for (int ind = 4; ind >= 0; ind--)
									mac.insert(ind * 2 + 2, ":");
						}
						//verify IP
						if (common::narrow(common::GetWndText(GetDlgItem(hWnd, IDC_DEVICEIP))) == "0.0.0.0")
						{
							MessageBox(hWnd, L"The IP-address is invalid.\n\nPlease correct before continuing.", L"Error", MB_OK | MB_ICONERROR);
							return 0;
						}
					}

					if (maclines.size() > 0 && common::GetWndText(GetDlgItem(hWnd, IDC_DEVICENAME)) != L"" && common::GetWndText(GetDlgItem(hWnd, IDC_DEVICEIP)) != L"")
					{
						if (common::GetWndText(hWnd) == DEVICEWINDOW_TITLE_MANAGE) //configuring existing device
						{
							int sel = (int)(SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
							if (sel == CB_ERR)
								break;

							Settings.Devices[sel].MAC = maclines;
							Settings.Devices[sel].Name = common::narrow(common::GetWndText(GetDlgItem(hWnd, IDC_DEVICENAME)));
							Settings.Devices[sel].IP = common::narrow(common::GetWndText(GetDlgItem(hWnd, IDC_DEVICEIP)));
							Settings.Devices[sel].Subnet = common::narrow(common::GetWndText(GetDlgItem(hWnd, IDC_SUBNET)));

							Settings.Devices[sel].HDMIinputcontrol = IsDlgButtonChecked(hWnd, IDC_HDMI_INPUT_NUMBER_CHECKBOX) == BST_CHECKED;
							Settings.Devices[sel].OnlyTurnOffIfCurrentHDMIInputNumberIs = atoi(common::narrow(common::GetWndText(GetDlgItem(hWnd, IDC_HDMI_INPUT_NUMBER))).c_str());

							Settings.Devices[sel].SetHDMIInputOnResume = IsDlgButtonChecked(hWnd, IDC_SET_HDMI_INPUT_CHECKBOX) == BST_CHECKED;
							Settings.Devices[sel].SetHDMIInputOnResumeToNumber = atoi(common::narrow(common::GetWndText(GetDlgItem(hWnd, IDC_SET_HDMI_INPUT_NUMBER))).c_str());

							if (IsDlgButtonChecked(hWnd, IDC_RADIO3))
								Settings.Devices[sel].WOLtype = WOL_SUBNETBROADCAST;
							else if (IsDlgButtonChecked(hWnd, IDC_RADIO2))
								Settings.Devices[sel].WOLtype = WOL_IPSEND;
							else
								Settings.Devices[sel].WOLtype = WOL_NETWORKBROADCAST;

							int SSL_selection = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_SSL), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
							if (SSL_selection != CB_ERR)
							{
								Settings.Devices[sel].SSL = SSL_selection == 0 ? true : false;
							}

							int ind = (int)SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_DELETESTRING, (WPARAM)sel, (LPARAM)0);
							SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_INSERTSTRING, (WPARAM)sel, (LPARAM)common::widen(Settings.Devices[sel].Name).c_str());
							SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)sel, (LPARAM)0);
							EnableWindow(GetDlgItem(hParentWnd, IDC_COMBO), true);
							EnableWindow(GetDlgItem(hParentWnd, IDOK), true);
						}
						else //adding a new device
						{
							settings::DEVICE sess;
							std::stringstream strs;
							strs << "Device";
							strs << Settings.Devices.size() + 1;
							sess.DeviceId = strs.str();
							sess.MAC = maclines;
							sess.Name = common::narrow(common::GetWndText(GetDlgItem(hWnd, IDC_DEVICENAME)));
							sess.IP = common::narrow(common::GetWndText(GetDlgItem(hWnd, IDC_DEVICEIP)));

							sess.Subnet = common::narrow(common::GetWndText(GetDlgItem(hWnd, IDC_SUBNET)));

							if (IsDlgButtonChecked(hWnd, IDC_RADIO3))
								sess.WOLtype = WOL_SUBNETBROADCAST;
							else if (IsDlgButtonChecked(hWnd, IDC_RADIO2))
								sess.WOLtype = WOL_IPSEND;
							else
								sess.WOLtype = WOL_IPSEND;

							sess.HDMIinputcontrol = IsDlgButtonChecked(hWnd, IDC_HDMI_INPUT_NUMBER_CHECKBOX) == BST_CHECKED;
							sess.OnlyTurnOffIfCurrentHDMIInputNumberIs = atoi(common::narrow(common::GetWndText(GetDlgItem(hWnd, IDC_HDMI_INPUT_NUMBER))).c_str());
							sess.SetHDMIInputOnResume = IsDlgButtonChecked(hWnd, IDC_SET_HDMI_INPUT_CHECKBOX) == BST_CHECKED;
							sess.SetHDMIInputOnResumeToNumber = atoi(common::narrow(common::GetWndText(GetDlgItem(hWnd, IDC_SET_HDMI_INPUT_NUMBER))).c_str());

							int SSL_selection = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_SSL), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
							if (SSL_selection != CB_ERR)
							{
								sess.SSL = SSL_selection == 0 ? true : false;
							}

							Settings.Devices.push_back(sess);

							int index = (int)SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)common::widen(sess.Name).c_str());
							if (index != CB_ERR)
							{
								SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)index, (LPARAM)0);
								CheckDlgButton(hParentWnd, IDC_CHECK_ENABLE, BST_CHECKED);
								EnableWindow(GetDlgItem(hParentWnd, IDC_COMBO), true);
								EnableWindow(GetDlgItem(hParentWnd, IDC_CHECK_ENABLE), true);

								SetDlgItemText(hParentWnd, IDC_SPLIT, L"C&onfigure");
								EnableWindow(GetDlgItem(hParentWnd, IDOK), true);
							}
							else {
								MessageBox(hWnd, L"Failed to add a new device.\n\nUnknown error.", L"Error", MB_OK | MB_ICONERROR);
							}
						}

						EndDialog(hWnd, 0);

						EnableWindow(GetParent(hWnd), true);
					}
					else
					{
						MessageBox(hWnd, L"The configuration is incorrect or missing information.\n\nPlease correct before continuing.", L"Error", MB_OK | MB_ICONERROR);
						return 0;
					}
				}
			}break;

			case IDCANCEL:
			{
				if (IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
				{
					if (MessageBox(hWnd, L"You have made changes to the configuration.\n\nDo you want to discard the changes?", L"Error", MB_YESNOCANCEL | MB_ICONQUESTION) == IDYES)
					{
						EndDialog(hWnd, 0);
						EnableWindow(GetParent(hWnd), true);
					}
				}
				else
				{
					EndDialog(hWnd, 0);
					EnableWindow(GetParent(hWnd), true);
				}
			}break;
			default:break;
			}
		}break;
		case EN_CHANGE:
		{
			switch (LOWORD(wParam))
			{
			case IDC_DEVICENAME:
			case IDC_DEVICEIP:
			case IDC_HDMI_INPUT_NUMBER:
			case IDC_SET_HDMI_INPUT_NUMBER:
			case IDC_DEVICEMACS:
			case IDC_SUBNET:

			{
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			default:break;
			}
		}break;
		case CBN_SELCHANGE:
		{
			EnableWindow(GetDlgItem(hWnd, IDOK), true);
		}break;
		default:break;
		}
	}break;

	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		if ((HWND)lParam == GetDlgItem(hWnd, IDC_HDMI_INPUT_NUMBER_CHECKBOX)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_SET_HDMI_INPUT_CHECKBOX)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_RADIO1)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_RADIO2)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_RADIO3))
		{
			SetBkMode(hdcStatic, TRANSPARENT);
		}
		return(INT_PTR)hBackbrush;
	}break;

	case WM_PAINT:
	{
		RECT rc = { 0 };
		GetClientRect(hWnd, &rc);
		PAINTSTRUCT ps;
		//		PAINTSTRUCT psPaint;
		InvalidateRect(hWnd, NULL, false);

		//		GetClientRect(hWnd, &rc);
		int width = rc.right;
		int height = rc.bottom;
		HDC hdc = BeginPaint(hWnd, &ps);
		HDC backbuffDC = CreateCompatibleDC(hdc);
		HBITMAP backbuffer = CreateCompatibleBitmap(hdc, width, height);
		int savedDC = SaveDC(backbuffDC);
		SelectObject(backbuffDC, backbuffer);

		FillRect(backbuffDC, &rc, (HBRUSH)hBackbrush);

		BitBlt(hdc, 0, 0, width, height, backbuffDC, 0, 0, SRCCOPY);
		RestoreDC(backbuffDC, savedDC);

		DeleteObject(backbuffer);
		DeleteDC(backbuffDC);

		ReleaseDC(hWnd, hdc);
		EndPaint(hWnd, &ps);
		return 0;
	}break;
	case WM_ERASEBKGND:
	{
		return true;
	}break;

	case WM_CLOSE:
	{
		EndDialog(hWnd, 0);
	}break;
	case WM_DESTROY:
	{
		hDeviceWindow = NULL;
	}break;
	default:
		return false;
	}
	return true;
}

//   Process messages for the options window
LRESULT CALLBACK OptionsWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		LVCOLUMN lvC;
		LVITEM lvi;
		DWORD status = ERROR_SUCCESS;
		EVT_HANDLE hResults = NULL;
		std::wstring path = L"System";
		std::wstring query = L"Event/System[EventID=1074]";
		std::vector<std::wstring> str;

		SendDlgItemMessage(hWnd, IDC_STATIC_C, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_COMBO_MODE, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_TIMEOUT, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_LIST, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_COMBO_TIMING, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
		std::wstring s;
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_TIMING), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		s = L"Default";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_TIMING), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		s = L"Early";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_TIMING), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_TIMING), (UINT)CB_SETCURSEL, (WPARAM)Settings.Prefs.TimingPreshutdown ? 1 : 0, (LPARAM)0);

		SendDlgItemMessage(hWnd, IDC_SPIN, UDM_SETRANGE, (WPARAM)NULL, MAKELPARAM(100, 1));
		SendDlgItemMessage(hWnd, IDC_SPIN, UDM_SETPOS, (WPARAM)NULL, (LPARAM)Settings.Prefs.PowerOnTimeout);

		for (auto& item : Settings.Prefs.EventLogRestartString)
		{
			if (std::find(str.begin(), str.end(), common::widen(item)) == str.end())
				str.push_back(common::widen(item));
		}
		for (auto& item : Settings.Prefs.EventLogShutdownString)
		{
			if (std::find(str.begin(), str.end(), common::widen(item)) == str.end())
				str.push_back(common::widen(item));
		}

		hResults = EvtQuery(NULL, path.c_str(), query.c_str(), EvtQueryChannelPath | EvtQueryReverseDirection);
		if (hResults)
		{
			EVT_HANDLE hEv[100];
			DWORD dwReturned = 0;

			if (EvtNext(hResults, 100, hEv, INFINITE, 0, &dwReturned))
			{
				for (DWORD i = 0; i < dwReturned; i++)
				{
					DWORD dwBufferSize = 0;
					DWORD dwBufferUsed = 0;
					DWORD dwPropertyCount = 0;
					LPWSTR pRenderedContent = NULL;

					if (!EvtRender(NULL, hEv[i], EvtRenderEventXml, dwBufferSize, pRenderedContent, &dwBufferUsed, &dwPropertyCount))
					{
						if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
						{
							dwBufferSize = dwBufferUsed;
							pRenderedContent = (LPWSTR)malloc(dwBufferSize);
							if (pRenderedContent)
							{
								EvtRender(NULL, hEv[i], EvtRenderEventXml, dwBufferSize, pRenderedContent, &dwBufferUsed, &dwPropertyCount);
							}
						}
					}

					if (pRenderedContent)
					{
						std::wstring s = pRenderedContent;
						free(pRenderedContent);

						std::wstring strfind = L"<Data Name='param5'>";
						size_t f = s.find(strfind);
						if (f != std::wstring::npos)
						{
							size_t e = s.find(L"<", f + 1);
							if (e != std::wstring::npos)
							{
								std::wstring sub = s.substr(f + strfind.length(), e - (f + strfind.length()));
								if (std::find(str.begin(), str.end(), sub) == str.end())
								{
									str.push_back(sub);
								}
							}
						}
					}
					EvtClose(hEv[i]);
					hEv[i] = NULL;
				}
			}
			EvtClose(hResults);
		}
		lvC.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		lvC.iSubItem = 0; lvC.cx = 140;	lvC.fmt = LVCFMT_LEFT;
		lvC.pszText = (LPWSTR)L"Eventlog 1074";
		ListView_InsertColumn(GetDlgItem(hWnd, IDC_LIST), 0, &lvC);
		ListView_SetExtendedListViewStyle(GetDlgItem(hWnd, IDC_LIST), LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);
		memset(&lvi, 0, sizeof(LVITEM));
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
		lvi.state = 0;
		lvi.stateMask = 0;
		lvi.iItem = 1;
		lvi.iSubItem = 0;
		lvi.lParam = (LPARAM)0;
		int i = 0;

		for (auto& item : str)
		{
			lvi.iItem = i;
			int row = ListView_InsertItem(GetDlgItem(hWnd, IDC_LIST), &lvi);
			ListView_SetItemText(GetDlgItem(hWnd, IDC_LIST), row, 0, (LPWSTR)item.c_str());
			if (std::find(Settings.Prefs.EventLogRestartString.begin(), Settings.Prefs.EventLogRestartString.end(), common::narrow(item)) != Settings.Prefs.EventLogRestartString.end())
			{
				ListView_SetCheckState(GetDlgItem(hWnd, IDC_LIST), row, true);
			}
			i++;
		}
		CheckDlgButton(hWnd, IDC_LOGGING, Settings.Prefs.Logging ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_AUTOUPDATE, Settings.Prefs.AutoUpdate ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_BLANK, Settings.Prefs.BlankScreenWhenIdle ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_REMOTE, Settings.Prefs.RemoteStreamingCheck ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_TOPOLOGY, Settings.Prefs.AdhereTopology ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_TOPOLOGY_LOGON, Settings.Prefs.KeepTopologyOnBoot ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_API, Settings.Prefs.ExternalAPI ? BST_CHECKED : BST_UNCHECKED);

		std::wstring ls;
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_MODE), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		ls = L"Power efficiency (display off)";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_MODE), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ls.c_str());
		ls = L"Compatibility (display blanked)";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_MODE), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ls.c_str());
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_MODE), (UINT)CB_SETCURSEL, (WPARAM)Settings.Prefs.TopologyPreferPowerEfficiency ? 0 : 1, (LPARAM)0);

		//		EnableWindow(GetDlgItem(hWnd, IDC_COMBO_MODE), Settings.Prefs.AdhereTopology ? true :  false);
		EnableWindow(GetDlgItem(hWnd, IDC_COMBO_MODE), false); //REMOVE
		EnableWindow(GetDlgItem(hWnd, IDC_CHECK_TOPOLOGY_LOGON), Settings.Prefs.AdhereTopology ? true : false);
		EnableWindow(GetDlgItem(hWnd, IDOK), false);
	}break;
	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDC_CHECK_BLANK:
			{
				if (Settings.Prefs.version < 2 && !Settings.Prefs.ResetAPIkeys)
				{
					int mess = MessageBox(hWnd, L"Enabling this option will enforce re-pairing of all your devices.\n\n Do you want to enable this option?", L"Device pairing", MB_YESNO | MB_ICONQUESTION);
					if (mess == IDNO)
					{
						CheckDlgButton(hWnd, IDC_CHECK_BLANK, BST_UNCHECKED);
					}
					if (mess == IDYES)
					{
						CheckDlgButton(hWnd, IDC_CHECK_BLANK, BST_CHECKED);
						Settings.Prefs.ResetAPIkeys = true;
						EnableWindow(GetDlgItem(hWnd, IDOK), true);
					}
				}
				else
				{
					EnableWindow(GetDlgItem(hWnd, IDOK), true);
				}
			}break;
			case IDC_CHECK_TOPOLOGY:
			{
				if (Settings.Prefs.version < 2 && !Settings.Prefs.ResetAPIkeys)
				{
					int mess = MessageBox(hWnd, L"Enabling this option will enforce re-pairing of all your devices.\n\n Do you want to enable this option?", L"Device pairing", MB_YESNO | MB_ICONQUESTION);
					if (mess == IDNO)
					{
						CheckDlgButton(hWnd, IDC_CHECK_TOPOLOGY, BST_UNCHECKED);
					}
					if (mess == IDYES)
					{
						CheckDlgButton(hWnd, IDC_CHECK_TOPOLOGY, BST_CHECKED);
						Settings.Prefs.ResetAPIkeys = true;
						EnableWindow(GetDlgItem(hWnd, IDOK), true);
					}
				}
				else
				{
					bool found = false;
					if (Settings.Devices.size() == 0)
					{
						MessageBox(hWnd, L"Please configure all devices before enabling and configuring this option", L"Error", MB_ICONEXCLAMATION | MB_OK);
						CheckDlgButton(hWnd, IDC_CHECK_TOPOLOGY, BST_UNCHECKED);
					}
					else
					{
						for (auto& k : Settings.Devices)
						{
							if (k.UniqueDeviceKey != "")
								found = true;
						}

						if (found)
						{
							EnableWindow(GetDlgItem(hWnd, IDOK), true);
						}
						else
						{
							hTopologyWindow = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_CONFIGURE_TOPOLOGY), hWnd, (DLGPROC)ConfigureTopologyWndProc);
							EnableWindow(hWnd, false);
							ShowWindow(hTopologyWindow, SW_SHOW);
						}
					}
				}
				EnableWindow(GetDlgItem(hWnd, IDC_CHECK_TOPOLOGY_LOGON), IsDlgButtonChecked(hWnd, IDC_CHECK_TOPOLOGY));
			}break;

			case IDC_LOGGING:
			case IDC_AUTOUPDATE:
			case IDC_CHECK_REMOTE:
			case IDC_CHECK_API:
			case IDC_CHECK_TOPOLOGY_LOGON:

			{
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;

			case IDOK:
			{
				HWND hParentWnd = GetParent(hWnd);
				if (hParentWnd)
				{
					Settings.Prefs.PowerOnTimeout = atoi(common::narrow(common::GetWndText(GetDlgItem(hWnd, IDC_TIMEOUT))).c_str());
					Settings.Prefs.Logging = IsDlgButtonChecked(hWnd, IDC_LOGGING);

					bool TempAutoUpdate = IsDlgButtonChecked(hWnd, IDC_AUTOUPDATE);
					if (TempAutoUpdate && !Settings.Prefs.AutoUpdate)
					{
						std::thread thread_obj(VersionCheckThread, hMainWnd);
						thread_obj.detach();
					}
					Settings.Prefs.AutoUpdate = IsDlgButtonChecked(hWnd, IDC_AUTOUPDATE);
					Settings.Prefs.BlankScreenWhenIdle = IsDlgButtonChecked(hWnd, IDC_CHECK_BLANK) == BST_CHECKED;
					Settings.Prefs.RemoteStreamingCheck = IsDlgButtonChecked(hWnd, IDC_CHECK_REMOTE) == BST_CHECKED;
					Settings.Prefs.AdhereTopology = IsDlgButtonChecked(hWnd, IDC_CHECK_TOPOLOGY) == BST_CHECKED;
					Settings.Prefs.KeepTopologyOnBoot = IsDlgButtonChecked(hWnd, IDC_CHECK_TOPOLOGY_LOGON) == BST_CHECKED;
					Settings.Prefs.ExternalAPI = IsDlgButtonChecked(hWnd, IDC_CHECK_API) == BST_CHECKED;

					int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_MODE), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
					if (sel == 1)
						Settings.Prefs.TopologyPreferPowerEfficiency = false;
					else
						Settings.Prefs.TopologyPreferPowerEfficiency = true;

					sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_TIMING), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
					if (sel == 1)
						Settings.Prefs.TimingPreshutdown = true;
					else
						Settings.Prefs.TimingPreshutdown = false;

					int count = ListView_GetItemCount(GetDlgItem(hWnd, IDC_LIST));
					Settings.Prefs.EventLogRestartString.clear();
					Settings.Prefs.EventLogShutdownString.clear();
					for (int i = 0; i < count; i++)
					{
						std::vector<wchar_t> bufText(256);
						std::wstring st;
						ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST), i, 0, &bufText[0], (int)bufText.size());
						st = &bufText[0];
						if (ListView_GetCheckState(GetDlgItem(hWnd, IDC_LIST), i))
						{
							Settings.Prefs.EventLogRestartString.push_back(common::narrow(st));
						}
						else
						{
							Settings.Prefs.EventLogShutdownString.push_back(common::narrow(st));
						}
					}
					EndDialog(hWnd, 0);
					EnableWindow(GetParent(hWnd), true);
					EnableWindow(GetDlgItem(GetParent(hWnd), IDOK), true);
				}
			}break;

			case IDCANCEL:
			{
				Settings.Prefs.ResetAPIkeys = false;

				EndDialog(hWnd, 0);
				EnableWindow(GetParent(hWnd), true);
			}break;
			default:break;
			}
		}break;
		case EN_CHANGE:
		{
			switch (LOWORD(wParam))
			{
			case IDC_TIMEOUT:
			{
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			default:break;
			}
		}break;
		case CBN_SELCHANGE:
		{
			EnableWindow(GetDlgItem(hWnd, IDOK), true);
		}break;
		default:break;
		}
	}break;
	case WM_NOTIFY:
	{
		switch (((NMHDR*)lParam)->code)
		{
		case NM_CLICK:
		{
			//show log
			if (wParam == IDC_SYSLINK)
			{
				std::wstring str = Settings.Prefs.DataPath;
				str += LOG_FILE;
				ShellExecute(NULL, L"open", str.c_str(), NULL, Settings.Prefs.DataPath.c_str(), SW_SHOW);
			}
			//clear log
			else if (wParam == IDC_SYSLINK2)
			{
				std::wstring str = Settings.Prefs.DataPath;
				str += LOG_FILE;
				std::wstring mess = L"Do you want to clear the log?\n\n";
				mess += str;
				if (MessageBox(hWnd, mess.c_str(), L"Clear log?", MB_YESNO | MB_ICONQUESTION) == IDYES)
				{
					CommunicateWithService(L"-clearlog");
				}
			}
			// explain the restart words
			else if (wParam == IDC_SYSLINK3)
			{
				MessageBox(hWnd, L"This application rely on events in the windows event log to determine whether a reboot or shutdown was initiated by the user."
					"\n\nThese events are localised in the language of your operating system, and the user must therefore assist with manually indicating which "
					"word or phrase that refers to the system restarting.\n\nPlease put a checkmark for every word or phrase that refers to 'restart'in the list and "
					"make sure that all other checkboxes are cleared.\n\n"
					"The timing option determine the timing of managing shutdown/restart. When set to \"Default\" the app will trigger the shutdown/restart routine "
					"as late as possible during system shutdown. This will maximize the opportunity for detecting if the system is restarting or shutting down. \n\nIf "
					"however the devices are not shutting down properly during system shutdown you may want to consider changing to the \"Early\" method. This option will "
					"trigger the shutdown/restart routine earlier, which will leave more time during shutdown for properly shutting down the devices, but there is "
					"instead a higher risk of not properly detecting restarts.",

					L"Restart words", MB_OK | MB_ICONINFORMATION);
			}
			// explain the power on timeout, logging etc
			else if (wParam == IDC_SYSLINK7)
			{
				MessageBox(hWnd, L"The power on timeout determines the maximum number of seconds the app will attempt to connect to devices when powering on. "
					"Please consider increasing this value if your PC needs more time to establish contact with the WebOS-devices during system boot.\n\n"
					"The option to enable logging is very useful for troubleshooting and if you are experiencing issues with the operations of this app.\n\n"
					"The option to automatically notify when a new version is available ensure that you are always notified when a new version of LGTV Companion is available for download.",
					L"Global options", MB_OK | MB_ICONINFORMATION);
			}
			// explain the power saving options
			else if (wParam == IDC_SYSLINK5)
			{
				MessageBox(hWnd, L"The user idle mode will automatically blank the screen, i e turn the transmitters off, in the absence of user input from keyboard, mouse and/or controllers. "
					"The difference, when compared to both the screensaver and windows power plan settings, is that those OS implemented power saving features "
					"utilize more obscured variables for determining user idle / busy states, and which can also be programmatically overridden f.e. by games, "
					"media players, production software or your web browser, In short, and simplified, this option is a more aggressively configured screen and "
					"power saver. \n\n"
					"The option to support remote streaming hosts will power off managed devices while the system is acting as streaming host or being remoted into. Supported "
					"hosts include Nvidia gamestream, Moonlight, Steam Link and RDP. \n\nPlease note that the devices will remain powered off until the remote connection is disconnected. \n\n"
					"NOTE! Support for detecting Sunshine streaming host require Sunshine to be installed (i.e. not portable install) and Sunshine logging level be at minimum on level \"Info\" (default)",
					L"Andvanced power options", MB_OK | MB_ICONINFORMATION);
			}
			// explain the multi-monitor conf
			else if (wParam == IDC_SYSLINK6)
			{
				MessageBox(hWnd, L"The option to support the windows multi-monitor topology ensures that the "
					"power state of individual devices will match the enabled or disabled state in the Windows monitor configuration, i e "
					"when an HDMI-output is disabled in the graphics card configuration the associated device will also power off. "
					"If you have a multi-monitor system and a compatible WebOS device it is recommended to configure and enable this feature.\n\n"
					"The option to keep the topology configuration also on the logon screen will apply the last known configuration at the logon "
					"screen at system boot.\n\n"
					"PLEASE NOTE! This option will not work on all system configuration and may fail to power on the devices again appropriately. "
					"Enabling \"Always Ready\" in the settings of compatible WebOS devices (2022-models, C2, G2 etc, and later) will ensure that "
					"the feature works properly.\n\n"
					"ALSO NOTE! A change of GPU or adding more displays may invalidate the configuration. If so, please run the configuration guide "
					"again to ensure correct operation.\n\n",
					L"Multi-monitor support", MB_OK | MB_ICONINFORMATION);
			}
			
			// explain the external api
			else if (wParam == IDC_SYSLINK11)
			{
				if (MessageBox(hWnd, L"Scripts or other applications can use the \"External API\" to be notified of power events. The "
					"usability of this is up to the creativity of the user, but can for example be used to trigger power state changes "
					"of non-LG devices, execute scripts, trigger RGB profiles etc. The functionality and implementation is currently in BETA and "
					"is preferrably discussed in the Discord #external-api channel (https://discord.gg/7KkTPrP3fq)\n\n"
					"The current implementation implements intra-process-communication via a named pipe and example scripts are available "
					"in the #external-api channel.\n\nDo you wish to go to Discord? Please click \"Yes\" to go to Discord or click \"No\" "
					"to close this information window", L"External API", MB_YESNO | MB_ICONQUESTION) == IDYES)
				{
					ShellExecute(0, 0, DISCORDLINK, 0, 0, SW_SHOW);
				}
				
			}
			else if (wParam == IDC_SYSLINK_CONF)
			{
				if (Settings.Devices.size() == 0)
				{
					MessageBox(hWnd, L"Please configure devices before enabling and configuring this option", L"Error", MB_ICONEXCLAMATION | MB_OK);
					CheckDlgButton(hWnd, IDC_CHECK_TOPOLOGY, BST_UNCHECKED);
				}
				else
				{
					hTopologyWindow = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_CONFIGURE_TOPOLOGY), hWnd, (DLGPROC)ConfigureTopologyWndProc);
					EnableWindow(hWnd, false);
					ShowWindow(hTopologyWindow, SW_SHOW);
				}
			}
			else if (wParam == IDC_SYSLINK_CONF2)
			{
				hUserIdleConfWindow = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_ADVANCEDIDLE), hWnd, (DLGPROC)UserIdleConfWndProc);
				EnableWindow(hWnd, false);
				ShowWindow(hUserIdleConfWindow, SW_SHOW);
			}
		}break;
		case LVN_ITEMCHANGED:
		{
			if (wParam == IDC_LIST)
			{
				LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
				if (pnmv->uChanged & LVIF_STATE) // item state has been changed
				{
					bool bPrevState = (((pnmv->uOldState & LVIS_STATEIMAGEMASK) >> 12) - 1) == 1 ? true : false;   // Old check box state
					bool bChecked = (((pnmv->uNewState & LVIS_STATEIMAGEMASK) >> 12) - 1) == 1 ? true : false;

					if (bPrevState == bChecked) // No change in check box
						break;

					EnableWindow(GetDlgItem(hWnd, IDOK), true);
				}
			}
		}break;
		default:break;
		}
	}break;

	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		SetTextColor(hdcStatic, COLORREF(COLOR_STATIC));
		if ((HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_BLANK)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_REMOTE)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_LOGGING)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_AUTOUPDATE))
		{
			SetBkMode(hdcStatic, TRANSPARENT);
		}
		return(INT_PTR)hBackbrush;
	}break;

	case WM_PAINT:
	{
		RECT rc = { 0 };
		GetClientRect(hWnd, &rc);
		PAINTSTRUCT ps;
		//		PAINTSTRUCT psPaint;
		InvalidateRect(hWnd, NULL, false);

		//		GetClientRect(hWnd, &rc);
		int width = rc.right;
		int height = rc.bottom;
		HDC hdc = BeginPaint(hWnd, &ps);
		HDC backbuffDC = CreateCompatibleDC(hdc);
		HBITMAP backbuffer = CreateCompatibleBitmap(hdc, width, height);
		int savedDC = SaveDC(backbuffDC);
		SelectObject(backbuffDC, backbuffer);

		FillRect(backbuffDC, &rc, (HBRUSH)hBackbrush);

		BitBlt(hdc, 0, 0, width, height, backbuffDC, 0, 0, SRCCOPY);
		RestoreDC(backbuffDC, savedDC);

		DeleteObject(backbuffer);
		DeleteDC(backbuffDC);

		ReleaseDC(hWnd, hdc);
		EndPaint(hWnd, &ps);
		return 0;
	}break;
	case WM_ERASEBKGND:
	{
		return true;
	}break;

	case WM_CLOSE:
	{
		EndDialog(hWnd, 0);
	}break;
	case WM_DESTROY:
	{
		hOptionsWindow = NULL;
	}	break;
	default:
		return false;
	}
	return true;
}

//   Process messages for the options window
LRESULT CALLBACK ConfigureTopologyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		SendDlgItemMessage(hWnd, IDC_COMBO, WM_SETFONT, (WPARAM)hEditfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_NO_1, WM_SETFONT, (WPARAM)hEditfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_NO_2, WM_SETFONT, (WPARAM)hEditfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_NO_3, WM_SETFONT, (WPARAM)hEditfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_NO_4, WM_SETFONT, (WPARAM)hEditfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_NO_5, WM_SETFONT, (WPARAM)hEditfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_NO_6, WM_SETFONT, (WPARAM)hEditfont, MAKELPARAM(TRUE, 0));

		SendDlgItemMessage(hWnd, IDC_STATIC_T_11, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_T_12, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_T_13, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_T_14, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_T_15, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_T_16, WM_SETFONT, (WPARAM)hEditSmallfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_STATUS_1, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_STATUS_2, WM_SETFONT, (WPARAM)hEditMediumBoldfont, MAKELPARAM(TRUE, 0));
		SetWindowText(GetDlgItem(hWnd, IDC_STATIC_STATUS_2), L"Not configured!");
		for (auto& k : Settings.Devices)
		{
			if (k.UniqueDeviceKey != "")
			{
				SetWindowText(GetDlgItem(hWnd, IDC_STATIC_STATUS_2), L"Waiting for input...");
			}
		}

		SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)-1, (LPARAM)0);

		if (Settings.Devices.size() > 0)
		{
			for (const auto& item : Settings.Devices)
			{
				std::stringstream s;
				s << item.DeviceId << ": " << item.Name << "(" << item.IP << ")";
				SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)common::widen(s.str()).c_str());
			}
			SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
		}
		SendMessage(hWnd, APP_TOP_PHASE_1, NULL, NULL);
		iTopConfDisplay = 0;
	}break;
	case APP_TOP_PHASE_1:
	{
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_1), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_2), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_3), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_4), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_11), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_12), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_13), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_14), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_5), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_15), false);
		EnableWindow(GetDlgItem(hWnd, IDC_COMBO), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_6), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_16), false);
		SetWindowText(GetDlgItem(hWnd, IDOK), L"&Start");
		iTopConfPhase = 1;
	}break;
	case APP_TOP_PHASE_2:
	{
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_1), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_2), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_3), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_4), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_11), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_12), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_13), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_14), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_5), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_15), true);
		EnableWindow(GetDlgItem(hWnd, IDC_COMBO), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_6), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_16), false);
		SetWindowText(GetDlgItem(hWnd, IDC_STATIC_STATUS_2), L"Updating configuration!");
		SetWindowText(GetDlgItem(hWnd, IDOK), L"&Next");
		iTopConfPhase = 2;
	}break;
	case APP_TOP_PHASE_3:
	{
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_1), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_2), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_3), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_4), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_11), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_12), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_13), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_14), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_5), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_15), false);
		EnableWindow(GetDlgItem(hWnd, IDC_COMBO), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_6), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_16), true);
		SetWindowText(GetDlgItem(hWnd, IDOK), L"&Finish");
		iTopConfPhase = 3;
		for (auto& k : Settings.Devices)
		{
			if (k.UniqueDeviceKey_Temp != "")
			{
				SetWindowText(GetDlgItem(hWnd, IDC_STATIC_STATUS_2), L"All OK!");
			}
		}
	}break;
	case APP_TOP_NEXT_DISPLAY:
	{
		std::vector<settings::DISPLAY_INFO> displays = QueryDisplays();
		if (displays.size() > iTopConfDisplay)
		{
			RECT DialogRect;
			RECT DisplayRect;
			GetWindowRect(hWnd, &DialogRect);
			DisplayRect = displays[iTopConfDisplay].monitorinfo.rcWork;

			int x = DisplayRect.left + (DisplayRect.right - DisplayRect.left) / 2 - (DialogRect.right - DialogRect.left) / 2;
			int y = DisplayRect.top + (DisplayRect.bottom - DisplayRect.top) / 2 - (DialogRect.bottom - DialogRect.top) / 2;
			int cx = DialogRect.right - DialogRect.left;
			int cy = DialogRect.bottom - DialogRect.top;
			MoveWindow(hWnd, x, y, cx, cy, true);
		}
	}break;

	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDOK:
			{
				switch (iTopConfPhase) // three phases - intro, match displays, finalise
				{
				case 1:
				{
					std::vector<settings::DISPLAY_INFO> displays = QueryDisplays();
					// No WebOs devices attached
					if (displays.size() == 0)
					{
						MessageBox(hWnd, L"To configure your devices, ensure that all your WebOS-devices are powered ON, connected to to your PC and enabled (with an extended desktop in case of multiple displays).", L"No WebOS devices detected", MB_OK | MB_ICONWARNING);
						break;
					}
					// If exactly one physical device connected/enabled and exactly one device cofigured it is considered an automatic match
					else if (Settings.Devices.size() == 1 && displays.size() == 1)
					{
						if (MessageBox(hWnd, L"Your device can be automatically configured.\n\nDo you want to accept the automatic configuration?", L"Automatic match", MB_YESNO) == IDYES)
						{
							Settings.Devices[0].UniqueDeviceKey_Temp = common::narrow(displays[0].target.monitorDevicePath);
							SendMessage(hWnd, APP_TOP_PHASE_3, NULL, NULL);
							SetWindowText(GetDlgItem(hWnd, IDC_STATIC_STATUS_2), L"All OK!");
							break;
						}
					}
					SendMessage(hWnd, APP_TOP_PHASE_2, NULL, NULL);
					SendMessage(hWnd, APP_TOP_NEXT_DISPLAY, NULL, NULL);
				}break;
				case 2:
				{
					std::vector<settings::DISPLAY_INFO> displays = QueryDisplays();
					int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
					if (sel == CB_ERR || Settings.Devices.size() <= sel)
						break;
					Settings.Devices[sel].UniqueDeviceKey_Temp = common::narrow(displays[iTopConfDisplay].target.monitorDevicePath);

					iTopConfDisplay++;
					if (iTopConfDisplay >= displays.size()) // all displays iterated
					{
						SendMessage(hWnd, APP_TOP_PHASE_3, NULL, NULL);
					}
					else // more displays are connected
					{
						SendMessage(hWnd, APP_TOP_NEXT_DISPLAY, NULL, NULL);
					}
				}break;
				case 3:
				{
					for (auto& k : Settings.Devices)
					{
						k.UniqueDeviceKey = k.UniqueDeviceKey_Temp;
					}
					EndDialog(hWnd, 0);
					EnableWindow(GetParent(hWnd), true);
					EnableWindow(GetDlgItem(GetParent(hWnd), IDOK), true);
				}break;
				default:break;
				}
			}break;
			case IDCANCEL:
			{
				bool conf = false;
				for (auto& k : Settings.Devices)
				{
					if (k.UniqueDeviceKey != "")
					{
						conf = true;
						break;
					}
				}
				if (!conf)
				{
					CheckDlgButton(GetParent(hWnd), IDC_CHECK_TOPOLOGY, BST_UNCHECKED);
					EnableWindow(GetDlgItem(GetParent(hWnd), IDC_CHECK_TOPOLOGY_LOGON), false);
				}
				EndDialog(hWnd, 0);
				EnableWindow(GetParent(hWnd), true);
			}break;
			default:break;
			}
		}break;
		default:break;
		}
	}break;
	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		if ((HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_1)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_2)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_3)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_4)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_T_1)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_T_2)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_T_3)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_T_4)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_STATUS_1)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_STATUS_2))
		{
			SetBkMode(hdcStatic, TRANSPARENT);
		}
		if ((HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_STATUS_2))
		{
			std::wstring s = common::GetWndText(GetDlgItem(hWnd, IDC_STATIC_STATUS_2));
			if (s.find(L"OK!") != std::wstring::npos)
				SetTextColor(hdcStatic, COLORREF(COLOR_GREEN));
			else if (s.find(L"Updating") != std::wstring::npos)
				SetTextColor(hdcStatic, COLORREF(COLOR_BLUE));
			else if (s.find(L"Waiting") != std::wstring::npos)
				SetTextColor(hdcStatic, COLORREF(COLOR_BLUE));

			else
				SetTextColor(hdcStatic, COLORREF(COLOR_RED));
		}

		else if ((HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_1)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_2)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_3)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_4))
			SetTextColor(hdcStatic, COLORREF(COLOR_BLUE));
		else
			SetTextColor(hdcStatic, COLORREF(COLOR_STATIC));

		return(INT_PTR)hBackbrush;
	}break;

	case WM_PAINT:
	{
		RECT rc = { 0 };
		GetClientRect(hWnd, &rc);
		PAINTSTRUCT ps;
		//		PAINTSTRUCT psPaint;
		InvalidateRect(hWnd, NULL, false);

		//		GetClientRect(hWnd, &rc);
		int width = rc.right;
		int height = rc.bottom;
		HDC hdc = BeginPaint(hWnd, &ps);
		HDC backbuffDC = CreateCompatibleDC(hdc);
		HBITMAP backbuffer = CreateCompatibleBitmap(hdc, width, height);
		int savedDC = SaveDC(backbuffDC);
		SelectObject(backbuffDC, backbuffer);

		FillRect(backbuffDC, &rc, (HBRUSH)hBackbrush);

		BitBlt(hdc, 0, 0, width, height, backbuffDC, 0, 0, SRCCOPY);
		RestoreDC(backbuffDC, savedDC);

		DeleteObject(backbuffer);
		DeleteDC(backbuffDC);

		ReleaseDC(hWnd, hdc);
		EndPaint(hWnd, &ps);
		return 0;
	}break;
	case WM_ERASEBKGND:
	{
		return true;
	}break;

	case WM_CLOSE:
	{
		EndDialog(hWnd, 0);
		EnableWindow(GetParent(hWnd), true);
	}break;
	case  WM_DESTROY:
	{
		hTopologyWindow = NULL;
	}break;
	default:
		return false;
	}
	return true;
}

//   Process messages for the user idle conf window
LRESULT CALLBACK UserIdleConfWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		SendDlgItemMessage(hWnd, IDC_LIST, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_LIST2, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_EDIT_TIME, WM_SETFONT, (WPARAM)hEditMediumfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_SPIN, UDM_SETRANGE, (WPARAM)NULL, MAKELPARAM(240, 1));
		SendDlgItemMessage(hWnd, IDC_SPIN, UDM_SETPOS, (WPARAM)NULL, (LPARAM)Settings.Prefs.BlankScreenWhenIdleDelay);

		WhitelistTemp = Settings.Prefs.WhiteList;
		ExclusionsTemp = Settings.Prefs.FsExclusions;
		SendMessage(hWnd, APP_LISTBOX_REDRAW, 1, 0);
		SendMessage(hWnd, APP_LISTBOX_REDRAW, 2, 0);

		CheckDlgButton(hWnd, IDC_CHECK_WHITELIST, Settings.Prefs.bIdleWhitelistEnabled ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_FULLSCREEN, !Settings.Prefs.bFullscreenCheckEnabled ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_FS_EXCLUSIONS, Settings.Prefs.bIdleFsExclusionsEnabled ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_MUTE, Settings.Prefs.MuteSpeakers ? BST_CHECKED : BST_UNCHECKED);

		EnableWindow(GetDlgItem(hWnd, IDC_LIST), Settings.Prefs.bIdleWhitelistEnabled);
		EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_ADD), Settings.Prefs.bIdleWhitelistEnabled);
		EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_EDIR), Settings.Prefs.bIdleWhitelistEnabled);
		EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_DELETE), Settings.Prefs.bIdleWhitelistEnabled);

		EnableWindow(GetDlgItem(hWnd, IDC_CHECK_FS_EXCLUSIONS), IsDlgButtonChecked(hWnd, IDC_CHECK_FULLSCREEN));
		EnableWindow(GetDlgItem(hWnd, IDC_LIST2), IsDlgButtonChecked(hWnd, IDC_CHECK_FULLSCREEN) ? IsDlgButtonChecked(hWnd, IDC_CHECK_FS_EXCLUSIONS) : false);
		EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_ADD2), IsDlgButtonChecked(hWnd, IDC_CHECK_FULLSCREEN) ? IsDlgButtonChecked(hWnd, IDC_CHECK_FS_EXCLUSIONS) : false);
		EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_EDIR2), IsDlgButtonChecked(hWnd, IDC_CHECK_FULLSCREEN) ? IsDlgButtonChecked(hWnd, IDC_CHECK_FS_EXCLUSIONS) : false);
		EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_DELETE2), IsDlgButtonChecked(hWnd, IDC_CHECK_FULLSCREEN) ? IsDlgButtonChecked(hWnd, IDC_CHECK_FS_EXCLUSIONS) : false);
		
		EnableWindow(GetDlgItem(hWnd, IDOK), false);
	}break;
	case APP_LISTBOX_EDIT:
	{
		switch (wParam)
		{
		case 1:
		{
			int index = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_GETCURSEL, 0, 0);
			if (index != LB_ERR)
			{
				int data = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_GETITEMDATA, index, 0);
				if (data != LB_ERR && data < WhitelistTemp.size())
				{
					hWhitelistConfWindow = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_WHITELIST_EDIT), hWnd, (DLGPROC)WhitelistConfWndProc);
					SetWindowText(hWhitelistConfWindow, L"Edit whitelisted process");
					SetWindowText(GetDlgItem(hWhitelistConfWindow, IDOK), L"Change");
					SetWindowText(GetDlgItem(hWhitelistConfWindow, IDC_EDIT_NAME), WhitelistTemp[data].Name.c_str());
					SetWindowText(GetDlgItem(hWhitelistConfWindow, IDC_EDIT_PROCESS), WhitelistTemp[data].Application.c_str());
					EnableWindow(hWnd, false);
					ShowWindow(hWhitelistConfWindow, SW_SHOW);
				}
			}
		}break;
		case 2:
		{
			int index = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST2), LB_GETCURSEL, 0, 0);
			if (index != LB_ERR)
			{
				int data = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST2), LB_GETITEMDATA, index, 0);
				if (data != LB_ERR && data < ExclusionsTemp.size())
				{
					hWhitelistConfWindow = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_WHITELIST_EDIT), hWnd, (DLGPROC)WhitelistConfWndProc);
					SetWindowText(hWhitelistConfWindow, L"Edit fullscreen exclusion process");
					SetWindowText(GetDlgItem(hWhitelistConfWindow, IDOK), L"Change");
					SetWindowText(GetDlgItem(hWhitelistConfWindow, IDC_EDIT_NAME), ExclusionsTemp[data].Name.c_str());
					SetWindowText(GetDlgItem(hWhitelistConfWindow, IDC_EDIT_PROCESS), ExclusionsTemp[data].Application.c_str());
					EnableWindow(hWnd, false);
					ShowWindow(hWhitelistConfWindow, SW_SHOW);
				}
			}
		}break;
		default:break;
		}
	}break;

	case APP_LISTBOX_ADD:
	{
		switch (wParam)
		{
		case 1:
		{
			hWhitelistConfWindow = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_WHITELIST_EDIT), hWnd, (DLGPROC)WhitelistConfWndProc);
			SetWindowText(hWhitelistConfWindow, L"Add whitelisted process");
			SetWindowText(GetDlgItem(hWhitelistConfWindow, IDOK), L"Add");
			EnableWindow(hWnd, false);
			ShowWindow(hWhitelistConfWindow, SW_SHOW);
		}break;
		case 2:
		{
			hWhitelistConfWindow = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_WHITELIST_EDIT), hWnd, (DLGPROC)WhitelistConfWndProc);
			SetWindowText(hWhitelistConfWindow, L"Add fullscreen exclusion process");
			SetWindowText(GetDlgItem(hWhitelistConfWindow, IDOK), L"Add");
			EnableWindow(hWnd, false);
			ShowWindow(hWhitelistConfWindow, SW_SHOW);
		}break;
		default:break;
		}
	}break;
	case APP_LISTBOX_DELETE:
	{
		switch (wParam)
		{
		case 1:
		{
			int index = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_GETCURSEL, 0, 0);
			if (index != LB_ERR)
			{
				TCHAR* text;
				int len = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_GETTEXTLEN, index, 0);
				text = new TCHAR[len + 1];
				if (SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_GETTEXT, (WPARAM)index, (LPARAM)text) != LB_ERR)
				{
					std::wstring s = L"Do you want to delete '";
					s += text;
					s += L"' from the whitelist?";
					if (MessageBox(hWnd, s.c_str(), L"Delete item", MB_YESNO | MB_ICONQUESTION) == IDYES)
					{
						int data = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_GETITEMDATA, index, 0);
						if (data != LB_ERR && data < WhitelistTemp.size())
						{
							WhitelistTemp.erase(WhitelistTemp.begin() + data);
							SendMessage(hWnd, APP_LISTBOX_REDRAW, 1, 0);
							EnableWindow(GetDlgItem(hWnd, IDOK), true);
						}
						else
						{
							MessageBox(hWnd, L"Could not delete item!", L"Error", MB_OK | MB_ICONINFORMATION);
						}
					}
				}
				else
				{
					MessageBox(hWnd, L"There was an error when managing the listbox", L"Error", MB_OK | MB_ICONINFORMATION);
				}
			}
		}break;
		case 2:
		{
			int index = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST2), LB_GETCURSEL, 0, 0);
			if (index != LB_ERR)
			{
				TCHAR* text;
				int len = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST2), LB_GETTEXTLEN, index, 0);
				text = new TCHAR[len + 1];
				if (SendMessage(GetDlgItem(hWnd, IDC_LIST2), LB_GETTEXT, (WPARAM)index, (LPARAM)text) != LB_ERR)
				{
					std::wstring s = L"Do you want to delete '";
					s += text;
					s += L"' from the fullscreen exclusions?";
					if (MessageBox(hWnd, s.c_str(), L"Delete item", MB_YESNO | MB_ICONQUESTION) == IDYES)
					{
						int data = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST2), LB_GETITEMDATA, index, 0);
						if (data != LB_ERR && data < ExclusionsTemp.size())
						{
							ExclusionsTemp.erase(ExclusionsTemp.begin() + data);
							SendMessage(hWnd, APP_LISTBOX_REDRAW, 2, 0);
							EnableWindow(GetDlgItem(hWnd, IDOK), true);
						}
						else
						{
							MessageBox(hWnd, L"Could not delete item!", L"Error", MB_OK | MB_ICONINFORMATION);
						}
					}
				}
				else
				{
					MessageBox(hWnd, L"There was an error when managing the listbox", L"Error", MB_OK | MB_ICONINFORMATION);
				}
			}

		}break;
		default:break;
		}
	}break;
	case APP_LISTBOX_REDRAW:
	{
		switch (wParam)
		{
		case 1:
		{
			int i = 0;
			SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_RESETCONTENT, 0, 0);
			for (auto& w : WhitelistTemp)
			{
				if (w.Application != L"" && w.Name != L"")
				{
					int index = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_ADDSTRING, 0, (LPARAM)(w.Name.c_str()));
					SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_SETITEMDATA, index, (LPARAM)i);
					i++;
				}
			}
			if (SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_GETCOUNT, 0, 0) > 0)
			{
				SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_SETCURSEL, 0, 0);
				ShowWindow(GetDlgItem(hWnd, IDC_SYSLINK_EDIR), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_SYSLINK_DELETE), SW_SHOW);
			}
			else
			{
				ShowWindow(GetDlgItem(hWnd, IDC_SYSLINK_EDIR), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_SYSLINK_DELETE), SW_HIDE);
			}
		}break;
		case 2:
		{
			int i = 0;
			SendMessage(GetDlgItem(hWnd, IDC_LIST2), LB_RESETCONTENT, 0, 0);
			for (auto& w : ExclusionsTemp)
			{
				if (w.Application != L"" && w.Name != L"")
				{
					int index = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST2), LB_ADDSTRING, 0, (LPARAM)(w.Name.c_str()));
					SendMessage(GetDlgItem(hWnd, IDC_LIST2), LB_SETITEMDATA, index, (LPARAM)i);
					i++;
				}
			}
			if (SendMessage(GetDlgItem(hWnd, IDC_LIST2), LB_GETCOUNT, 0, 0) > 0)
			{
				SendMessage(GetDlgItem(hWnd, IDC_LIST2), LB_SETCURSEL, 0, 0);
				ShowWindow(GetDlgItem(hWnd, IDC_SYSLINK_EDIR2), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_SYSLINK_DELETE2), SW_SHOW);
			}
			else
			{
				ShowWindow(GetDlgItem(hWnd, IDC_SYSLINK_EDIR2), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_SYSLINK_DELETE2), SW_HIDE);
			}
		}break;
		default:break;
		}
	}break;
	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case LBN_DBLCLK:
		{
			switch (LOWORD(wParam))
			{
			case IDC_LIST:
			{
				PostMessage(hWnd, APP_LISTBOX_EDIT, 1, 0);
			}break;
			case IDC_LIST2:
			{
				PostMessage(hWnd, APP_LISTBOX_EDIT, 2, 0);
			}break;
			default:break;
			}		
		}break;

		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDC_CHECK_WHITELIST:
			{
				EnableWindow(GetDlgItem(hWnd, IDC_LIST), IsDlgButtonChecked(hWnd, IDC_CHECK_WHITELIST));
				EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_ADD), IsDlgButtonChecked(hWnd, IDC_CHECK_WHITELIST));
				EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_EDIR), IsDlgButtonChecked(hWnd, IDC_CHECK_WHITELIST));
				EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_DELETE), IsDlgButtonChecked(hWnd, IDC_CHECK_WHITELIST));
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDC_CHECK_FS_EXCLUSIONS:
			{
				EnableWindow(GetDlgItem(hWnd, IDC_LIST2), IsDlgButtonChecked(hWnd, IDC_CHECK_FS_EXCLUSIONS));
				EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_ADD2), IsDlgButtonChecked(hWnd, IDC_CHECK_FS_EXCLUSIONS));
				EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_EDIR2), IsDlgButtonChecked(hWnd, IDC_CHECK_FS_EXCLUSIONS));
				EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_DELETE2), IsDlgButtonChecked(hWnd, IDC_CHECK_FS_EXCLUSIONS));
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDC_CHECK_FULLSCREEN:
			{
				EnableWindow(GetDlgItem(hWnd, IDC_CHECK_FS_EXCLUSIONS), IsDlgButtonChecked(hWnd, IDC_CHECK_FULLSCREEN));
				EnableWindow(GetDlgItem(hWnd, IDC_LIST2), IsDlgButtonChecked(hWnd, IDC_CHECK_FULLSCREEN)?IsDlgButtonChecked(hWnd, IDC_CHECK_FS_EXCLUSIONS):false);
				EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_ADD2), IsDlgButtonChecked(hWnd, IDC_CHECK_FULLSCREEN)? IsDlgButtonChecked(hWnd, IDC_CHECK_FS_EXCLUSIONS):false);
				EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_EDIR2), IsDlgButtonChecked(hWnd, IDC_CHECK_FULLSCREEN)?IsDlgButtonChecked(hWnd, IDC_CHECK_FS_EXCLUSIONS):false);
				EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_DELETE2), IsDlgButtonChecked(hWnd, IDC_CHECK_FULLSCREEN)?IsDlgButtonChecked(hWnd, IDC_CHECK_FS_EXCLUSIONS):false);
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDC_CHECK_MUTE:
			{
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDOK:
			{
				Settings.Prefs.BlankScreenWhenIdleDelay = atoi(common::narrow(common::GetWndText(GetDlgItem(hWnd, IDC_EDIT_TIME))).c_str());
				Settings.Prefs.bFullscreenCheckEnabled = !IsDlgButtonChecked(hWnd, IDC_CHECK_FULLSCREEN);
				Settings.Prefs.bIdleWhitelistEnabled = IsDlgButtonChecked(hWnd, IDC_CHECK_WHITELIST);
				Settings.Prefs.bIdleFsExclusionsEnabled = IsDlgButtonChecked(hWnd, IDC_CHECK_FS_EXCLUSIONS);
				Settings.Prefs.MuteSpeakers = IsDlgButtonChecked(hWnd, IDC_CHECK_MUTE);
				Settings.Prefs.WhiteList = WhitelistTemp;
				Settings.Prefs.FsExclusions = ExclusionsTemp;
				EndDialog(hWnd, 0);
				EnableWindow(GetParent(hWnd), true);
				EnableWindow(GetDlgItem(GetParent(hWnd), IDOK), true);
			}break;
			case IDCANCEL:
			{
				EndDialog(hWnd, 0);
				EnableWindow(GetParent(hWnd), true);
			}break;
			default:break;
			}
		}break;
		case EN_CHANGE:
		{
			switch (LOWORD(wParam))
			{
			case IDC_EDIT_TIME:
			{
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			default:break;
			}
		}break;
		default:break;
		}
	}break;
	case WM_NOTIFY:
	{
		switch (((NMHDR*)lParam)->code)
		{
		case NM_CLICK:
		{
			if (wParam == IDC_SYSLINK_INFO_1)
			{
				MessageBox(hWnd, L"Please configure the time, in minutes, without user input from keyboard, mouse or game controllers before "
					"triggering the user idle mode.", L"User idle time configuration", MB_OK | MB_ICONINFORMATION);
			}
			else if (wParam == IDC_SYSLINK_INFO_2)
			{
				MessageBox(hWnd, L"Enable to ensure that when a whitelisted process is running the user idle mode will not be triggered. This can be useful to prevent user idle mode "
					"while running a specific movie player, for example.", L"User idle mode whitelist configuration", MB_OK | MB_ICONINFORMATION);
			}
			else if (wParam == IDC_SYSLINK_INFO_3)
			{
				MessageBox(hWnd, L"Enable the fullscreen user idle mode to ensure that user idle mode triggers also when running an application or game in fullscreen."
					"\n\nAdditionally, it is possible to configure processes which are excluded from the fullscreen user idle mode detection.", L"User idle mode fullscreen configuration", MB_OK | MB_ICONINFORMATION);
			}
			else if (wParam == IDC_SYSLINK_ADD)
			{
				PostMessage(hWnd, APP_LISTBOX_ADD, 1, 0);
			}
			else if (wParam == IDC_SYSLINK_EDIR)
			{
				PostMessage(hWnd, APP_LISTBOX_EDIT, 1, 0);
			}
			else if (wParam == IDC_SYSLINK_DELETE)
			{
				PostMessage(hWnd, APP_LISTBOX_DELETE, 1, 0);
			}

			else if (wParam == IDC_SYSLINK_ADD2)
			{
				PostMessage(hWnd, APP_LISTBOX_ADD, 2, 0);
			}
			else if (wParam == IDC_SYSLINK_EDIR2)
			{
				PostMessage(hWnd, APP_LISTBOX_EDIT, 2, 0);
			}
			else if (wParam == IDC_SYSLINK_DELETE2)
			{
				PostMessage(hWnd, APP_LISTBOX_DELETE, 2, 0);
			}
		}break;
		default:break;
		}
	}break;
	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		SetTextColor(hdcStatic, COLORREF(COLOR_STATIC));
		if ((HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_WHITELIST)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_FULLSCREEN)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_FS_EXCLUSIONS))
		{
			SetBkMode(hdcStatic, TRANSPARENT);
		}
		return(INT_PTR)hBackbrush;
	}break;

	case WM_PAINT:
	{
		RECT rc = { 0 };
		GetClientRect(hWnd, &rc);
		PAINTSTRUCT ps;
		//		PAINTSTRUCT psPaint;
		InvalidateRect(hWnd, NULL, false);

		//		GetClientRect(hWnd, &rc);
		int width = rc.right;
		int height = rc.bottom;
		HDC hdc = BeginPaint(hWnd, &ps);
		HDC backbuffDC = CreateCompatibleDC(hdc);
		HBITMAP backbuffer = CreateCompatibleBitmap(hdc, width, height);
		int savedDC = SaveDC(backbuffDC);
		SelectObject(backbuffDC, backbuffer);

		FillRect(backbuffDC, &rc, (HBRUSH)hBackbrush);

		BitBlt(hdc, 0, 0, width, height, backbuffDC, 0, 0, SRCCOPY);
		RestoreDC(backbuffDC, savedDC);

		DeleteObject(backbuffer);
		DeleteDC(backbuffDC);

		ReleaseDC(hWnd, hdc);
		EndPaint(hWnd, &ps);
		return 0;
	}break;
	case WM_ERASEBKGND:
	{
		return true;
	}break;

	case WM_CLOSE:
	{
		EndDialog(hWnd, 0);
		EnableWindow(GetParent(hWnd), true);
	}break;
	case WM_DESTROY:
	{
		hUserIdleConfWindow = NULL;
	}break;
	default:
		return false;
	}
	return true;
}

//   Process messages for the user idle conf window
LRESULT CALLBACK WhitelistConfWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		SendDlgItemMessage(hWnd, IDC_EDIT_NAME, WM_SETFONT, (WPARAM)hEditfont, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_EDIT_PROCESS, WM_SETFONT, (WPARAM)hEditfont, MAKELPARAM(TRUE, 0));
	}break;

	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDOK:
			{
				std::wstring wnd = common::GetWndText(hWnd);
				std::wstring name = common::GetWndText(GetDlgItem(hWnd, IDC_EDIT_NAME));
				std::wstring proc = common::GetWndText(GetDlgItem(hWnd, IDC_EDIT_PROCESS));

				if (name.find_last_of(L"\\/") != std::string::npos)
				{
					name = name.substr(name.find_last_of(L"\\/"));
					name.erase(0, 1);
				}
				if (proc.find_last_of(L"\\/") != std::string::npos)
				{
					proc = proc.substr(proc.find_last_of(L"\\/"));
					proc.erase(0, 1);
				}

				if (name != L"" && proc != L"")
				{
					if (common::GetWndText(GetDlgItem(hWnd, IDOK)) == L"Add") // add item
					{
						settings::PROCESSLIST w;
						w.Name = name;
						w.Application = proc;
						if (wnd.find(L"fullscreen") == std::wstring::npos)
						{
							WhitelistTemp.push_back(w);
						}
						else
						{
							ExclusionsTemp.push_back(w);
						}
					}
					else //change item
					{
						if (hUserIdleConfWindow)
						{
							if (wnd.find(L"fullscreen") == std::wstring::npos)
							{
								int index = (int)SendMessage(GetDlgItem(hUserIdleConfWindow, IDC_LIST), LB_GETCURSEL, 0, 0);
								if (index != LB_ERR)
								{
									int data = (int)SendMessage(GetDlgItem(hUserIdleConfWindow, IDC_LIST), LB_GETITEMDATA, index, 0);
									if (data != LB_ERR && data < WhitelistTemp.size())
									{
										WhitelistTemp[data].Application = proc;
										WhitelistTemp[data].Name = name;
									}
								}
							}
							else
							{
								int index = (int)SendMessage(GetDlgItem(hUserIdleConfWindow, IDC_LIST2), LB_GETCURSEL, 0, 0);
								if (index != LB_ERR)
								{
									int data = (int)SendMessage(GetDlgItem(hUserIdleConfWindow, IDC_LIST2), LB_GETITEMDATA, index, 0);
									if (data != LB_ERR && data < ExclusionsTemp.size())
									{
										ExclusionsTemp[data].Application = proc;
										ExclusionsTemp[data].Name = name;
									}
								}
							}
						}
					}

					EndDialog(hWnd, 0);
					SendMessage(GetParent(hWnd), APP_LISTBOX_REDRAW, 1, 0);
					SendMessage(GetParent(hWnd), APP_LISTBOX_REDRAW, 2, 0);
					EnableWindow(GetParent(hWnd), true);
					EnableWindow(GetDlgItem(GetParent(hWnd), IDOK), true);
				}
				else
				{
					MessageBox(hWnd, L"Please ensure that both display name and process executable name are properly "
						"configured before continuing. Please note that process executable name should not include the path.",
						L"Invalid configuration", MB_OK | MB_ICONERROR);
				}
			}break;
			case IDCANCEL:
			{
				EndDialog(hWnd, 0);
				EnableWindow(GetParent(hWnd), true);
			}break;
			default:break;
			}
		}break;
		case EN_CHANGE:
		{
			switch (LOWORD(wParam))
			{
			case IDC_EDIT_NAME:
			case IDC_EDIT_PROCESS:
			{
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			default:break;
			}
		}break;
		default:break;
		}
	}break;
	case WM_NOTIFY:
	{
		switch (((NMHDR*)lParam)->code)
		{
		case NM_CLICK:
		{
			if (wParam == IDC_SYSLINK_BROWSE)
			{
				IFileOpenDialog* pfd;
				DWORD dwOptions;
				LPWSTR Path;
				IShellItem* psi;
				COMDLG_FILTERSPEC aFileTypes[] = {
					{ L"Executable files", L"*.exe" },
					{ L"All files", L"*.*" },
				};

				if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
					if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pfd)))) {
						if (SUCCEEDED(pfd->GetOptions(&dwOptions)))
						{
							pfd->SetOptions(dwOptions | FOS_FILEMUSTEXIST);
							pfd->SetFileTypes(_countof(aFileTypes), aFileTypes);
							if (SUCCEEDED(pfd->Show(hWnd)))
								if (SUCCEEDED(pfd->GetResult(&psi))) {
									if (SUCCEEDED(psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &Path)))
									{
										TCHAR buffer[MAX_PATH] = { 0 };
										GetModuleFileName(NULL, buffer, MAX_PATH);

										std::wstring path = Path;
										std::wstring exe, name;
										std::wstring::size_type pos = path.find_last_of(L"\\/");
										if (pos != std::wstring::npos)
										{
											exe = path.substr(pos);
											exe.erase(0, 1);
											pos = exe.find_last_of(L".");
											if (pos != std::wstring::npos)
											{
												name = exe.substr(0, pos);
											}
											if (common::GetWndText(GetDlgItem(hWnd, IDC_EDIT_NAME)) == L"")
												SetWindowText(GetDlgItem(hWnd, IDC_EDIT_NAME), name.c_str());
											SetWindowText(GetDlgItem(hWnd, IDC_EDIT_PROCESS), exe.c_str());
										}

										CoTaskMemFree(Path);
									}
									psi->Release();
								}
						}
						pfd->Release();
					}
					CoUninitialize();
				}
			}
		}break;
		default:break;
		}
	}break;
	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		SetTextColor(hdcStatic, COLORREF(COLOR_STATIC));
		return(INT_PTR)hBackbrush;
	}break;

	case WM_PAINT:
	{
		RECT rc = { 0 };
		GetClientRect(hWnd, &rc);
		PAINTSTRUCT ps;
		//		PAINTSTRUCT psPaint;
		InvalidateRect(hWnd, NULL, false);

		//		GetClientRect(hWnd, &rc);
		int width = rc.right;
		int height = rc.bottom;
		HDC hdc = BeginPaint(hWnd, &ps);
		HDC backbuffDC = CreateCompatibleDC(hdc);
		HBITMAP backbuffer = CreateCompatibleBitmap(hdc, width, height);
		int savedDC = SaveDC(backbuffDC);
		SelectObject(backbuffDC, backbuffer);

		FillRect(backbuffDC, &rc, (HBRUSH)hBackbrush);

		BitBlt(hdc, 0, 0, width, height, backbuffDC, 0, 0, SRCCOPY);
		RestoreDC(backbuffDC, savedDC);

		DeleteObject(backbuffer);
		DeleteDC(backbuffDC);

		ReleaseDC(hWnd, hdc);
		EndPaint(hWnd, &ps);
		return 0;
	}break;
	case WM_ERASEBKGND:
	{
		return true;
	}break;

	case WM_CLOSE:
	{
		EndDialog(hWnd, 0);
		EnableWindow(GetParent(hWnd), true);
	}break;
	case WM_DESTROY:
	{
		hWhitelistConfWindow = NULL;
	}break;
	default:
		return false;
	}
	return true;
}

//   If the application is already running, send the command line parameters to that other process
bool MessageExistingProcess(std::wstring CmdLine)
{
	std::wstring WindowTitle;
	WindowTitle = APPNAME;
	WindowTitle += L" v";
	WindowTitle += APP_VERSION;
	std::wstring sWinSearch = WindowTitle;
	HWND Other_hWnd = FindWindow(NULL, sWinSearch.c_str());
	if (Other_hWnd)
	{
		COPYDATASTRUCT cds;
		cds.cbData = CmdLine == L"" ? 0 : (DWORD)(CmdLine.size() * sizeof(WCHAR) + sizeof(WCHAR));
		cds.lpData = CmdLine == L"" ? NULL : (PVOID)CmdLine.data();
		cds.dwData = NOTIFY_NEW_COMMANDLINE;
		SendMessage(Other_hWnd, WM_COPYDATA, NULL, (LPARAM)&cds);

		return true;
	}

	return false;
}
//   Add an event in the event log. Type can be: EVENTLOG_SUCCESS, EVENTLOG_ERROR_TYPE, EVENTLOG_INFORMATION_TYPE
void SvcReportEvent(WORD Type, std::wstring string)
{
	HANDLE hEventSource;
	LPCTSTR lpszStrings[2];

	hEventSource = RegisterEventSource(NULL, APPNAME);

	if (hEventSource)
	{
		std::wstring s;
		switch (Type)
		{
		case EVENTLOG_SUCCESS:
			s = string;
			s += L" succeeded";
			break;
		case EVENTLOG_INFORMATION_TYPE:
		default:
			s = string;
			break;
		}

		lpszStrings[0] = APPNAME;
		lpszStrings[1] = s.c_str();

		ReportEvent(hEventSource,        // event log handle
			Type,               // event type
			0,                   // event category
			0,                   // event identifier
			NULL,                // no security identifier
			2,                   // size of lpszStrings array
			0,                   // no binary data
			lpszStrings,         // array of strings
			NULL);               // no binary data

		DeregisterEventSource(hEventSource);
	}
}
//   Send the commandline to the service
void CommunicateWithService(std::wstring sData)
{
	if(!pPipeClient->Send(sData))
		MessageBox(hMainWnd, L"Failed to connect to named pipe. Service may be stopped.", L"Error", MB_OK | MB_ICONEXCLAMATION);
}
void VersionCheckThread(HWND hWnd)
{
	IStream* stream;
	char buff[100];
	std::string s;
	unsigned long bytesRead;

	if (URLOpenBlockingStream(0, VERSIONCHECKLINK, &stream, 0, 0))
		return;// error

	while (true)
	{
		stream->Read(buff, 100, &bytesRead);

		if (0U == bytesRead)
			break;
		s.append(buff, bytesRead);
	};

	stream->Release();

	size_t find = s.find("\"tag_name\":", 0);
	if (find != std::string::npos)
	{
		size_t begin = s.find_first_of("0123456789", find);
		if (begin != std::string::npos)
		{
			size_t end = s.find("\"", begin);
			std::string lastver = s.substr(begin, end - begin);

			std::vector <std::string> local_ver = common::stringsplit(common::narrow(APP_VERSION), ".");
			std::vector <std::string> remote_ver = common::stringsplit(lastver, ".");

			if (local_ver.size() < 3 || remote_ver.size() < 3)
				return;
			int local_ver_major = atoi(local_ver[0].c_str());
			int local_ver_minor = atoi(local_ver[1].c_str());
			int local_ver_patch = atoi(local_ver[2].c_str());

			int remote_ver_major = atoi(remote_ver[0].c_str());
			int remote_ver_minor = atoi(remote_ver[1].c_str());
			int remote_ver_patch = atoi(remote_ver[2].c_str());

			if ((remote_ver_major > local_ver_major) ||
				(remote_ver_major == local_ver_major) && (remote_ver_minor > local_ver_minor) ||
				(remote_ver_major == local_ver_major) && (remote_ver_minor == local_ver_minor) && (remote_ver_patch > local_ver_patch))
			{
				PostMessage(hWnd, APP_NEW_VERSION, 0, 0);
			}
		}
	}
	return;
}

std::vector<settings::DISPLAY_INFO> QueryDisplays()
{
	std::string logmsg = "Enumerating active displays...";
	Log(logmsg);

	std::vector<settings::DISPLAY_INFO> targets;
	//populate targets struct with information about attached displays
	EnumDisplayMonitors(NULL, NULL, meproc, (LPARAM)&targets);
	return targets;
}
static BOOL CALLBACK meproc(HMONITOR hMonitor, HDC hdc, LPRECT lprcMonitor, LPARAM pData)
{
	if (!pData)
		return false;
	std::string logmsg;
	std::vector<settings::DISPLAY_INFO>* targets = (std::vector<settings::DISPLAY_INFO> *) pData;
	UINT32 requiredPaths, requiredModes;
	std::vector<DISPLAYCONFIG_PATH_INFO> paths;
	std::vector<DISPLAYCONFIG_MODE_INFO> modes;
	MONITORINFOEX mi;
	LONG isError = ERROR_INSUFFICIENT_BUFFER;

	ZeroMemory(&mi, sizeof(mi));
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(hMonitor, &mi);
	{
		logmsg = "Display: ";
		logmsg += common::narrow(mi.szDevice);
		logmsg += " ------------------------------";
		Log(logmsg);
	}
	isError = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, &requiredModes);
	if (isError)
	{
		logmsg = "Error! GetDisplayConfigBufferSizes() failed.";
		Log(logmsg);
		targets->clear();
		return false;
	}
	paths.resize(requiredPaths);
	modes.resize(requiredModes);

	isError = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, paths.data(), &requiredModes, modes.data(), NULL);
	if (isError)
	{
		logmsg = "Error! QueryDisplayConfig() failed.";
		Log(logmsg);
		targets->clear();
		return false;
	}
	paths.resize(requiredPaths);
	modes.resize(requiredModes);

	for (auto& p : paths)
	{

		DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
		sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
		sourceName.header.size = sizeof(sourceName);
		sourceName.header.adapterId = p.sourceInfo.adapterId;
		sourceName.header.id = p.sourceInfo.id;

		DisplayConfigGetDeviceInfo(&sourceName.header);
		{
			logmsg = "Source: ";
			logmsg += common::narrow(sourceName.viewGdiDeviceName);
			Log(logmsg);
		}
		if (wcscmp(mi.szDevice, sourceName.viewGdiDeviceName) == 0)
		{
			DISPLAYCONFIG_TARGET_DEVICE_NAME name;
			name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
			name.header.size = sizeof(name);
			name.header.adapterId = p.sourceInfo.adapterId;
			name.header.id = p.targetInfo.id;
			DisplayConfigGetDeviceInfo(&name.header);
			{
				logmsg = "Source match! Target friendly name: ";
				logmsg += common::narrow(name.monitorFriendlyDeviceName);
				Log(logmsg);
			}
			std::wstring FriendlyName = name.monitorFriendlyDeviceName;
			transform(FriendlyName.begin(), FriendlyName.end(), FriendlyName.begin(), ::tolower);
			if (FriendlyName.find(L"lg tv") != std::wstring::npos)
			{
				{
					logmsg = "Friendly name match! It is an LG TV!";
					Log(logmsg);
				}
				settings::DISPLAY_INFO di;
				di.monitorinfo = mi;
				di.hMonitor = hMonitor;
				di.hdcMonitor = hdc;
				di.rcMonitor2 = *(LPRECT)lprcMonitor;
				di.target = name;
				targets->push_back(di);
			}
		}
	}
	return true;
}
bool MessageDaemon(std::wstring cmdline)
{
	std::wstring cmd = cmdline;
	transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
	bool bIdle = (cmd.find(L"-idle")) != std::wstring::npos;
	bool bPresent = (cmd.find(L"-unidle")) != std::wstring::npos;

	if (bIdle && bPresent)
		return false;
	if (!(bIdle || bPresent))
		return false;

	std::wstring WindowTitle;
	WindowTitle = APPNAME;
	WindowTitle += L" Daemon v";
	WindowTitle += APP_VERSION;
	std::wstring sWinSearch = WindowTitle;
	HWND Daemon_hWnd = FindWindow(NULL, sWinSearch.c_str());
	if (Daemon_hWnd)
	{
		if (bIdle)
			SendMessage(Daemon_hWnd, APP_USER_IDLE_ON, NULL, NULL);
		else if (bPresent)
			SendMessage(Daemon_hWnd, APP_USER_IDLE_OFF, NULL, NULL);
	}
	return false;
}

void NamedPipeCallback(std::wstring message)
{
	return;
}

bool isSameSubnet(const char* ip1, const char* ip2, const char* subnetMask)
{
	in_addr addr1, addr2, mask;
	inet_pton(AF_INET, ip1, &addr1);
	inet_pton(AF_INET, ip2, &addr2);
	inet_pton(AF_INET, subnetMask, &mask);

	return (addr1.s_addr & mask.s_addr) == (addr2.s_addr & mask.s_addr);
}
//   Write some debug info to a log file
void Log(std::string ss)
{
	//disable 
	return;

	std::ofstream m;
	time_t rawtime;
	struct tm timeinfo;
	char buffer[80];
	std::wstring path = Settings.Prefs.DataPath;

	time(&rawtime);
	localtime_s(&timeinfo, &rawtime);

	strftime(buffer, 80, "%a %H:%M:%S > ", &timeinfo);
	puts(buffer);

	std::string s = buffer;
	s += ss;
	s += "\n";

	path += L"ui_log.txt";
	m.open(path.c_str(), std::ios::out | std::ios::app);
	if (m.is_open())
	{
		m << s.c_str();
		m.close();
	}
}
// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <dbghelp.h>
#include <fstream>
#include <memory>
#include <algorithm>
#include <vector>
#include <sstream>
#include <chrono>
#include "git.h"
#include "CodeParser.hpp"
#include "IniFile.hpp"
#include "TextConv.hpp"
#include "FileMap.hpp"
#include "FileSystem.h"
#include "FileReplacement.h"
#include "Events.h"
#include "SonicRModLoader.h"

using std::ifstream;
using std::string;
using std::wstring;
using std::unique_ptr;
using std::vector;
using std::unordered_map;
using std::chrono::duration;
using std::chrono::system_clock;
using std::milli;
using std::ratio;

/**
* Hook Sonic R's CreateFileA() import.
*/
static void HookCreateFileA(void)
{
	ULONG ulSize = 0;
	PROC pNewFunction;
	PROC pActualFunction;

	PCSTR pcszModName;

	// SADX module handle. (main executable)
	HMODULE hModule = GetModuleHandle(nullptr);
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc;

	pNewFunction = (PROC)MyCreateFileA;
	// Get the actual CreateFileA() using GetProcAddress().
	pActualFunction = GetProcAddress(GetModuleHandle(L"Kernel32.dll"), "CreateFileA");

	pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(
		hModule, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &ulSize);

	if (pImportDesc == nullptr)
		return;

	for (; pImportDesc->Name; pImportDesc++)
	{
		// get the module name
		pcszModName = (PCSTR)((PBYTE)hModule + pImportDesc->Name);

		// check if the module is kernel32.dll
		if (pcszModName != nullptr && _stricmp(pcszModName, "Kernel32.dll") == 0)
		{
			// get the module
			PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)((PBYTE)hModule + pImportDesc->FirstThunk);

			for (; pThunk->u1.Function; pThunk++)
			{
				PROC* ppfn = (PROC*)&pThunk->u1.Function;
				if (*ppfn == pActualFunction)
				{
					// Found CreateFileA().
					DWORD dwOldProtect = 0;
					VirtualProtect(ppfn, sizeof(pNewFunction), PAGE_WRITECOPY, &dwOldProtect);
					WriteData(ppfn, pNewFunction);
					VirtualProtect(ppfn, sizeof(pNewFunction), dwOldProtect, &dwOldProtect);
					// FIXME: Would it be listed multiple times?
					break;
				} // Function that we are looking for
			}
		}
	}
}

// Code Parser.
static CodeParser codeParser;
using FrameRatio = duration<double, ratio<1, 30>>;
static auto frame_start = system_clock::now();
static auto frame_ratio = FrameRatio(1);
static duration<double, milli> present_time = {};

static void __cdecl ProcessCodes(int fps)
{
	codeParser.processCodeList();
	RaiseEvents(modFrameEvents);
	int v2 = abs(GetTime() - FrameStartTime);
	while (system_clock::now() - frame_start < frame_ratio);
	FrameEndTime = 1000 / (v2 + 1);
	frame_start = system_clock::now();
}

static bool dbgConsole;
// File for logging debugging output.
static FILE *dbgFile = nullptr;

/**
* Sonic R Debug Output function.
* @param Format Format string.
* @param args Arguments.
* @return Return value from vsnprintf().
*/
static int __cdecl SonicRDebugOutput(const char *Format, ...)
{
	va_list ap;
	va_start(ap, Format);
	int result = vsnprintf(nullptr, 0, Format, ap) + 1;
	va_end(ap);
	char *buf = new char[result + 1];
	va_start(ap, Format);
	result = vsnprintf(buf, result + 1, Format, ap);
	va_end(ap);

	// Console output.
	if (dbgConsole)
	{
		fputs(buf, stdout);
		fflush(stdout);
	}

	// File output.
	if (dbgFile)
	{
		fputs(buf, dbgFile);
		fflush(dbgFile);
	}

	delete[] buf;
	return result;
}

void *loc_42DDF0 = (void*)0x42DDF0;
__declspec(naked) void MusicPatch()
{
	__asm
	{
		add esp, 8
		mov eax, [ebp + 0C8h]
		mov eax, [eax + ebx * 8]
		jmp loc_42DDF0
	}
}

StdcallFunctionPointer(int, _WinMain, (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd), 0x4330A0);
int __stdcall InitMods(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	FILE *f_ini = _wfopen(L"mods\\SonicRModLoader.ini", L"r");
	if (!f_ini)
	{
		MessageBox(nullptr, L"mods\\SonicRModLoader.ini could not be read!", L"Sonic R Mod Loader", MB_ICONWARNING);
		return _WinMain(hInstance, hPrevInstance, lpCmdLine, nShowCmd);
	}
	unique_ptr<IniFile> ini(new IniFile(f_ini));
	fclose(f_ini);

	HookCreateFileA();

	// Get exe's path and filename.
	wchar_t pathbuf[MAX_PATH];
	GetModuleFileName(nullptr, pathbuf, MAX_PATH);
	wstring exepath(pathbuf);
	wstring exefilename;
	string::size_type slash_pos = exepath.find_last_of(L"/\\");
	if (slash_pos != string::npos)
	{
		exefilename = exepath.substr(slash_pos + 1);
		if (slash_pos > 0)
			exepath = exepath.substr(0, slash_pos);
	}

	// Convert the EXE filename to lowercase.
	transform(exefilename.begin(), exefilename.end(), exefilename.begin(), ::towlower);

	// Process the main Mod Loader settings.
	const IniGroup *settings = ini->getGroup("");

	if (settings->getBool("DebugConsole"))
	{
		// Enable the debug console.
		// TODO: setvbuf()?
		AllocConsole();
		SetConsoleTitle(L"Sonic R Mod Loader output");
		freopen("CONOUT$", "wb", stdout);
		dbgConsole = true;
	}

	if (settings->getBool("DebugFile"))
	{
		// Enable debug logging to a file.
		// dbgFile will be nullptr if the file couldn't be opened.
		dbgFile = _wfopen(L"mods\\SonicRModLoader.log", L"a+");
	}

	// Is any debug method enabled?
	if (dbgConsole || dbgFile)
	{
		WriteJump(PrintDebug, SonicRDebugOutput);
		WriteData((void**)0x404BD3, (void*)&SonicRDebugOutput);
		// There's a couple other functions that were compiled out and got merged with the debug printing function.
		// This code replaces those calls with nops, otherwise the game would crash on invalid pointers.
		char jmpnop[5] = { 0x90u, 0x90u, 0x90u, 0x90u, 0x90u };
		WriteData((void*)0x43AB3D, jmpnop);
		WriteData((void*)0x43A567, jmpnop);
		WriteData((void*)0x43A843, jmpnop);
		WriteData((void*)0x41489E, jmpnop);
		WriteData((void*)0x41534D, jmpnop);
		WriteData((void*)0x41DA7B, jmpnop);
		PrintDebug("Sonic R Mod Loader (API version %d), built " __TIMESTAMP__ "\n",
			ModLoaderVer);
#ifdef MODLOADER_GIT_VERSION
#ifdef MODLOADER_GIT_DESCRIBE
		PrintDebug("%s, %s\n", MODLOADER_GIT_VERSION, MODLOADER_GIT_DESCRIBE);
#else /* !MODLOADER_GIT_DESCRIBE */
		PrintDebug("%s\n", MODLOADER_GIT_VERSION);
#endif /* MODLOADER_GIT_DESCRIBE */
#endif /* MODLOADER_GIT_VERSION */
	}

	*(int*)0x502F1C = 1;
	WriteJump((void*)0x42DD4D, MusicPatch);

	// Map of files to replace and/or swap.
	// This is done with a second map instead of sadx_fileMap directly
	// in order to handle multiple mods.
	unordered_map<string, string> filereplaces;

	vector<std::pair<ModInitFunc, string>> initfuncs;
	vector<std::pair<string, string>> errors;

	PrintDebug("Loading mods...\n");
	for (unsigned int i = 1; i <= 999; i++)
	{
		char key[8];
		snprintf(key, sizeof(key), "Mod%u", i);
		if (!settings->hasKey(key))
			break;

		const string mod_dirA = "mods\\" + settings->getString(key);
		const wstring mod_dir = L"mods\\" + settings->getWString(key);
		const wstring mod_inifile = mod_dir + L"\\mod.ini";
		FILE *f_mod_ini = _wfopen(mod_inifile.c_str(), L"r");
		if (!f_mod_ini)
		{
			PrintDebug("Could not open file mod.ini in \"mods\\%s\".\n", mod_dirA.c_str());
			errors.push_back(std::pair<string, string>(mod_dirA, "mod.ini missing"));
			continue;
		}
		unique_ptr<IniFile> ini_mod(new IniFile(f_mod_ini));
		fclose(f_mod_ini);

		const IniGroup *const modinfo = ini_mod->getGroup("");
		const string mod_nameA = modinfo->getString("Name");
		PrintDebug("%u. %s\n", i, mod_nameA.c_str());

		if (ini_mod->hasGroup("IgnoreFiles"))
		{
			const IniGroup *group = ini_mod->getGroup("IgnoreFiles");
			auto data = group->data();
			for (unordered_map<string, string>::const_iterator iter = data->begin();
				iter != data->end(); ++iter)
			{
				fileMap.addIgnoreFile(iter->first, i);
				PrintDebug("Ignored file: %s\n", iter->first.c_str());
			}
		}

		if (ini_mod->hasGroup("ReplaceFiles"))
		{
			const IniGroup *group = ini_mod->getGroup("ReplaceFiles");
			auto data = group->data();
			for (unordered_map<string, string>::const_iterator iter = data->begin();
				iter != data->end(); ++iter)
			{
				filereplaces[FileMap::normalizePath(iter->first)] =
					FileMap::normalizePath(iter->second);
			}
		}

		if (ini_mod->hasGroup("SwapFiles"))
		{
			const IniGroup *group = ini_mod->getGroup("SwapFiles");
			auto data = group->data();
			for (unordered_map<string, string>::const_iterator iter = data->begin();
				iter != data->end(); ++iter)
			{
				filereplaces[FileMap::normalizePath(iter->first)] =
					FileMap::normalizePath(iter->second);
				filereplaces[FileMap::normalizePath(iter->second)] =
					FileMap::normalizePath(iter->first);
			}
		}

		// Check for Data replacements.
		const string modSysDirA = mod_dirA + "\\files";
		if (DirectoryExists(modSysDirA))
			fileMap.scanFolder(modSysDirA, i);

		// Check if the mod has a DLL file.
		if (modinfo->hasKeyNonEmpty("DLLFile"))
		{
			// Prepend the mod directory.
			// TODO: SetDllDirectory().
			wstring dll_filename = mod_dir + L'\\' + modinfo->getWString("DLLFile");
			HMODULE module = LoadLibrary(dll_filename.c_str());
			if (module == nullptr)
			{
				DWORD error = GetLastError();
				LPSTR buffer;
				size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buffer, 0, nullptr);

				string message(buffer, size);
				LocalFree(buffer);

				const string dll_filenameA = UTF16toMBS(dll_filename, CP_ACP);
				PrintDebug("Failed loading mod DLL \"%s\": %s\n", dll_filenameA.c_str(), message.c_str());
				errors.push_back(std::pair<string, string>(mod_nameA, "DLL error - " + message));
			}
			else
			{
				const ModInfo *info = (const ModInfo *)GetProcAddress(module, "SonicRModInfo");
				if (info)
				{
					const ModInitFunc init = (const ModInitFunc)GetProcAddress(module, "Init");
					if (init)
						initfuncs.push_back({ init, mod_dirA });
					const PatchList *patches = (const PatchList *)GetProcAddress(module, "Patches");
					if (patches)
						for (int j = 0; j < patches->Count; j++)
							WriteData(patches->Patches[j].address, patches->Patches[j].data, patches->Patches[j].datasize);
					const PointerList *jumps = (const PointerList *)GetProcAddress(module, "Jumps");
					if (jumps)
						for (int j = 0; j < jumps->Count; j++)
							WriteJump(jumps->Pointers[j].address, jumps->Pointers[j].data);
					const PointerList *calls = (const PointerList *)GetProcAddress(module, "Calls");
					if (calls)
						for (int j = 0; j < calls->Count; j++)
							WriteCall(calls->Pointers[j].address, calls->Pointers[j].data);
					const PointerList *pointers = (const PointerList *)GetProcAddress(module, "Pointers");
					if (pointers)
						for (int j = 0; j < pointers->Count; j++)
							WriteData((void **)pointers->Pointers[j].address, pointers->Pointers[j].data);
					RegisterEvent(modFrameEvents, module, "OnFrame");
				}
				else
				{
					const string dll_filenameA = UTF16toMBS(dll_filename, CP_ACP);
					PrintDebug("File \"%s\" is not a valid mod file.\n", dll_filenameA.c_str());
					errors.push_back(std::pair<string, string>(mod_nameA, "Not a valid mod file."));
				}
			}
		}
	}

	if (!errors.empty())
	{
		std::stringstream message;
		message << "The following mods didn't load correctly:" << std::endl;

		for (auto& i : errors)
			message << std::endl << i.first << ": " << i.second;

		MessageBoxA(nullptr, message.str().c_str(), "Mods failed to load", MB_OK | MB_ICONERROR);
	}

	// Replace filenames. ("ReplaceFiles", "SwapFiles")
	for (auto iter = filereplaces.cbegin(); iter != filereplaces.cend(); ++iter)
	{
		fileMap.addReplaceFile(iter->first, iter->second);
	}

	for (unsigned int i = 0; i < initfuncs.size(); i++)
		initfuncs[i].first(initfuncs[i].second.c_str());

	PrintDebug("Finished loading mods\n");

	// Check for patches.
	ifstream patches_str("mods\\Patches.dat", ifstream::binary);
	if (patches_str.is_open())
	{
		CodeParser patchParser;
		static const char codemagic[6] = { 'c', 'o', 'd', 'e', 'v', '5' };
		char buf[sizeof(codemagic)];
		patches_str.read(buf, sizeof(buf));
		if (!memcmp(buf, codemagic, sizeof(codemagic)))
		{
			int codecount_header;
			patches_str.read((char*)&codecount_header, sizeof(codecount_header));
			PrintDebug("Loading %d patches...\n", codecount_header);
			patches_str.seekg(0);
			int codecount = patchParser.readCodes(patches_str);
			if (codecount >= 0)
			{
				PrintDebug("Loaded %d patches.\n", codecount);
				patchParser.processCodeList();
			}
			else
			{
				PrintDebug("ERROR loading patches: ");
				switch (codecount)
				{
				case -EINVAL:
					PrintDebug("Patch file is not in the correct format.\n");
					break;
				default:
					PrintDebug("%s\n", strerror(-codecount));
					break;
				}
			}
		}
		else
		{
			PrintDebug("Patch file is not in the correct format.\n");
		}
		patches_str.close();
	}

	// Check for codes.
	ifstream codes_str("mods\\Codes.dat", ifstream::binary);
	if (codes_str.is_open())
	{
		static const char codemagic[6] = { 'c', 'o', 'd', 'e', 'v', '5' };
		char buf[sizeof(codemagic)];
		codes_str.read(buf, sizeof(buf));
		if (!memcmp(buf, codemagic, sizeof(codemagic)))
		{
			int codecount_header;
			codes_str.read((char*)&codecount_header, sizeof(codecount_header));
			PrintDebug("Loading %d codes...\n", codecount_header);
			codes_str.seekg(0);
			int codecount = codeParser.readCodes(codes_str);
			if (codecount >= 0)
			{
				PrintDebug("Loaded %d codes.\n", codecount);
				codeParser.processCodeList();
			}
			else
			{
				PrintDebug("ERROR loading codes: ");
				switch (codecount)
				{
				case -EINVAL:
					PrintDebug("Code file is not in the correct format.\n");
					break;
				default:
					PrintDebug("%s\n", strerror(-codecount));
					break;
				}
			}
		}
		else
		{
			PrintDebug("Code file is not in the correct format.\n");
		}
		codes_str.close();
	}

	WriteJump(FrameDelay, ProcessCodes);

	return _WinMain(hInstance, hPrevInstance, lpCmdLine, nShowCmd);
}

static const char verchk[] = "Sonic R";
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		if (memcmp(verchk, (const char *)0x45F664, sizeof(verchk)) != 0)
			MessageBox(nullptr, L"The mod loader was not designed for this version of the game. You will need the 2004 version of Sonic R to use mods.", L"Sonic R Mod Loader", MB_ICONWARNING);
		else
			WriteCall((void*)0x449E92, InitMods);
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}


#include "Includes.hpp"

const char* cConsole = "[sneakyevil.eu] NVIDIA API Tools";
void ToggleConsoleVisibility()
{
    static bool bHidden;
    ShowWindow(nGlobal::hWindowConsole, bHidden);
    bHidden = !bHidden;
}

void ConsoleHotkey()
{
    RegisterHotKey(0, 1, MOD_ALT | MOD_NOREPEAT, VK_F2);

    while (1)
    {
        MSG msg = { 0 };
        while (GetMessageA(&msg, 0, 0, 0) != 0)
        {
            if (msg.message == WM_HOTKEY) ToggleConsoleVisibility();
        }
        Sleep(1000);
    }
}

std::string HWND2EXE(HWND hInput)
{
    static DWORD dPID;
    GetWindowThreadProcessId(hInput, &dPID);
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 pe32; pe32.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnap, &pe32))
        {
            while (Process32Next(hSnap, &pe32))
            {
                if (pe32.th32ProcessID == dPID)
                {
                    CloseHandle(hSnap);
                    return pe32.szExeFile;
                }
            }
        }
        CloseHandle(hSnap);
    }
    return "";
}

void SetDVCLevel(int iLevel)
{
    static int iOldLevel = -1;
    if (iLevel == iOldLevel) return;
    iOldLevel = iLevel;
    (*nGlobal::nAPI::SetDVCLevel)(nGlobal::nAPI::iHandle, 0, static_cast<int>(iLevel * 1.26)); // Since 63 is 100% vibrance and we have settings between 0 - 50 this is "perfect" math for that.
}

void WorkingWindow()
{
    HWND hCurrentWindow = nullptr;
    HWND hCurrentCheckWindow;
    std::string sWindowName;
    while (1)
    {
        hCurrentCheckWindow = GetForegroundWindow();
        if (hCurrentWindow != hCurrentCheckWindow)
        {
            hCurrentWindow = hCurrentCheckWindow;
            sWindowName = HWND2EXE(hCurrentWindow);
            CSettings TempSettings;
            TempSettings.iVibrance = 0;
            if (!nGlobal::vSettings.empty())
            {
                for (CSettings* SettingsInfo : nGlobal::vSettings)
                {
                    if (sWindowName.find(SettingsInfo->sName) != std::string::npos)
                    {
                        TempSettings = *SettingsInfo;
                        break;
                    }
                }
            }
            SetDVCLevel(TempSettings.iVibrance);
        }
        Sleep(5000); // Minimalize cpu usage.
    }
}

#define GREEN   10
#define RED     12
#define YELLOW  14
#define WHITE   15
void PrintfColor(WORD wColor, const char* pFormat, ...)
{
    SetConsoleTextAttribute(nGlobal::hOutputConsole, wColor);
    char cTemp[1024];
    char *pArgs;
    va_start(pArgs, pFormat);
    _vsnprintf_s(cTemp, sizeof(cTemp) - 1, pFormat, pArgs);
    va_end(pArgs);
    cTemp[sizeof(cTemp) - 1] = 0;
    printf(cTemp);
}

int main()
{
    CreateEventA(0, 0, 0, "NVAPIT00LS");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxA(0, "Application is already running!", cConsole, MB_OK | MB_ICONERROR);
        return 0;
    }
    AllocConsole();
    SetConsoleTitleA(cConsole);
    nGlobal::hOutputConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    nGlobal::hWindowConsole = FindWindowA(0, cConsole);
    ToggleConsoleVisibility();

    std::thread tConsoleHotkey(ConsoleHotkey);

    nGlobal::nAPI::hMod = LoadLibraryA("nvapi.dll");
    if (!nGlobal::nAPI::hMod)
    {
        MessageBoxA(0, "Couldn't load \"nvapi.dll\".", cConsole, MB_OK | MB_ICONERROR);
        return 0;
    }

    std::ifstream iFile("settings.json");
    if (iFile.is_open())
    {
        nlohmann::json jData;
        // Ugly try & catch when parsing fails.
        try 
        {
            jData = nlohmann::json::parse(iFile);
        }
        catch (...) 
        {

        }
        if (!jData.is_discarded() && !jData["Settings"].empty())
        {
            int iCount = 0;
            for (auto iT = jData["Settings"].begin(); iT != jData["Settings"].end(); ++iT)
            {
                CSettings* NewSettings = new CSettings;
                GetSettings(NewSettings->sName, jData, iCount, "name");
                GetSettings(NewSettings->iVibrance, jData, iCount, "vibrance");
                if (NewSettings->iVibrance != -1) NewSettings->iVibrance = std::clamp(NewSettings->iVibrance, 0, 50);
                nGlobal::vSettings.push_back(NewSettings);
                iCount++;
            }
        }
        iFile.close();
    }
   
    PrintfColor(WHITE, "[*] Loaded Settings:");
    if (nGlobal::vSettings.empty())  PrintfColor(RED, "\n\n[-] Settings are empty or broken json?");
    else
    {
        for (CSettings* SettingsInfo : nGlobal::vSettings)
        {
            printf("\n\n");
            PrintfColor(GREEN, "[+] ");
            PrintfColor(WHITE, "%s:\n", SettingsInfo->sName.c_str());
            if (SettingsInfo->iVibrance >= 0) PrintfColor(GREEN, "    [+] Vibrance: %i", SettingsInfo->iVibrance);
        }
    }

    // Very bad codenz.
    nGlobal::nAPI::QueryInterface = tNVAPI_QueryInterface(GetProcAddress(nGlobal::nAPI::hMod, "nvapi_QueryInterface"));
    (*tNVAPI_EnumNvidiaDisplayHandle_t((*nGlobal::nAPI::QueryInterface)(0x9ABDD40D)))(0, &nGlobal::nAPI::iHandle); // Hardcoded monitor index 0, so better make loop.
    nGlobal::nAPI::SetDVCLevel = tNVAPI_SetDVCLevel((*nGlobal::nAPI::QueryInterface)(0x172409B4));
    nGlobal::nAPI::nInfo.version = sizeof(NV_DISPLAY_DVC_INFO) | 0x10000;
    (*tNVAPI_GetDVCInfo((*nGlobal::nAPI::QueryInterface)(0x4085DE45)))(nGlobal::nAPI::iHandle, 0, &nGlobal::nAPI::nInfo);

    std::thread tWorkingWindow(WorkingWindow);
    tWorkingWindow.join();
    return 0;
}
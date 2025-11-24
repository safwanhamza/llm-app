/*
 * WinSysMon.cpp
 * * A comprehensive Text-Based System Monitor for Windows 10/11.
 * * FEATURES:
 * - Real-time Process Enumeration (PID, Name, Threads, Memory, Priority)
 * - Service Enumeration (Name, Display Name, Status)
 * - System Performance Monitoring (Global CPU %, RAM Usage)
 * - Process Termination Capability
 * - Module (DLL) Inspection
 * - Double-Buffered Console UI (Flicker-free)
 * - Event Logging
 * * COMPILATION (MSVC):
 * cl WinSysMon.cpp /EHsc /std:c++17 /O2
 * * COMPILATION (MinGW):
 * g++ WinSysMon.cpp -o WinSysMon.exe -std=c++17 -lpdh -ladvapi32 -luser32
 * * AUTHOR: Gemini (AI)
 * DATE: 2023
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define _WIN32_WINNT 0x0A00 // Target Windows 10

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <tchar.h>
#include <strsafe.h>

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <deque>

// Link necessary libraries automatically (MSVC specific)
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "psapi.lib")

// ==========================================
// UTILITY & HELPERS
// ==========================================

namespace Utils {
    // Convert wstring to string for simple display needs
    std::string WideToString(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    // Format bytes to human readable string (KB, MB, GB)
    std::wstring FormatBytes(SIZE_T bytes) {
        const double kb = 1024.0;
        const double mb = kb * 1024.0;
        const double gb = mb * 1024.0;

        std::wstringstream ss;
        ss << std::fixed << std::setprecision(2);

        if (bytes > gb) ss << (bytes / gb) << L" GB";
        else if (bytes > mb) ss << (bytes / mb) << L" MB";
        else if (bytes > kb) ss << (bytes / kb) << L" KB";
        else ss << bytes << L" B";

        return ss.str();
    }

    std::wstring GetErrorString(DWORD errorMessageID) {
        LPWSTR messageBuffer = nullptr;
        size_t size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, NULL);
        
        std::wstring message(messageBuffer, size);
        LocalFree(messageBuffer);
        return message;
    }
}

// ==========================================
// LOGGER SUBSYSTEM
// ==========================================

enum class LogLevel { INFO, WARNING, ERR, DEBUG };

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::wstring message;
};

class Logger {
private:
    std::deque<LogEntry> logs;
    const size_t maxLogs = 50;
    std::mutex logMutex;

public:
    void Log(LogLevel level, const std::wstring& msg) {
        std::lock_guard<std::mutex> lock(logMutex);
        logs.push_front({ std::chrono::system_clock::now(), level, msg });
        if (logs.size() > maxLogs) logs.pop_back();
    }

    std::vector<LogEntry> GetRecentLogs(size_t count = 10) {
        std::lock_guard<std::mutex> lock(logMutex);
        size_t n = std::min(count, logs.size());
        return std::vector<LogEntry>(logs.begin(), logs.begin() + n);
    }
};

static Logger g_Logger; // Global logger instance

// ==========================================
// DATA STRUCTURES
// ==========================================

struct ProcessData {
    DWORD pid;
    DWORD parentPid;
    DWORD threadCount;
    DWORD priorityClass;
    SIZE_T workingSetSize; // Memory in bytes
    std::wstring name;
    std::wstring user;
};

struct ServiceData {
    std::wstring serviceName;
    std::wstring displayName;
    DWORD status; // e.g., SERVICE_RUNNING
};

struct ModuleData {
    std::wstring moduleName;
    std::wstring modulePath;
    HMODULE hModule;
    DWORD size;
};

// ==========================================
// SYSTEM MONITOR ENGINE
// ==========================================

class SystemMonitor {
private:
    PDH_HQUERY cpuQuery;
    PDH_HCOUNTER cpuTotal;
    bool pdhInitialized = false;

public:
    SystemMonitor() {
        InitializePDH();
    }

    ~SystemMonitor() {
        if (pdhInitialized) {
            PdhCloseQuery(cpuQuery);
        }
    }

    void InitializePDH() {
        if (PdhOpenQuery(NULL, NULL, &cpuQuery) == ERROR_SUCCESS) {
            // Add counter for total processor time
            // \Processor(_Total)\% Processor Time
            if (PdhAddCounter(cpuQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal) == ERROR_SUCCESS) {
                PdhCollectQueryData(cpuQuery);
                pdhInitialized = true;
            } else {
                g_Logger.Log(LogLevel::ERR, L"Failed to add PDH counter.");
            }
        } else {
            g_Logger.Log(LogLevel::ERR, L"Failed to open PDH query.");
        }
    }

    double GetCpuUsage() {
        if (!pdhInitialized) return 0.0;

        PDH_FMT_COUNTERVALUE counterVal;
        PdhCollectQueryData(cpuQuery);
        
        DWORD type;
        if (PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, &type, &counterVal) == ERROR_SUCCESS) {
            return counterVal.doubleValue;
        }
        return 0.0;
    }

    void GetMemoryStatus(DWORD& percent, SIZE_T& total, SIZE_T& avail) {
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        percent = memInfo.dwMemoryLoad;
        total = memInfo.ullTotalPhys;
        avail = memInfo.ullAvailPhys;
    }

    std::vector<ProcessData> EnumProcesses() {
        std::vector<ProcessData> procs;
        HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hProcessSnap == INVALID_HANDLE_VALUE) {
            g_Logger.Log(LogLevel::ERR, L"Failed to snapshot processes.");
            return procs;
        }

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (!Process32First(hProcessSnap, &pe32)) {
            CloseHandle(hProcessSnap);
            return procs;
        }

        do {
            ProcessData pd;
            pd.pid = pe32.th32ProcessID;
            pd.parentPid = pe32.th32ParentProcessID;
            pd.threadCount = pe32.cntThreads;
            pd.priorityClass = pe32.pcPriClassBase;
            pd.name = pe32.szExeFile;
            pd.workingSetSize = 0; // Filled below

            // Open process to get memory info
            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pd.pid);
            if (hProc) {
                PROCESS_MEMORY_COUNTERS_EX pmc;
                if (GetProcessMemoryInfo(hProc, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                    pd.workingSetSize = pmc.WorkingSetSize;
                }
                CloseHandle(hProc);
            }

            procs.push_back(pd);

        } while (Process32Next(hProcessSnap, &pe32));

        CloseHandle(hProcessSnap);
        
        // Sort by PID descending usually, but let's do by Memory Usage
        std::sort(procs.begin(), procs.end(), [](const ProcessData& a, const ProcessData& b) {
            return a.workingSetSize > b.workingSetSize;
        });

        return procs;
    }

    std::vector<ServiceData> EnumServices() {
        std::vector<ServiceData> services;
        SC_HANDLE hScManager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
        if (!hScManager) return services;

        DWORD bytesNeeded = 0;
        DWORD count = 0;
        DWORD resumeHandle = 0;

        EnumServicesStatusEx(hScManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, 
            NULL, 0, &bytesNeeded, &count, &resumeHandle, NULL);

        if (GetLastError() != ERROR_MORE_DATA) {
            CloseServiceHandle(hScManager);
            return services;
        }

        std::vector<BYTE> buffer(bytesNeeded);
        LPENUM_SERVICE_STATUS_PROCESS pServices = reinterpret_cast<LPENUM_SERVICE_STATUS_PROCESS>(buffer.data());

        if (EnumServicesStatusEx(hScManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
            buffer.data(), bytesNeeded, &bytesNeeded, &count, &resumeHandle, NULL)) {
            
            for (DWORD i = 0; i < count; ++i) {
                ServiceData sd;
                sd.serviceName = pServices[i].lpServiceName;
                sd.displayName = pServices[i].lpDisplayName;
                sd.status = pServices[i].ServiceStatusProcess.dwCurrentState;
                services.push_back(sd);
            }
        }

        CloseServiceHandle(hScManager);
        return services;
    }

    bool KillProcess(DWORD pid) {
        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProc == NULL) {
            g_Logger.Log(LogLevel::ERR, L"Failed to open process for termination: " + std::to_wstring(pid));
            return false;
        }
        
        bool result = TerminateProcess(hProc, 1);
        CloseHandle(hProc);
        
        if (result) {
            g_Logger.Log(LogLevel::INFO, L"Terminated process: " + std::to_wstring(pid));
        } else {
            g_Logger.Log(LogLevel::ERR, L"Failed to terminate process.");
        }
        return result;
    }

    std::vector<ModuleData> GetProcessModules(DWORD pid) {
        std::vector<ModuleData> modules;
        HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (hModuleSnap == INVALID_HANDLE_VALUE) return modules;

        MODULEENTRY32 me32;
        me32.dwSize = sizeof(MODULEENTRY32);

        if (Module32First(hModuleSnap, &me32)) {
            do {
                ModuleData md;
                md.moduleName = me32.szModule;
                md.modulePath = me32.szExePath;
                md.hModule = me32.hModule;
                md.size = me32.modBaseSize;
                modules.push_back(md);
            } while (Module32Next(hModuleSnap, &me32));
        }
        CloseHandle(hModuleSnap);
        return modules;
    }
};

// ==========================================
// CONSOLE UI ENGINE (Double Buffered)
// ==========================================

class ConsoleUI {
private:
    HANDLE hOut;
    HANDLE hIn;
    SMALL_RECT windowRect;
    int width;
    int height;
    CHAR_INFO* buffer;
    COORD bufferSize;
    COORD bufferCoord;
    SMALL_RECT writeRegion;

    // Colors
    const WORD COL_DEFAULT = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    const WORD COL_HEADER = FOREGROUND_GREEN | FOREGROUND_INTENSITY | BACKGROUND_BLUE;
    const WORD COL_HIGHLIGHT = BACKGROUND_GREEN | FOREGROUND_BLACK;
    const WORD COL_WARNING = FOREGROUND_RED | FOREGROUND_INTENSITY;
    const WORD COL_DIM = FOREGROUND_INTENSITY;

public:
    ConsoleUI() {
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        hIn = GetStdHandle(STD_INPUT_HANDLE);
        
        // Enable mouse input
        DWORD mode;
        GetConsoleMode(hIn, &mode);
        SetConsoleMode(hIn, mode | ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT);

        // Hide Cursor
        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(hOut, &cursorInfo);
        cursorInfo.bVisible = FALSE;
        SetConsoleCursorInfo(hOut, &cursorInfo);

        UpdateSize();
        buffer = new CHAR_INFO[width * height];
    }

    ~ConsoleUI() {
        delete[] buffer;
    }

    void UpdateSize() {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hOut, &csbi);
        width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        
        windowRect = {0, 0, (SHORT)(width - 1), (SHORT)(height - 1)};
        bufferSize = {(SHORT)width, (SHORT)height};
        bufferCoord = {0, 0};
        writeRegion = windowRect;
    }

    void Clear(WORD attr = 0) {
        if (attr == 0) attr = COL_DEFAULT;
        for (int i = 0; i < width * height; ++i) {
            buffer[i].Char.UnicodeChar = L' ';
            buffer[i].Attributes = attr;
        }
    }

    void Write(int x, int y, const std::wstring& text, WORD attr = 0) {
        if (attr == 0) attr = COL_DEFAULT;
        for (size_t i = 0; i < text.length(); ++i) {
            if (x + i >= width || y >= height || x + i < 0 || y < 0) continue;
            int idx = y * width + (x + i);
            buffer[idx].Char.UnicodeChar = text[i];
            buffer[idx].Attributes = attr;
        }
    }

    // Helper to draw a box
    void DrawBox(int x, int y, int w, int h, WORD attr = 0) {
        if (attr == 0) attr = COL_DEFAULT;
        // Corners
        Write(x, y, L"\x250C", attr); // Top Left
        Write(x + w - 1, y, L"\x2510", attr); // Top Right
        Write(x, y + h - 1, L"\x2514", attr); // Bot Left
        Write(x + w - 1, y + h - 1, L"\x2518", attr); // Bot Right
        
        // Edges
        for (int i = 1; i < w - 1; ++i) {
            Write(x + i, y, L"\x2500", attr);
            Write(x + i, y + h - 1, L"\x2500", attr);
        }
        for (int i = 1; i < h - 1; ++i) {
            Write(x, y + i, L"\x2502", attr);
            Write(x + w - 1, y + i, L"\x2502", attr);
        }
    }

    void Render() {
        WriteConsoleOutputW(hOut, buffer, bufferSize, bufferCoord, &writeRegion);
    }

    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    HANDLE GetInputHandle() const { return hIn; }
};

// ==========================================
// APPLICATION LOGIC
// ==========================================

enum class AppState {
    PROCESS_LIST,
    SERVICE_LIST,
    MODULE_VIEW
};

class Application {
private:
    bool running = true;
    ConsoleUI ui;
    SystemMonitor monitor;
    AppState state = AppState::PROCESS_LIST;
    
    // Data Caches
    std::vector<ProcessData> processes;
    std::vector<ServiceData> services;
    std::vector<ModuleData> modules;
    
    // State management
    int selectedIndex = 0;
    int scrollOffset = 0;
    DWORD selectedPid = 0; // For module view
    
    // Timers
    std::chrono::steady_clock::time_point lastUpdate;
    int updateIntervalMs = 1000;

public:
    void Run() {
        g_Logger.Log(LogLevel::INFO, L"WinSysMon Started.");
        
        while (running) {
            ProcessInput();
            UpdateData();
            Draw();
            std::this_thread::sleep_for(std::chrono::milliseconds(30)); // Cap frame rate
        }
    }

private:
    void ProcessInput() {
        DWORD eventsAvailable;
        GetNumberOfConsoleInputEvents(ui.GetInputHandle(), &eventsAvailable);
        
        if (eventsAvailable == 0) return;

        INPUT_RECORD ir[10];
        DWORD read;
        ReadConsoleInput(ui.GetInputHandle(), ir, 10, &read);

        for (DWORD i = 0; i < read; ++i) {
            if (ir[i].EventType == KEY_EVENT && ir[i].Event.KeyEvent.bKeyDown) {
                WORD vk = ir[i].Event.KeyEvent.wVirtualKeyCode;
                
                switch (vk) {
                    case VK_ESCAPE:
                        if (state == AppState::MODULE_VIEW) {
                            state = AppState::PROCESS_LIST;
                        } else {
                            running = false;
                        }
                        break;
                    case VK_UP:
                        if (selectedIndex > 0) {
                            selectedIndex--;
                            if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
                        }
                        break;
                    case VK_DOWN:
                        {
                            int maxItems = GetItemCount();
                            if (selectedIndex < maxItems - 1) {
                                selectedIndex++;
                                int listHeight = ui.GetHeight() - 8;
                                if (selectedIndex >= scrollOffset + listHeight) scrollOffset++;
                            }
                        }
                        break;
                    case VK_TAB:
                        // Toggle views
                        if (state == AppState::PROCESS_LIST) {
                            state = AppState::SERVICE_LIST;
                            RefreshData(true);
                        }
                        else if (state == AppState::SERVICE_LIST) {
                            state = AppState::PROCESS_LIST;
                            RefreshData(true);
                        }
                        selectedIndex = 0;
                        scrollOffset = 0;
                        break;
                    case VK_RETURN:
                        if (state == AppState::PROCESS_LIST) {
                            if (selectedIndex >= 0 && selectedIndex < processes.size()) {
                                selectedPid = processes[selectedIndex].pid;
                                modules = monitor.GetProcessModules(selectedPid);
                                state = AppState::MODULE_VIEW;
                                selectedIndex = 0;
                                scrollOffset = 0;
                            }
                        }
                        break;
                    case VK_DELETE:
                        if (state == AppState::PROCESS_LIST) {
                            if (selectedIndex >= 0 && selectedIndex < processes.size()) {
                                DWORD pid = processes[selectedIndex].pid;
                                std::wstring name = processes[selectedIndex].name;
                                monitor.KillProcess(pid);
                                g_Logger.Log(LogLevel::WARNING, L"User requested kill for: " + name);
                                RefreshData(true); // Force update
                            }
                        }
                        break;
                }
            }
            else if (ir[i].EventType == WINDOW_BUFFER_SIZE_EVENT) {
                ui.UpdateSize();
            }
        }
    }

    int GetItemCount() {
        switch(state) {
            case AppState::PROCESS_LIST: return (int)processes.size();
            case AppState::SERVICE_LIST: return (int)services.size();
            case AppState::MODULE_VIEW: return (int)modules.size();
            default: return 0;
        }
    }

    void RefreshData(bool force = false) {
        auto now = std::chrono::steady_clock::now();
        if (force || std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() > updateIntervalMs) {
            
            // Only update process list if we are viewing it (heavy op)
            if (state == AppState::PROCESS_LIST) {
                 // Try to keep selection stable by finding PID
                 DWORD currentPid = 0;
                 if (selectedIndex < processes.size()) currentPid = processes[selectedIndex].pid;

                 processes = monitor.EnumProcesses();
                 
                 // Restore selection
                 if (currentPid != 0) {
                     bool found = false;
                     for(int i=0; i<processes.size(); i++) {
                         if (processes[i].pid == currentPid) {
                             selectedIndex = i;
                             found = true;
                             break;
                         }
                     }
                     if (!found) selectedIndex = 0;
                 }
            } else if (state == AppState::SERVICE_LIST) {
                services = monitor.EnumServices();
            }

            lastUpdate = now;
        }
    }

    void UpdateData() {
        RefreshData();
    }

    void Draw() {
        ui.Clear();
        int w = ui.GetWidth();
        int h = ui.GetHeight();

        // 1. Draw Header
        ui.DrawBox(0, 0, w, 3);
        ui.Write(2, 1, L"WinSysMon v1.0 | Tabs: Switch View | Enter: Details | Del: Kill Process | Esc: Back/Exit", FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        
        // 2. Draw System Stats
        DrawSystemStats();

        // 3. Draw Main Content Area
        int contentTop = 7;
        int contentHeight = h - contentTop - 8; // Reserved for footer
        ui.DrawBox(0, contentTop - 1, w, contentHeight + 2);

        if (state == AppState::PROCESS_LIST) DrawProcessList(1, contentTop, w - 2, contentHeight);
        else if (state == AppState::SERVICE_LIST) DrawServiceList(1, contentTop, w - 2, contentHeight);
        else if (state == AppState::MODULE_VIEW) DrawModuleList(1, contentTop, w - 2, contentHeight);

        // 4. Draw Logger Footer
        DrawLogger(0, h - 6, w, 6);

        ui.Render();
    }

    void DrawSystemStats() {
        double cpu = monitor.GetCpuUsage();
        DWORD memLoad; SIZE_T totMem, availMem;
        monitor.GetMemoryStatus(memLoad, totMem, availMem);

        std::wstringstream ss;
        ss << L" CPU Usage: " << std::fixed << std::setprecision(1) << cpu << L"% ";
        ui.Write(2, 4, ss.str(), FOREGROUND_RED | FOREGROUND_INTENSITY);

        ss.str(L"");
        ss << L" Memory: " << memLoad << L"% (" << Utils::FormatBytes(totMem - availMem) << L" / " << Utils::FormatBytes(totMem) << L") ";
        ui.Write(30, 4, ss.str(), FOREGROUND_CYAN | FOREGROUND_INTENSITY);
        
        std::wstring modeStr = L" MODE: ";
        if (state == AppState::PROCESS_LIST) modeStr += L"PROCESSES";
        else if (state == AppState::SERVICE_LIST) modeStr += L"SERVICES";
        else if (state == AppState::MODULE_VIEW) modeStr += L"MODULES (PID " + std::to_wstring(selectedPid) + L")";

        ui.Write(ui.GetWidth() - (int)modeStr.length() - 2, 4, modeStr, FOREGROUND_YELLOW | FOREGROUND_INTENSITY);
    }

    void DrawProcessList(int x, int y, int w, int h) {
        // Header
        ui.Write(x + 1, y, L"PID", FOREGROUND_INTENSITY);
        ui.Write(x + 8, y, L"Name", FOREGROUND_INTENSITY);
        ui.Write(x + 40, y, L"Threads", FOREGROUND_INTENSITY);
        ui.Write(x + 50, y, L"Memory", FOREGROUND_INTENSITY);
        ui.Write(x + 65, y, L"Priority", FOREGROUND_INTENSITY);
        ui.Write(x, y + 1, std::wstring(w, L'-'), FOREGROUND_INTENSITY);

        int startY = y + 2;
        int listCapacity = h - 2;

        for (int i = 0; i < listCapacity; ++i) {
            int idx = scrollOffset + i;
            if (idx >= processes.size()) break;

            const auto& p = processes[idx];
            WORD attr = (idx == selectedIndex) ? (BACKGROUND_GREEN | FOREGROUND_BLACK) : (FOREGROUND_WHITE);

            std::wstring line = std::wstring(w, L' '); // Clear line background
            ui.Write(x, startY + i, line, attr);

            std::wstringstream ss;
            
            // PID
            ss << p.pid;
            ui.Write(x + 1, startY + i, ss.str(), attr);
            
            // Name
            std::wstring name = p.name;
            if (name.length() > 30) name = name.substr(0, 27) + L"...";
            ui.Write(x + 8, startY + i, name, attr);

            // Threads
            ui.Write(x + 40, startY + i, std::to_wstring(p.threadCount), attr);

            // Memory
            ui.Write(x + 50, startY + i, Utils::FormatBytes(p.workingSetSize), attr);

            // Priority
            ui.Write(x + 65, startY + i, std::to_wstring(p.priorityClass), attr);
        }
    }

    void DrawServiceList(int x, int y, int w, int h) {
        ui.Write(x + 1, y, L"Status", FOREGROUND_INTENSITY);
        ui.Write(x + 10, y, L"Service Name", FOREGROUND_INTENSITY);
        ui.Write(x + 45, y, L"Display Name", FOREGROUND_INTENSITY);
        ui.Write(x, y + 1, std::wstring(w, L'-'), FOREGROUND_INTENSITY);

        int startY = y + 2;
        int listCapacity = h - 2;

        for (int i = 0; i < listCapacity; ++i) {
            int idx = scrollOffset + i;
            if (idx >= services.size()) break;

            const auto& s = services[idx];
            WORD attr = (idx == selectedIndex) ? (BACKGROUND_GREEN | FOREGROUND_BLACK) : (FOREGROUND_WHITE);
            WORD statusAttr = attr;
            
            std::wstring statusStr = (s.status == SERVICE_RUNNING) ? L"RUNNING" : L"STOPPED";
            if (idx != selectedIndex) {
                 statusAttr = (s.status == SERVICE_RUNNING) ? FOREGROUND_GREEN : FOREGROUND_RED;
            }

            std::wstring line = std::wstring(w, L' ');
            ui.Write(x, startY + i, line, attr);

            ui.Write(x + 1, startY + i, statusStr, statusAttr);
            
            std::wstring sName = s.serviceName;
            if (sName.length() > 33) sName = sName.substr(0, 30) + L"...";
            ui.Write(x + 10, startY + i, sName, attr);

            std::wstring dName = s.displayName;
            if (dName.length() > 40) dName = dName.substr(0, 37) + L"...";
            ui.Write(x + 45, startY + i, dName, attr);
        }
    }

    void DrawModuleList(int x, int y, int w, int h) {
        ui.Write(x + 1, y, L"Module Name", FOREGROUND_INTENSITY);
        ui.Write(x + 30, y, L"Base Address", FOREGROUND_INTENSITY);
        ui.Write(x + 50, y, L"Size", FOREGROUND_INTENSITY);
        ui.Write(x + 65, y, L"Path", FOREGROUND_INTENSITY);
        ui.Write(x, y + 1, std::wstring(w, L'-'), FOREGROUND_INTENSITY);

        int startY = y + 2;
        int listCapacity = h - 2;

        if (modules.empty()) {
             ui.Write(x + 1, startY, L"No modules found or access denied.", FOREGROUND_RED);
             return;
        }

        for (int i = 0; i < listCapacity; ++i) {
            int idx = scrollOffset + i;
            if (idx >= modules.size()) break;

            const auto& m = modules[idx];
            WORD attr = (idx == selectedIndex) ? (BACKGROUND_GREEN | FOREGROUND_BLACK) : (FOREGROUND_WHITE);

            std::wstring line = std::wstring(w, L' ');
            ui.Write(x, startY + i, line, attr);

            ui.Write(x + 1, startY + i, m.moduleName, attr);
            
            std::wstringstream ssAddr;
            ssAddr << L"0x" << std::hex << (uintptr_t)m.hModule;
            ui.Write(x + 30, startY + i, ssAddr.str(), attr);

            ui.Write(x + 50, startY + i, Utils::FormatBytes(m.size), attr);

            std::wstring path = m.modulePath;
            if (path.length() > 40) path = L"..." + path.substr(path.length() - 37);
            ui.Write(x + 65, startY + i, path, attr);
        }
    }

    void DrawLogger(int x, int y, int w, int h) {
        ui.DrawBox(x, y, w, h);
        ui.Write(x + 2, y, L" Event Log ", FOREGROUND_MAGENTA | FOREGROUND_INTENSITY);

        auto logs = g_Logger.GetRecentLogs(h - 2);
        int currentY = y + 1;
        for (const auto& log : logs) {
            WORD color = FOREGROUND_WHITE;
            if (log.level == LogLevel::ERR) color = FOREGROUND_RED;
            if (log.level == LogLevel::WARNING) color = FOREGROUND_YELLOW;

            // Simple time format
            auto tt = std::chrono::system_clock::to_time_t(log.timestamp);
            struct tm tm; 
            localtime_s(&tm, &tt);
            char timeBuf[10];
            std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm);
            
            std::string tb(timeBuf);
            std::wstring wTime(tb.begin(), tb.end());

            ui.Write(x + 1, currentY++, L"[" + wTime + L"] " + log.message, color);
        }
    }
};

// ==========================================
// ENTRY POINT
// ==========================================

int main() {
    // Set Console Title
    SetConsoleTitle(L"WinSysMon - System Monitor Utility");

    // Optional: Hide scrollbars and set buffer size to window size for cleaner UI
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(hOut, &info);
    COORD newSize = { (SHORT)(info.srWindow.Right - info.srWindow.Left + 1), (SHORT)(info.srWindow.Bottom - info.srWindow.Top + 1) };
    SetConsoleScreenBufferSize(hOut, newSize);

    try {
        Application app;
        app.Run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        system("pause");
        return 1;
    }

    return 0;
}

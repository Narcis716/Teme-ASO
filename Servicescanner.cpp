// ============================================================
//  ServiceScanner.cpp
//  Identifică toate serviciile sistem ACTIVE (Running) pe mașina
//  curentă folosind Windows Service Control Manager API.
//
//  Compilare (MSVC / Developer Command Prompt):
//    cl ServiceScanner.cpp /EHsc /W4 /std:c++17 /link advapi32.lib
//
//  Compilare (MinGW/g++):
//    g++ ServiceScanner.cpp -o ServiceScanner.exe -ladvapi32 -std=c++17
// ============================================================

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <winsvc.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <tchar.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <ctime>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")

// ─── Structură pentru un serviciu ──────────────────────────────────────────
struct ServiceInfo {
    std::wstring serviceName;       // Nume intern (cheie registru)
    std::wstring displayName;       // Nume afișat utilizatorului
    std::wstring binaryPath;        // Calea executabilului
    std::wstring description;       // Descriere serviciu
    std::wstring startTypeStr;      // Tip pornire (Auto/Manual/Disabled)
    std::wstring accountName;       // Contul sub care rulează
    DWORD        pid;               // Process ID
    DWORD        currentState;      // SERVICE_RUNNING etc.
    DWORD        processType;       // Win32 / Kernel / etc.
    DWORD        startType;         // SERVICE_AUTO_START etc.
};

// ─── Utilitare ──────────────────────────────────────────────────────────────
static std::wstring StateToString(DWORD state)
{
    switch (state) {
        case SERVICE_STOPPED:          return L"STOPPED";
        case SERVICE_START_PENDING:    return L"START_PENDING";
        case SERVICE_STOP_PENDING:     return L"STOP_PENDING";
        case SERVICE_RUNNING:          return L"RUNNING";
        case SERVICE_CONTINUE_PENDING: return L"CONTINUE_PENDING";
        case SERVICE_PAUSE_PENDING:    return L"PAUSE_PENDING";
        case SERVICE_PAUSED:           return L"PAUSED";
        default:                       return L"UNKNOWN";
    }
}

static std::wstring StartTypeToString(DWORD st)
{
    switch (st) {
        case SERVICE_BOOT_START:   return L"Boot";
        case SERVICE_SYSTEM_START: return L"System";
        case SERVICE_AUTO_START:   return L"Automatic";
        case SERVICE_DEMAND_START: return L"Manual";
        case SERVICE_DISABLED:     return L"Disabled";
        default:                   return L"Unknown";
    }
}

static std::wstring TypeToString(DWORD type)
{
    std::wstring r;
    if (type & SERVICE_KERNEL_DRIVER)       r += L"KernelDriver ";
    if (type & SERVICE_FILE_SYSTEM_DRIVER)  r += L"FileSystemDriver ";
    if (type & SERVICE_WIN32_OWN_PROCESS)   r += L"Win32OwnProcess ";
    if (type & SERVICE_WIN32_SHARE_PROCESS) r += L"Win32SharedProcess ";
    if (type & SERVICE_INTERACTIVE_PROCESS) r += L"Interactive ";
    if (r.empty()) r = L"Unknown";
    return r;
}

// Obține CPU% pentru un PID (snapshot simplu, valoare instantanee relativă)
static std::wstring GetProcessMemoryMB(DWORD pid)
{
    if (pid == 0) return L"N/A";
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
                               FALSE, pid);
    if (!hProc) return L"N/A";

    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    std::wstring result = L"N/A";
    if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
        double mb = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
        std::wostringstream oss;
        oss << std::fixed << std::setprecision(1) << mb << L" MB";
        result = oss.str();
    }
    CloseHandle(hProc);
    return result;
}

// Recuperează descrierea din configurația extinsă a serviciului
static std::wstring GetServiceDescription(SC_HANDLE hSvc)
{
    DWORD needed = 0;
    QueryServiceConfig2W(hSvc, SERVICE_CONFIG_DESCRIPTION, nullptr, 0, &needed);
    if (needed == 0) return L"";

    std::vector<BYTE> buf(needed);
    if (!QueryServiceConfig2W(hSvc, SERVICE_CONFIG_DESCRIPTION,
                              buf.data(), needed, &needed))
        return L"";

    auto* desc = reinterpret_cast<SERVICE_DESCRIPTIONW*>(buf.data());
    return desc->lpDescription ? desc->lpDescription : L"";
}

// ─── Colectare servicii ─────────────────────────────────────────────────────
static std::vector<ServiceInfo> CollectRunningServices(bool allStates = false)
{
    std::vector<ServiceInfo> results;

    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (!hSCM) {
        std::wcerr << L"[EROARE] Nu s-a putut deschide Service Control Manager.\n"
                   << L"         Rulați ca Administrator!\n";
        return results;
    }

    // Prima apelare pentru a afla dimensiunea buffer-ului
    DWORD bytesNeeded = 0, servicesReturned = 0, resumeHandle = 0;
    DWORD queryType  = SERVICE_WIN32 | SERVICE_DRIVER;
    DWORD queryState = allStates ? SERVICE_STATE_ALL : SERVICE_RUNNING;

    EnumServicesStatusExW(hSCM, SC_ENUM_PROCESS_INFO, queryType, queryState,
                          nullptr, 0, &bytesNeeded, &servicesReturned,
                          &resumeHandle, nullptr);

    if (GetLastError() != ERROR_MORE_DATA && bytesNeeded == 0) {
        CloseServiceHandle(hSCM);
        return results;
    }

    std::vector<BYTE> buffer(bytesNeeded);
    resumeHandle = 0;

    if (!EnumServicesStatusExW(hSCM, SC_ENUM_PROCESS_INFO, queryType, queryState,
                               buffer.data(), bytesNeeded, &bytesNeeded,
                               &servicesReturned, &resumeHandle, nullptr)) {
        std::wcerr << L"[EROARE] EnumServicesStatusEx a eșuat: "
                   << GetLastError() << L"\n";
        CloseServiceHandle(hSCM);
        return results;
    }

    auto* services = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());

    for (DWORD i = 0; i < servicesReturned; ++i) {
        ServiceInfo info{};
        info.serviceName = services[i].lpServiceName;
        info.displayName = services[i].lpDisplayName;
        info.pid         = services[i].ServiceStatusProcess.dwProcessId;
        info.currentState= services[i].ServiceStatusProcess.dwCurrentState;
        info.processType = services[i].ServiceStatusProcess.dwServiceType;

        // Detalii suplimentare din configurație
        SC_HANDLE hSvc = OpenServiceW(hSCM, services[i].lpServiceName,
                                      SERVICE_QUERY_CONFIG);
        if (hSvc) {
            DWORD cfgNeeded = 0;
            QueryServiceConfigW(hSvc, nullptr, 0, &cfgNeeded);
            if (cfgNeeded > 0) {
                std::vector<BYTE> cfgBuf(cfgNeeded);
                auto* cfg = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(cfgBuf.data());
                if (QueryServiceConfigW(hSvc, cfg, cfgNeeded, &cfgNeeded)) {
                    info.binaryPath   = cfg->lpBinaryPathName  ? cfg->lpBinaryPathName  : L"";
                    info.accountName  = cfg->lpServiceStartName ? cfg->lpServiceStartName : L"";
                    info.startType    = cfg->dwStartType;
                    info.startTypeStr = StartTypeToString(cfg->dwStartType);
                }
            }
            info.description = GetServiceDescription(hSvc);
            CloseServiceHandle(hSvc);
        }

        results.push_back(std::move(info));
    }

    CloseServiceHandle(hSCM);

    // Sortare după nume afișat
    std::sort(results.begin(), results.end(),
              [](const ServiceInfo& a, const ServiceInfo& b) {
                  return _wcsicmp(a.displayName.c_str(), b.displayName.c_str()) < 0;
              });

    return results;
}

// ─── Afișare în consolă ─────────────────────────────────────────────────────
static void PrintTable(const std::vector<ServiceInfo>& svcList)
{
    const int W_IDX   =  4;
    const int W_NAME  = 36;
    const int W_PID   =  7;
    const int W_STATE =  14;
    const int W_TYPE  =  20;
    const int W_START =  10;
    const int W_MEM   =  10;
    const int W_ACCT  =  30;

    auto sep = [&]() {
        std::wcout << std::wstring(W_IDX+W_NAME+W_PID+W_STATE+W_TYPE+W_START+W_MEM+W_ACCT+16, L'─') << L"\n";
    };

    auto truncW = [](const std::wstring& s, int w) -> std::wstring {
        if ((int)s.size() <= w) return s;
        return s.substr(0, w - 2) + L"..";
    };

    std::wcout << L"\n";
    sep();
    std::wcout << std::left
        << std::setw(W_IDX)   << L"#"
        << std::setw(W_NAME)  << L"Nume Afișat"
        << std::setw(W_PID)   << L"PID"
        << std::setw(W_STATE) << L"Stare"
        << std::setw(W_TYPE)  << L"Tip"
        << std::setw(W_START) << L"Pornire"
        << std::setw(W_MEM)   << L"Memorie"
        << std::setw(W_ACCT)  << L"Cont"
        << L"\n";
    sep();

    int idx = 1;
    for (const auto& s : svcList) {
        std::wstring mem = GetProcessMemoryMB(s.pid);
        std::wstring pidStr = s.pid ? std::to_wstring(s.pid) : L"-";

        std::wcout << std::left
            << std::setw(W_IDX)   << idx++
            << std::setw(W_NAME)  << truncW(s.displayName, W_NAME)
            << std::setw(W_PID)   << pidStr
            << std::setw(W_STATE) << StateToString(s.currentState)
            << std::setw(W_TYPE)  << truncW(TypeToString(s.processType), W_TYPE)
            << std::setw(W_START) << s.startTypeStr
            << std::setw(W_MEM)   << mem
            << std::setw(W_ACCT)  << truncW(s.accountName, W_ACCT)
            << L"\n";
    }
    sep();
    std::wcout << L"  Total: " << svcList.size() << L" servicii\n\n";
}

// ─── Export CSV ─────────────────────────────────────────────────────────────
static void ExportCSV(const std::vector<ServiceInfo>& svcList,
                      const std::wstring& path)
{
    std::wofstream f(path);
    if (!f) {
        std::wcerr << L"Nu s-a putut crea fișierul CSV: " << path << L"\n";
        return;
    }
    // BOM UTF-16 LE pentru Excel
    f << L'\xFEFF';
    f << L"Nr,NumeInternal,NumeAfisat,PID,Stare,Tip,TipPornire,Memorie(MB),Cont,BinaryPath,Descriere\n";

    int idx = 1;
    for (const auto& s : svcList) {
        std::wstring mem = GetProcessMemoryMB(s.pid);

        auto esc = [](std::wstring v) -> std::wstring {
            // Înlocuiește ghilimelele și învelește în ghilimele
            size_t pos = 0;
            while ((pos = v.find(L'"', pos)) != std::wstring::npos) {
                v.replace(pos, 1, L"\"\""); pos += 2;
            }
            return L'"' + v + L'"';
        };

        f << idx++ << L","
          << esc(s.serviceName) << L","
          << esc(s.displayName) << L","
          << (s.pid ? std::to_wstring(s.pid) : L"") << L","
          << esc(StateToString(s.currentState)) << L","
          << esc(TypeToString(s.processType)) << L","
          << esc(s.startTypeStr) << L","
          << esc(mem) << L","
          << esc(s.accountName) << L","
          << esc(s.binaryPath) << L","
          << esc(s.description) << L"\n";
    }
    std::wcout << L"  [OK] CSV exportat: " << path << L"\n";
}

// ─── Afișare detalii serviciu individual ────────────────────────────────────
static void PrintDetail(const ServiceInfo& s)
{
    std::wcout << L"\n";
    std::wcout << L"  ┌─ DETALII SERVICIU ──────────────────────────────────┐\n";
    std::wcout << L"  │ Nume intern   : " << s.serviceName << L"\n";
    std::wcout << L"  │ Nume afișat   : " << s.displayName << L"\n";
    std::wcout << L"  │ PID           : " << (s.pid ? std::to_wstring(s.pid) : L"N/A") << L"\n";
    std::wcout << L"  │ Stare         : " << StateToString(s.currentState) << L"\n";
    std::wcout << L"  │ Tip           : " << TypeToString(s.processType) << L"\n";
    std::wcout << L"  │ Tip pornire   : " << s.startTypeStr << L"\n";
    std::wcout << L"  │ Cont          : " << s.accountName << L"\n";
    std::wcout << L"  │ Memorie WS    : " << GetProcessMemoryMB(s.pid) << L"\n";
    std::wcout << L"  │ Executabil    : " << s.binaryPath << L"\n";
    std::wcout << L"  │ Descriere     : " << (s.description.empty() ? L"(nedisponibilă)" : s.description) << L"\n";
    std::wcout << L"  └─────────────────────────────────────────────────────┘\n\n";
}

// ─── Meniu interactiv ────────────────────────────────────────────────────────
static void InteractiveMode(std::vector<ServiceInfo>& svcList)
{
    while (true) {
        std::wcout << L"\n┌─ ACȚIUNI ──────────────────────────────────────────┐\n";
        std::wcout << L"│  [1] Reîncarcă lista serviciilor active             │\n";
        std::wcout << L"│  [2] Afișează TOATE serviciile (inclusiv oprite)    │\n";
        std::wcout << L"│  [3] Caută după nume                                │\n";
        std::wcout << L"│  [4] Detalii serviciu (după număr din tabel)        │\n";
        std::wcout << L"│  [5] Exportă în CSV                                 │\n";
        std::wcout << L"│  [0] Ieșire                                         │\n";
        std::wcout << L"└────────────────────────────────────────────────────┘\n";
        std::wcout << L"  Alegere: ";

        std::wstring choice;
        std::wcin >> choice;
        std::wcin.ignore(1000, L'\n');

        if (choice == L"0") break;

        if (choice == L"1") {
            svcList = CollectRunningServices(false);
            PrintTable(svcList);
        }
        else if (choice == L"2") {
            auto all = CollectRunningServices(true);
            PrintTable(all);
        }
        else if (choice == L"3") {
            std::wcout << L"  Termen căutare: ";
            std::wstring term;
            std::getline(std::wcin, term);
            std::transform(term.begin(), term.end(), term.begin(), ::towlower);

            std::vector<ServiceInfo> found;
            for (const auto& s : svcList) {
                std::wstring dn = s.displayName, sn = s.serviceName;
                std::transform(dn.begin(), dn.end(), dn.begin(), ::towlower);
                std::transform(sn.begin(), sn.end(), sn.begin(), ::towlower);
                if (dn.find(term) != std::wstring::npos ||
                    sn.find(term) != std::wstring::npos)
                    found.push_back(s);
            }
            if (found.empty())
                std::wcout << L"  Niciun rezultat.\n";
            else
                PrintTable(found);
        }
        else if (choice == L"4") {
            std::wcout << L"  Număr din tabel: ";
            std::wstring numStr;
            std::getline(std::wcin, numStr);
            try {
                int n = std::stoi(numStr);
                if (n >= 1 && n <= (int)svcList.size())
                    PrintDetail(svcList[n - 1]);
                else
                    std::wcout << L"  Număr invalid.\n";
            } catch (...) {
                std::wcout << L"  Introduceți un număr valid.\n";
            }
        }
        else if (choice == L"5") {
            // Generează nume fișier cu timestamp
            std::time_t t = std::time(nullptr);
            std::tm tm_info{};
            localtime_s(&tm_info, &t);
            wchar_t ts[32];
            wcsftime(ts, 32, L"%Y%m%d_%H%M%S", &tm_info);
            std::wstring csvPath = std::wstring(L"servicii_") + ts + L".csv";
            ExportCSV(svcList, csvPath);
        }
        else {
            std::wcout << L"  Opțiune necunoscută.\n";
        }
    }
}

// ─── Banner ──────────────────────────────────────────────────────────────────
static void PrintBanner()
{
    std::wcout
        << L"\n"
        << L"  ╔══════════════════════════════════════════════════════╗\n"
        << L"  ║       SERVICE SCANNER  —  Windows SCM Inspector     ║\n"
        << L"  ║   Identificare servicii sistem active (Running)     ║\n"
        << L"  ╚══════════════════════════════════════════════════════╝\n\n";
}

// ─── ENTRY POINT ─────────────────────────────────────────────────────────────
int wmain(int argc, wchar_t* argv[])
{
    // Setează consola la UTF-16
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    PrintBanner();

    // Mod batch (argumente linie comandă)
    bool batchCSV = false;
    bool allStates = false;
    std::wstring csvOut;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--all")        allStates = true;
        if (arg == L"--csv" && i+1 < argc) { batchCSV = true; csvOut = argv[++i]; }
        if (arg == L"--help" || arg == L"-h") {
            std::wcout
                << L"  Utilizare: ServiceScanner.exe [opțiuni]\n"
                << L"  Opțiuni:\n"
                << L"    (nimic)         Mod interactiv — servicii RUNNING\n"
                << L"    --all           Include și serviciile oprite\n"
                << L"    --csv <fișier>  Export direct CSV și ieșire\n"
                << L"    --help          Acest ajutor\n\n";
            return 0;
        }
    }

    std::wcout << L"  Se interoghează Service Control Manager...\n";
    auto svcList = CollectRunningServices(allStates);

    if (svcList.empty()) {
        std::wcout << L"  Lista este goală sau accesul a fost refuzat.\n";
        return 1;
    }

    PrintTable(svcList);

    if (batchCSV) {
        ExportCSV(svcList, csvOut);
        return 0;
    }

    // Mod interactiv
    InteractiveMode(svcList);

    std::wcout << L"\n  La revedere!\n\n";
    return 0;
}
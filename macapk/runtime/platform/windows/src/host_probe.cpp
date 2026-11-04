// host_probe.cpp -- Windows, this process and the display adapters, for doctor.
#include "doctor.h"

#include <windows.h>
#include <dxgi.h>

#include <cstdio>

namespace winapk {

std::string Utf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring Wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

/// Names an IMAGE_FILE_MACHINE_* value.
static std::string MachineName(USHORT m) {
    switch (m) {
        case IMAGE_FILE_MACHINE_ARM64: return "arm64";
        case 0xA641: return "arm64ec";  // IMAGE_FILE_MACHINE_ARM64EC
        case IMAGE_FILE_MACHINE_AMD64: return "x64";
        case IMAGE_FILE_MACHINE_I386: return "x86";
        default: return "unknown";
    }
}

std::string PeArch(const std::wstring& path) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return "unknown";
    IMAGE_DOS_HEADER dos{};
    DWORD got = 0;
    std::string arch = "unknown";
    if (ReadFile(f, &dos, sizeof dos, &got, nullptr) && got == sizeof dos && dos.e_magic == IMAGE_DOS_SIGNATURE &&
        SetFilePointer(f, dos.e_lfanew, nullptr, FILE_BEGIN) != INVALID_SET_FILE_POINTER) {
        DWORD sig = 0;
        IMAGE_FILE_HEADER fh{};
        if (ReadFile(f, &sig, sizeof sig, &got, nullptr) && sig == IMAGE_NT_SIGNATURE &&
            ReadFile(f, &fh, sizeof fh, &got, nullptr) && got == sizeof fh) {
            arch = MachineName(fh.Machine);
        }
    }
    CloseHandle(f);
    return arch;
}

/// Reads a REG_SZ or REG_DWORD value of HKLM as text, "" when absent.
static std::wstring RegText(const wchar_t* key, const wchar_t* name) {
    wchar_t buf[512];
    DWORD size = sizeof buf, type = 0;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, key, name, RRF_RT_REG_SZ | RRF_RT_REG_DWORD, &type, buf, &size) != ERROR_SUCCESS) {
        return {};
    }
    if (type == REG_DWORD) return std::to_wstring(*reinterpret_cast<DWORD*>(buf));
    return buf;
}

void ProbeHost(Report& r) {
    // RtlGetVersion reports the real version; GetVersionEx lies to unmanifested programs.
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    OSVERSIONINFOW v{sizeof v};
    if (auto fn = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"))) {
        fn(&v);
    }
    std::wstring ubr = RegText(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"UBR");
    char build[64];
    snprintf(build, sizeof build, "%lu.%lu.%lu.%s", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber,
             ubr.empty() ? "0" : Utf8(ubr).c_str());
    r.host.osBuild = build;

    // IsWow64Process2: the process machine is UNKNOWN when it is not emulated.
    USHORT proc = IMAGE_FILE_MACHINE_UNKNOWN, native = IMAGE_FILE_MACHINE_UNKNOWN;
    IsWow64Process2(GetCurrentProcess(), &proc, &native);
    r.host.osArch = MachineName(native);
    r.host.processArch = proc == IMAGE_FILE_MACHINE_UNKNOWN ? r.host.osArch : MachineName(proc);
    // An x64 program on ARM64 is emulated without WOW64 machine reporting: ask the image.
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    if (proc == IMAGE_FILE_MACHINE_UNKNOWN) r.host.processArch = PeArch(self);

    r.host.cpu = Utf8(RegText(L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString"));
    MEMORYSTATUSEX mem{sizeof mem};
    if (GlobalMemoryStatusEx(&mem)) r.host.memoryMB = mem.ullTotalPhys / (1024 * 1024);

    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) return;
    IDXGIAdapter1* a = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &a) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 d{};
        a->GetDesc1(&d);
        Adapter ad;
        ad.name = Utf8(d.Description);
        ad.vendorId = d.VendorId;
        ad.deviceId = d.DeviceId;
        ad.software = (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 || d.VendorId == 0x1414;  // Microsoft
        LARGE_INTEGER umd{};
        if (SUCCEEDED(a->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd))) {
            char ver[64];
            snprintf(ver, sizeof ver, "%u.%u.%u.%u", HIWORD(umd.HighPart), LOWORD(umd.HighPart), HIWORD(umd.LowPart),
                     LOWORD(umd.LowPart));
            ad.driverVersion = ver;
        }
        r.adapters.push_back(ad);
        a->Release();
    }
    factory->Release();
}

}  // namespace winapk

#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include "MinHook.h"

#define IDA_BASE 0x00400000

#define THRESHOLD_SMALL_EGO 16252928 
#define THRESHOLD_LARGE_WIN 52428800

enum class GameVersion {
    Unknown_Retail,
    Debug_Ego,
    Debug_Log
};

struct GameOffsets {
    ptrdiff_t PTreeControl;
    ptrdiff_t CThingDialog_ControlCentre;
    ptrdiff_t CThingDialog_ThingManager;
    ptrdiff_t ControlCentre_Component;
    ptrdiff_t ControlCentre_DefMgr;
    ptrdiff_t GroupDef;
};

struct GameAddresses {
    uintptr_t CThingDialog_Ctor;
    uintptr_t GetDefNameFromGlobalIndex;
    uintptr_t GDefStringTable;
    uintptr_t PeekDefMgr;
    uintptr_t GetThingTypeDefClassName;
    uintptr_t GetNoInstantiatedDefs;
    uintptr_t GetDefNameFromClassIndex;
    uintptr_t GetString;
    uintptr_t GetGlobalIndex;
    uintptr_t GetPDef;
    uintptr_t CWideString_Ctor;
    uintptr_t CWideString_Dtor;
    uintptr_t CCharString_Dtor;
    uintptr_t SortTree;
    uintptr_t UpdateTreeView;
    uintptr_t AddEntry;
};

GameAddresses g_Addrs;
GameOffsets g_Offsets;

// Structs padded to largest known size (FableWin Debug) to prevent stack corruption
struct CWideString {
    void* PStringData;
    const wchar_t* DebugString;
    char _pad[32];
};

struct CCharString {
    void* PStringData;
    const char* DebugString;
    char _pad[32];
};

struct CDefString { int TablePos; };

typedef void* (__thiscall* _PeekDefMgr)(void*);
typedef void* (__thiscall* _GetClassName)(void*, int);
typedef int(__thiscall* _GetNoDefs)(void*, void*);
typedef void(__thiscall* _GetDefName)(void*, CDefString*, void*, int);
typedef CDefString* (__thiscall* _GetDefNameFromGlobalIndex)(void*, CDefString*, int);
typedef void(__thiscall* _GetString)(void*, CCharString*, int);
typedef int(__thiscall* _GetGlobalIndex)(void*, void*, int);
typedef void* (__thiscall* _GetPDef)(void*, void*, void*, int);
typedef void(__thiscall* _CWideString_Ctor)(CWideString*, const wchar_t*);
typedef void(__thiscall* _CWideString_Dtor)(CWideString*);
typedef void(__thiscall* _CCharString_Dtor)(CCharString*);
typedef int(__thiscall* _AddEntry)(void*, const CWideString*, unsigned int, int, bool);
typedef void(__thiscall* _SortTree)(void*);
typedef void(__thiscall* _UpdateTreeView)(void*);

_PeekDefMgr fnPeekDefMgr = nullptr;
_GetClassName fnGetClassName = nullptr;
_GetNoDefs fnGetNoDefs = nullptr;
_GetDefName fnGetDefName = nullptr;
_GetDefNameFromGlobalIndex fnGetDefNameFromGlobalIndex = nullptr;
_GetString fnGetString = nullptr;
_GetGlobalIndex fnGetGlobalIndex = nullptr;
_GetPDef fnGetPDef = nullptr;
_CWideString_Ctor fnCtorWide = nullptr;
_CWideString_Dtor fnDtorWide = nullptr;
_CCharString_Dtor fnDtorChar = nullptr;
_AddEntry fnAddEntry = nullptr;
_SortTree fnSortTree = nullptr;
_UpdateTreeView fnUpdateTreeView = nullptr;
void* pGDefStringTable = nullptr;


void InitAddresses(GameVersion version) {
    if (version == GameVersion::Debug_Log) {

        g_Offsets.PTreeControl = 0x14C;
        g_Offsets.CThingDialog_ControlCentre = 0x144;
        g_Offsets.CThingDialog_ThingManager = 0x148;
        g_Offsets.ControlCentre_DefMgr = 0x854;
        g_Offsets.ControlCentre_Component = 0;
        g_Offsets.GroupDef = 0x4C;

        g_Addrs.CThingDialog_Ctor = 0x028F2E90;
        g_Addrs.GetDefNameFromGlobalIndex = 0x03062780;
        g_Addrs.GDefStringTable = 0x04BAC660;
        g_Addrs.PeekDefMgr = 0x00000000;
        g_Addrs.GetThingTypeDefClassName = 0x01D33D30;
        g_Addrs.GetNoInstantiatedDefs = 0x03062710;
        g_Addrs.GetDefNameFromClassIndex = 0x030627C0;
        g_Addrs.GetString = 0x02FCCF40;
        g_Addrs.GetGlobalIndex = 0x030622E0;
        g_Addrs.GetPDef = 0x03061E20;
        g_Addrs.CWideString_Ctor = 0x02F65650;
        g_Addrs.CWideString_Dtor = 0x02F65630;
        g_Addrs.CCharString_Dtor = 0x02F60F80;
        g_Addrs.SortTree = 0x0326E620;
        g_Addrs.UpdateTreeView = 0x0326E7D0;
        g_Addrs.AddEntry = 0x0326E0A0;
    }
    else if (version == GameVersion::Debug_Ego) {

        g_Offsets.PTreeControl = 0x130;
        g_Offsets.CThingDialog_ControlCentre = 0;
        g_Offsets.CThingDialog_ThingManager = 0;
        g_Offsets.ControlCentre_Component = 0x83C;
        g_Offsets.ControlCentre_DefMgr = 0;
        g_Offsets.GroupDef = 0x48;

        g_Addrs.CThingDialog_Ctor = 0x008B2410;
        g_Addrs.GetDefNameFromGlobalIndex = 0x00B04B10;
        g_Addrs.GDefStringTable = 0x01327F30;
        g_Addrs.PeekDefMgr = 0x00415162;
        g_Addrs.GetThingTypeDefClassName = 0x00528240;
        g_Addrs.GetNoInstantiatedDefs = 0x00B052D0;
        g_Addrs.GetDefNameFromClassIndex = 0x00B05330;
        g_Addrs.GetString = 0x00B28A90;
        g_Addrs.GetGlobalIndex = 0x00B05260;
        g_Addrs.GetPDef = 0x00B05EA0;
        g_Addrs.CWideString_Ctor = 0x00B02C10;
        g_Addrs.CWideString_Dtor = 0x00B02740;
        g_Addrs.CCharString_Dtor = 0x00AFA3F0;
        g_Addrs.SortTree = 0x00BB80B0;
        g_Addrs.UpdateTreeView = 0x00BB80C0;
        g_Addrs.AddEntry = 0x00BB8040;
    }
}

intptr_t g_AddressDelta = 0;
void* Rebase(uintptr_t idaAddress) {
    if (idaAddress == 0) return nullptr;
    return (void*)(idaAddress + g_AddressDelta);
}

void SetupFunctionPointers() {
    fnPeekDefMgr = (_PeekDefMgr)Rebase(g_Addrs.PeekDefMgr);
    fnGetClassName = (_GetClassName)Rebase(g_Addrs.GetThingTypeDefClassName);
    fnGetNoDefs = (_GetNoDefs)Rebase(g_Addrs.GetNoInstantiatedDefs);
    fnGetDefName = (_GetDefName)Rebase(g_Addrs.GetDefNameFromClassIndex);
    fnGetDefNameFromGlobalIndex = (_GetDefNameFromGlobalIndex)Rebase(g_Addrs.GetDefNameFromGlobalIndex);
    fnGetString = (_GetString)Rebase(g_Addrs.GetString);
    fnGetGlobalIndex = (_GetGlobalIndex)Rebase(g_Addrs.GetGlobalIndex);
    fnGetPDef = (_GetPDef)Rebase(g_Addrs.GetPDef);
    fnCtorWide = (_CWideString_Ctor)Rebase(g_Addrs.CWideString_Ctor);
    fnDtorWide = (_CWideString_Dtor)Rebase(g_Addrs.CWideString_Dtor);
    fnDtorChar = (_CCharString_Dtor)Rebase(g_Addrs.CCharString_Dtor);
    fnAddEntry = (_AddEntry)Rebase(g_Addrs.AddEntry);
    fnSortTree = (_SortTree)Rebase(g_Addrs.SortTree);
    fnUpdateTreeView = (_UpdateTreeView)Rebase(g_Addrs.UpdateTreeView);
    pGDefStringTable = Rebase(g_Addrs.GDefStringTable);
}

class SafeGameWString {
    CWideString ws;
public:
    SafeGameWString(const wchar_t* text) {
        memset(&ws, 0, sizeof(ws));
        fnCtorWide(&ws, text);
    }
    ~SafeGameWString() { fnDtorWide(&ws); }
    CWideString* ptr() { return &ws; }
};

const char* GetSafeAnsi(void* pStringData) {
    if (!pStringData || IsBadReadPtr(pStringData, 4)) return nullptr;
    void* pDereferenced = *(void**)pStringData;
    if (pDereferenced && !IsBadReadPtr(pDereferenced, 1)) return (const char*)pDereferenced;
    if (!IsBadReadPtr(pStringData, 1)) return (const char*)pStringData;
    return nullptr;
}

struct CategoryInfo { const wchar_t* Name; int ID; };

CategoryInfo g_Categories[] = {
    { L"Creatures", 1 },
    { L"Player Creatures", 2 },
    { L"Buildings", 3 },
    { L"Villages", 4 },
    { L"Objects", 5 },
    //{ L"Sound Emitters", 6 },
    { L"Holy Sites", 7 },
    //{ L"Noises", 8 },
    { L"Switches", 9 },
    { L"Other", 10 },
    { L"Markers", 11 },
    { L"Physical Switches", 12 }
};

struct FolderNode { int GroupIndex; int TreeHandle; };
FolderNode g_FolderCache[500];
int g_FolderCacheCount = 0;

int GetFolderHandle(void* pTree, int groupID, const wchar_t* name, int parentHandle) {
    for (int i = 0; i < g_FolderCacheCount; i++) {
        if (g_FolderCache[i].GroupIndex == groupID) return g_FolderCache[i].TreeHandle;
    }
    SafeGameWString wsName(name);
    int handle = fnAddEntry(pTree, wsName.ptr(), 0, parentHandle, 0);
    if (g_FolderCacheCount < 500) {
        g_FolderCache[g_FolderCacheCount] = { groupID, handle };
        g_FolderCacheCount++;
    }
    return handle;
}

typedef void* (__thiscall* _CThingDialog_Ctor)(void* pThis, const char* name, void* cc, void* tm);
_CThingDialog_Ctor Original_CThingDialog = nullptr;

void* __fastcall Detour_CThingDialog(void* pThis, void* edx, const char* name, void* cc, void* tm) {
    Original_CThingDialog(pThis, name, cc, tm);

    void* pTree = *(void**)((char*)pThis + g_Offsets.PTreeControl);
    if (!pTree) return pThis;

    void* pDefMgr = nullptr;
    void* pThingMgr = nullptr;

    if (g_Offsets.CThingDialog_ControlCentre != 0) {
        void* pControlCentre = *(void**)((char*)pThis + g_Offsets.CThingDialog_ControlCentre);
        pThingMgr = *(void**)((char*)pThis + g_Offsets.CThingDialog_ThingManager);

        if (pControlCentre && !IsBadReadPtr(pControlCentre, 4)) {
            pDefMgr = *(void**)((char*)pControlCentre + g_Offsets.ControlCentre_DefMgr);
        }
    }
    else {
        pThingMgr = tm;
        if (fnPeekDefMgr && cc) {
            void* pComponent = *(void**)((char*)cc + g_Offsets.ControlCentre_Component);
            pDefMgr = fnPeekDefMgr(pComponent);
        }
    }

    if (!pDefMgr || IsBadReadPtr(pDefMgr, 4)) return pThis;
    if (!pThingMgr || IsBadReadPtr(pThingMgr, 4)) return pThis;

    for (const auto& cat : g_Categories) {

        g_FolderCacheCount = 0;

        SafeGameWString catName(cat.Name);
        int catHandle = fnAddEntry(pTree, catName.ptr(), cat.ID, 0, 0);

        void* className = fnGetClassName(pThingMgr, cat.ID);
        if (!className || IsBadReadPtr(className, 4)) continue;

        int count = fnGetNoDefs(pDefMgr, className);

        for (int j = 1; j < count; j++) {
            int gIndex = fnGetGlobalIndex(pDefMgr, className, j);

            int groupID = 0;

            wchar_t folderName[128] = L"General";

            if (groupID > 0) {
                CDefString groupDefStr;
                fnGetDefNameFromGlobalIndex(pDefMgr, &groupDefStr, groupID);
                CCharString groupCharName;
                memset(&groupCharName, 0, sizeof(groupCharName));

                fnGetString(pGDefStringTable, &groupCharName, groupDefStr.TablePos);

                const char* pRawGroup = GetSafeAnsi(groupCharName.PStringData);
                if (pRawGroup) MultiByteToWideChar(CP_ACP, 0, pRawGroup, -1, folderName, 128);
                if (groupCharName.PStringData) fnDtorChar(&groupCharName);
            }

            int folderHandle = (wcscmp(folderName, L"General") == 0) ? catHandle : GetFolderHandle(pTree, groupID > 0 ? groupID : -1, folderName, catHandle);

            CDefString defStr;
            fnGetDefName(pDefMgr, &defStr, className, j);
            CCharString charName;
            memset(&charName, 0, sizeof(charName));

            fnGetString(pGDefStringTable, &charName, defStr.TablePos);

            const char* pRawAnsi = GetSafeAnsi(charName.PStringData);
            if (pRawAnsi) {
                wchar_t wBuf[256];
                if (MultiByteToWideChar(CP_ACP, 0, pRawAnsi, -1, wBuf, 256) > 0) {
                    SafeGameWString safeName(wBuf);
                    fnAddEntry(pTree, safeName.ptr(), gIndex, folderHandle, 1);
                }
            }
            if (charName.PStringData) fnDtorChar(&charName);
        }
    }

    fnSortTree(pTree);
    fnUpdateTreeView(pTree);

    return pThis;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        char exePath[MAX_PATH];
        if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0) return FALSE;

        HANDLE hFile = CreateFileA(exePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return FALSE;

        LARGE_INTEGER size;
        if (!GetFileSizeEx(hFile, &size)) {
            CloseHandle(hFile);
            return FALSE;
        }
        CloseHandle(hFile);

        GameVersion version = GameVersion::Unknown_Retail;

        if (size.QuadPart < THRESHOLD_SMALL_EGO) {
            version = GameVersion::Debug_Ego;
        }
        else if (size.QuadPart > THRESHOLD_LARGE_WIN) {
            version = GameVersion::Debug_Log;
        }
        else {
            return TRUE;
        }

        InitAddresses(version);

        if (g_Addrs.CThingDialog_Ctor == 0) return TRUE;

        g_AddressDelta = (intptr_t)GetModuleHandle(NULL) - IDA_BASE;
        void* hookAddr = Rebase(g_Addrs.CThingDialog_Ctor);

        SetupFunctionPointers();
        MH_Initialize();
        MH_CreateHook(hookAddr, &Detour_CThingDialog, reinterpret_cast<LPVOID*>(&Original_CThingDialog));
        MH_EnableHook(MH_ALL_HOOKS);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        MH_Uninitialize();
    }
    return TRUE;
}
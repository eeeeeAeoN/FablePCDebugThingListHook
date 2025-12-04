#include <windows.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <algorithm>
#include "MinHook.h"

#define IDA_BASE 0x00400000
#define IDA_CThingDialog_Ctor 0x008B2410
#define IDA_GetDefNameFromGlobalIndex 0x00B04B10
#define IDA_GDefStringTable 0x01327F30
#define IDA_PeekDefMgr 0x00415162
#define IDA_GetThingTypeDefClassName 0x00528240
#define IDA_GetNoInstantiatedDefs 0x00B052D0
#define IDA_GetDefNameFromClassIndex 0x00B05330
#define IDA_GetString 0x00B28A90
#define IDA_GetGlobalIndex 0x00B05260
#define IDA_AddEntry 0x00BB8040
#define IDA_GetPDef 0x00B05EA0
#define IDA_CWideString_Ctor 0x00B02C10
#define IDA_CWideString_Dtor 0x00B02740
#define IDA_CCharString_Dtor 0x00AFA3F0
#define IDA_SortTree 0x00BB80B0
#define IDA_UpdateTreeView 0x00BB80C0

#define OFFSET_PTreeControl 0x130
#define OFFSET_ControlCentre_Component 0x83C
#define OFFSET_GroupDef 0x48

struct CThingDialog { char _pad[OFFSET_PTreeControl]; void* PTreeControl; };
struct CDefString { int TablePos; };
struct CCharString { void* PStringData; };
struct CWideString { void* PStringData; };

intptr_t g_AddressDelta = 0;
void* Rebase(uintptr_t idaAddress) { return (void*)(idaAddress + g_AddressDelta); }

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

void SetupAddresses() {
    fnPeekDefMgr = (_PeekDefMgr)Rebase(IDA_PeekDefMgr);
    fnGetClassName = (_GetClassName)Rebase(IDA_GetThingTypeDefClassName);
    fnGetNoDefs = (_GetNoDefs)Rebase(IDA_GetNoInstantiatedDefs);
    fnGetDefName = (_GetDefName)Rebase(IDA_GetDefNameFromClassIndex);
    fnGetDefNameFromGlobalIndex = (_GetDefNameFromGlobalIndex)Rebase(IDA_GetDefNameFromGlobalIndex);
    fnGetString = (_GetString)Rebase(IDA_GetString);
    fnGetGlobalIndex = (_GetGlobalIndex)Rebase(IDA_GetGlobalIndex);
    fnGetPDef = (_GetPDef)Rebase(IDA_GetPDef);
    fnCtorWide = (_CWideString_Ctor)Rebase(IDA_CWideString_Ctor);
    fnDtorWide = (_CWideString_Dtor)Rebase(IDA_CWideString_Dtor);
    fnDtorChar = (_CCharString_Dtor)Rebase(IDA_CCharString_Dtor);
    fnAddEntry = (_AddEntry)Rebase(IDA_AddEntry);
    fnSortTree = (_SortTree)Rebase(IDA_SortTree);
    fnUpdateTreeView = (_UpdateTreeView)Rebase(IDA_UpdateTreeView);
    pGDefStringTable = Rebase(IDA_GDefStringTable);
}

class SafeGameWString {
    CWideString ws;
public:
    SafeGameWString(const wchar_t* text) { fnCtorWide(&ws, text); }
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
    { L"Holy Sites", 7 },
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

void* __fastcall Detour_CThingDialog(CThingDialog* pThis, void* edx, const char* name, void* cc, void* tm) {
    Original_CThingDialog(pThis, name, cc, tm);
    void* pTree = pThis->PTreeControl;
    if (!pTree) return pThis;

    void* pComponent = *(void**)((char*)cc + OFFSET_ControlCentre_Component);
    void* pDefMgr = fnPeekDefMgr(pComponent);
    void* pThingMgr = tm;

    for (const auto& cat : g_Categories) {
        g_FolderCacheCount = 0;
        SafeGameWString catName(cat.Name);
        int catHandle = fnAddEntry(pTree, catName.ptr(), cat.ID, 0, 0);

        void* className = fnGetClassName(pThingMgr, cat.ID);
        if (!className || IsBadReadPtr(className, 4)) continue;

        int count = fnGetNoDefs(pDefMgr, className);
        for (int j = 1; j < count; j++) {
            int gIndex = fnGetGlobalIndex(pDefMgr, className, j);

            char smartPtrBuf[16] = { 0 };
            ((void(__thiscall*)(void*, void*, void*, int))fnGetPDef)(pDefMgr, smartPtrBuf, className, j);
            char* pDefObj = *(char**)smartPtrBuf;
            int groupID = (pDefObj && !IsBadReadPtr(pDefObj, 0x50)) ? *(int*)(pDefObj + OFFSET_GroupDef) : 0;

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
        g_AddressDelta = (intptr_t)GetModuleHandle(NULL) - IDA_BASE;
        void* hookAddr = Rebase(IDA_CThingDialog_Ctor);

        SetupAddresses();
        MH_Initialize();
        MH_CreateHook(hookAddr, &Detour_CThingDialog, reinterpret_cast<LPVOID*>(&Original_CThingDialog));
        MH_EnableHook(MH_ALL_HOOKS);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        MH_Uninitialize();
    }
    return TRUE;
}
#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include "MinHook.h" 

// =============================================================
// 1. LOGGING
// =============================================================
void Log(const char* fmt, ...) {
    FILE* f;
    if (fopen_s(&f, "C:\\Temp\\FableHook.log", "a+") != 0 || !f) return;
    va_list args;
    va_start(args, fmt);
    fprintf(f, "[HOOK] ");
    vfprintf(f, fmt, args);
    fprintf(f, "\n");
    va_end(args);
    fflush(f);
    fclose(f);
}

// =============================================================
// 2. PATTERN SCANNING
// =============================================================
void* PatternScan(const char* signature, const char* mask) {
    MODULEINFO modInfo = { 0 };
    HMODULE hModule = GetModuleHandle(NULL);
    GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO));
    const char* start = (const char*)modInfo.lpBaseOfDll;
    const char* end = start + modInfo.SizeOfImage;
    size_t sigLen = strlen(mask);
    for (const char* p = start; p < end - sigLen; p++) {
        bool found = true;
        for (size_t i = 0; i < sigLen; i++) {
            if (mask[i] != '?' && p[i] != signature[i]) {
                found = false; break;
            }
        }
        if (found) return (void*)p;
    }
    return nullptr;
}

// =============================================================
// 3. ADDRESSES & OFFSETS
// =============================================================
#define IDA_BASE                        0x00400000

// ADDRESSES
#define IDA_CThingDialog_Ctor           0x008B2410 
#define IDA_GDefStringTable             0x01327F30
#define IDA_PeekDefMgr                  0x00415162
#define IDA_GetThingTypeDefClassName    0x00528240
#define IDA_GetNoInstantiatedDefs       0x00B052D0
#define IDA_GetDefNameFromClassIndex    0x00B05330
#define IDA_GetString                   0x00B28A90
#define IDA_GetGlobalIndex              0x00B05260 
#define IDA_AddEntry                    0x00BB8040
#define IDA_CWideString_Ctor            0x00B02C10 
#define IDA_CWideString_Dtor            0x00B02740 
#define IDA_CCharString_Dtor            0x00AFA3F0 
#define IDA_SortTree                    0x00BB80B0
#define IDA_UpdateTreeView              0x00BB80C0

#define OFFSET_PTreeControl             0x130
#define OFFSET_ControlCentre_Component  0x83C 

intptr_t g_AddressDelta = 0;
void* Rebase(uintptr_t idaAddress) { return (void*)(idaAddress + g_AddressDelta); }

// =============================================================
// 4. TYPES
// =============================================================
struct CThingDialog { char _pad[OFFSET_PTreeControl]; void* PTreeControl; };
struct CDefString { int TablePos; };
struct CCharString { void* PStringData; };
struct CWideString { void* PStringData; };

typedef void* (__thiscall* _PeekDefMgr)(void*);
typedef void* (__thiscall* _GetClassName)(void*, int);
typedef int(__thiscall* _GetNoDefs)(void*, void*);
typedef void(__thiscall* _GetDefName)(void*, CDefString*, void*, int);
typedef void(__thiscall* _GetString)(void*, CCharString*, int);
typedef int(__thiscall* _GetGlobalIndex)(void*, void*, int);
typedef void(__thiscall* _CWideString_Ctor)(CWideString*, const wchar_t*);
typedef void(__thiscall* _CWideString_Dtor)(CWideString*);
typedef void(__thiscall* _CCharString_Dtor)(CCharString*);
typedef int(__thiscall* _AddEntry)(void*, const CWideString*, unsigned int, int, bool);
typedef void(__thiscall* _SortTree)(void*);
typedef void(__thiscall* _UpdateTreeView)(void*);

_PeekDefMgr      fnPeekDefMgr = nullptr;
_GetClassName    fnGetClassName = nullptr;
_GetNoDefs       fnGetNoDefs = nullptr;
_GetDefName      fnGetDefName = nullptr;
_GetString       fnGetString = nullptr;
_GetGlobalIndex  fnGetGlobalIndex = nullptr;
_CWideString_Ctor fnCtorWide = nullptr;
_CWideString_Dtor fnDtorWide = nullptr;
_CCharString_Dtor fnDtorChar = nullptr;
_AddEntry        fnAddEntry = nullptr;
_SortTree        fnSortTree = nullptr;
_UpdateTreeView  fnUpdateTreeView = nullptr;
void* pGDefStringTable = nullptr;

void SetupAddresses() {
    fnPeekDefMgr = (_PeekDefMgr)Rebase(IDA_PeekDefMgr);
    fnGetClassName = (_GetClassName)Rebase(IDA_GetThingTypeDefClassName);
    fnGetNoDefs = (_GetNoDefs)Rebase(IDA_GetNoInstantiatedDefs);
    fnGetDefName = (_GetDefName)Rebase(IDA_GetDefNameFromClassIndex);
    fnGetString = (_GetString)Rebase(IDA_GetString);
    fnGetGlobalIndex = (_GetGlobalIndex)Rebase(IDA_GetGlobalIndex);
    fnCtorWide = (_CWideString_Ctor)Rebase(IDA_CWideString_Ctor);
    fnDtorWide = (_CWideString_Dtor)Rebase(IDA_CWideString_Dtor);
    fnDtorChar = (_CCharString_Dtor)Rebase(IDA_CCharString_Dtor);
    fnAddEntry = (_AddEntry)Rebase(IDA_AddEntry);
    fnSortTree = (_SortTree)Rebase(IDA_SortTree);
    fnUpdateTreeView = (_UpdateTreeView)Rebase(IDA_UpdateTreeView);
    pGDefStringTable = Rebase(IDA_GDefStringTable);
}

// =============================================================
// 5. HELPER CLASSES
// =============================================================

class SafeGameWString {
    CWideString ws;
public:
    SafeGameWString(const wchar_t* text) { fnCtorWide(&ws, text); }
    ~SafeGameWString() { fnDtorWide(&ws); }
    CWideString* ptr() { return &ws; }
};

struct CategoryInfo { const wchar_t* Name; int ID; };
CategoryInfo g_Categories[] = {
    { L"Buildings", 3 },
    { L"Creatures", 1 },
    { L"Player Creatures", 2 },
    { L"Villages", 4 },
    { L"Objects", 5 },
    { L"Sound Emitters", 6 },
    { L"Holy Sites", 7 },
    { L"Switches", 9 },
    { L"Markers", 11 },
};

// Group Cache
struct GroupCache {
    wchar_t name[64];
    int handle;
};
GroupCache g_GroupCache[100];
int g_GroupCount = 0;

int GetOrCreateGroup(void* pTree, const wchar_t* groupName, int parentHandle) {
    for (int i = 0; i < g_GroupCount; i++) {
        if (wcscmp(g_GroupCache[i].name, groupName) == 0) {
            return g_GroupCache[i].handle;
        }
    }
    SafeGameWString wsName(groupName);
    int handle = fnAddEntry(pTree, wsName.ptr(), 0, parentHandle, 0);

    if (g_GroupCount < 100) {
        wcscpy_s(g_GroupCache[g_GroupCount].name, groupName);
        g_GroupCache[g_GroupCount].handle = handle;
        g_GroupCount++;
    }
    return handle;
}

// =============================================================
// 6. HOOK LOGIC
// =============================================================

typedef void* (__thiscall* _CThingDialog_Ctor)(void* pThis, const char* name, void* cc, void* tm);
_CThingDialog_Ctor Original_CThingDialog = nullptr;

void* __fastcall Detour_CThingDialog(CThingDialog* pThis, void* edx,
    const char* name, void* cc, void* tm)
{
    Original_CThingDialog(pThis, name, cc, tm);
    Log("--- Hook Started: Fixed String Pointers & Folders ---");

    void* pTree = pThis->PTreeControl;
    if (!pTree) return pThis;

    void* pComponent = *(void**)((char*)cc + OFFSET_ControlCentre_Component);
    void* pDefMgr = fnPeekDefMgr(pComponent);
    void* pThingMgr = tm;

    for (int i = 0; i < sizeof(g_Categories) / sizeof(CategoryInfo); i++) {

        g_GroupCount = 0;

        // --- LEVEL 1: CATEGORY ---
        SafeGameWString catName(g_Categories[i].Name);
        int catHandle = fnAddEntry(pTree, catName.ptr(), g_Categories[i].ID, 0, 0);

        void* className = fnGetClassName(pThingMgr, g_Categories[i].ID);
        int count = fnGetNoDefs(pDefMgr, className);
        Log("Category: %S (ID: %d) - Found %d items", g_Categories[i].Name, g_Categories[i].ID, count);

        int addedCount = 0;

        for (int j = 1; j < count; j++) {

            int gIndex = fnGetGlobalIndex(pDefMgr, className, j);

            CDefString defStr;
            fnGetDefName(pDefMgr, &defStr, className, j);

            CCharString charName;
            memset(&charName, 0, sizeof(charName));
            fnGetString(pGDefStringTable, &charName, defStr.TablePos);

            if (charName.PStringData) {

                // --- FIX 1: DOUBLE DEREFERENCE ---
                // CCharString.PStringData points to a Data Struct. 
                // The first member of that struct is usually the char* to the actual text.
                // We try to read it as a pointer first.

                const char* pRawAnsi = nullptr;

                // Try Dereferencing (Primary Method for Fable Strings)
                void* pDereferenced = *(void**)charName.PStringData;

                if (!IsBadReadPtr(pDereferenced, 1)) {
                    pRawAnsi = (const char*)pDereferenced;
                }
                // Fallback: If dereferencing failed, maybe it's a direct pointer?
                else if (!IsBadReadPtr(charName.PStringData, 1)) {
                    pRawAnsi = (const char*)charName.PStringData;
                }

                if (pRawAnsi) {

                    wchar_t wBuf[256];
                    if (MultiByteToWideChar(CP_ACP, 0, pRawAnsi, -1, wBuf, 256) > 0) {

                        SafeGameWString safeName(wBuf);

                        // --- FIX 2: RESTORE FOLDERS ---
                        // We parse the string to create subgroups
                        wchar_t groupNameBuf[64];
                        wcscpy_s(groupNameBuf, L"Misc"); // Default

                        // Look for Underscore: "Object_Chair" -> "Object"
                        wchar_t* firstUnderscore = wcschr(wBuf, L'_');
                        if (firstUnderscore) {
                            size_t len = firstUnderscore - wBuf;
                            if (len > 63) len = 63;
                            wcsncpy_s(groupNameBuf, wBuf, len);
                            groupNameBuf[len] = L'\0';
                        }
                        else {
                            // Use "General" if no underscore found
                            wcscpy_s(groupNameBuf, L"General");
                        }

                        // --- LEVEL 2: GROUP ---
                        int groupHandle = GetOrCreateGroup(pTree, groupNameBuf, catHandle);

                        // --- LEVEL 3: ITEM ---
                        fnAddEntry(pTree, safeName.ptr(), gIndex, groupHandle, 1);
                        addedCount++;
                    }
                }
            }
            if (charName.PStringData) fnDtorChar(&charName);
        }
        Log("   > Added: %d", addedCount);
    }

    fnSortTree(pTree);
    fnUpdateTreeView(pTree);

    return pThis;
}

// =============================================================
// 7. MAIN
// =============================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        remove("C:\\Temp\\FableHook.log");
        Log("Initializing FableHook...");

        const char* sig = "\x55\x8B\xEC\x83\xE4\xF8\x81\xEC\xBC\x02\x00\x00";
        const char* mask = "xxxxxxxxxxxx";
        void* realHookAddress = PatternScan(sig, mask);

        if (!realHookAddress) { Log("Scan Failed"); return FALSE; }

        g_AddressDelta = (intptr_t)realHookAddress - (intptr_t)IDA_CThingDialog_Ctor;
        SetupAddresses();

        MH_Initialize();
        MH_CreateHook(realHookAddress, &Detour_CThingDialog, reinterpret_cast<LPVOID*>(&Original_CThingDialog));
        MH_EnableHook(MH_ALL_HOOKS);

        Log("Ready.");
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        MH_Uninitialize();
    }
    return TRUE;
}
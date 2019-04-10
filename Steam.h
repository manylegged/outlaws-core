
#pragma once

#if HAS_GOG

#elif __APPLE__
#  define HAS_STEAM 1
#elif RELEASE
#  if RELEASE_STEAM
#    define HAS_STEAM 1
#  endif
#  if RELEASE_GOG
#    define HAS_GOG 1
#  endif
#else
#  define HAS_STEAM 1
#  define HAS_GOG 1
#endif

#if HAS_STEAM

#  if __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#  endif

#  include "../sdk/public/steam/steam_api.h"

#  if __clang__
#    pragma clang diagnostic pop
#  endif

#  define STEAM_CALLBACK_NAMED( CLASS, NAME )                     \
    STEAM_CALLBACK( CLASS, On ## NAME, NAME ## _t, m_ ##NAME)

#  define STEAM_CALLBACK_CONS( CLASS, NAME )      \
    m_ ## NAME(this, &CLASS::On ## NAME)

#  define STEAM_CALLRESULT( CLASS, NAME )                    \
    void On ## NAME(NAME ## _t *result, bool bIOFailure);  \
    CCallResult<CLASS, NAME ## _t> m_ ## NAME

#else

#  define STEAM_CALLBACK_NAMED( CLASS, NAME )
#  define STEAM_CALLBACK_CONS( CLASS, NAME )
#  define STEAM_CALLRESULT( CLASS, NAME )

typedef uint64 PublishedFileId_t;
    
#endif

bool isSteamInitialized();
bool isGOGInitialized();
bool isGOGLoggedOn();

// stats helper/convenience functions
bool setupSteamStats(const char** stats, const AchievementStat *achievements);
void steamAchievementSet(const char* statname);
void steamStatIncrement(const char* statname, int incr);
void steamStatMax(const char* statname, int value);
void steamStatSet(const char* statname, int value);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// enums -> string

#if HAS_STEAM
const char* EResult2String(EResult res);

#define CHECK_STEAM_RESULT(DATA) \
    DPRINT(STEAM, ("%s -> %s", __func__, EResult2String((DATA)->m_eResult)))

const char* EItemUpdateStatus2String(EItemUpdateStatus status);
const char* EPublishedFileVisibility2String(ERemoteStoragePublishedFileVisibility status);
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// index ISteamRemoteStorage files. Checking for existence is otherwise unexpectedly slow

// faster version of ISteamRemoteStorage::FileExists (actually returns file size)
int SteamFileExists(const string &fname);
bool SteamFileDelete(const char* fname);
bool SteamFileForget(const char* fname);
int SteamFileCount();
void SteamForEachFile(std::function<void(const string&, int)> fun);

// ISteamRemoteStorage::FileWrite / FileRead which use/maintain file index and report errors
extern int g_steamReadsFailed;
bool steamFileWrite(const char* fname, const char* data, int size, int ucsize);
string steamFileRead(int size, const char* fname);
int steamDeleteRecursive(const char *path);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void steamInitialize();
void steamShutdown();

// return 2 letter language code based on steam language
string getSteamLanguageCode();

void registerGOGListener(void *it);

#if HAS_GOG
bool CheckGOGError(const char* func);
#define GOG_CALL(X) ((X), CheckGOGError(#X))
string getGOGFriendList();
#endif

bool isFieldsDLCInstalled();
bool isFieldsDLCOwned();
void checkFieldsDLCInstalled();
const char* fieldsDLCContentRoot();

bool isCloudEnabled();


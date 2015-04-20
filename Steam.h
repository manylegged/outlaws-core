
#ifndef MYSTEAM_H
#define MYSTEAM_H

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#endif

#include "../sdk/public/steam/steam_api.h"

#if __clang__
#pragma clang diagnostic pop
#endif

#define STEAM_CALLBACK_NAMED( CLASS, NAME ) \
    STEAM_CALLBACK( CLASS, On ## NAME, NAME ## _t, m_ ##NAME)

#define STEAM_CALLBACK_CONS( CLASS, NAME )      \
    m_ ## NAME(this, &CLASS::On ## NAME)

#define STEAM_CALLRESULT( CLASS, NAME )                    \
    void On ## NAME(NAME ## _t *result, bool bIOFailure);  \
    CCallResult<CLASS, NAME ## _t> m_ ## NAME


// steam stats/achievements wrapper class
// moves steam API calls to another thread to avoid blocking
struct SteamStats {

private:
    std::unordered_map<std::string, int> m_stats;
    mutable std::mutex                   m_mutex;
    volatile int                         m_pending = 0;
    std::unordered_set< std::string >    m_updates;
public:
    const char**                         steamStats = NULL; // null terminated array of stat names for this game
    
    STEAM_CALLBACK_NAMED(SteamStats, UserStatsReceived);

    SteamStats();

    int GetStat(const char* name);

    void SetStat(const char* name, int value)
    {
        std::lock_guard<std::mutex> l(m_mutex);
        m_stats[name] = value;
        m_updates.insert(name);
    }

    void SetAchievement(const char* name);

    // actually set the stats
    // call periodically from steam thread
    void processCallbacks();

    static SteamStats &instance();
};


// stats helper/convenience functions
    
inline void steamAchievementSet(const char* statname)
{
    SteamStats::instance().SetAchievement(statname);
}

inline void steamStatIncrement(const char* statname, int incr)
{
    SteamStats::instance().SetStat(statname, SteamStats::instance().GetStat(statname) + incr);
}

inline void steamStatMax(const char* statname, int value)
{
    if (value > SteamStats::instance().GetStat(statname))
        SteamStats::instance().SetStat(statname, value);
}

inline void steamStatSet(const char* statname, int value)
{
    SteamStats::instance().SetStat(statname, value);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// enums -> string

 const char* EResult2String(EResult res);

#define CHECK_STEAM_RESULT(DATA) \
    DPRINT(MODS, ("%s -> %s", __func__, EResult2String((DATA)->m_eResult)))

 const char* EItemUpdateStatus2String(EItemUpdateStatus status);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// index ISteamRemoteStorage files. Checking for existence is otherwise unexpectedly slow

// faster version of ISteamRemoteStorage::FileExists (actually returns file size)
int SteamFileExists(const char* fname);
bool SteamFileDelete(const char* fname);

// ISteamRemoteStorage::FileWrite / FileRead which use/maintain file index and report errors
bool steamFileWrite(const char* fname, const char* data, int size, int ucsize);
string steamFileRead(ISteamRemoteStorage *ss, const char* fname);

#endif

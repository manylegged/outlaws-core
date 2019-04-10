
#include "StdAfx.h"
#include "Steam.h"

#if HAS_GOG
#  include "../gog_galaxy/Include/galaxy/GalaxyApi.h"
#endif

static DEFINE_CVAR(bool, kSteamCloudEnable, true);
static DEFINE_CVAR(bool, kForceAppSandbox, false);
static DEFINE_CVAR(bool, kSteamEnable, true);
static DEFINE_CVAR(bool, kSteamBenchmark, false);
static DEFINE_CVAR(bool, kGOGEnable, true);

static DEFINE_CVAR(int, kSteamCallbackInterval, 50);
static DEFINE_CVAR(int, kGOGLoginWaitSecond, 2);

static const char* kGOGClientId = "52046877955667918";
static const uint64_t kGOGProductId = 1487278562;
static const uint64_t kGOGFieldsProductId = 2125664342;

enum GOGLoginState { GOG_DISCONNECTED, GOG_WAIT, GOG_SIGNED_IN, GOG_LOGIN_FAILED };

static OL_Thread s_steamThread;
static GOGLoginState s_gog_state = GOG_DISCONNECTED;
static bool s_gog_logged_on = false;
static bool s_steam_init = false;
static bool s_steam_subscribed = false;

void olSleep(int ms);
bool isOLExiting();

bool isAppSandboxed();

bool isAppSandboxed()
{
#if __APPLE__
    return kForceAppSandbox || OL_IsSandboxed();
#else
    return false;
#endif
}

#if HAS_GOG

std::mutex                              s_gog_mutex;
std::map<galaxy::api::GalaxyID, string> s_gog_friends;
int                                     s_gog_friend_count      = -1;
int                                     s_gog_friend_info_count = 0;


static void finishGOGInit();

bool CheckGOGError(const char* func)
{
    const galaxy::api::IError* error = galaxy::api::GetError();
    if (!error)
        return true;
    DPRINT(GOG, ("%s -> %s: %s", func, error->GetName(), error->GetMsg()));
    return false;
}

static const char* GOGAuthFailureReasonToString(galaxy::api::IAuthListener::FailureReason reason)
{
    switch (reason)
    {
    default:
        CASE_STR(galaxy::api::IAuthListener::FAILURE_REASON_UNDEFINED);
        CASE_STR(galaxy::api::IAuthListener::FAILURE_REASON_GALAXY_SERVICE_NOT_AVAILABLE);
        CASE_STR(galaxy::api::IAuthListener::FAILURE_REASON_GALAXY_SERVICE_NOT_SIGNED_IN);
        CASE_STR(galaxy::api::IAuthListener::FAILURE_REASON_CONNECTION_FAILURE);
        CASE_STR(galaxy::api::IAuthListener::FAILURE_REASON_NO_LICENSE);
        CASE_STR(galaxy::api::IAuthListener::FAILURE_REASON_INVALID_CREDENTIALS);
        CASE_STR(galaxy::api::IAuthListener::FAILURE_REASON_GALAXY_NOT_INITIALIZED);
        CASE_STR(galaxy::api::IAuthListener::FAILURE_REASON_EXTERNAL_SERVICE_FAILURE);
    }
}

struct GOGAuthListener : public galaxy::api::GlobalAuthListener {

    void OnAuthSuccess() override
    {
        DPRINT(GOG, ("OnAuthSuccess"));
        finishGOGInit();
    }

    void OnAuthFailure(galaxy::api::IAuthListener::FailureReason reason) override
    {
        DPRINT(GOG, ("OnAuthFailure: %s", GOGAuthFailureReasonToString(reason)));
        finishGOGInit();
    }

    void OnAuthLost() override
    {
        DPRINT(GOG, ("OnAuthLost"));
    }
};

static const char* GOGStateToString(uint32_t state)
{
    switch (state)
    {
    default: return "none";
    case galaxy::api::IOperationalStateChangeListener::OperationalState::OPERATIONAL_STATE_SIGNED_IN:
        return "signed in";
    case galaxy::api::IOperationalStateChangeListener::OperationalState::OPERATIONAL_STATE_LOGGED_ON:
        return "logged on";
    case galaxy::api::IOperationalStateChangeListener::OperationalState::OPERATIONAL_STATE_SIGNED_IN|
        galaxy::api::IOperationalStateChangeListener::OperationalState::OPERATIONAL_STATE_LOGGED_ON:
        return "logged on and signed in";
    }
}

static void finishGOGInit()
{
    std::unique_lock<std::mutex> l(s_gog_mutex);
    
    if (galaxy::api::Friends())
    {
        // authenticated with GOG client
        const bool signed_in = galaxy::api::User()->SignedIn();
        // also connected to the internet and authenticated with GOG server
        const bool logged_on = galaxy::api::User()->IsLoggedOn();

        const char* name = signed_in ? galaxy::api::Friends()->GetPersonaName() : "";
        CheckGOGError("GetPersonaName");
        uint64 uid = galaxy::api::User()->GetGalaxyID().GetRealID();
        CheckGOGError("GetGalaxyID");
        
        DPRINT(GOG, ("User '%s' %lld signed in: %s logged on: %s", name, uid,
                     signed_in ? "YES" : "NO",
                     logged_on ? "YES" : "NO"));

        s_gog_state = signed_in ? GOG_SIGNED_IN : GOG_LOGIN_FAILED;
        s_gog_logged_on = logged_on;
    }

    if (galaxy::api::Utils())
    {
        galaxy::api::OverlayState state = galaxy::api::Utils()->GetOverlayState();
        DPRINT(GOG, ("OverlayState: %s",
                     (state == galaxy::api::OVERLAY_STATE_UNDEFINED) ? "UNDEFINED" :
                     (state == galaxy::api::OVERLAY_STATE_NOT_SUPPORTED) ? "NOT_SUPPORTED" :
                     (state == galaxy::api::OVERLAY_STATE_DISABLED) ? "DISABLED" :
                     (state == galaxy::api::OVERLAY_STATE_FAILED_TO_INITIALIZE) ? "FAILED_TO_INITIALIZE" :
                     (state == galaxy::api::OVERLAY_STATE_INITIALIZED) ? "INITIALIZED" : "UNKNOWN"));
    }
}

struct GOGStateListener : public galaxy::api::GlobalOperationalStateChangeListener {

    void OnOperationalStateChanged(uint32_t operationalState) override
    {
        DPRINT(GOG, ("OnOperationalStateChanged: %s", GOGStateToString(operationalState)));
    }
};

struct GOGFriendListener : public galaxy::api::GlobalFriendListListener {

    void OnFriendListRetrieveSuccess() override
    {
        std::lock_guard<std::mutex> l(s_gog_mutex);
        
        uint32_t count = galaxy::api::Friends()->GetFriendCount();
        CheckGOGError("GetFriendCount");
        s_gog_friend_count = count;
        char buf[128] = {};
        
        for (uint32_t i=0; i<count; i++)
        {
            galaxy::api::GalaxyID fid = galaxy::api::Friends()->GetFriendByIndex(i);
            CheckGOGError("GetFriendByIndex");
            buf[0] = '\0';
            if (GOG_CALL( galaxy::api::Friends()->GetFriendPersonaNameCopy(fid, buf, sizeof(buf)) )
                && buf[0])
            {
                s_gog_friends[fid] = string(buf, str_len(buf));
                s_gog_friend_info_count++;
            }
            else
            {
                s_gog_friends[fid] = string();
            }
        }
        // should call RequestUserInformation automatically
    }

    void OnFriendListRetrieveFailure(FailureReason failureReason) override
    {
        DPRINT(GOG, ("OnFriendListRetrieveFailure: %s",
                     (failureReason == FAILURE_REASON_UNDEFINED) ? "UNDEFINED" :
                     (failureReason == FAILURE_REASON_CONNECTION_FAILURE) ? "CONNECTION_FAILURE" :
                     "UNKNOWN"));
    }
};

struct GOGUserListener : public galaxy::api::GlobalUserInformationRetrieveListener {

    void OnUserInformationRetrieveSuccess(galaxy::api::GalaxyID userID) override
    {
        std::lock_guard<std::mutex> l(s_gog_mutex);
        string &name = s_gog_friends[userID];
        if (name.empty())
        {
            char buf[128] = {};
            GOG_CALL( galaxy::api::Friends()->GetFriendPersonaNameCopy(userID, buf, sizeof(buf)) );
            if (buf[0])
            {
                name = string(buf, str_len(buf));
                s_gog_friend_info_count++;
            }
        }
    }

    void OnUserInformationRetrieveFailure(galaxy::api::GalaxyID userID, FailureReason failureReason) override
    {
        DPRINT(GOG, ("OnUserInformationRetrieveFailure: %s",
                     (failureReason == FAILURE_REASON_UNDEFINED) ? "UNDEFINED" :
                     (failureReason == FAILURE_REASON_CONNECTION_FAILURE) ? "CONNECTION_FAILURE" :
                     "UNKNOWN"));
        s_gog_friend_info_count = -1;
    }
};

string getGOGFriendList()
{
    if (!isGOGInitialized())
        return string();

    if (s_gog_friend_count == -1)
    {
        if (!GOG_CALL( galaxy::api::Friends()->RequestFriendList() ))
            return string();

        std::unique_lock<std::mutex> l(s_gog_mutex);
        while (s_gog_friend_count == -1 || s_gog_friend_count < s_gog_friend_info_count)
        {
            l.unlock();
            olSleep(10);
            if (isOLExiting() || s_gog_friend_info_count == -1)
                return string();
            l.lock();
        }
    }

    string s;
    for_ (it, s_gog_friends)
    {
        if (it.second.empty())
            continue;
        if (s.size())
            s +=",";
        s += it.second;
    }
    return s;
}

vector<galaxy::api::IGalaxyListener*> s_gog_listeners;

#endif

void registerGOGListener(void *it)
{
#if HAS_GOG
    s_gog_listeners.push_back((galaxy::api::IGalaxyListener*)it);
#endif
}


bool isSteamInitialized()
{
    return s_steam_subscribed;
}

bool isGOGInitialized()
{
    return s_gog_state == GOG_SIGNED_IN;
}

bool isGOGLoggedOn()
{
    return s_gog_logged_on;
}


// steam stats/achievements wrapper class
// moves steam API calls to another thread to avoid blocking
struct SteamStats {

private:
    std::unordered_map<std::string, bool> m_achievements;
    std::unordered_map<std::string, int> m_stats;
    mutable std::mutex                   m_mutex;
    volatile int                         m_pending = 0;
    std::unordered_set< std::string >    m_updates;
    const char**                         m_steamStats = NULL; // null terminated array of stat names for this game
    const AchievementStat *              m_gog_achievements = NULL;
    bool                                 m_ready = false;
public:
    std::mutex                           deleteMutex;
    std::mutex                           indexMutex;
    
    STEAM_CALLBACK_NAMED(SteamStats, UserStatsReceived);

    SteamStats();

    void Init(const char** stats, const AchievementStat *achievements)
    {
        m_steamStats = stats;
        m_gog_achievements = achievements;
        
#if HAS_STEAM
        if (SteamUserStats())
            SteamUserStats()->RequestCurrentStats();
#endif

#if HAS_GOG
        if (isGOGInitialized() && galaxy::api::Stats())
            galaxy::api::Stats()->RequestUserStatsAndAchievements();
#endif
    }

    int GetStat(const char* name);

    void SetStat(const char* name, int value)
    {
        std::lock_guard<std::mutex> l(m_mutex);
        if (map_get(m_stats, name, -value) == value)
            return;
        m_stats[name] = value;
        m_updates.insert(name);
    }

    void SetAchievement(const char* name);

    void ReadStats();

    // actually set the stats
    // call periodically from steam thread
    void processCallbacks();

    static SteamStats &instance();
};

bool setupSteamStats(const char** stats, const AchievementStat *achievements)
{
#if HAS_GOG
    {
        std::unique_lock<std::mutex> l(s_gog_mutex);
        if (s_gog_state == GOG_WAIT)
        {
            return false;
        }
    }
#endif
    
    SteamStats::instance().Init(stats, achievements);
    return true;
}

void steamAchievementSet(const char* statname)
{
    SteamStats::instance().SetAchievement(statname);
}

void steamStatIncrement(const char* statname, int incr)
{
    SteamStats::instance().SetStat(statname, SteamStats::instance().GetStat(statname) + incr);
}

void steamStatMax(const char* statname, int value)
{
    if (value > SteamStats::instance().GetStat(statname))
        SteamStats::instance().SetStat(statname, value);
}

void steamStatSet(const char* statname, int value)
{
    SteamStats::instance().SetStat(statname, value);
}


#if HAS_STEAM
SteamStats::SteamStats() : STEAM_CALLBACK_CONS(SteamStats, UserStatsReceived)
#else
SteamStats::SteamStats()
#endif
{
}

void SteamStats::ReadStats()
{
    std::lock_guard<std::mutex> l(m_mutex);
    for (const char** ptr=m_steamStats; *ptr; ++ptr)
    {
        const char *name = *ptr;
        int &val = m_stats[name];
#if HAS_STEAM
        if (SteamUserStats())
        {
            bool success = SteamUserStats()->GetStat(name, &val);
            DPRINT(STEAM, ("Read stat %s=%d: %s", name, val, success ? "OK" : "FAILED"));
        }
#endif
#if HAS_GOG
        if (isGOGInitialized() && galaxy::api::Stats())
        {
            val = galaxy::api::Stats()->GetStatInt(name);
            bool success = CheckGOGError("GetStatInt");
            DPRINT(GOG, ("Read stat %s=%d: %s", name, val, success ? "OK" : "FAILED"));
        }
#endif
    }

    m_ready = true;
}

#if HAS_GOG

struct GOGStatsListener : public galaxy::api::GlobalUserStatsAndAchievementsRetrieveListener {

    void OnUserStatsAndAchievementsRetrieveSuccess(galaxy::api::GalaxyID userID) override
    {
        DPRINT(GOG, ("OnUserStatsAndAchievementsRetrieveSuccess"));
        SteamStats::instance().ReadStats();
    }

    void OnUserStatsAndAchievementsRetrieveFailure(galaxy::api::GalaxyID userID, FailureReason failureReason) override
    {
        DPRINT(GOG, ("OnUserStatsAndAchievementsRetrieveFailure: %s",
                     (failureReason == FAILURE_REASON_CONNECTION_FAILURE) ? "CONNECTION_FAILURE" :
                     (failureReason == FAILURE_REASON_UNDEFINED) ? "UNDEFINED" : "UNKNOWN"));
    }
    
};


#endif

int SteamStats::GetStat(const char* name)
{
    std::lock_guard<std::mutex> l(m_mutex);
    auto it = m_stats.find(name);
    if (it != m_stats.end())
        return it->second;
    int32_t val = 0;
    
#if HAS_STEAM
    if (SteamUserStats())
    {
        bool success = SteamUserStats()->GetStat(name, &val);
        m_stats[name] = val;
        DPRINT(STEAM, ("Fallback Read stat %s=%d: %s", name, val, success ? "OK" : "FAILED"));
        return val;
    }
#endif

#if HAS_GOG
    if (isGOGInitialized() && galaxy::api::Stats())
    {
        val = galaxy::api::Stats()->GetStatInt(name);
        m_stats[name] = val;
        bool success = CheckGOGError("GetStatInt");
        DPRINT(GOG, ("Fallback Read stat %s=%d: %s", name, val, success ? "OK" : "FAILED"));
        return val;
    }
#endif

    return val;
}

void SteamStats::SetAchievement(const char* name)
{
    if (m_achievements[name])
        return;
    bool aok = false;
#if HAS_STEAM
    if (SteamUserStats())
    {
        aok = SteamUserStats()->SetAchievement(name);
        DPRINT(STEAM, ("Set Achievement %s: %s", name, aok ? "OK" : "FAILED"));
    }
#endif

#if HAS_GOG
    if (isGOGInitialized() && galaxy::api::Stats())
    {
        aok = GOG_CALL( galaxy::api::Stats()->SetAchievement(name) );
        DPRINT(GOG, ("Set Achievement %s: %s", name, aok ? "OK" : "FAILED"));
    }
#endif
    
    std::lock_guard<std::mutex> l(m_mutex);
    if (aok)
        m_achievements[name] = true;
    ++m_pending;
}


void SteamStats::processCallbacks()
{
    if (m_ready)
    {
        std::lock_guard<std::mutex> l(SteamStats::instance().deleteMutex);
#if HAS_STEAM
        ISteamUserStats *ss = s_steam_subscribed ? SteamUserStats() : NULL;
#endif
#if HAS_GOG
        galaxy::api::IStats *is = isGOGInitialized() ? galaxy::api::Stats() : NULL;
#endif
        
        if (m_stats.size())
        {
            std::lock_guard<std::mutex> l0(m_mutex);
            foreach (const string &name, m_updates)
            {
                const int value = m_stats[name];
                
#if HAS_STEAM
                if (ss)
                {
                    bool success = ss->SetStat(name.c_str(), value);
                    DPRINT(STEAM, ("Set Stat %s = %d: %s", name.c_str(), value, success ? "OK" : "FAILED"));
                }
#endif
#if HAS_GOG
                if (is)
                {
                    bool success = GOG_CALL( is->SetStatInt(name.c_str(), value) );
                    DPRINT(GOG, ("Set Stat %s = %d: %s", name.c_str(), value, success ? "OK" : "FAILED"));

                    for (const AchievementStat *as=m_gog_achievements; as->minval; ++as)
                    {
                        if (as->stat == name && value >= as->minval)
                        {
                            // SetAchievement(as->achievement);
                            
                            bool &has = m_achievements[as->achievement];
                            if (has)
                                continue;
                            
                            bool aok = GOG_CALL( is->SetAchievement(as->achievement) );
                            DPRINT(GOG, ("Set Achievement %s: %s", as->achievement, aok ? "OK" : "FAILED"));
                            if (aok)
                                has = true;
                            m_pending++;
                        }
                    }
                }
#endif                
            }
            m_pending += m_updates.size();
            m_updates.clear();
        }

        const int pending = m_pending;
        if (pending)
        {
#if HAS_STEAM
            if (ss)
            {
                const bool sok = ss->StoreStats();
                DPRINT(STEAM, ("Stored %d Stats: %s", pending, sok ? "OK" : "FAILED"));
            }
#endif
#if HAS_GOG
            if (is)
            {
                const bool sok = GOG_CALL( is->StoreStatsAndAchievements() );
                DPRINT(GOG, ("Stored %d Stats: %s", pending, sok ? "OK" : "FAILED"));
            }
#endif
            std::lock_guard<std::mutex> l0(m_mutex);
            m_pending -= pending;
        }
    
    }

#if HAS_STEAM
    SteamAPI_RunCallbacks();
#endif

#if HAS_GOG
    galaxy::api::ProcessData();
#endif
}

SteamStats & SteamStats::instance()
{
    static SteamStats it;
    return it;
}

#if HAS_STEAM

void SteamStats::OnUserStatsReceived(UserStatsReceived_t *callback)
{
    DPRINT(STEAM, ("OnUserStatsRetrieved"));
    SteamStats::instance().ReadStats();
}

const char* EResult2String(EResult res)
{
    switch (res)
    {
    case k_EResultOK: return "OK // success";
    case k_EResultFail: return "Fail // generic failure ";
    case k_EResultNoConnection: return "NoConnection // no/failed network connection";
    case k_EResultInvalidPassword: return "InvalidPassword // password/ticket is invalid";
    case k_EResultLoggedInElsewhere: return "LoggedInElsewhere // same user logged in elsewhere";
    case k_EResultInvalidProtocolVer: return "InvalidProtocolVer // protocol version is incorrect";
    case k_EResultInvalidParam: return "InvalidParam // a parameter is incorrect";
    case k_EResultFileNotFound: return "FileNotFound // file was not found";
    case k_EResultBusy: return "Busy // called method busy - action not taken";
    case k_EResultInvalidState: return "InvalidState // called object was in an invalid state";
    case k_EResultInvalidName: return "InvalidName // name is invalid";
    case k_EResultInvalidEmail: return "InvalidEmail // email is invalid";
    case k_EResultDuplicateName: return "DuplicateName // name is not unique";
    case k_EResultAccessDenied: return "AccessDenied // access is denied";
    case k_EResultTimeout: return "Timeout // operation timed out";
    case k_EResultBanned: return "Banned // VAC2 banned";
    case k_EResultAccountNotFound: return "AccountNotFound // account not found";
    case k_EResultInvalidSteamID: return "InvalidSteamID // steamID is invalid";
    case k_EResultServiceUnavailable: return "ServiceUnavailable // The requested service is currently unavailable";
    case k_EResultNotLoggedOn: return "NotLoggedOn // The user is not logged on";
    case k_EResultPending: return "Pending // Request is pending (may be in process, or waiting on third party)";
    case k_EResultEncryptionFailure: return "EncryptionFailure // Encryption or Decryption failed";
    case k_EResultInsufficientPrivilege: return "InsufficientPrivilege // Insufficient privilege";
    case k_EResultLimitExceeded: return "LimitExceeded // Too much of a good thing";
    case k_EResultRevoked: return "Revoked // Access has been revoked (used for revoked guest passes)";
    case k_EResultExpired: return "Expired // License/Guest pass the user is trying to access is expired";
    case k_EResultAlreadyRedeemed: return "AlreadyRedeemed // Guest pass has already been redeemed by account, cannot be acked again";
    case k_EResultDuplicateRequest: return "DuplicateRequest // The request is a duplicate and the action has already occurred in the past, ignored this time";
    case k_EResultAlreadyOwned: return "AlreadyOwned // All the games in this guest pass redemption request are already owned by the user";
    case k_EResultIPNotFound: return "IPNotFound // IP address not found";
    case k_EResultPersistFailed: return "PersistFailed // failed to write change to the data store";
    case k_EResultLockingFailed: return "LockingFailed // failed to acquire access lock for this operation";
    case k_EResultLogonSessionReplaced: return "LogonSessionReplaced ";
    case k_EResultConnectFailed: return "ConnectFailed ";
    case k_EResultHandshakeFailed: return "HandshakeFailed ";
    case k_EResultIOFailure: return "IOFailure ";
    case k_EResultRemoteDisconnect: return "RemoteDisconnect ";
    case k_EResultShoppingCartNotFound: return "ShoppingCartNotFound // failed to find the shopping cart requested";
    case k_EResultBlocked: return "Blocked // a user didn't allow it";
    case k_EResultIgnored: return "Ignored // target is ignoring sender";
    case k_EResultNoMatch: return "NoMatch // nothing matching the request found";
    case k_EResultAccountDisabled: return "AccountDisabled ";
    case k_EResultServiceReadOnly: return "ServiceReadOnly // this service is not accepting content changes right now";
    case k_EResultAccountNotFeatured: return "AccountNotFeatured // account doesn't have value, so this feature isn't available";
    case k_EResultAdministratorOK: return "AdministratorOK // allowed to take this action, but only because requester is admin";
    case k_EResultContentVersion: return "ContentVersion // A Version mismatch in content transmitted within the Steam protocol.";
    case k_EResultTryAnotherCM: return "TryAnotherCM // The current CM can't service the user making a request, user should try another.";
    case k_EResultPasswordRequiredToKickSession: return "PasswordRequiredToKickSession // You are already logged in elsewhere, this cached credential login has failed.";
    case k_EResultAlreadyLoggedInElsewhere: return "AlreadyLoggedInElsewhere // You are already logged in elsewhere, you must wait";
    case k_EResultSuspended: return "Suspended // Long running operation (content download) suspended/paused";
    case k_EResultCancelled: return "Cancelled // Operation canceled (typically by user: content download)";
    case k_EResultDataCorruption: return "DataCorruption // Operation canceled because data is ill formed or unrecoverable";
    case k_EResultDiskFull: return "DiskFull // Operation canceled - not enough disk space.";
    case k_EResultRemoteCallFailed: return "RemoteCallFailed // an remote call or IPC call failed";
    case k_EResultPasswordUnset: return "PasswordUnset // Password could not be verified as it's unset server side";
    case k_EResultExternalAccountUnlinked: return "ExternalAccountUnlinked // External account (PSN, Facebook...) is not linked to a Steam account";
    case k_EResultPSNTicketInvalid: return "PSNTicketInvalid // PSN ticket was invalid";
    case k_EResultExternalAccountAlreadyLinked: return "ExternalAccountAlreadyLinked // External account (PSN, Facebook...) is already linked to some other account, must explicitly request to replace/delete the link first";
    case k_EResultRemoteFileConflict: return "RemoteFileConflict // The sync cannot resume due to a conflict between the local and remote files";
    case k_EResultIllegalPassword: return "IllegalPassword // The requested new password is not legal";
    case k_EResultSameAsPreviousValue: return "SameAsPreviousValue // new value is the same as the old one ( secret question and answer )";
    case k_EResultAccountLogonDenied: return "AccountLogonDenied // account login denied due to 2nd factor authentication failure";
    case k_EResultCannotUseOldPassword: return "CannotUseOldPassword // The requested new password is not legal";
    case k_EResultInvalidLoginAuthCode: return "InvalidLoginAuthCode // account login denied due to auth code invalid";
    case k_EResultAccountLogonDeniedNoMail: return "AccountLogonDeniedNoMail // account login denied due to 2nd factor auth failure - and no mail has been sent";
    case k_EResultHardwareNotCapableOfIPT: return "HardwareNotCapableOfIPT // ";
    case k_EResultIPTInitError: return "IPTInitError // ";
    case k_EResultParentalControlRestricted: return "ParentalControlRestricted // operation failed due to parental control restrictions for current user";
    case k_EResultFacebookQueryError: return "FacebookQueryError // Facebook query returned an error";
    case k_EResultExpiredLoginAuthCode: return "ExpiredLoginAuthCode // account login denied due to auth code expired";
    case k_EResultIPLoginRestrictionFailed: return "IPLoginRestrictionFailed ";
    case k_EResultAccountLockedDown: return "AccountLockedDown ";
    case k_EResultAccountLogonDeniedVerifiedEmailRequired: return "AccountLogonDeniedVerifiedEmailRequired ";
    case k_EResultNoMatchingURL: return "NoMatchingURL ";
    case k_EResultBadResponse: return "BadResponse // parse failure, missing field, etc.";
    case k_EResultRequirePasswordReEntry: return "RequirePasswordReEntry // The user cannot complete the action until they re-enter their password";
    case k_EResultValueOutOfRange: return "ValueOutOfRange // the value entered is outside the acceptable range";
    case k_EResultUnexpectedError: return "UnexpectedError // something happened that we didn't expect to ever happen";
    case k_EResultDisabled: return "Disabled // The requested service has been configured to be unavailable";
    case k_EResultInvalidCEGSubmission: return "InvalidCEGSubmission // The set of files submitted to the CEG server are not valid !";
    case k_EResultRestrictedDevice: return "RestrictedDevice // The device being used is not allowed to perform this action";
    case k_EResultRegionLocked: return "RegionLocked // The action could not be complete because it is region restricted";
    case k_EResultRateLimitExceeded: return "RateLimitExceeded // Temporary rate limit exceeded, try again later, different from k_EResultLimitExceeded which may be permanent";
    case k_EResultAccountLoginDeniedNeedTwoFactor: return "AccountLoginDeniedNeedTwoFactor // Need two-factor code to login";
    case k_EResultItemDeleted: return "ItemDeleted // The thing we're trying to access has been deleted";
    case k_EResultAccountLoginDeniedThrottle: return "AccountLoginDeniedThrottle // login attempt failed, try to throttle response to possible attacker";
    case k_EResultTwoFactorCodeMismatch: return "TwoFactorCodeMismatch // two factor code mismatch";
    case k_EResultTwoFactorActivationCodeMismatch: return "TwoFactorActivationCodeMismatch // activation code for two-factor didn't match";
    case k_EResultAccountAssociatedToMultiplePartners: return "AccountAssociatedToMultiplePartners // account has been associated with multiple partners";
    case k_EResultNotModified: return "NotModified // data not modified";
    default: return "Unknown EResult";
    }
}

const char* EItemUpdateStatus2String(EItemUpdateStatus status)
{
    switch (status)
    {
	case k_EItemUpdateStatusInvalid: return "handle was invalid, job might be finished, listen too SubmitItemUpdateResult_t";
    case k_EItemUpdateStatusPreparingConfig: return "processing configuration data";
    case k_EItemUpdateStatusPreparingContent: return "reading and processing content files";
    case k_EItemUpdateStatusUploadingContent: return "uploading content changes to Steam";
    case k_EItemUpdateStatusUploadingPreviewFile: return "uploading new preview file image";
    case k_EItemUpdateStatusCommittingChanges: return "committing all changes";
    default: return "Unkown EUpdateStatus";
    }
}

const char* EPublishedFileVisibility2String(ERemoteStoragePublishedFileVisibility status)
{
    switch (status)
    {
    case k_ERemoteStoragePublishedFileVisibilityPublic: return "Public";
    case k_ERemoteStoragePublishedFileVisibilityFriendsOnly: return "FriendsOnly";
    case k_ERemoteStoragePublishedFileVisibilityPrivate: return "Private";
    default: return "Unknown Visibility";
    }
}

#endif

static unordered_map<string, int> &cloudIndex()
{
    static unordered_map<string, int> *files = NULL;
    if (!files)
    {
        // std::lock_guard<std::mutex> m(SteamStats::instance().indexMutex);
        files = new unordered_map<string, int>();
#if HAS_STEAM
        ISteamRemoteStorage *ss = SteamRemoteStorage();
        if (ss)
        {
            const int32 count = ss->GetFileCount();
            int totalBytes = 0;
            for (int i=0; i<count; i++)
            {
                int32 size = 0;
                const char* name = ss->GetFileNameAndSize(i, &size);
                if (name)
                {
                    (*files)[name] = size;
                    totalBytes += size;
                    DPRINT(SAVE, ("steam cloud/%s %d bytes", name, size));
                }
            }
            DPRINT(STEAM, ("Created Steam Cloud file index with %d files, %s",
                           (int)files->size(), str_bytes_format(totalBytes).c_str()));
        }
#endif

#if HAS_GOG
        galaxy::api::IStorage *is = galaxy::api::Storage();
        if (is)
        {
            const uint32_t count = is->GetFileCount();
            if (count)
            {
                int totalBytes = 0;

                for (int i=0; i<count; i++)
                {
                    const char* name = is->GetFileNameByIndex(i);
                    CheckGOGError("GetFileNameByIndex");
                    if (name)
                    {
                        uint32_t size = is->GetFileSize(name);
                        CheckGOGError("GetFileSize()");
                        (*files)[name] = size;
                        totalBytes += size;
                        DPRINT(SAVE, ("gog cloud/%s %d bytes", name, size));
                    }
                }
                DPRINT(GOG, ("Created GOG Galaxy Cloud file index with %d files, %s",
                             (int)files->size(), str_bytes_format(totalBytes).c_str()));
            }
        }
#endif

    }
    return *files;
}


int SteamFileExists(const string &fname)
{
    std::lock_guard<std::mutex> m(SteamStats::instance().indexMutex);
    return map_get(cloudIndex(), fname, 0);
}

bool SteamFileDelete(const char* fname)
{
    if (!SteamFileExists(fname))
        return false;

    {
        std::lock_guard<std::mutex> m(SteamStats::instance().indexMutex);
        cloudIndex().erase(fname);
    }
    
#if HAS_STEAM
    ISteamRemoteStorage *ss = SteamRemoteStorage();
    if (ss)
    {
        const bool success = ss->FileDelete(fname);
        ASSERTF(success, "FileDelete failed on '%s'", fname);
        return success;
    }
#endif

#if HAS_GOG
    galaxy::api::IStorage *is = galaxy::api::Storage();
    if (is)
    {
        return GOG_CALL( is->FileDelete(fname) );
    }
#endif
    
    return false;
}

bool SteamFileForget(const char* fname)
{
#if HAS_STEAM
    ISteamRemoteStorage *ss = SteamRemoteStorage();
    return ss && ss->FileForget(fname);
#else
    return false;
#endif
}

int SteamFileCount()
{
    std::lock_guard<std::mutex> m(SteamStats::instance().indexMutex);
    return cloudIndex().size();
}

void SteamForEachFile(std::function<void(const string&, int)> fun)
{
    std::lock_guard<std::mutex> m(SteamStats::instance().indexMutex);
    foreach (const auto &it, cloudIndex())
    {
        fun(it.first, it.second);
    }
}

bool isCloudEnabled()
{
    return kSteamCloudEnable && !OLG_UseDevSavePath() &&
        (
#if HAS_STEAM
            SteamRemoteStorage() ||
#endif
#if HAS_GOG
            galaxy::api::Storage() ||
#endif
            false);
}

bool steamFileWrite(const char* fname, const char* data, int size, int ucsize)
{
#if HAS_STEAM || HAS_GOG
#  if HAS_STEAM
    ISteamRemoteStorage *ss = s_steam_subscribed ? SteamRemoteStorage(): NULL;
    if (ss && !ss->FileWrite(fname, data, size))
    {
        int32 totalBytes=0, availableBytes=0;
        ss->GetQuota(&totalBytes, &availableBytes);
        const int32 count = ss->GetFileCount();
        ASSERT_FAILED("FileWrite", "Failed to save '%s' (size=%s, uncompressed=%s) (%d/%d files used) (%s/%s available) (EnabledForAccount=%s, ForApp=%s)",
                      fname,
                      str_bytes_format(size).c_str(),
                      str_bytes_format(ucsize).c_str(),
                      count, 1000,
                      str_bytes_format(availableBytes).c_str(),
                      str_bytes_format(totalBytes).c_str(),
                      ss->IsCloudEnabledForAccount() ? "YES" : "NO",
                      ss->IsCloudEnabledForApp() ? "YES" : "NO");

        ASSERT(size < k_unMaxCloudFileChunkSize);
        OL_ScheduleUploadLog("Steam Write Failed");
        return false;
    }
#  endif

#  if HAS_GOG
    galaxy::api::IStorage *is = galaxy::api::Storage();
    if (is && !GOG_CALL( is->FileWrite(fname, data, size) ))
    {
        ASSERT_FAILED("FileWrite", "failed to save '%s' (size=%s, uncompressed=%s)",
                      fname, str_bytes_format(size).c_str(), str_bytes_format(ucsize).c_str());
        return false;
    }
#  endif

    {
        std::lock_guard<std::mutex> m(SteamStats::instance().indexMutex);
        cloudIndex()[fname] = size;
    }
    DPRINT(SAVE, ("save cloud/%s %d bytes", fname, size));
    return true;
#else
    return false;
#endif
}

int g_steamReadsFailed;

string steamFileRead(int size, const char* fname)
{
    string data;
#if HAS_STEAM || HAS_GOG
    if (!size)
        return data;
    data.resize(size);
    int32 bytes = 0;
#  if HAS_STEAM
    ISteamRemoteStorage *ss = s_steam_subscribed ? SteamRemoteStorage(): NULL;
    if (ss)
        bytes = ss->FileRead(fname, &data[0], size);
#  endif
#  if HAS_GOG
    galaxy::api::IStorage *is = galaxy::api::Storage();
    if (is && bytes == 0)
    {
        bytes = is->FileRead(fname, &data[0], size);
        CheckGOGError("FileRead");
    }
#  endif
    ASSERTF(bytes == size, "Expected %d, got %d: '%s'", size, bytes, fname);
    if (bytes != size)
    {
        g_steamReadsFailed++;
        data.resize(bytes);
    }
    if (bytes) {
        DPRINT(SAVE, ("load cloud/%s %d bytes", fname, bytes));
    }
#endif
    return data;
}


int steamDeleteRecursive(const char *path)
{
    int deleted = 0;
#if HAS_STEAM || HAS_GOG
#  if HAS_STEAM
    if (!SteamRemoteStorage())
        return 0;
#  endif
#  if HAS_GOG
    if (!galaxy::api::Storage())
        return 0;
#  endif
    
    std::lock_guard<std::mutex> l(SteamStats::instance().deleteMutex);
    if (SteamFileDelete(path))
    {
        DPRINT(STEAM, ("Deleted cloud file '%s'", path));
        return 1;
    }
    vector<string> files;
    const string base = str_path_standardize(path) + "/";
    {
        std::lock_guard<std::mutex> m(SteamStats::instance().indexMutex);
        foreach(const auto &it, cloudIndex())
        {
            if (str_startswith(it.first, base))
                files.push_back(it.first);
        }
    }

    for_(name, files)
    {
        if (SteamFileDelete(name.c_str()))
        {
            deleted++;
            DPRINT(SAVE, ("Deleted steam/%s", name.c_str()));
        }
    }
    DPRINT(STEAM, ("Deleted %d/%d cloud files from '%s' (%s)",
        deleted, (int)files.size(), path, (files.size() == 1 ? files[0].c_str() : "")));
#endif
    return deleted;
}


static void runSteamTests()
{
#if HAS_STEAM
    ISteamRemoteStorage *ss = SteamRemoteStorage();
    if (!ss)
        return;

    const double kMicroSec = 1e6;
    const int    kIters    = 5000;

    const char* testfile = "data/save0/map1.lua";
    const char* small_testfile = "data/save0/save.lua";
    const char* output = "data/test_dummy.txt";

    DPRINT(STEAM, ("running benchmarks"));
    
    {
        const double start = OL_GetCurrentTime();
        for (int i=0; i<kIters; i++)
            SteamRemoteStorage();
        const double us = (OL_GetCurrentTime() - start) * kMicroSec / kIters;
        DPRINT(STEAM, ("SteamRemoteStorage() is %.1fus/call", us));
    }
    
    {
        ss->FileExists("data/some_nonexistent_file_name");
        
        const double start = OL_GetCurrentTime();
        for (int i=0; i<kIters; i++)
            ss->FileExists("data/some_nonexistent_file_name");
        const double us = (OL_GetCurrentTime() - start) * kMicroSec / kIters;
        DPRINT(STEAM, ("ISteamRemoteStorage::FileExists is %.1fus/call (missing)", us));
    }

    {
        SteamFileExists("_");
        const double start = OL_GetCurrentTime();
        for (int i=0; i<kIters; i++)
            SteamFileExists("data/some_nonexistent_file_name");
        const double us = (OL_GetCurrentTime() - start) * kMicroSec / kIters;
        DPRINT(STEAM, ("SteamFileExists is %.1fus/call (missing)", us));
    }

    {
        const double start = OL_GetCurrentTime();
        for (int i=0; i<kIters; i++)
            OL_FileDirectoryPathExists("data/some_nonexistent_file_name");
        const double us = (OL_GetCurrentTime() - start) * kMicroSec / kIters;
        DPRINT(STEAM, ("OL_FileDirectoryPathExists is %.1fus/call (missing)", us));
    }

    {
        const double start = OL_GetCurrentTime();
        for (int i=0; i<kIters; i++)
            ss->FileExists(testfile);
        const double us = (OL_GetCurrentTime() - start) * kMicroSec / kIters;
        DPRINT(STEAM, ("ISteamRemoteStorage::FileExists is %.1fus/call (existing)", us));
    }

    {
        const double start = OL_GetCurrentTime();
        for (int i=0; i<kIters; i++)
            SteamFileExists(testfile);
        const double us = (OL_GetCurrentTime() - start) * kMicroSec / kIters;
        DPRINT(STEAM, ("SteamFileExists is %.1fus/call (existing)", us));
    }

    {
        const double start = OL_GetCurrentTime();
        for (int i=0; i<kIters; i++)
            OL_FileDirectoryPathExists(testfile);
        const double us = (OL_GetCurrentTime() - start) * kMicroSec / kIters;
        DPRINT(STEAM, ("OL_FileDirectoryPathExists is %.1fus/call (existing)", us));
    }

    {
        const double start = OL_GetCurrentTime();
        for (int i=0; i<kIters; i++)
            ss->GetFileSize(testfile);
        const double us = (OL_GetCurrentTime() - start) * kMicroSec / kIters;
        DPRINT(STEAM, ("ISteamRemoteStorage::GetFileSize is %.1fus/call", us));
    }

    for (int j=0; j<2; j++)
    {
        string data;
        const char* file = (j == 0) ? testfile : small_testfile;
        const int size = ss->GetFileSize(file);
        data.resize(size);

        {
            const double start = OL_GetCurrentTime();
            for (int i=0; i<kIters; i++)
                ss->FileRead(file, &data[0], size);
            const double us = (OL_GetCurrentTime() - start) * kMicroSec / kIters;
            DPRINT(STEAM, ("ISteamRemoteStorage::FileRead is %.1fus/call (%.1fk file)", us, size / 1024.0));
        }

        {
            const double start = OL_GetCurrentTime();
            for (int i=0; i<kIters; i++)
                ss->FileWrite(output, &data[0], size);
            const double us = (OL_GetCurrentTime() - start) * kMicroSec / kIters;
            DPRINT(STEAM, ("ISteamRemoteStorage::FileWrite is %.1fus/call (%.1fk file)", us, size / 1024.0));
        }
    }
#endif
}

static string getSteamLanguageCode1(const string &lang)
{
    if      (lang == "german")     { return "de"; }
    else if (lang == "polish")     { return "pl"; }
    else if (lang == "spanish")    { return "es"; }
    else if (lang == "swedish")    { return "sv"; }
    else if (lang == "schinese" || str_startswith(lang, "chinese"))   { return "zh"; }
    else if (str_startswith(lang, "portuguese")) { return "pt"; }
    else if (lang == "brazilian")  { return "pt"; }
    else if (lang == "turkish")    { return "tr"; }
    return lang.substr(0, 2);
}

string getSteamLanguageCode()
{
    string code;
#if HAS_STEAM
    if (SteamApps())
    {
        string lang = SteamApps()->GetCurrentGameLanguage();
        code = getSteamLanguageCode1(lang);
        DPRINT(STEAM, ("Language: %s (%s)", lang.c_str(), code.c_str()));
        return code;
    }
#endif
#if HAS_GOG
    if (galaxy::api::Apps())
    {
        string lang = galaxy::api::Apps()->GetCurrentGameLanguage(kGOGProductId);
        code = getSteamLanguageCode1(lang);
        DPRINT(GOG, ("Language: %s (%s)", lang.c_str(), code.c_str()));
        return code;
    }
#endif
    return code;
}

static DEFINE_CVAR(bool, kFieldsDLCEnable, true);

static bool is_fields_dlc_installed = false;
static bool is_fields_dlc_owned = false;

void checkFieldsDLCInstalled()
{
#if IS_DEVEL
    is_fields_dlc_installed = kFieldsDLCEnable && OLG_UseDevSavePath();
#endif
    if (is_fields_dlc_installed)
        return;
#if HAS_STEAM
    ISteamApps *sa = SteamApps();
    is_fields_dlc_installed = sa && sa->BIsDlcInstalled(kSteamAppidFieldsDLC);
    is_fields_dlc_owned = false;
    if (sa)
    {
        const int dc = sa->GetDLCCount();
        for (int i=0; i<dc; i++)
        {
            AppId_t appid = 0;
            bool available = false;
            char name[128];
            bool ok = sa->BGetDLCDataByIndex(i, &appid, &available, name, sizeof(name));

            if (appid == kSteamAppidFieldsDLC && available)
                is_fields_dlc_owned = true;

            DPRINT(STEAM, ("DLC[%d] appid=%d, available=%s, name=\"%s\": %s",
                           i, (int)appid, available ? "YES" : "NO", name, ok ? "OK" : "FAILED"));
        }
        DPRINT(STEAM, ("Fields DLC Installed: %s, Owned: %s",
                       isFieldsDLCInstalled() ? "YES" : "NO",
                       isFieldsDLCOwned() ? "YES" : "NO"));
    }
#endif

#if HAS_GOG
    if (galaxy::api::Apps())
    {
        is_fields_dlc_installed = galaxy::api::Apps()->IsDlcInstalled(kGOGFieldsProductId);
        is_fields_dlc_owned = is_fields_dlc_installed;
        DPRINT(GOG, ("Fields DLC Installed: %s, Owned: %s",
                     isFieldsDLCInstalled() ? "YES" : "NO",
                     isFieldsDLCOwned() ? "YES" : "NO"));
    }
#endif

#if !HAS_STEAM && !HAS_GOG
    is_fields_dlc_installed = OL_FileDirectoryPathExists("fields/gravity_exists.txt");
    is_fields_dlc_owned = is_fields_dlc_installed;
    DPRINT(FILE, ("Fields DLC Installed: %s, Owned: %s",
                  isFieldsDLCInstalled() ? "YES" : "NO",
                  isFieldsDLCOwned() ? "YES" : "NO"));
#endif
}

bool isFieldsDLCInstalled()
{
    return is_fields_dlc_installed;
}

bool isFieldsDLCOwned()
{
    return is_fields_dlc_owned;
}

const char* fieldsDLCContentRoot()
{
    return "fields";
}

#if HAS_STEAM
static void __cdecl SteamAPIDebugTextHook( int nSeverity, const char *pchDebugText )
{
    // nSeverity >= 1 is a warning
    DPRINT(STEAM, ("[%d] %s", nSeverity, pchDebugText));
}
#endif


static void *steamThreadMain(void *arg)
{
    thread_setup("Reasm Steam");
    DPRINT(STEAM, ("Callback thread starting"));
    while (!isOLExiting())
    {
        OL_ThreadBeginIteration();
        SteamStats::instance().processCallbacks();
        OL_ThreadEndIteration();
        olSleep(kSteamCallbackInterval);
    }
    thread_cleanup();
    DPRINT(STEAM, ("Callback thread exiting"));
    return NULL;
}

void steamInitialize()
{
    bool startThread = false;
    
#if HAS_STEAM
    if (kSteamEnable)
    {
        s_steam_init = SteamAPI_Init();
        DPRINT(STEAM, ("Init: %s", s_steam_init ? "OK" : "FAILED"));

        if (SteamClient())
        {
            startThread = true;
            SteamClient()->SetWarningMessageHook( &SteamAPIDebugTextHook );
        }
    
        if (SteamUtils())
        {
            SteamUtils()->SetWarningMessageHook( &SteamAPIDebugTextHook );
        }
        
        if (SteamUser())
        {
            s_steam_subscribed = SteamApps()->BIsSubscribed();
            DPRINT(STEAM, ("User '%s' %lld Subscribed: %s, Logged in: %s",
                           SteamFriends()->GetPersonaName(),
                           SteamUser()->GetSteamID().ConvertToUint64(),
                           s_steam_subscribed ? "YES" : "NO",
                           SteamUser()->BLoggedOn() ? "YES" : "NO"));
        }

        const char* tenfoot = std::getenv("SteamTenfoot");
        if (tenfoot)
            DPRINT(STEAM, ("SteamTenfoot: '%s'", tenfoot));
        // if (std::getenv("SteamTenfoot")) // fullscreen if running big picture
        // OLG_SetFullscreenPref(2);
        
        if (kSteamBenchmark)
            runSteamTests();

    }
#endif

#if HAS_GOG
    if (kGOGEnable)
    {
        static const char* kGOGClientSecret = "02cda2a1f5e0a03a135339ea79db011ad10ee03f7a1b1d4b0b2b2b8402cdaea3";

        GOG_CALL( galaxy::api::Init(galaxy::api::InitOptions(kGOGClientId, kGOGClientSecret)) );

        s_gog_listeners.push_back(new GOGAuthListener());
        s_gog_listeners.push_back(new GOGStateListener());
        s_gog_listeners.push_back(new GOGStatsListener());
        s_gog_listeners.push_back(new GOGFriendListener());
        s_gog_listeners.push_back(new GOGUserListener());

        DPRINT(GOG, ("Init: %s", galaxy::api::User() ? "OK" : "FAILED"));

        if (galaxy::api::User())
        {
            GOG_CALL( galaxy::api::User()->SignInGalaxy() );
            startThread = true;
            s_gog_state = GOG_WAIT;
        }
    }
#endif

    checkFieldsDLCInstalled();
    
    if (startThread)
        s_steamThread = thread_create(steamThreadMain, NULL);
}

void steamShutdown()
{
#if HAS_STEAM
    if (s_steam_init)
    {
        DPRINT(STEAM, ("Shutdown Begins"));
        SteamAPI_Shutdown();
        DPRINT(STEAM, ("Shutdown Ends"));
    }
#endif

#if HAS_GOG
    if (s_gog_listeners.size())
    {
        DPRINT(GOG, ("Shutdown Begins"));
        vec_clear_deep(s_gog_listeners);
        galaxy::api::Shutdown();
        DPRINT(GOG, ("Shutdown Ends"));
    }
#endif


    thread_join(s_steamThread);
}

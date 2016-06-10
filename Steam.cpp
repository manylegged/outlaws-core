
#include "StdAfx.h"

DEFINE_CVAR(bool, kSteamEnable, true);
static DEFINE_CVAR(bool, kSteamBenchmark, false);

SteamStats::SteamStats() : STEAM_CALLBACK_CONS(SteamStats, UserStatsReceived)
{
    if (SteamUserStats())
        SteamUserStats()->RequestCurrentStats();
}


int SteamStats::GetStat(const char* name)
{
    std::lock_guard<std::mutex> l(m_mutex);
    auto it = m_stats.find(name);
    if (it != m_stats.end())
        return it->second;        
    if (!SteamUserStats())
        return 0;

    int32 val = 0;
    bool success = SteamUserStats()->GetStat(name, &val);
    m_stats[name] = val;
    DPRINT(STEAM, ("Fallback Read stat %s=%d: %s", name, val, success ? "OK" : "FAILED"));
    return val;
}

void SteamStats::SetAchievement(const char* name)
{
    ISteamUserStats *ss = SteamUserStats();
    if (!ss || m_achievements[name])
        return;
    const bool aok = ss->SetAchievement(name);
    DPRINT(STEAM, ("Set Achievement %s: %s", name, aok ? "OK" : "FAILED"));
    std::lock_guard<std::mutex> l(m_mutex);
    if (aok)
        m_achievements[name] = true;
    ++m_pending;
}

void SteamStats::processCallbacks()
{
    ISteamUserStats *ss = SteamUserStats();

    if (ss && m_stats.size())
    {
        std::lock_guard<std::mutex> l(m_mutex);
        foreach (const string &name, m_updates)
        {
            const int value = m_stats[name];
            bool success = ss->SetStat(name.c_str(), value);
            DPRINT(STEAM, ("Set Stat %s = %d: %s", name.c_str(), value, success ? "OK" : "FAILED"));
        }
        m_pending += m_updates.size();
        m_updates.clear();
    }

    const int pending = m_pending;
    if (pending && SteamUserStats())
    {
        const bool sok = SteamUserStats()->StoreStats();
        DPRINT(STEAM, ("Stored %d Stats: %s", pending, sok ? "OK" : "FAILED"));
        std::lock_guard<std::mutex> l(m_mutex);
        m_pending -= pending;
    }

    SteamAPI_RunCallbacks();
}


void SteamStats::OnUserStatsReceived(UserStatsReceived_t *callback)
{
    ISteamUserStats *ss = SteamUserStats();
    std::lock_guard<std::mutex> l(m_mutex);
    for (const char** ptr=steamStats; *ptr; ++ptr)
    {
        const char *name = *ptr;
        int &val = m_stats[name];
        bool success = ss->GetStat(name, &val);
        DPRINT(STEAM, ("Read stat %s=%d: %s", name, val, success ? "OK" : "FAILED"));
    }

}

SteamStats & SteamStats::instance()
{
    static SteamStats it;
    return it;
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
    default: return "Unnkown EUpdateStatus";
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

static std::mutex &steamIndexMutex()
{
    static std::mutex m;
    return m;
}

static unordered_map<string, int> &steamIndex()
{
    static unordered_map<string, int> *files = NULL;
    if (!files)
    {
        files = new unordered_map<string, int>();
        ISteamRemoteStorage *ss = SteamRemoteStorage();
        if (ss)
        {
            std::lock_guard<std::mutex> m(steamIndexMutex());
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
                }
            }
            DPRINT(STEAM, ("Created Steam Cloud file index with %d files, %s",
                           (int)files->size(), str_bytes_format(totalBytes).c_str()));
        }
    }
    return *files;
}

int SteamFileExists(const char* fname)
{
    return map_get(steamIndex(), fname, 0);
}

bool SteamFileDelete(const char* fname)
{
    ISteamRemoteStorage *ss = SteamRemoteStorage();
    if (!ss || !SteamFileExists(fname))
        return false;
    
    {
        std::lock_guard<std::mutex> m(steamIndexMutex());
        steamIndex().erase(fname);
    }
    const bool success = ss->FileDelete(fname);
    ASSERTF(success, "FileDelete failed on '%s'", fname);
    return success;
}

bool SteamFileForget(const char* fname)
{
    ISteamRemoteStorage *ss = SteamRemoteStorage();
    return ss && ss->FileForget(fname);
}

int SteamFileCount()
{
    std::lock_guard<std::mutex> m(steamIndexMutex());
    return steamIndex().size();
}

void SteamForEachFile(std::function<void(const string&, int)> fun)
{
    if (!SteamRemoteStorage())
        return;
    std::lock_guard<std::mutex> m(steamIndexMutex());
    foreach (const auto &it, steamIndex())
    {
        fun(it.first, it.second);
    }
}

bool steamFileWrite(const char* fname, const char* data, int size, int ucsize)
{
    ISteamRemoteStorage *ss = SteamRemoteStorage();
    if (!ss->FileWrite(fname, data, size))
    {
        int32 totalBytes=0, availableBytes=0;
        ss->GetQuota(&totalBytes, &availableBytes);
        const int32 count = SteamRemoteStorage()->GetFileCount();
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

    {
        std::lock_guard<std::mutex> m(steamIndexMutex());
        steamIndex()[fname] = size;
    }
    DPRINT(SAVE, ("save steam/%s", fname));
    return true;
}

int g_steamReadsFailed;

string steamFileRead(ISteamRemoteStorage *ss, const char* fname)
{
    const int32 size = SteamFileExists(fname);
    if (!size) {
        ASSERT_FAILED("FileRead", "File is empty: '%s'", fname);
        return string();
    }
    string data;
    data.resize(size);
    const int32 bytes = ss->FileRead(fname, &data[0], size);
    ASSERTF(bytes == size, "Expected %d, got %d: '%s'", size, bytes, fname);
    if (bytes != size)
    {
        g_steamReadsFailed++;
    }
    data.resize(bytes);
    if (bytes) {
        DPRINT(SAVE, ("load steam/%s", fname));
    }
    return data;
}

int steamDeleteRecursive(const char *path)
{
    ISteamRemoteStorage *ss = SteamRemoteStorage();
    if (!ss)
        return 0;
    if (SteamFileDelete(path))
    {
        DPRINT(STEAM, ("Deleted cloud file '%s'", path));
        return 1;
    }
    int deleted = 0;
    vector<string> files;
    const string base = str_path_standardize(path) + "/";
    {
        std::lock_guard<std::mutex> m(steamIndexMutex());
        foreach(const auto &it, steamIndex())
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
    return deleted;
}


static void runSteamTests()
{
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
}

static string getSteamLanguageCode1(const string &lang)
{
    if      (lang == "german")     { return "de"; }
    else if (lang == "polish")     { return "pl"; }
    else if (lang == "spanish")    { return "es"; }
    else if (lang == "swedish")    { return "sv"; }
    else if (lang == "chinese")    { return "zh"; }
    else if (lang == "portuguese") { return "pt"; }
    else if (lang == "brazilian")  { return "pt"; }
    else if (lang == "turkish")    { return "tr"; }
    return lang.substr(0, 2);
}

string getSteamLanguageCode()
{
    if (!SteamApps())
        return string();
    string lang = SteamApps()->GetCurrentGameLanguage();
    string code = getSteamLanguageCode1(lang);
    DPRINT(STEAM, ("Language: %s (%s)", lang.c_str(), code.c_str()));
    return code;
}

static void __cdecl SteamAPIDebugTextHook( int nSeverity, const char *pchDebugText )
{
    // nSeverity >= 1 is a warning
    DPRINT(STEAM, ("%s", pchDebugText));
}


bool steamInitialize()
{
    if (!kSteamEnable)
        return false;
    const bool steam = SteamAPI_Init();
    DPRINT(STEAM, ("Init: %s", steam ? "OK" : "FAILED"));

    if (SteamClient())
    {
        SteamClient()->SetWarningMessageHook( &SteamAPIDebugTextHook );
    }
        
    if (SteamUser())
    {
        DPRINT(STEAM, ("User '%s' %lld Logged in: %s",
                       SteamFriends()->GetPersonaName(),
                       SteamUser()->GetSteamID().ConvertToUint64(),
                       SteamUser()->BLoggedOn() ? "YES" : "NO"));
    }

    if (kSteamBenchmark)
        runSteamTests();

    return steam;
}

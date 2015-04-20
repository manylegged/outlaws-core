
#ifndef MYSTEAM_H
#define MYSTEAM_H

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

    SteamStats() : STEAM_CALLBACK_CONS(SteamStats, UserStatsReceived)
    {
        if (SteamUserStats())
            SteamUserStats()->RequestCurrentStats();
    }

    int GetStat(const char* name)
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

    void SetStat(const char* name, int value)
    {
        std::lock_guard<std::mutex> l(m_mutex);
        m_stats[name] = value;
        m_updates.insert(name);
    }

    void SetAchievement(const char* name)
    {
        ISteamUserStats *ss = SteamUserStats();
        if (!ss)
            return;
        const bool aok = ss->SetAchievement(name);
        DPRINT(STEAM, ("Set Achievement %s: %s", name, aok ? "OK" : "FAILED"));

        std::lock_guard<std::mutex> l(m_mutex);
        ++m_pending;
    }

    // actually set the stats
    // call periodically from steam thread
    void processCallbacks()
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

    static SteamStats &instance()
    {
        static SteamStats it;
        return it;
    }
};


inline void SteamStats::OnUserStatsReceived(UserStatsReceived_t *callback)
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

inline const char* EResult2String(EResult res)
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

#define CHECK_STEAM_RESULT(DATA) \
    DPRINT(MODS, ("%s -> %s", __func__, EResult2String((DATA)->m_eResult)))


inline const char* EItemUpdateStatus2String(EItemUpdateStatus status)
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


#endif

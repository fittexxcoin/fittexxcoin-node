// Copyright (c) 2014-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2020 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/fittexxcoin-config.h>
#endif

#include <timedata.h>

#include <netaddress.h>
#include <sync.h>
#include <threadsafety.h>
#include <ui_interface.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <warnings.h>

static RecursiveMutex cs_nTimeOffset;
static int64_t nTimeOffset GUARDED_BY(cs_nTimeOffset) = 0;

/**
 * "Never go to sea with two chronometers; take one or three."
 * Our three time sources are:
 *  - System clock
 *  - Median of other nodes clocks
 *  - The user (asking the user to fix the system clock if the first two
 * disagree)
 */
int64_t GetTimeOffset() {
    LOCK(cs_nTimeOffset);
    return nTimeOffset;
}

int64_t GetAdjustedTime() {
    return GetTime() + GetTimeOffset();
}

static uint64_t abs64(int64_t n) {
    const uint64_t un = n;
    return (n >= 0) ? un : -un;
}

#define FITTEXXCOIN_TIMEDATA_MAX_SAMPLES 200

void AddTimeData(const CNetAddr &ip, int64_t nOffsetSample) {
    LOCK(cs_nTimeOffset);
    // Ignore duplicates
    static std::set<CNetAddr> setKnown GUARDED_BY(cs_nTimeOffset);
    if (setKnown.size() == FITTEXXCOIN_TIMEDATA_MAX_SAMPLES) {
        return;
    }
    if (!setKnown.insert(ip).second) {
        return;
    }

    // Add data
    static CMedianFilter<int64_t> vTimeOffsets GUARDED_BY(cs_nTimeOffset) {FITTEXXCOIN_TIMEDATA_MAX_SAMPLES, 0};
    vTimeOffsets.input(nOffsetSample);
    LogPrint(BCLog::NET,
             "added time data, samples %d, offset %+d (%+d minutes)\n",
             vTimeOffsets.size(), nOffsetSample, nOffsetSample / 60);

    // There is a known issue here (see issue #4521):
    //
    // - The structure vTimeOffsets contains up to 200 elements, after which any
    // new element added to it will not increase its size, replacing the oldest
    // element.
    //
    // - The condition to update nTimeOffset includes checking whether the
    // number of elements in vTimeOffsets is odd, which will never happen after
    // there are 200 elements.
    //
    // But in this case the 'bug' is protective against some attacks, and may
    // actually explain why we've never seen attacks which manipulate the clock
    // offset.
    //
    // So we should hold off on fixing this and clean it up as part of a timing
    // cleanup that strengthens it in a number of other ways.
    //
    if (vTimeOffsets.size() >= 5 && vTimeOffsets.size() % 2 == 1) {
        const int64_t nMedian = vTimeOffsets.median();
        const auto & vSorted = vTimeOffsets.sorted();
        // Only let other nodes change our time by so much
        if (abs64(nMedian) <=
            uint64_t(std::max<int64_t>(
                0, gArgs.GetArg("-maxtimeadjustment",
                                DEFAULT_MAX_TIME_ADJUSTMENT)))) {
            nTimeOffset = nMedian;
        } else {
            nTimeOffset = 0;

            static bool fDone;
            if (!fDone) {
                // If nobody has a time different than ours but within 5 minutes
                // of ours, give a warning
                bool fMatch = false;
                for (const int64_t nOffset : vSorted) {
                    if (nOffset != 0 && abs64(nOffset) < 5 * 60) {
                        fMatch = true;
                        break;
                    }
                }

                if (!fMatch) {
                    fDone = true;
                    std::string strMessage =
                        strprintf(_("Please check that your computer's date "
                                    "and time are correct! If your clock is "
                                    "wrong, %s will not work properly."),
                                  PACKAGE_NAME);
                    SetMiscWarning(strMessage);
                    uiInterface.ThreadSafeMessageBox(
                        strMessage, "", CClientUIInterface::MSG_WARNING);
                }
            }
        }

        if (LogAcceptCategory(BCLog::NET)) {
            for (const int64_t n : vSorted) {
                LogPrintToBeContinued(BCLog::NET, "%+d  ", n);
            }

            LogPrintToBeContinued(BCLog::NET, "|  ");
            LogPrint(BCLog::NET, "nTimeOffset = %+d  (%+d minutes)\n",
                     nTimeOffset, nTimeOffset / 60);
        }
    }
}

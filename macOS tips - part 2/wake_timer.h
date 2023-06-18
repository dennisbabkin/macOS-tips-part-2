//
//  wake_timer.h
//  macOS tips - part 2
//
//  Created by dennisbabkin.com on 6/17/23.
//
//  This project is a part of the blog post.
//  For more details, check:
//
//      https://dennisbabkin.com/blog/?i=AAA11500
//
//  Demonstration of how to wake macOS from sleep
//


#ifndef WakeTimer_h
#define WakeTimer_h

#include <stdio.h>
#include <assert.h>

#include <string>
#include <vector>

#include "rdr_wrtr.h"           //Reader/writer lock classes from "macOS tips - part 1"

#include <CoreFoundation/CoreFoundation.h>







struct WakeTimer
{
    ///'pstrTimerBundleID' = string with unique bundle identifier for this timer. ex: "com.dennisbabkin.wake01"
    ///                 INFO: This ID must remain the same between different instances of this app,
    ///                 as it will be saved in the global scope on disk by the macOS
    WakeTimer(const char* pstrTimerBundleID)
    {
        assert(pstrTimerBundleID && pstrTimerBundleID[0]);       //Must be provided
        
        if(pstrTimerBundleID)
        {
            _strTmrBundleID = pstrTimerBundleID;
        }
        
    }
    
    ~WakeTimer()
    {
        //Stop wake event
        stopWakeEvent();
    }
    
    
#define WAKE_TIMER_EVENT_TYPE kIOPMAutoWake
    
    
    ///Set wake event as the relative time from the current moment
    ///WARNING: When wake timer fires it will also temporarily wake the screen for a few seconds!
    ///'msFromNow' = number of ms from "now" to wake the system
    ///'pdtOutWhen' = if not 0, receives UTC date/time when the wake event was set for. Or receives 0 if didn't set it.
    ///RETURN:
    ///     = true if set OK
    bool setWakeEventRelative(UInt32 msFromNow,
                              CFAbsoluteTime* pdtOutWhen = nullptr)
    {
        bool bRes = false;
        
        //Time when to fire the wake event
        CFAbsoluteTime dtWhen = CFAbsoluteTimeGetCurrent() + ((CFTimeInterval)msFromNow) / 1000.0;
        
        if(true)
        {
            //Act from within a lock
            WRITER_LOCK wrl(_lock);
         
            bRes = _setWakeEvent(dtWhen,
                                 WAKE_TIMER_EVENT_TYPE,
                                 pdtOutWhen);
        }
        
        return bRes;
    }
 
    
    ///Set wake event as the absolute time
    ///WARNING: When wake timer fires it will also temporarily wake the screen for a few seconds!
    ///'dtWake' = UTC date/time when to set this wake event (use Set_CFAbsoluteTime() function to create it)
    ///'pdtOutWhen' = if not 0, receives UTC date/time when the wake event was set for. Or receives 0 if didn't set it.
    ///RETURN:
    ///     = true if set OK
    bool setWakeEventAbsolute(CFAbsoluteTime dtWake,
                              CFAbsoluteTime* pdtOutWhen = nullptr)
    {
        bool bRes = false;
        
        if(true)
        {
            //Act from within a lock
            WRITER_LOCK wrl(_lock);
         
            bRes = _setWakeEvent(dtWake,
                                 WAKE_TIMER_EVENT_TYPE,
                                 pdtOutWhen);
        }
        
        return bRes;
    }
    
    
    ///Stop wake event that was set by setWakeEvent*() functions
    ///INFO: Does nothing if event was not set.
    ///RETURN:
    ///     = true if no errors
    bool stopWakeEvent()
    {
        bool bRes = true;
        
        if(true)
        {
            //Act from within a lock
            WRITER_LOCK wrl(_lock);
            
            //Cancel all events
            size_t szCnt;
            if(!_cancelEvents(_strTmrBundleID.c_str(), NULL, szCnt))
            {
                //Failed
                bRes = false;
            }
            
            //Reset parameters
            _dtmWake = 0;
            
            _bWakeEvtSet = false;
        }
        
        return bRes;
    }
    
    
    ///Get this wake event info
    ///'pdtOutWhenWake' = if not 0, receives cached UTC date/time when the wake event was supposed to be set for.
    ///'pstrOutBundleID' = if not 0, receives the bundle ID for this wake event - is always returned, even if return is false
    ///RETURN:
    ///     = true if wake event was set
    ///     = false if wake event was not set - all data returned cannot be used
    bool getWakeEventInfo(CFAbsoluteTime* pdtOutWhenWake = nullptr,
                          std::string* pstrOutBundleID = nullptr)
    {
        bool bResult = false;
        
        CFAbsoluteTime dtmWake = 0;
        std::string strBundle;

        if(true)
        {
            //Act from within a lock
            READER_LOCK rdl(_lock);
            
            dtmWake = _dtmWake;
            strBundle = _strTmrBundleID;
         
            bResult = _bWakeEvtSet;
        }
        
        if(pdtOutWhenWake)
            *pdtOutWhenWake = dtmWake;
        if(pstrOutBundleID)
            *pstrOutBundleID = strBundle;
        
        return bResult;
    }
    
    
    
    ///Cancel specific (wake) event(s)
    ///'pstrBundleID' = bundle ID to cancel events for, ex: "com.dennisbabkin.wake01", or 0 or "" to cancel all events
    ///'pstrEventType' = event type to cancel events for, or 0 or  "" to cancel for all event types.
    ///             Ex. kIOPMAutoWake, kIOPMAutoPowerOn,  or kIOPMAutoWakeOrPowerOn
    ///'pszOutCountCanceled' = if not 0, receives the number of events that were canceled
    ///RETURN:
    ///     = true if no errors
    ///     =false if at least one error took place
    bool cancelEvents(const char* pstrBundleID,
                      const char* pstrEventType,
                      size_t* pszOutCountCanceled = nullptr)
    {
        bool bRes;
        size_t szCnt;
        
        if(true)
        {
            //Act from within a lock
            WRITER_LOCK wrl(_lock);
            
            bRes = _cancelEvents(pstrBundleID, pstrEventType, szCnt);
        }
        
        if(pszOutCountCanceled)
            *pszOutCountCanceled = szCnt;
        
        return bRes;
    }

    
    
    
    ///Put OS to sleep
    ///INFO: Any user can call it. Or, in other words, it does not require administrative permissions.
    ///INFO: This function runs asynchronously, or in other words, it initiates sleep and then returns.
    ///RETURN:
    ///     = kIOReturnSuccess if success
    ///     = Other if error
    static IOReturn performSleep()
    {
        IOReturn iResult;
        
        io_connect_t ioConn = IOPMFindPowerManagement(MACH_PORT_NULL);
        if(ioConn != 0)
        {
            iResult = IOPMSleepSystem(ioConn);
            
            //Free port
            IOServiceClose(ioConn);
        }
        else
        {
            //Failed
            iResult = kIOReturnOffline;
        }
        
        return iResult;
    }
    
    
    ///RETURN:
    ///     = true if full-sleep is supported by hardware
    ///     = false if only doze-sleep is supported
    static bool isFullSleepSupported()
    {
        return IOPMSleepEnabled();
    }
    
    
    
#define DIFF_UNIX_EPOCH_AND_MAC_TIME_SEC 978307200      //Difference in seconds between Mac time and Unix Epoch

    
    ///Set 'pOutDtm' from an absolute date & time (for the local time zone)
    ///RETURN:
    ///     = true if success
    static bool Set_CFAbsoluteTime(CFAbsoluteTime* pOutDtm,
                                   int nYear,       //4-digit year
                                   int nMonth,      //[1-12]
                                   int nDay,        //[1-31]
                                   int nHour,       //[0-23]
                                   int nMinute,     //[0-59]
                                   int nSecond,     //[0-59]
                                   int nMillisecond //[0-999]
                                  )
    {
        bool bRes = false;
        
        CFAbsoluteTime dtm = 0;
        
        struct tm t = {};
        t.tm_year = nYear - 1900;       //years since 1900
        t.tm_mon = nMonth - 1;          //months since January [0-11]
        t.tm_mday = nDay;               //day of the month [1-31]
        t.tm_hour = nHour;              //hours since midnight [0-23]
        t.tm_min = nMinute;             //minutes after the hour [0-59]
        t.tm_sec = nSecond;             //seconds after the minute [0-60]
        
        t.tm_isdst = -1;

        //Convert to number of seconds since midnight Jan 1, 1970
        time_t time = mktime(&t);
        if(time != -1)
        {
            //CFAbsoluteTime: Time in fractional seconds since midnight of Jan 1, 2001
            dtm = time;
            
            //Adjust from Jan 1, 1970 to Jan 1, 2001
            dtm -= DIFF_UNIX_EPOCH_AND_MAC_TIME_SEC;
            
            //And apply milliseconds
            dtm += (double)nMillisecond / 1000.0;
            
            bRes = true;
        }
        
        if(pOutDtm)
            *pOutDtm = dtm;
        
        return bRes;
    }


    
private:
    
    ///IMPORTANT: Must be called from within a lock!
    ///'dtWhen' = date/time in UTC
    ///'pstrEventType' = event type, eg: kIOPMAutoWake, kIOPMAutoPowerOn, or kIOPMAutoWakeOrPowerOn
    ///'pdtOutWhen' = if not 0, receives when wake timer was set, or 0 if error
    ///RETURN:
    ///     = true if success
    bool _setWakeEvent(CFAbsoluteTime dtWhen,
                       const char* pstrEventType,
                       CFAbsoluteTime* pdtOutWhen = nullptr)
    {
        bool bRes = false;
        
        CFDateRef refDtm = CFDateCreate(kCFAllocatorDefault, dtWhen);
        if(refDtm)
        {
            CFStringRef refEventType = CFStringCreateWithCString(kCFAllocatorDefault,
                                                                 pstrEventType,
                                                                 kCFStringEncodingUTF8);

            if(refEventType)
            {
                CFStringRef refID = CFStringCreateWithCString(kCFAllocatorDefault,
                                                              _strTmrBundleID.c_str(),
                                                              kCFStringEncodingUTF8);
                
                if(refID)
                {
                    //Cancel previous wake event (if it was set)
                    size_t szCnt;
                    if(!_cancelEvents(_strTmrBundleID.c_str(),
                                      pstrEventType,
                                      szCnt))
                    {
                        //Failed
                        assert(false);
                    }
                    
                    
                    //Create new wake event
                    IOReturn ioRes = IOPMSchedulePowerEvent(refDtm,
                                                            refID,
                                                            refEventType);
                    
                    if(ioRes == kIOReturnSuccess)
                    {
                        //Done
                        bRes = true;
                        
                        _bWakeEvtSet = true;
                    }
                    else
                    {
                        //Error
                        //eg: kIOReturnNotPrivileged = 0xE00002C1 = -536870207
                        assert(false);
                        
                        //Reset parameters
                        _dtmWake = 0;
                        
                        _bWakeEvtSet = false;
                    }
                 
                    CFRelease(refID);
                    refID = NULL;
                }
                else
                {
                    //Error
                    assert(false);
               }
             
                CFRelease(refEventType);
                refEventType = NULL;
            }
            else
            {
                //Error
                assert(false);
            }

            //Free
            CFRelease(refDtm);
        }
        else
        {
            //Error
            assert(false);
        }
        
        
        //Set time when we set it?
        if(pdtOutWhen)
        {
            //Retrieve when the wake timer was set from the OS itself
            if(!_getWakeEventTime(pdtOutWhen))
            {
                //Error
                assert(false);
            }
        }
        
        return bRes;
    }
    
    
    
    
    ///IMPORTANT: Must be called from within a lock!
    ///'pstrBundleID' = bundle ID to cancel events for, ex: "com.dennisbabkin.wake01", or 0 or "" to cancel all events
    ///'pstrEventType' = event type to cancel events for, or 0 or  "" to cancel for all event types.
    ///             Ex. kIOPMAutoWake, kIOPMAutoPowerOn,  or kIOPMAutoWakeOrPowerOn
    ///'szCntCanceled' = receives the number of events that were canceled
    ///RETURN:
    ///     = true if no errors
    ///     =false if at least one error took place
    bool _cancelEvents(const char* pstrBundleID,
                       const char* pstrEventType,
                       size_t& szCntCanceled)
    {
        bool bResult = true;
        
        szCntCanceled = 0;
        
        CFStringRef refID = NULL;
        if(pstrBundleID &&
           pstrBundleID[0])
        {
            refID = CFStringCreateWithCString(kCFAllocatorDefault,
                                              pstrBundleID,
                                              kCFStringEncodingUTF8);
            if(!refID)
            {
                //Error
                assert(false);
                bResult = false;
            }
        }
        
        CFStringRef refEvtType = NULL;
        if(pstrEventType &&
           pstrEventType[0])
        {
            refEvtType = CFStringCreateWithCString(kCFAllocatorDefault,
                                                   pstrEventType,
                                                   kCFStringEncodingUTF8);
            if(!refEvtType)
            {
                //Error
                assert(false);
                bResult = false;
            }
        }
        
        CFArrayRef refArr = NULL;
        
        
        struct REM_EVT_INFO
        {
            CFDateRef refDate;
            CFStringRef refID;
            CFStringRef refType;
            
            void clearIt()
            {
                refDate = NULL;
                refID = NULL;
                refType = NULL;
            }
        };
        
        
        CFTypeRef resVal;
        REM_EVT_INFO rei;
        
        std::vector<REM_EVT_INFO> arrRem;
        
        
        if(bResult)
        {
            //Enumerate all wake events in the system
            refArr = IOPMCopyScheduledPowerEvents();
            if(refArr)
            {
                CFIndex nCnt = CFArrayGetCount(refArr);
                
                for(CFIndex i = 0; i < nCnt; i++)
                {
                    CFDictionaryRef refDic = (CFDictionaryRef)CFArrayGetValueAtIndex(refArr, i);
                    if(refDic)
                    {
                        CFTypeID type = CFGetTypeID(refDic);
                        if(type == CFDictionaryGetTypeID())
                        {
                            //See if we need to remove this entry
                            rei.clearIt();
                            
                            //Get the ID
                            if(CFDictionaryGetValueIfPresent(refDic,
                                                             CFSTR(kIOPMPowerEventAppNameKey),
                                                             &resVal))
                            {
                                type = CFGetTypeID(resVal);
                                if(type == CFStringGetTypeID())
                                {
                                    //See if it's our ID
                                    if(refID == NULL ||
                                       CFStringCompare(refID,
                                                       (CFStringRef)resVal,
                                                       kCFCompareCaseInsensitive) == kCFCompareEqualTo)
                                    {
                                        //Remove it
                                        rei.refID = (CFStringRef)resVal;
                                    }
                                }
                                else
                                {
                                    //Bad type
                                    assert(false);
                                    bResult = false;
                                }
                            }
                            else
                            {
                                //No such key
                                assert(false);
                                bResult = false;
                            }
                            
                            
                            if(rei.refID)
                            {
                                //See if we need to match the type
                                
                                //Get the event type
                                if(CFDictionaryGetValueIfPresent(refDic,
                                                                 CFSTR(kIOPMPowerEventTypeKey),
                                                                 &resVal))
                                {
                                    type = CFGetTypeID(resVal);
                                    if(type == CFStringGetTypeID())
                                    {
                                        //See if it's our ID
                                        if(refEvtType == NULL ||
                                           CFStringCompare(refEvtType,
                                                           (CFStringRef)resVal,
                                                           0) == kCFCompareEqualTo)
                                        {
                                            //Remove it
                                            rei.refType = (CFStringRef)resVal;
                                        }
                                    }
                                    else
                                    {
                                        //Bad type
                                        assert(false);
                                        bResult = false;
                                    }
                                }
                                else
                                {
                                    //Error
                                    assert(false);
                                    bResult = false;
                                }
                                
                                
                                if(rei.refType)
                                {
                                    //Get the date from this event
                                    if(CFDictionaryGetValueIfPresent(refDic,
                                                                     CFSTR(kIOPMPowerEventTimeKey),
                                                                     &resVal))
                                    {
                                        type = CFGetTypeID(resVal);
                                        if(type == CFDateGetTypeID())
                                        {
                                            rei.refDate = (CFDateRef)resVal;
                                            
                                            //Add it to our list (we'll delete it later)
                                            arrRem.push_back(rei);
                                        }
                                        else
                                        {
                                            //Bad type
                                            assert(false);
                                            bResult = false;
                                        }
                                    }
                                    else
                                    {
                                        //Error
                                        assert(false);
                                        bResult = false;
                                    }
                                }
                            }
                        }
                        else
                        {
                            //Error
                            assert(false);
                            bResult = false;
                        }
                    }
                    else
                    {
                        //Error
                        assert(false);
                        bResult = false;
                    }
                }
            }
            else
            {
                //There are no scheduled events
            }
            
            
            size_t szCntRem = arrRem.size();
            if(szCntRem > 0)
            {
                const REM_EVT_INFO* pREI = arrRem.data();
                for(size_t i = 0; i < szCntRem; i++, pREI++)
                {
                    assert(pREI->refID);
                    assert(pREI->refDate);
                    assert(pREI->refType);
                    
                    IOReturn ioRes = IOPMCancelScheduledPowerEvent(pREI->refDate,
                                                                   pREI->refID,
                                                                   pREI->refType);
                    
                    if(ioRes == kIOReturnSuccess)
                    {
                        //Done
                        szCntCanceled++;
                    }
                    else
                    {
                        //Failed - don't report some errors
                        if(ioRes != kIOReturnNotFound)      //This may happen because of a delay of us finding it and then canceling it later
                        {
                            assert(false);
                            bResult = false;
                        }
                    }
                }
                
            }
        }
        
        //Free mem
        if(refID)
        {
            CFRelease(refID);
            refID = nullptr;
        }
        
        if(refEvtType)
        {
            CFRelease(refEvtType);
            refEvtType = nullptr;
        }
        
        if(refArr)
        {
            CFRelease(refArr);
            refArr = nullptr;
        }
        
        return bResult;
    }
    
    
    ///Retrieves wake date/time from this struct
    ///'pdtOut' = if not 0, receives wake event date/time in UTC
    ///RETURN:
    ///     = true if retrieved OK
    bool _getWakeEventTime(CFAbsoluteTime* pdtOut)
    {
        bool bRes = false;

        CFAbsoluteTime dtWake = 0;
        
        CFStringRef refID = CFStringCreateWithCString(kCFAllocatorDefault,
                                                      _strTmrBundleID.c_str(),
                                                      kCFStringEncodingUTF8);
        if(refID)
        {
            CFTypeRef resVal;
            
            //Enumerate all global wake events
            CFArrayRef refArr = IOPMCopyScheduledPowerEvents();
            if(refArr)
            {
                CFIndex nCnt = CFArrayGetCount(refArr);
                
                for(CFIndex i = 0; i < nCnt; i++)
                {
                    CFDictionaryRef refDic = (CFDictionaryRef)CFArrayGetValueAtIndex(refArr, i);
                    if(refDic &&
                       CFGetTypeID(refDic) == CFDictionaryGetTypeID())
                    {
                        //Find our bundle ID
                        if(CFDictionaryGetValueIfPresent(refDic,
                                                         CFSTR(kIOPMPowerEventAppNameKey),
                                                         &resVal) &&
                           CFGetTypeID(resVal) == CFStringGetTypeID() &&
                           CFStringCompare(refID,
                                           (CFStringRef)resVal,
                                           kCFCompareCaseInsensitive) == kCFCompareEqualTo)
                        {
                            //See if this is the right event type
                            if(CFDictionaryGetValueIfPresent(refDic,
                                                             CFSTR(kIOPMPowerEventTypeKey),
                                                             &resVal) &&
                               CFGetTypeID(resVal) == CFStringGetTypeID() &&
                               CFStringCompare(CFSTR(WAKE_TIMER_EVENT_TYPE),
                                               (CFStringRef)resVal,
                                               0) == kCFCompareEqualTo)
                            {
                                //Get date/time
                                if(CFDictionaryGetValueIfPresent(refDic,
                                                                 CFSTR(kIOPMPowerEventTimeKey),
                                                                 &resVal) &&
                                   CFGetTypeID(resVal) == CFDateGetTypeID())
                                {
                                    //Got it
                                    dtWake = CFDateGetAbsoluteTime((CFDateRef)resVal);
                                    
                                    bRes = true;
                                }
                                else
                                {
                                    assert(false);
                                }
                             
                                break;
                            }
                        }
                    }
                    else
                    {
                        assert(false);
                    }
                }
                
                //Free mem
                CFRelease(refArr);
            }
            else
            {
                assert(false);
            }
            
         
            //Free
            CFRelease(refID);
            refID = NULL;
        }
        else
        {
            assert(false);
        }
        
        if(pdtOut)
            *pdtOut = dtWake;
        
        return bRes;
    }
    

private:
    ///Copy constructor and assignments are NOT available!
    WakeTimer(const WakeTimer& s) = delete;
    WakeTimer& operator = (const WakeTimer& s) = delete;
    
private:
    
    RDR_WRTR _lock;                     //Lock for accessing this struct

    std::string _strTmrBundleID;        //Bundle ID for this timer
    
    bool _bWakeEvtSet = false;          //true if we set the wake event
    
    CFAbsoluteTime _dtmWake = 0;        //UTC date/time when wake event was scheduled
};




#endif /* WakeTimer_h */

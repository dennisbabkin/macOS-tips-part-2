//
//  main.cpp
//  macOS tips - part 2
//
//  Created by dennisbabkin.com on 6/17/23.
//
//  This project is a part of the blog post.
//  For more details, check:
//
//      https://dennisbabkin.com/blog/?i=AAA11500
//



#include <iostream>
#include <string>

#include <assert.h>                     //Assertions
#include <sys/time.h>

#include <Carbon/Carbon.h>
#include <sys/reboot.h>

#include "types.h"
#include "notif_reboot_shutdown.h"
#include "notif_sleep_wake.h"
#include "wake_timer.h"

#include "synched_data.h"               //Synchronization template class from "macOS tips - part 1"
#include "CFString_conv.h"



//Forward function declarations
void addSignalCallbacks(int sig);
void signalCallback(int sig, siginfo_t *info, void *context);
void callback_RebootShutdownLogout(mach_msg_header_t* pHeader,
                                   const char* pPortName,
                                   const void* pParam1,
                                   const void* pParam2);
CURRENT_REBOOT_SHUTDOWN_STATE get_CURRENT_REBOOT_SHUTDOWN_STATE_by_port_name(const char* pPortName);
std::string current_time_as_string();

void callback_SleepWake(natural_t msgType,
                        void *msgArgument,
                        io_connect_t portSleepWake,
                        const void* pParam1,
                        const void* pParam2);

bool RebootShutdownSoft(bool bReboot);
bool RebootShutdownHard(bool bReboot);






//Global variables
static const struct
{
    const char* pName;
    CURRENT_REBOOT_SHUTDOWN_STATE state;
}
gkNotifNames[] =
{
    { kLWShutdownInitiated, CRS_STATE_ShutdownUIShown, },
    { kLWRestartInitiated,  CRS_STATE_RestartUIShown, },
    { kLWLogoutInitiated,   CRS_STATE_LogoutUIShown, },
    { kLWLogoutCancelled,   CRS_STATE_Cancelled, },
    { kLWPointOfNoReturn,   CRS_STATE_PointOfNoReturn, },
};

Notif_RebootShutdown g_Ntfs[SIZEOF(gkNotifNames)];              //Classes to service: reboot, shutdown, logout notifications
SYNCHED_DATA<REBOOT_SHUTDOWN_STATE> g_RebootShutdownState({});  //Current state of the macOS

Notif_SleepWake g_NtfSleepWake;                                 //Class to service: sleep/wake notifications
WakeTimer g_WkTmr("com.dennisbabkin.wake01");                   //Timer for waking macOS from sleep








//Entry point for this executable
//
int main(int argc, const char * argv[])
{
    //Assuming that we're the launch-daemon/agent, we need to handle some signals
    addSignalCallbacks(SIGTERM);
    addSignalCallbacks(SIGINT);
    
    
    //Register to receive notifications of shutdown, reboot & user logout
    static_assert(SIZEOF(g_Ntfs) == SIZEOF(gkNotifNames), "Number of elements in each array must be the same!");
    
    for(int n = 0; n < SIZEOF(g_Ntfs); n++)
    {
        if(!g_Ntfs[n].init_Notifications(gkNotifNames[n].pName,
                                         callback_RebootShutdownLogout))
        {
            //Failed
            assert(false);
        }
    }

    
    //Register to receive sleep/wake notifications
    if(!g_NtfSleepWake.init_SleepWakeNotifications(callback_SleepWake))
    {
        //Failed
        assert(false);
    }
    

    
    //Test wake timer
    if(false)
    {
        //Set to wake up 30 seconds from now
        CFAbsoluteTime dtWhen;
        if(g_WkTmr.setWakeEventRelative(30 * 1000, &dtWhen))        //Specify delay in ms
        {
            std::string strWhen;
            FormatDateTimeAsStr(dtWhen, &strWhen);
            printf("%s > Set wake event for: %s\n",
                   current_time_as_string().c_str(),
                   strWhen.c_str());
            
            //Put macOS to sleep
            IOReturn result = WakeTimer::performSleep();
            assert(result == kIOReturnSuccess);
        }
        else
        {
            //Failed
            assert(false);
        }
    }
    
    
    
    //Enter the run-loop (to process our notifications)
    printf("%s > Ready to listen for power events...\n", current_time_as_string().c_str());
    CFRunLoopRun();
    
    
    
    //Are we rebooting or shutting down?
    REBOOT_SHUTDOWN_STATE rss;
    g_RebootShutdownState.get(&rss);
    bool bRebooting = rss == macOS_State_Rebooting || rss == macOS_State_Shutting_Down;

    //Unregister notifications
    for(int n = 0; n < SIZEOF(g_Ntfs); n++)
    {
        if(!g_Ntfs[n].remove_Notifications(bRebooting))
        {
            //Failed
            assert(false);
        }
    }
    
    //Unregister sleep/wake notifications
    if(!g_NtfSleepWake.remove_SleepWakeNotifications())
    {
        //Failed
        assert(false);
    }
    
    return 0;
}



///Callback for some system SIGNAL events
void signalCallback(int sig, siginfo_t *info, void *context)
{
    //WARNING:
    //        There is a very limited set of system functions that can be called
    //        from a signal handler! Schedule any action from the main-loop later!
    //
    //        For the list of allowed functions check the manual for the sigaction():
    //          x-man-page://2/sigaction
    //
    
    if(sig == SIGTERM ||
       sig == SIGINT)
    {
        //Signal the main run-loop to quit, so that we can do our cleanup
        CFRunLoopStop(CFRunLoopGetMain());
    }
}


///Add callback for the 'sig' signal event
void addSignalCallbacks(int sig)
{
    struct sigaction sa = {};
    
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = signalCallback;
    
    if(sigaction(sig, &sa, nullptr) == -1)
    {
        //Failed to set
        assert(false);
        abort();
    }
}



///Return string with the current date and time
std::string current_time_as_string()
{
    //Get current time
    timeval tv = {};
    gettimeofday(&tv, nullptr);
    
    tm dtm = {};
    localtime_r(&tv.tv_sec, &dtm);
    
    char buff[128];
    buff[0] = 0;
    
    snprintf(buff, SIZEOF(buff),
             "%04u-%02u-%02u %02u:%02u:%02u.%06u"
                             ,
             1900 + dtm.tm_year,
             1 + dtm.tm_mon,
             dtm.tm_mday,
             dtm.tm_hour,
             dtm.tm_min,
             dtm.tm_sec,
             tv.tv_usec);
    
    buff[SIZEOF(buff) - 1] = 0;
    
    return buff;
}




///Callback that is invoked for the reboot, shutdown, or logout notifications
void callback_RebootShutdownLogout(mach_msg_header_t* pHeader,
                                   const char* pPortName,
                                   const void* pParam1,
                                   const void* pParam2)
{
    UNREFERENCED_PARAMETER(pHeader);
    UNREFERENCED_PARAMETER(pParam1);
    UNREFERENCED_PARAMETER(pParam2);
    
    //See what notification it is
    printf("%s > Received notification: %s\n", current_time_as_string().c_str(), pPortName);
    
    
    //Convert port name to a state value
    CURRENT_REBOOT_SHUTDOWN_STATE new_state = get_CURRENT_REBOOT_SHUTDOWN_STATE_by_port_name(pPortName);
    
    //Keep previous state
    static CURRENT_REBOOT_SHUTDOWN_STATE prev_state = CRS_STATE_Unknown;
    
    if(new_state == CRS_STATE_Cancelled)
    {
        //User canceled the UI
        new_state = CRS_STATE_Unknown;
    }
    else if(new_state == CRS_STATE_PointOfNoReturn)
    {
        //Determine the new OS state
        REBOOT_SHUTDOWN_STATE rss = macOS_State_Default;
        
        if(prev_state == CRS_STATE_ShutdownUIShown)
        {
            rss = macOS_State_Shutting_Down;
        }
        else if(prev_state == CRS_STATE_RestartUIShown)
        {
            rss = macOS_State_Rebooting;
        }
        else if(prev_state == CRS_STATE_LogoutUIShown)
        {
            rss = macOS_State_Logging_Out;
        }
        else
        {
            //Some unexpected value
            assert(false);
        }
        
        //Remember masOS state
        g_RebootShutdownState.set(&rss);

        new_state = CRS_STATE_Unknown;
    }
    
    //Remember as previous state
    prev_state = new_state;
}




///Convert 'pPortName' into CURRENT_REBOOT_SHUTDOWN_STATE enumeration
///RETURN:
///     = Matching state value from CURRENT_REBOOT_SHUTDOWN_STATE enum, or
///     = CRS_STATE_Unknown if not matched
CURRENT_REBOOT_SHUTDOWN_STATE get_CURRENT_REBOOT_SHUTDOWN_STATE_by_port_name(const char* pPortName)
{
    if(pPortName &&
       pPortName[0])
    {
        for(int i = 0; i < SIZEOF(gkNotifNames); i++)
        {
            if(strcasecmp(gkNotifNames[i].pName, pPortName))
            {
                return gkNotifNames[i].state;
            }
        }
    }
    
    return CRS_STATE_Unknown;
}




///Notification when macOS enters sleep, or wakes up from it
void callback_SleepWake(natural_t msgType,
                        void *msgArgument,
                        io_connect_t portSleepWake,
                        const void* pParam1,
                        const void* pParam2)
{
    UNREFERENCED_PARAMETER(pParam1);
    UNREFERENCED_PARAMETER(pParam2);
    
    IOReturn ioRet;
    std::string strPwrEvent;
    
    //Determine what type of notification did we receive
    switch(msgType)
    {
        case kIOMessageCanSystemSleep:
        {
            //Indicates that the system is pondering an idle sleep, but gives apps
            //the chance to veto that sleep attempt.
            //
            strPwrEvent = "CanSystemSleep";

            //Decide if we need to prevent idle sleep ...
            //INFO: We will allow it here.
            bool bAllowIdleSleep = true;

            if(bAllowIdleSleep)
            {
                //Allow sleep
                ioRet = IOAllowPowerChange(portSleepWake,
                                           (intptr_t)msgArgument);
            }
            else
            {
                //Prevent sleep
                ioRet = IOCancelPowerChange(portSleepWake,
                                            (intptr_t)msgArgument);
            }

            assert(ioRet == KERN_SUCCESS);
        }
        break;

        case kIOMessageSystemWillNotSleep:
        {
            //Is delivered when some app client has vetoed an idle sleep request.
            //kIOMessageSystemWillNotSleep may follow a kIOMessageCanSystemSleep
            //notification, but will not otherwise be sent
            //
            strPwrEvent = "SystemWillNotSleep";
        }
        break;

        case kIOMessageSystemWillSleep:
        {
            //Is delivered at the point the system is initiating a non-abortable sleep.
            //
            strPwrEvent = "SystemWillSleep";

            //We must acknowledge it though
            ioRet =  IOAllowPowerChange(portSleepWake,
                                        (intptr_t)msgArgument);
            
            assert(ioRet == KERN_SUCCESS);
        }
        break;

        case kIOMessageSystemWillPowerOn:
        {
            //Is delivered at early wakeup time, before most hardware has been
            //powered on. Be aware that any attempts to access disk, network,
            //the display, etc. may result in errors or blocking your process
            //until those resources become available.
            //
            strPwrEvent = "SystemWillPowerOn";
        }
        break;

        case kIOMessageSystemHasPoweredOn:
        {
            //Is delivered at wakeup completion time, after all device drivers
            //and hardware have handled the wakeup event. Expect this event 1-5
            //or more seconds after initiating system wakeup
            //
            strPwrEvent = "SystemHasPoweredOn";
        }
        break;

        case kIOMessageCanDevicePowerOff:
        {
            //(As practice has shown) this event is not really delivered anymore...
            strPwrEvent = "CanDevicePowerOff";
        }
        break;

        case kIOMessageDeviceWillNotPowerOff:
        {
            //(As practice has shown) this event is not really delivered anymore...
            strPwrEvent = "DeviceWillNotPowerOff";
        }
        break;

        case kIOMessageCanSystemPowerOff:
        {
            //(As practice has shown) this event is not really delivered anymore...
            strPwrEvent = "CanSystemPowerOff";
        }
        break;

        case kIOMessageDeviceWillPowerOn:
        {
            //(As practice has shown) this event is not really delivered anymore...
            strPwrEvent = "DeviceWillPowerOn";
        }
        break;

        case kIOMessageDeviceHasPoweredOff:
        {
            //(As practice has shown) this event is not really delivered anymore...
            strPwrEvent = "DeviceHasPoweredOff";
        }
        break;

        default:
        {
            //Some unrecognized event
            char buff[64];
            snprintf(buff, SIZEOF(buff), "SleepEvent=%d", msgType);
            strPwrEvent = buff;
        }
        break;
    }
    
    
    //Output it
    printf("%s > Received notification: %s\n", current_time_as_string().c_str(), strPwrEvent.c_str());
}




///Perform "soft" reboot or shutdown of the OS
///INFO: "Soft" power action will show a UI if some programs have unsaved data, or refuse the power action.
///'bReboot' = true to reboot, false to shut down
///RETURN:
///     - true if successfully started the operation (note that it may be canceled by a user later)
bool RebootShutdownSoft(bool bReboot)
{
    bool bRes = false;
    
    AEEventID evt;
    if(bReboot)
    {
        evt = kAERestart;
    }
    else
    {
        evt = kAEShutDown;
    }
    
    static const ProcessSerialNumber kPSNOfSystemProcess = { 0, kSystemProcess };
    
    AEAddressDesc targetDesc;
    OSStatus err = AECreateDesc(typeProcessSerialNumber,
                                &kPSNOfSystemProcess,
                                sizeof(kPSNOfSystemProcess),
                                &targetDesc);
    if(err == noErr)
    {
        AppleEvent appleEventToSend = {typeNull, nullptr};
        
        err = AECreateAppleEvent(kCoreEventClass,
                                 evt,
                                 &targetDesc,
                                 kAutoGenerateReturnID,
                                 kAnyTransactionID,
                                 &appleEventToSend);
        
        if(err == noErr)
        {
            AppleEvent eventReply = {typeNull, nullptr};
            
            err = AESend(&appleEventToSend,
                         &eventReply,
                         kAENoReply,
                         kAENormalPriority,
                         kAEDefaultTimeout,
                         nullptr,
                         nullptr);
            
            if(err == noErr)
            {
                //All good
                bRes = true;
                
                //Free
                AEDisposeDesc(&eventReply);
            }
            else
            {
                //Error
                assert(false);
            }
            
            //Free
            AEDisposeDesc(&appleEventToSend);
        }
        else
        {
            //Error
            assert(false);
        }
        
        //Free data
        AEDisposeDesc(&targetDesc);
    }
    else
    {
        //Error
        assert(false);
    }
    
    return bRes;
}




///Perform "hard" reboot or shutdown of the OS
///INFO: "Hard" power action will be performed regardless of unsaved user data, which may lead to user data loss!
///'bReboot' = true to reboot, false to shut down
///RETURN:
///     - true if successfully started the operation
///     - false if failed, check errno for details
bool RebootShutdownHard(bool bReboot)
{
    int nPwrOpFlgs;
    
    if(bReboot)
    {
        //Hard reboot
        nPwrOpFlgs = RB_AUTOBOOT;
    }
    else
    {
        //Hars shutdown
        nPwrOpFlgs = RB_HALT;
    }
    
    //INFO: As I see in practice this function rarely returns, or
    //      reboot is executed really fast...
    int nErr = reboot(nPwrOpFlgs);
    
    //Set error code before returning
    errno = nErr;
    
    return nErr == 0;
}



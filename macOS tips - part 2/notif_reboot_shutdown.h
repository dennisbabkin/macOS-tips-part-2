//
//  notif_reboot_shutdown.h
//  macOS tips - part 2
//
//  Created by dennisbabkin.com on 6/17/23.
//
//  This project is a part of the blog post.
//  For more details, check:
//
//      https://dennisbabkin.com/blog/?i=AAA11500
//
//  Demonstration of how to receive notifications of a shutdown, reboot or user logout
//


#ifndef notif_reboot_shutdown_h
#define notif_reboot_shutdown_h


#include "rdr_wrtr.h"       //Reader/writer lock classes from "macOS tips - part 1"

#include <CoreFoundation/CoreFoundation.h>
#include <notify.h>
#include <mach/mach_port.h>








//User clicked shutdown to show the UI. (It may be aborted later.)
#define kLWShutdownInitiated "com.apple.system.loginwindow.shutdownInitiated"

//User clicked restart to show the UI. (It may be aborted later.)
#define kLWRestartInitiated  "com.apple.system.loginwindow.restartinitiated"

//User clicked `logout user` to show the UI. (It may be aborted later.)
#define kLWLogoutInitiated   "com.apple.system.loginwindow.logoutInitiated"

//A previously shown UI for shutdown, restart, or logout has been cancelled.
#define kLWLogoutCancelled   "com.apple.system.loginwindow.logoutcancelled"

//A previously shown shutdown, restart, or logout was initiated and can no longer be cancelled!
#define kLWPointOfNoReturn   "com.apple.system.loginwindow.logoutNoReturn"






struct Notif_RebootShutdown
{
    Notif_RebootShutdown()
    {
    }
    
    ~Notif_RebootShutdown()
    {
        //Remove callback from a destructor
        if(!remove_Notifications(false))
        {
            //Should not fail here!
            assert(false);
        }
    }

    ///Set the callback to receive reboot, shutdown & logoff notifications (cannot be called repeatedly)
    ///'pPortName' = name of the mach port for the notification:
    ///           - "com.apple.system.loginwindow.shutdownInitiated" for when shut-down warning is shown, may be aborted
    ///           - "com.apple.system.loginwindow.restartinitiated" for when restart warning is shown, may be aborted
    ///           - "com.apple.system.loginwindow.logoutInitiated" for when log-out warning is shown, may be aborted
    ///           - "com.apple.system.loginwindow.logoutcancelled" previous shutdown, restart, logout was aborted
    ///           - "com.apple.system.loginwindow.logoutNoReturn" previous shutdown, restart, logout is proceeding, can't abort
    ///'pfn' = callback function that is called when event happens, or null not to call it
    ///'pParam1' = passed directly into 'pfn' when it's called
    ///'pParam2' = passed directly into 'pfn' when it's called
    ///RETURN:
    ///     - true if success
    bool init_Notifications(const char* pPortName,
                            void (*pfn)(mach_msg_header_t* pHeader,
                                        const char* pPortName,
                                        const void* pParam1,
                                        const void* pParam2) = nullptr,
                            const void* pParam1 = nullptr,
                            const void* pParam2 = nullptr)
    {
        bool bRes = false;
        
        //We must have a port name
        if(pPortName &&
            pPortName[0])
        {
            //Act from within a lock
            WRITER_LOCK wrl(_lock);
            
            if(!_bCallbackSet)
            {
                //Remember parameters
                _pfnCallback = pfn;
                _pParam1 = pParam1;
                _pParam2 = pParam2;
                
                //Register for notifications
                uint32_t nResShtdn = notify_register_mach_port(pPortName,
                                                               &_shutDownMachPort,
                                                               (_shutDownMachPort == MACH_PORT_NULL) ? 0 : NOTIFY_REUSE,
                                                               &_nShutdownNtf);
                
                if(nResShtdn == NOTIFY_STATUS_OK)
                {
                    //Add to run-loop notifications
                    CFMachPortContext ctx = {};
                    ctx.info = this;
                    
                    _strPortName = pPortName;
                    
                    Boolean bShouldFree = false;
                    
                    _shutDownMachPortRef = CFMachPortCreateWithPort(kCFAllocatorDefault,
                                                                    _shutDownMachPort,
                                                                    _onCallback,
                                                                    &ctx,
                                                                    &bShouldFree);
                    if(_shutDownMachPortRef)
                    {
                        _shutdownRunLoopRef = CFMachPortCreateRunLoopSource(nullptr, _shutDownMachPortRef, 0);
                        if(_shutdownRunLoopRef)
                        {
                            //Add to the run loop
                            CFRunLoopAddSource(CFRunLoopGetMain(),
                                                _shutdownRunLoopRef,
                                                kCFRunLoopDefaultMode);
                            
                            //Set flag that we set it
                            _bCallbackSet = true;
                            
                            //Done
                            bRes = true;
                        }
                        else
                        {
                            //Failed
                            assert(false);
                        }
                    }
                    else
                    {
                        //Failed
                        assert(false);
                    }
                    
                    //We are creating the object here - thus 'bShouldFree' should never be true
                    assert(!bShouldFree);
                    
                    if(!bRes)
                    {
                        //Clear port name if we failed
                        _strPortName.clear();
                    }
                }
                else
                {
                    //Failed
                    assert(false);
                }
            }
            else
            {
                //Can't init again
                assert(false);
            }
        }
        else
        {
            //No port name
            assert(false);
        }

        return bRes;
    }

    ///Checks if the callback to receive notifications was set
    ///RETURN:
    ///     - true if yes
    bool is_ReceivingNotifications()
    {
        //Act from within a lock
        READER_LOCK rdl(_lock);
        
        return _bCallbackSet;
    }
    
    
    ///RETURN:
    ///     = Port name that is currently used for this class,
    ///     = "" if it was not initialized
    const char* getPortName()
    {
        //Act from within a lock
        READER_LOCK rdl(_lock);
        
        return _strPortName.c_str();
    }
        
    ///Remove the callback that was set by init_Notifications()
    ///INFO: It does nothing if the callback wasn't set before.
    ///'bRebooting' = true if we're calling it when the OS is rebooting
    ///RETURN:
    ///     - true if no errors
    bool remove_Notifications(bool bRebooting)
    {
        bool bRes = true;
        
        //Act from within a lock
        WRITER_LOCK wrl(_lock);
        
        if(_bCallbackSet)
        {
            //Unregister from receiving notifications
            if(_nShutdownNtf)
            {
                uint32_t resCancel = notify_cancel(_nShutdownNtf);
                if(resCancel != NOTIFY_STATUS_OK)
                {
                    if(bRebooting &&
                        resCancel == NOTIFY_STATUS_SERVER_NOT_FOUND)
                    {
                        //Not an error
                    }
                    else
                    {
                        //Error
                        assert(false);
                        
                        bRes = false;
                    }
                }
                
                _nShutdownNtf = 0;
            }
            
            if(_shutdownRunLoopRef)
            {
                CFRunLoopRemoveSource(CFRunLoopGetMain(),
                                      _shutdownRunLoopRef,
                                      kCFRunLoopDefaultMode);
                
                CFRelease(_shutdownRunLoopRef);
                _shutdownRunLoopRef = nullptr;
            }
            
            if(_shutDownMachPortRef)
            {
                CFRelease(_shutDownMachPortRef);
                _shutDownMachPortRef = nullptr;
            }
            
            
            //Reset parameters
            _bCallbackSet = false;
            
            _strPortName.clear();
            
            _shutDownMachPort = MACH_PORT_NULL;
            
            _pfnCallback = nullptr;
            _pParam1 = nullptr;
            _pParam2 = nullptr;
        }
        
        return bRes;
    }

    
private:
    
    static void _onCallback(CFMachPortRef port,
                            void *msg,
                            CFIndex size,
                            void *info)
    {
        Notif_RebootShutdown* pThis = (Notif_RebootShutdown*)info;
        assert(pThis);
        
        //Act from within a lock
        READER_LOCK rdl(pThis->_lock);
        
        if(pThis->_pfnCallback)
        {
            //Invoke the callback
            mach_msg_header_t *header = (mach_msg_header_t *)msg;
            
            pThis->_pfnCallback(header,
                                pThis->_strPortName.c_str(),
                                pThis->_pParam1,
                                pThis->_pParam2);
        }

    }
    
    
private:
    ///Copy constructor and assignments are NOT available!
    Notif_RebootShutdown(const Notif_RebootShutdown& s) = delete;
    Notif_RebootShutdown& operator = (const Notif_RebootShutdown& s) = delete;
    
private:
    
    RDR_WRTR _lock;                     //Lock for accessing this struct

    bool _bCallbackSet = false;         //true if we set callback OK

    std::string _strPortName;           //Port name for Mach port notifications

    int _nShutdownNtf = 0;
    mach_port_t _shutDownMachPort = MACH_PORT_NULL;
    CFMachPortRef _shutDownMachPortRef = nullptr;
    CFRunLoopSourceRef _shutdownRunLoopRef = nullptr;

    
    void (*_pfnCallback)(mach_msg_header_t* pHeader,
                            const char* pPortName,
                            const void* pParam1,
                            const void* pParam2) = nullptr;
    const void* _pParam1 = nullptr;
    const void* _pParam2 = nullptr;
};









#endif /* notif_reboot_shutdown_h */

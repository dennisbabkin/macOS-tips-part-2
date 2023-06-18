//
//  notif_sleep_wake.h
//  macOS tips - part 2
//
//  Created by dennisbabkin.com on 6/17/23.
//
//  This project is a part of the blog post.
//  For more details, check:
//
//      https://dennisbabkin.com/blog/?i=AAA11500
//
//  Demonstration of how to receive notifications of entering a sleep mode, or waking up from it
//


#ifndef Notif_SleepWake_h
#define Notif_SleepWake_h


#include <stdio.h>
#include <assert.h>

#include "rdr_wrtr.h"           //Reader/writer lock classes from "macOS tips - part 1"

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>





struct Notif_SleepWake
{
    Notif_SleepWake()
    {
    }

    ~Notif_SleepWake()
    {
        //Remove callback from a destructor
        if(!remove_SleepWakeNotifications())
        {
            //Should not fail here!
            assert(false);
        }
    }

    

    
    
    ///Set the callback to receive sleep/wake notifications (cannot be called repeatedly)
    ///'pfn' = callback function that is called when event happens, or null not to call it
    ///'pParam1' = passed directly into 'pfn' when it's called
    ///'pParam2' = passed directly into 'pfn' when it's called
    ///RETURN:
    ///     - true if success
    bool init_SleepWakeNotifications(void (*pfn)(natural_t msgType,
                                                 void *msgArgument,
                                                 io_connect_t portSleepWake,
                                                 const void* pParam1,
                                                 const void* pParam2) = nullptr,
                                     const void* pParam1 = nullptr,
                                     const void* pParam2 = nullptr)
    {
        bool bRes = false;
        
        //Act from within a lock
        WRITER_LOCK wrl(_lock);
        
        if(!_bCallbackSet)
        {
            //Remember parameters
            _pfnCallback = pfn;
            _pParam1 = pParam1;
            _pParam2 = pParam2;
            
            //Register for sleep/wake notifications
            _pwrSleepWakeKernelPort = IORegisterForSystemPower(this,
                                                               &_pwrSleepWakeNtfPort,
                                                               _pwrSleepWakeCallback,
                                                               &_pwrSleepWakeNotifier);
            
            if(_pwrSleepWakeKernelPort != MACH_PORT_NULL)
            {
                //Add it to the run-loop
                CFRunLoopAddSource(CFRunLoopGetMain(),
                                   IONotificationPortGetRunLoopSource(_pwrSleepWakeNtfPort),
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
            //Can't init again
            assert(false);
        }
        
        return bRes;
    }
    
    
    ///Checks if the callback to receive wake/sleep notifications was set
    ///RETURN:
    ///     - true if yes
    bool is_ReceivingSleepWakeNotifications()
    {
        //Act from within a lock
        READER_LOCK rdl(_lock);
        
        return _bCallbackSet;
    }
    
    
    ///Remove the callback that was set by init_SleepWakeNotifications()
    ///INFO: It does nothing if the callback wasn't set before.
    ///RETURN:
    ///     - true if no errors
    bool remove_SleepWakeNotifications()
    {
        bool bRes = true;
        
        //Act from within a lock
        WRITER_LOCK wrl(_lock);
        
        if(_bCallbackSet)
        {
            //Unregister sleep/wake notifications
            if(_pwrSleepWakeNtfPort != nullptr)
            {
                //Remove the sleep notification port from the application runloop
                CFRunLoopRemoveSource(CFRunLoopGetMain(),
                                      IONotificationPortGetRunLoopSource(_pwrSleepWakeNtfPort),
                                      kCFRunLoopDefaultMode);
                
                //Deregister from system sleep notifications
                IOReturn ioRes = IODeregisterForSystemPower(&_pwrSleepWakeNotifier);
                if(ioRes != kIOReturnSuccess)
                {
                    //Error
                    assert(false);
                    bRes = false;
                }
                
                //IORegisterForSystemPower implicitly opens the Root Power Domain,
                //so we need to close it here
                kern_return_t resKern = IOServiceClose(_pwrSleepWakeKernelPort);
                if(resKern != KERN_SUCCESS)
                {
                    //Error
                    assert(false);
                    bRes = false;
                }
                
                //Destroy the notification port allocated by IORegisterForSystemPower
                IONotificationPortDestroy(_pwrSleepWakeNtfPort);
            }
            else
            {
                //Should not be here
                assert(false);
            }
            
            
            //Reset parameters
            _bCallbackSet = false;
            
            _pwrSleepWakeKernelPort = {};
            _pwrSleepWakeNtfPort = nullptr;
            _pwrSleepWakeNotifier = {};
            
            _pfnCallback = nullptr;
            _pParam1 = nullptr;
            _pParam2 = nullptr;
            
        }
        
        return bRes;
    }
    
    
    
private:
    static void _pwrSleepWakeCallback(void* pContext,
                                       io_service_t svc,
                                       natural_t msgType,
                                       void *msgArgument)
    {
        Notif_SleepWake* pThis = (Notif_SleepWake*)pContext;
        assert(pThis);
        
        //Act from within a lock
        READER_LOCK rdl(pThis->_lock);
        
        if(pThis->_pfnCallback)
        {
            //Invoke the callback
            pThis->_pfnCallback(msgType,
                                msgArgument,
                                pThis->_pwrSleepWakeKernelPort,
                                pThis->_pParam1,
                                pThis->_pParam2);
        }
    }

    
    
private:
    ///Copy constructor and assignments are NOT available!
    Notif_SleepWake(const Notif_SleepWake& s) = delete;
    Notif_SleepWake& operator = (const Notif_SleepWake& s) = delete;
    
private:
    
    RDR_WRTR _lock;                     //Lock for accessing this struct

    bool _bCallbackSet = false;         //true if set callback OK

    io_connect_t            _pwrSleepWakeKernelPort = {};
    IONotificationPortRef   _pwrSleepWakeNtfPort = nullptr;
    io_object_t             _pwrSleepWakeNotifier = {};
    
    void (*_pfnCallback)(natural_t msgType,
                         void *msgArgument,
                         io_connect_t portSleepWake,
                         const void* pParam1,
                         const void* pParam2) = nullptr;
    const void* _pParam1 = nullptr;
    const void* _pParam2 = nullptr;
    
    
};



#endif /* Notif_SleepWake_h */

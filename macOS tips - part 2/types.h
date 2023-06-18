//
//  types.h
//  macOS tips - part 2
//
//  Created by dennisbabkin.com on 6/17/23.
//
//  This project is a part of the blog post.
//  For more details, check:
//
//      https://dennisbabkin.com/blog/?i=AAA11500
//
//  Custom types and definitions
//


#ifndef types_h
#define types_h



#define SIZEOF(f) (sizeof(f) / sizeof(f[0]))    //Helper preprocessor definition to get the number of elements in C array

#define UNREFERENCED_PARAMETER(f) (f)




enum REBOOT_SHUTDOWN_STATE
{
    macOS_State_Default,
    
    macOS_State_Rebooting,          //OS is currently rebooting
    macOS_State_Shutting_Down,      //OS is currently shutting down
    macOS_State_Logging_Out,        //OS is currently logging out a user
};



enum CURRENT_REBOOT_SHUTDOWN_STATE
{
    CRS_STATE_Unknown,
    
    CRS_STATE_ShutdownUIShown,          //Shutdown UI was shown to the user
    CRS_STATE_RestartUIShown,           //Restart UI was shown to the user
    CRS_STATE_LogoutUIShown,            //Log-out UI was shown to the user
    CRS_STATE_Cancelled,                //Previous operation was canceled
    CRS_STATE_PointOfNoReturn,          //Previous operation was initiated, or OK'ed
};





#endif /* types_h */

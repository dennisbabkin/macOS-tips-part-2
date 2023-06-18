//
//  CFString_conv.h
//  macOS tips - part 1 & 2
//
//  Created by dennisbabkin.com on 6/17/23.
//
//  This project is a part of the blog posts.
//  For more details, check:
//
//      https://dennisbabkin.com/blog/?i=AAA11400
//      https://dennisbabkin.com/blog/?i=AAA11500
//
//  Demonstration of string conversions from CFString and CFAbsoluteTime to std::string
//


#ifndef CFString_conv_h
#define CFString_conv_h

#include <CoreFoundation/CoreFoundation.h>




///Convert 'ref' into std::string
///'strOut' = receives converted string, or an empty string if conversion fails
///RETURN:
///     = true if converted successfully
bool GetString_From_CFStringRef(CFStringRef ref, std::string& strOut)
{
    bool bRes = false;
    
    if(ref)
    {
        const char* pStr = CFStringGetCStringPtr(ref, kCFStringEncodingUTF8);
        if(pStr)
        {
            //Simple example
            strOut = pStr;
            
            bRes = true;
        }
        else
        {
            //Need to do more work
            CFIndex nchSize = CFStringGetLength(ref);     //This is the size in characters
            if(nchSize > 0)
            {
                CFIndex ncbSz = CFStringGetMaximumSizeForEncoding(nchSize,
                                                                  kCFStringEncodingUTF8);
                if(ncbSz != kCFNotFound)
                {
                    UInt8* pBuff = new (std::nothrow) UInt8[ncbSz + 1];
                    if(pBuff)
                    {
                        if(CFStringGetCString(ref,
                                              (char*)pBuff,
                                              ncbSz + 1,
                                              kCFStringEncodingUTF8))
                        {
                            //Note that 'ncbSz' will most certainly include trailing 0's,
                            //thus we cannot assume that that is the length of our string!
                            
                            //Let's place a safety null there first
                            pBuff[ncbSz] = 0;
                            
                            strOut = (const char*)pBuff;
                            
                            bRes = true;
                        }
                        else
                            assert(false);
                        
                        //Free mem
                        delete[] pBuff;
                        pBuff = nullptr;
                    }
                    else
                        assert(false);
                }
                else
                    assert(false);
            }
        }
    }
    
    if(!bRes)
    {
        //Clear resulting string on error
        strOut.clear();
    }
    
    return bRes;
}




///Format 'dtm' as local date/time
///'pstrOut' = if not 0, receives formatted string, if success. Otherwise ""
///RETURN:
///     - true if success
bool FormatDateTimeAsStr(CFAbsoluteTime dtm,
                         std::string* pstrOut)
{
    bool bRes = false;
    
    CFDateRef refDate = CFDateCreate(kCFAllocatorDefault, dtm);
    if(refDate)
    {
        CFDateFormatterRef refDtFmtr = CFDateFormatterCreate(kCFAllocatorDefault, nullptr, kCFDateFormatterShortStyle, kCFDateFormatterLongStyle);
     
        if(refDtFmtr)
        {
            CFStringRef refStr = CFDateFormatterCreateStringWithDate(kCFAllocatorDefault, refDtFmtr, refDate);
            if(refStr)
            {
                if(pstrOut)
                {
                    //Convert to string
                    if(GetString_From_CFStringRef(refStr,
                                                  *pstrOut))
                    {
                        //Done
                        bRes = true;
                    }
                }
                else
                {
                    //Assume success
                    bRes = true;
                }
                
                CFRelease(refStr);
                refStr = nullptr;
            }
            
            CFRelease(refDtFmtr);
            refDtFmtr = nullptr;
        }
        
        CFRelease(refDate);
        refDate = nullptr;
    }
    
    if(!bRes &&
       pstrOut)
    {
        pstrOut->clear();
    }
    
    return bRes;
}




#endif /* CFString_conv_h */

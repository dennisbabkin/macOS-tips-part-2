//
//  rdr_wrtr.h
//  macOS tips - part 1
//
//  Created by dennisbabkin.com on 6/11/23.
//
//  This project is a part of the blog post.
//  For more details, check:
//
//      https://dennisbabkin.com/blog/?i=AAA11400
//
//  Demonstration of reader/writer lock classes
//


#ifndef rdr_wrtr_h
#define rdr_wrtr_h


#include <pthread.h>
#include <assert.h>


enum YesNoError
{
    Yes = 1,
    No = 0,
    Error = -1
};



struct RDR_WRTR
{
    RDR_WRTR()
    {
    }
    
    ///Acquire a shared lock
    ///INFO: This function does not return until the lock is available.
    ///      This function DOES NOT support reentrancy, or calling it
    ///      repeatedly from the same thread!
    void EnterReaderLock()
    {
        if(pthread_rwlock_rdlock(&_lock) != 0)
        {
            //Failed to enter - abort!
            //Most certainly you have an unsupported reentrancy in your logic!
            assert(false);
            abort();
        }
    }
    
    ///Leave a shared lock
    ///INFO: This function must be called once after the EnterReaderLock function.
    void LeaveReaderLock()
    {
        if(pthread_rwlock_unlock(&_lock) != 0)
        {
            //Failed to leave - abort!
            //There's either a failure in logic in the code that calls this function, or
            //there's some memory corruption...
            assert(false);
            abort();
        }
    }
    
    ///Debugging function - it checks if a reader lock was acquired, but it doesn't block!
    ///RETURN:
    ///     - Yes if lock was not available for reading (NOTE that it may be now!)
    ///     - No if lock was available for reading (NOTE that it may not be anymore!)
    ///     - Error if failed to determine, check errno for OS error code
    YesNoError WasReaderLocked()
    {
        YesNoError result = Error;

        //Try to acquire it for reading, but do not block!
        int nErr = pthread_rwlock_tryrdlock(&_lock);
        if(nErr == 0)
        {
            //Acquired it, need to release it
            nErr = pthread_rwlock_unlock(&_lock);
            if(nErr == 0)
            {
                result = No;
            }
            else
            {
                //Logical problem
                errno = nErr;
            }
        }
        else if(nErr == EBUSY)
        {
            //Lock was acquired
            result = Yes;
        }
        else
        {
            //Logical problem
            errno = nErr;
        }

        return result;
    }
    
    
    ///Acquire an exclusive lock
    ///INFO: This function does not return until the lock is available.
    ///      This function DOES NOT support reentrancy, or calling it
    ///      repeatedly from the same thread!
    void EnterWriterLock()
    {
        if(pthread_rwlock_wrlock(&_lock) != 0)
        {
            //Failed to enter - abort!
            //Most certainly you have an unsupported reentrancy in your logic!
            assert(false);
            abort();
        }
    }
    
    ///Leave an exclusive lock
    ///INFO: This function must be called once after the EnterWriterLock function.
    void LeaveWriterLock()
    {
        if(pthread_rwlock_unlock(&_lock) != 0)
        {
            //Failed to leave - abort!
            //There's either a failure in logic in the code that calls this function, or
            //there's some memory corruption...
            assert(false);
            abort();
        }
    }

    
    ///Debugging function - it checks if a writer lock was acquired, but it doesn't block!
    ///RETURN:
    ///     - Yes if lock was not available for writing (NOTE that it may be now!)
    ///     - No if lock was available for writing (NOTE that it may not be anymore!)
    ///     - Error if failed to determine, check errno for OS error code
    YesNoError WasWriterLocked()
    {
        YesNoError result = Error;

        //Try to acquire it for writing, but do not block
        int nErr = pthread_rwlock_trywrlock(&_lock);
        if(nErr == 0)
        {
            //Acquired it, need to release it
            nErr = pthread_rwlock_unlock(&_lock);
            if(nErr == 0)
            {
                result = No;
            }
            else
            {
                //Logical problem
                errno = nErr;
            }
        }
        else if(nErr == EBUSY)
        {
            //Lock was acquired
            result = Yes;
        }
        else
        {
            //Logical problem
            errno = nErr;
        }

        return result;
    }
    
private:
    ///Copy constructor and assignments are NOT available!
    RDR_WRTR(const RDR_WRTR& s) = delete;
    RDR_WRTR& operator = (const RDR_WRTR& s) = delete;
    
    
private:
    pthread_rwlock_t _lock = PTHREAD_RWLOCK_INITIALIZER;
};







struct READER_LOCK
{
    READER_LOCK(RDR_WRTR& rwl)
        : _rwl(rwl)
    {
        rwl.EnterReaderLock();
    }

    ~READER_LOCK()
    {
        _rwl.LeaveReaderLock();
    }

    
private:
    ///Copy constructor and assignments are NOT available!
    READER_LOCK(const READER_LOCK& s) = delete;
    READER_LOCK& operator = (const READER_LOCK& s) = delete;
    
private:
    RDR_WRTR& _rwl;
};







struct WRITER_LOCK
{
    WRITER_LOCK(RDR_WRTR& rwl)
        : _rwl(rwl)
    {
        rwl.EnterWriterLock();
    }

    ~WRITER_LOCK()
    {
        _rwl.LeaveWriterLock();
    }

private:
    ///Copy constructor and assignments are NOT available!
    WRITER_LOCK(const WRITER_LOCK& s) = delete;
    WRITER_LOCK& operator = (const WRITER_LOCK& s) = delete;
    
private:
    RDR_WRTR& _rwl;
};








struct WRITER_LOCK_COND
{
    WRITER_LOCK_COND(RDR_WRTR* p_rwl)
        : _p_rwl(p_rwl)
    {
        if(_p_rwl)
        {
            _p_rwl->EnterWriterLock();
        }
    }

    ~WRITER_LOCK_COND()
    {
        if(_p_rwl)
        {
            _p_rwl->LeaveWriterLock();
        }
    }

private:
    ///Copy constructor and assignments are NOT available!
    WRITER_LOCK_COND(const WRITER_LOCK_COND& s) = delete;
    WRITER_LOCK_COND& operator = (const WRITER_LOCK_COND& s) = delete;
    
private:
    RDR_WRTR* _p_rwl;
};










#endif /* rdr_wrtr_h */

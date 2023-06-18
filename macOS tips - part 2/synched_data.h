//
//  synched_data.h
//  macOS tips - part 1
//
//  Created by dennisbabkin.com on 6/11/23.
//
//  This project is a part of the blog post.
//  For more details, check:
//
//      https://dennisbabkin.com/blog/?i=AAA11400
//
//  Demonstration of a template class for synchronized access to data
//


#ifndef synched_data_h
#define synched_data_h



template <typename T>
struct SYNCHED_DATA
{
    SYNCHED_DATA(T v)
        : _var(v)
    {
    }

    ///Read the value into what is pointed by 'pV'
    void get(T* pV)
    {
        if(pV)
        {
            READER_LOCK rl(_lock);
            *pV = _var;
        }
    }

    ///Set the value to what is pointed by 'pV'
    void set(T* pV)
    {
        if(pV)
        {
            WRITER_LOCK rl(_lock);
            _var = *pV;
        }
    }

    ///Set the value to what is pointed by 'pV' and return its previous value
    T getAndSet(T* pV)
    {
        T prevVar;
    
        if(true)
        {
            WRITER_LOCK rl(_lock);
            prevVar = _var;

            if(pV)
            {
                _var = *pV;
            }
        }

        return prevVar;
    }

    ///Call the 'pfn' callback from within the writer lock, and pass it 'pParam1' and 'pParam2'
    ///RETURN: The final value stored in this class
    T callFunc_ToSet(void (*pfn)(T*, const void*, const void*),
                        const void* pParam1 = nullptr,
                        const void* pParam2 = nullptr)
    {
        WRITER_LOCK rl(_lock);

        pfn(&_var, pParam1, pParam2);

        return _var;
    }


private:
    ///Copy constructor and assignments are NOT available!
    SYNCHED_DATA(const SYNCHED_DATA& s) = delete;
    SYNCHED_DATA& operator = (const SYNCHED_DATA& s) = delete;

    T _var;
    RDR_WRTR _lock;
};


#endif /* synched_data_h */

#pragma once

#ifndef OBJECTPOOL_HXX
#define OBJECTPOOL_HXX

#include <ace/Malloc_T.h>
#include <vector>
#include <memory>
#include <cstddef>

template<typename T>
class ObjectPool
{
    public:
        explicit ObjectPool (size_t capacity)
                            : m_allocator (capacity)
                            , m_capacity (capacity)
        {
        }

        ~ObjectPool() = default;

        T* acquire()
        {
            void* obj = m_allocator.malloc();

            if (obj)
            {
                return new (obj) T(); // placement new
            }

            return nullptr;
        }

        void release (T* obj)
        {
            if (!obj) return;

            obj->~T();                            // explicit destructor
            m_allocator.free (obj);
        }

        size_t capacity() const
        { 
            return m_capacity; 
        }

    private:
        ACE_Cached_Allocator<T, ACE_Thread_Mutex> m_allocator;
        size_t m_capacity;
};


#endif // OBJECTPOOL_HXX

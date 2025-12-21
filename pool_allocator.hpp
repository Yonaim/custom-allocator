#ifndef POOL_ALLOCATOR_HPP
#define POOL_ALLOCATOR_HPP

#include <cstddef> // size_t, ptrdiff_t

/*
    - 복사/변환 생성자로 생성된 allocator는 같은 PoolState를 공유
    - PoolState에서 block_size별로 pool를 관리
*/

namespace yn
{
    // ------------------------------------------------------------
    // Shared state: owns multiple pools (one per block size)
    // ------------------------------------------------------------
    class PoolState
    {
      public:
        struct pool
        {
            size_t block_size;
            size_t chunk_size;
        };

      private:
        size_t ref_count;
    };

    // ------------------------------------------------------------
    // Allocator
    // ------------------------------------------------------------
    template <class T>
    class PoolAllocator
    {
      public:
        typedef T             *pointer;
        typedef const T       *const_pointer;
        typedef T              value_type;
        typedef std::size_t    size_type;
        typedef std::ptrdiff_t diefference_type;

        template <class U>
        struct rebind
        {
            typedef PoolAllocator<U> other;
        };

        // constructor
        PoolAllocator();
        PoolAllocator(const PoolAllocator &orig);
        PoolAllocator &operator=(cosnt PoolAllocator &orig);
        template <class U>
        PoolAllocator(const PoolAllocator<U> &orig);

        // API
        pointer   allocate(size_type n, const void *cvp = 0);
        void      deallocate(pointer p, size_type n);
        size_type max_size();
        void      construct(pointer p, const T &val);
        void      destory(pointer p);

      private:
        PoolState *_state;
    };
} // namespace yn

#endif

#ifndef POOL_ALLOCATOR_HPP
#define POOL_ALLOCATOR_HPP

#include <cstddef> // size_t, ptrdiff_t

/*
  - 복사/변환 생성자로 생성된 allocator는 같은 PoolAllocState를 공유
  - PoolAllocState에서 block_size별로 pool를 관리
*/

namespace yn
{
    // x를 alignment(a)의 배수로 올림
    // a는 2의 제곱수여야함!!! (즉, 0b100..0)
    // ex. x=33, a=16일 경우, 48
    static inline size_t align_up(size_t x, size_t a)
    {
        // (a-1) = 0b11..1
        // and 연산으로 (a-1)의 배수만 남김
        return (x + (a - 1)) & (~(a - 1));
    }

    // ------------------------------------------------------------
    // Pool Allocator State : owns multiple pools (one per block size)
    // ------------------------------------------------------------
    class PoolAllocState
    {
      public:
        struct FreeNode
        {
            FreeNode *next;
            // remain space...
        };

        struct ChunkNode
        {
            void      *mem;
            ChunkNode *next;
            ChunkNode(void *m, ChunkNode *n) : mem(m), next(n) {}
        };

        enum
        {
            BLOCK_PER_CHUNK = 64,
            BLOCK_SIZE_BASE = 64
        };

        // block_size 값에 따라 하나씩 존재
        // 해당하는 block_size의 chunk
        struct PoolNode
        {
            size_t     chunk_size; // upstream에서 한 번에 크게 가져오는 단위
            ChunkNode *chunk_list;
            size_t     block_size; // chunk를 block_size로 쪼개서 free_list로 관리하는 단위
            FreeNode  *block_list;
            PoolNode  *next;

            // block size만 정해지면 나머지는 enum 상수값에 의해 알아서 결정됨
            PoolNode(size_t bs)
                : block_size(align_up(bs, BLOCK_SIZE_BASE)),
                  chunk_size(block_size * BLOCK_PER_CHUNK), chunk_list(NULL), block_list(NULL),
                  next(NULL)
            {
            }
        };

      private:
        PoolNode *_pools;
        size_t    _refcount;

      public:
        PoolAllocState() : _pools(NULL), _refcount(1) {}
        ~PoolAllocState()
        {
            PoolNode  *p = _pools;
            ChunkNode *c;
            while (p)
            {
                c = p->chunk_list;
                while (c)
                {
                    ::operator delete(c->mem);
                }
                _pools = p->next;
                delete p;
                p = _pools;
            }
        }

        void retain() { ++_refcount; }
        void release()
        {
            if (--_refcount == 0)
                delete this;
        }
        void      refill(PoolNode *p) {}
        PoolNode *find_or_create_pool() {}
    };

    // ------------------------------------------------------------
    // Pool Allocator
    // ------------------------------------------------------------
    template <class T>
    class PoolAllocator
    {
      public:
        typedef T             *pointer;
        typedef const T       *const_pointer;
        typedef T              value_type;
        typedef size_t         size_type;
        typedef std::ptrdiff_t diefference_type;

        template <class U>
        struct rebind
        {
            typedef PoolAllocator<U> other;
        };

      private:
        PoolAllocState *_state;

      public:
        // constructor
        PoolAllocator() : _state() PoolAllocator(const PoolAllocator &orig);
        PoolAllocator &operator=(cosnt PoolAllocator &orig);
        template <class U>
        PoolAllocator(const PoolAllocator<U> &orig);

        // API
        pointer   allocate(size_type n, const void *cvp = 0);
        void      deallocate(pointer p, size_type n);
        size_type max_size();
        void      construct(pointer p, const T &val);
        void      destory(pointer p);
    };
} // namespace yn

#endif

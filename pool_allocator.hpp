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
            BLOCK_SIZE_GRANULARITY = 64
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
                : block_size(align_up(bs, BLOCK_SIZE_GRANULARITY)),
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
                    c = c->next;
                    delete c;
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

        // 하나의 chunk를 추가하고 쪼개 block 리스트에 추가
        void refill(PoolNode *p)
        {
            void      *mem = ::operator new(p->chunk_size);
            ChunkNode *new_chunk = new ChunkNode(mem, p->chunk_list);
            p->chunk_list = new_chunk;

            // carve blocks
            char *cur = (char *)mem;
            char *end = cur + p->chunk_size;
            while (cur + p->block_size <= end)
            {
                FreeNode *new_block = reinterpret_cast<FreeNode *>(cur);
                new_block->next = p->block_list;
                p->block_list = new_block;
                cur += p->block_size;
            }
        }

        PoolNode *find_or_create_pool(size_t bs)
        {
            PoolNode *p;

            bs = align_up(bs, BLOCK_SIZE_GRANULARITY);
            p = _pools;
            while (p && p->block_size != bs)
                p = p->next;
            if (!p)
            {
                p = new PoolNode(bs);
                p->next = _pools;
                _pools = p;
            }
            return p;
        }

        // allocator는 연속된 메모리를 돌려주어야하므로 무조건 올림 처리하여 할당
        void *allocate_bytes(size_t bytes)
        {
            bytes = align_up(bytes, BLOCK_SIZE_GRANULARITY);
            PoolNode *p = find_or_create_pool(bytes);
            FreeNode *out = p->block_list;
            p->block_list = out->next;
            return reinterpret_cast<void *>(out);
        }

        void deallocate_bytes(void *ptr, size_t bytes)
        {
            bytes = align_up(bytes, BLOCK_SIZE_GRANULARITY);
            PoolNode *p = find_or_create_pool(bytes);
            FreeNode *put = reinterpret_cast<FreeNode *>(ptr);
            put->next = p->block_list;
            p->block_list = put;
        }
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
        PoolAllocator() : _state(PoolAllocState()) {}
        PoolAllocator(const PoolAllocator &orig) : _state(orig._state) {}
        PoolAllocator &operator=(cosnt PoolAllocator &orig)
        {
            _state = orig._state;
            return *this;
        }
        template <class U>
        PoolAllocator(const PoolAllocator<U> &orig) : _state(orig._state)
        {
        }

        pointer allocate(size_type n, const void *cvp = 0)
        {
            if (n == 0)
                return NULL;
            void *mem = _state->allocate_bytes(n * sizeof(T));
            return reinterpret_cast<pointer>(mem);
        }

        void deallocate(pointer p, size_type n)
        {
            if (!p || n == 0)
                return;
            _state->deallocate_bytes(p, n * sizeof(T));
        }

        size_type max_size() { return std::numeric_limits<size_t>::max() / sizeof(T); }

        void construct(pointer p, const T &val)
        {
            // 위치 지정 new (이미 할당된 메모리에 생성자만 호출)
            new ((void *)p) T(val);
        }

        void destory(pointer p) { p->~T(); }
    };
} // namespace yn

#endif

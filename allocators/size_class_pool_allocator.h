#ifndef SIZE_CLASS_POOL_ALLOCATOR_H
#define SIZE_CLASS_POOL_ALLOCATOR_H

#include <cstddef> // size_t, ptrdiff_t
#include <limits>
#include <new>

/*
  - 복사/변환 생성자로 생성된 allocator는 같은 SizeClassPoolAllocState를 공유
  - SizeClassPoolAllocState에서 block_size별로 pool를 관리
*/

namespace yona
{

    // L (left): 해당 클래스에서 가장 작은 블럭 수
    // R (right): 해당 클래스에서 가장 큰 블럭 수
    // S (step): 해당 클래스 내의 증가 단위
    enum
    {
        // Class 1
        C1_L = 8,
        C1_R = 128,
        C1_S = 8,

        // Class 2
        C2_L = 144,
        C2_R = 512,
        C2_S = 16,

        // Class 3
        C3_L = 544,
        C3_R = 2048,
        C3_S = 32,

        // Class 4
        C4_L = 2112,
        C4_R = 8192,
        C4_S = 64,

        // count for each class
        N_C1 = (C1_R - C1_L) / C1_S + 1,      // 16
        N_C2 = (C2_R - C2_L) / C2_S + 1,      // 24
        N_C3 = (C3_R - C3_L) / C3_S + 1,      // 48
        N_C4 = (C4_R - C4_L) / C4_S + 1,      // 96
        N_CLASSES = N_C1 + N_C2 + N_C3 + N_C4 // 184
    };

    enum
    {
        INVALID_INDEX = (size_t)-1,
    };

    static inline size_t align_up(size_t x, size_t a) { return (x + (a - 1)) & (~(a - 1)); }

    static inline size_t bytes_to_idx(size_t bytes)
    {
        if (bytes == 0 || bytes > C4_R)
            return INVALID_INDEX;

        size_t bs;
        if (bytes <= C1_R)
        {
            bs = align_up(bytes, C1_S);
            return (bs - C1_L) / C1_S;
        }
        if (bytes <= C2_R)
        {
            bs = align_up(bytes, C2_S);
            return (bs - C2_L) / C2_S;
        }
        if (bytes <= C3_R)
        {
            bs = align_up(bytes, C3_S);
            return (bs - C3_L) / C3_S;
        }
        if (bytes <= C4_R)
        {
            bs = align_up(bytes, C4_S);
            return (bs - C4_L) / C4_S;
        }
    }

    // bs = block_size
    size_t idx_to_bs(size_t idx)
    {
        if (idx < N_C1)
            return C1_L + idx * C1_S;
        idx -= N_C1;
        if (idx < N_C2)
            return C2_L + idx * C2_S;
        idx -= N_C2;
        if (idx < N_C3)
            return C3_L + idx * C3_S;
        idx -= N_C3;
        if (idx < N_C4)
            return C4_L + idx * C4_S;
    }

    // ------------------------------------------------------------
    // State : owns multiple pools (one per block size)
    // ------------------------------------------------------------
    class SizeClassPoolAllocState
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
        };

        // block_size 값에 따라 하나씩 존재
        // 해당하는 block_size의 chunk
        struct Pool
        {
            size_t     chunk_size; // upstream에서 한 번에 크게 가져오는 단위
            ChunkNode *chunk_list;
            FreeNode  *block_list;

            // block size만 정해지면 나머지는 enum 상수값에 의해 알아서 결정됨
            Pool(size_t idx)
                : chunk_size(idx_to_bs(idx) * BLOCK_PER_CHUNK), chunk_list(NULL), block_list(NULL)
            {
            }
        };

      private:
        Pool  *_pools[N_CLASSES]; // lazy init
        size_t _refcount;

      public:
        SizeClassPoolAllocState() : _refcount(1)
        {
            for (int i = 0; i < N_CLASSES; i++)
                _pools[i] = NULL;
        }
        ~SizeClassPoolAllocState()
        {
            Pool      *p;
            ChunkNode *c;
            for (int i = 0; i < N_CLASSES; i++)
            {
                p = _pools[i];
                c = p->chunk_list;
                while (c)
                {
                    ChunkNode *old_c = c;
                    ::operator delete(c->mem);
                    delete c;
                    c = old_c->next;
                }
                delete p;
            }
        }

        void retain() { ++_refcount; }
        void release()
        {
            if (--_refcount == 0)
                delete this;
        }

        // 하나의 chunk를 추가하고 쪼개 block 리스트에 추가
        void refill(Pool *p)
        {
            void      *mem = ::operator new(p->chunk_size);
            ChunkNode *new_chunk = new ChunkNode(mem, p->chunk_list);
            p->chunk_list = new_chunk;

            // carve blocks
            char        *cur = (char *)mem;
            char        *end = cur + p->chunk_size;
            const size_t bs = idx_to_bs();
            while (cur + p->block_size <= end)
            {
                FreeNode *new_block = reinterpret_cast<FreeNode *>(cur);
                new_block->next = p->block_list;
                p->block_list = new_block;
                cur += p->block_size;
            }
        }

        // allocator는 연속된 메모리를 돌려주어야하므로 무조건 올림 처리하여 할당
        void *allocate_bytes(size_t bytes) {
            const size_t idx = bytes_to_idx(bytes);

            
        }


        void deallocate_bytes(void *ptr, size_t bytes) {}
    };

    // ------------------------------------------------------------
    // Pool Allocator
    // ------------------------------------------------------------
    template <class T>
    class SizeClassPoolAllocator
    {
      public:
        typedef T             *pointer;
        typedef const T       *const_pointer;
        typedef T              value_type;
        typedef size_t         size_type;
        typedef std::ptrdiff_t difference_type;

        template <class U>
        struct rebind
        {
            typedef SizeClassPoolAllocator<U> other;
        };

      private:
        SizeClassPoolAllocState *_state;

      public:
        // constructor
        SizeClassPoolAllocator() : _state(new SizeClassPoolAllocState) {}
        SizeClassPoolAllocator(const SizeClassPoolAllocator &orig) : _state(orig._state)
        {
            _state->retain();
        }
        SizeClassPoolAllocator &operator=(const SizeClassPoolAllocator &orig)
        {
            if (this != &orig)
            {
                _state->release();
                _state = orig._state;
                _state->retain();
            }
            return *this;
        }
        template <class U>
        SizeClassPoolAllocator(const SizeClassPoolAllocator<U> &orig) : _state(orig._state)
        {
            _state->retain();
        }

        ~SizeClassPoolAllocator() { _state->release(); }

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

        void destroy(pointer p) { p->~T(); }
    };
} // namespace yona

#endif

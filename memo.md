https://en.cppreference.com/w/cpp/named_req/Allocator.html

컨테이너가 allocate(1)을 호출하면:

1. PoolAllocator<T>::allocate(1)
2. 공유 상태 PoolState에게 위임
3. PoolState는 sizeof(T)에 해당하는 **블록 사이즈 풀(Pool)**을 찾음
4. 그 Pool의 free_list에서 블록 하나 pop
5. 비어있으면 upstream에서 큰 chunk를 받아서(예: 32KB)
6. chunk를 블록으로 쪼개 free_list에 쌓고 다시 pop
7. 포인터 반환
8. deallocate(1)은 반대로 free_list에 push.
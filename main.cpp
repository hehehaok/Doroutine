#include <iostream>
#include "doroutine.h"

void test_doroutine1() {
    std::cout << "yield before:3" << std::endl;
    std::cout << "yield before:2" << std::endl;
    std::cout << "yield before:1" << std::endl;
    KSC::Doroutine::GetThis()->yield();
    std::cout << "yield before:1" << std::endl;
    std::cout << "yield before:2" << std::endl;
    std::cout << "yield before:3" << std::endl;
}

void test_doroutine2() {
    std::cout << "yield before:c" << std::endl;
    std::cout << "yield before:b" << std::endl;
    std::cout << "yield before:a" << std::endl;
    KSC::Doroutine::GetThis()->yield();
    std::cout << "yield before:a" << std::endl;
    std::cout << "yield before:b" << std::endl;
    std::cout << "yield before:c" << std::endl;
}

void allocateAndPrint(size_t stackSize) {
    void *m_stack = nullptr;

    // First allocation.
    m_stack = KSC::StackAllocator::Alloc(stackSize);
    std::cout << "First allocation address: " << m_stack << std::endl;

    // Free the first allocation before attempting to allocate again.
    KSC::StackAllocator::Dealloc(m_stack);

    // Second allocation.
    m_stack = KSC::StackAllocator::Alloc(stackSize);
    std::cout << "Second allocation address: " << m_stack << std::endl;

    // Clean up.
    KSC::StackAllocator::Dealloc(m_stack);
}

int main() {
    KSC::Doroutine::threadMainDoroutineInit();
    KSC::Doroutine::ptr doroutine1 = std::make_shared<KSC::Doroutine>(test_doroutine1, 1024*4, false);
    KSC::Doroutine::ptr doroutine2 = std::make_shared<KSC::Doroutine>(test_doroutine2, 1024*3, false);
    std::cout << "doroutine1 ptr count is " << doroutine1.use_count() << std::endl;
    std::cout << "doroutine2 ptr count is " << doroutine2.use_count() << std::endl;

    doroutine1->resume();
    doroutine2->resume();
    std::cout << "doroutine1 ptr count is " << doroutine1.use_count() << std::endl;
    std::cout << "doroutine2 ptr count is " << doroutine2.use_count() << std::endl;

    doroutine1->resume();
    doroutine2->resume();
    std::cout << "doroutine1 ptr count is " << doroutine1.use_count() << std::endl;
    std::cout << "doroutine2 ptr count is " << doroutine2.use_count() << std::endl;

    std::cout << "test end" << std::endl;
    return 0;
}
/*************************************************************************
 *
 * This file is part of the SAMRAI distribution.  For full copyright
 * information, see COPYRIGHT and COPYING.LESSER.
 *
 * Copyright:     (c) 1997-2016 Lawrence Livermore National Security, LLC
 * Description:   STL allocator for allocating unified memory when available
 *
 ************************************************************************/

#ifndef included_tbox_STLAllocator
#define included_tbox_STLAllocator

#include "SAMRAI/SAMRAI_config.h"

#include <cstdlib>
#include <cstddef>

#if defined(ENABLE_SIMPOOL)
#include "DynamicPoolAllocator.hpp"
#endif

namespace SAMRAI {
namespace tbox {

template <class T, class BaseAllocator>
struct STLAllocator {
  typedef T value_type;
  typedef std::size_t size_type;

#if defined(ENABLE_SIMPOOL)
  typedef DynamicPoolAllocator<BaseAllocator> PoolType;
  PoolType &m;
  STLAllocator() : m(PoolType::getInstance()) { }
  STLAllocator(const STLAllocator& a) : m(a.m) { }
#else
  STLAllocator() {}
  STLAllocator(const STLAllocator& a) { }
#endif

  T* allocate(std::size_t n) {
#if defined(ENABLE_SIMPOOL)
    return static_cast<T*>( m.allocate( n * sizeof(T) ) );
#else
    return static_cast<T*>( BaseAllocator::allocate( n * sizeof(T) ) );
#endif
  }

  void deallocate(T* p, std::size_t n) {
#if defined(ENABLE_SIMPOOL)
    m.deallocate(p);
#else
    BaseAllocator::deallocate(p);
#endif
  }

  size_type max_size() const { return std::numeric_limits<size_type>::max(); }
};

template <class T, class U, class A, class B>
bool operator==(const STLAllocator<T,A>&, const STLAllocator<U,B>&) { return true; }

template <class T, class U, class A, class B>
bool operator!=(const STLAllocator<T,A>&, const STLAllocator<U,B>&) { return false; }

}
}

#endif

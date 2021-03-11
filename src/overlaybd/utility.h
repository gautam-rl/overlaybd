/*
 * utility.h
 *
 * Copyright (C) 2021 Alibaba Group.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * See the file COPYING included with this distribution for more details.
 */
#pragma once
#include <cstdint>
#include <cstddef>
#include <type_traits>

#define _unused(x) ((void)(x))

template <typename... Ts>
inline void __unused__(const Ts &...) {
}

template <typename T>
struct ptr_array_t {
    T *pbegin;
    T *pend;
    T *begin() {
        return pbegin;
    }
    T *end() {
        return pend;
    }
};

template <typename T>
ptr_array_t<T> ptr_array(T *pbegin, size_t n) {
    return {pbegin, pbegin + n};
}

template <typename T>
class Defer {
public:
    Defer(T fn) : m_func(fn) {
    }
    ~Defer() {
        m_func();
    }
    void operator=(const Defer<T> &) = delete;
    operator bool() {
        return true;
    }

private:
    T m_func;
};

template <typename T>
Defer<T> make_defer(T func) {
    return Defer<T>(func);
}

#define __INLINE__ __attribute__((always_inline))
#define __FORCE_INLINE__ __INLINE__ inline

#define _CONCAT_(a, b) a##b
#define _CONCAT(a, b) _CONCAT_(a, b)

#define DEFER(func) auto _CONCAT(__defer__, __LINE__) = make_defer([&]() __INLINE__ { func; })

template <typename T, size_t n>
constexpr size_t LEN(T (&x)[n]) {
    return n;
}

template <typename T>
struct is_function_pointer {
    static const bool value =
        std::is_pointer<T>::value && std::is_function<typename std::remove_pointer<T>::type>::value;
};

#define ENABLE_IF(COND) typename _CONCAT(__x__, __LINE__) = typename std::enable_if<COND>::type
#define IS_SAME(T, P) (std::is_same<typename std::remove_cv<T>::type, P>::value)
#define ENABLE_IF_SAME(T, P) ENABLE_IF(IS_SAME(T, P))
#define ENABLE_IF_NOT_SAME(T, P) ENABLE_IF(!IS_SAME(T, P))
#define IS_BASE_OF(A, B) (std::is_base_of<A, B>::value)
#define ENABLE_IF_BASE_OF(A, B) ENABLE_IF(IS_BASE_OF(A, B))
#define ENABLE_IF_NOT_BASE_OF(A, B) ENABLE_IF(!IS_BASE_OF(A, B))
#define IS_POINTER(T) (std::is_pointer<T>::value)
#define ENABLE_IF_POINTER(T) ENABLE_IF(IS_POINTER(T))
#define ENABLE_IF_NOT_POINTER(T) ENABLE_IF(!IS_POINTER(T))

inline bool is_power_of_2(uint64_t x) {
    return __builtin_popcountl(x) == 1;
}

template <typename INT>
struct xrange_t {
    const INT _begin, _end;
    const int64_t _step;
    struct iterator {
        const xrange_t *_xrange;
        INT i;
        iterator &operator++() {
            i += _xrange->_step;
            return *this;
        }
        INT operator*() {
            return i;
        }
        bool operator==(const iterator &rhs) const {
            return _xrange == rhs._xrange &&
                   (i == rhs.i || (i >= _xrange->_end && rhs.i >= _xrange->_end));
        }
        bool operator!=(const iterator &rhs) const {
            return !(*this == rhs);
        }
    };
    iterator begin() const {
        return iterator{this, _begin};
    }
    iterator end() const {
        return iterator{this, _end};
    }
};

// the xrange() series function are utilities to iterate a range of
// integers from begin (inclusive) to end (exclusive), imitating the
// xrange() function of Python
// usage: for (auto i: xrange(2, 8)) { ... }

template <typename T, ENABLE_IF(std::is_signed<T>::value)>
xrange_t<int64_t> xrange(T begin, T end, int64_t step = 1) {
    static_assert(std::is_integral<T>::value, "...");
    return xrange_t<int64_t>{begin, end, step};
}

template <typename T, ENABLE_IF(std::is_signed<T>::value)>
xrange_t<int64_t> xrange(T end) {
    return xrange<T>(0, end);
}

template <typename T, ENABLE_IF(!std::is_signed<T>::value)>
xrange_t<uint64_t> xrange(T begin, T end, int64_t step = 1) {
    static_assert(std::is_integral<T>::value, "...");
    return xrange_t<uint64_t>{begin, end, step};
}

template <typename T, ENABLE_IF(!std::is_signed<T>::value)>
xrange_t<uint64_t> xrange(T end) {
    return xrange<T>(0, end);
}

inline uint64_t alingn_down(uint64_t x, uint64_t alignment) {
    return x & ~(alignment - 1);
}

inline uint64_t alingn_up(uint64_t x, uint64_t alignment) {
    return alingn_down(x + alignment - 1, alignment);
}

template <typename T>
inline T *align_ptr(T *p, uint64_t alignment) {
    return (T *)alingn_up((uint64_t)p, alignment);
}

#define ALIGNED_MEM(name, size, alignment)                                                         \
    char __buf##name[(size) + (alignment)];                                                        \
    char *name = align_ptr(__buf##name, alignment);

#define ALIGNED_MEM4K(buf, size) ALIGNED_MEM(buf, size, 4096)

template <typename T>
void safe_delete(T *&obj) {
    delete obj;
    obj = nullptr;
}

class OwnedPtr_Base {
protected:
    void *m_ptr;
    OwnedPtr_Base(void *ptr, bool ownership) {
        m_ptr = (void *)((uint64_t)ptr & (uint64_t)ownership);
    }
    const static uint64_t mask = 1;
    void *get() {
        return (void *)((uint64_t)m_ptr & ~mask);
    }
    bool owned() {
        return (uint64_t)m_ptr & mask;
    }
};

template <typename T>
class OwnedPtr : OwnedPtr_Base {
public:
    OwnedPtr(T *ptr, bool ownership) : OwnedPtr_Base(ptr, ownership) {
    }
    ~OwnedPtr() {
        if (owned())
            delete (T *)get();
    }
    T *operator->() {
        return (T *)get();
    }
    operator T *() {
        return (T *)get();
    }
};

#define _unused(x) ((void)(x))

// release resource by RAII
#define WITH(init_expr) if (init_expr)

// release resource explicitly
#define WITH_Release(init_expr, release_expr)                                                      \
    if (init_expr)                                                                                 \
        if (DEFER(release_expr))

/* example of WITH
    WITH_Release(auto x=getx(), release(x)) {
        printf("x=%d, y=%d\n", x, __y);
    }

    WITH(auto x = getx()) {
                printf("x=%d, y=%d\n", x, __y);
    }
*/

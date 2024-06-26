/*
 * MIT License
 * Copyright (c) 2017 Charlie Curtsinger
 */

#if !defined(__INTERPOSE_HH)
#define __INTERPOSE_HH

#include <cstdint>
#include <functional>
#include <type_traits>
#include <dlfcn.h>

/// Function type inspection utility for interpose
template<typename F> struct fn_info {
  using type = F;
  using ret_type = void;
};

/// Specialize the fn_info template for functions with non-void return types
template<typename R, typename... Args> struct fn_info<R(Args...)> {
  using type = R(Args...);
  using ret_type = R;
};

/// Starting with C++17, some library implementations of malloc are marked noexcept.
// template<typename R, typename... Args>
// struct fn_info<R(Args...) noexcept> : fn_info<R(Args...)> {};

#if defined(__ELF__)

/**
 * The linux interposition process uses weak aliases to replace the original function
 * and creates a real::___ function that will perform dynamic symbol resolution on the
 * first call. Be careful when interposing on memory allocation functions in particular;
 * simple operations like printing or symbol resolution could trigger another call to
 * malloc or calloc, which can cause unbounded recursion.
 */
#define INTERPOSE(NAME) \
  namespace real { \
    template<typename... Args> \
    auto NAME(Args... args) -> decltype(::NAME(args...)) { \
      static decltype(::NAME)* real_##NAME; \
      decltype(::NAME)* func = __atomic_load_n(&real_##NAME, __ATOMIC_CONSUME); \
      if(!func) { \
        func = reinterpret_cast<decltype(::NAME)*>( \
          reinterpret_cast<uintptr_t>(dlsym(RTLD_NEXT, #NAME))); \
        __atomic_store_n(&real_##NAME, func, __ATOMIC_RELEASE); \
      } \
      return func(std::forward<Args>(args)...); \
    } \
  } \
  extern "C" decltype(::NAME) NAME __attribute__((weak, alias("__interpose_" #NAME))); \
  extern "C" fn_info<decltype(::NAME)>::ret_type __interpose_##NAME

#define INTERPOSE__C_GENERIC__(RETURN_TYPE, NAME, ARG_TYPE_AND_NAME_LIST, ...) \
  static RETURN_TYPE Real__##NAME ARG_TYPE_AND_NAME_LIST { \
    static __typeof__(NAME)* real_##NAME; \
    __typeof__(NAME)* func = __atomic_load_n(&real_##NAME, __ATOMIC_CONSUME); \
    if(!func) { \
      func = (__typeof__(NAME)*)( \
        (uintptr_t)(dlsym(RTLD_NEXT, #NAME))); \
      __atomic_store_n(&real_##NAME, func, __ATOMIC_RELEASE); \
    } \
    __VA_ARGS__; \
  } \
  extern "C" __typeof__(NAME) NAME __attribute__((weak, alias("__interpose_" #NAME))); \
  extern "C" RETURN_TYPE __interpose_##NAME ARG_TYPE_AND_NAME_LIST

#define INTERPOSE_C(RETURN_TYPE, NAME, ARG_TYPE_AND_NAME_LIST, ARG_NAME_LIST) \
  INTERPOSE__C_GENERIC__(RETURN_TYPE, NAME, ARG_TYPE_AND_NAME_LIST, return func ARG_NAME_LIST)

#define INTERPOSE_C_LAMBDA(RETURN_TYPE, NAME, ARG_TYPE_AND_NAME_LIST, CALL_OLD_FUNC) \
  INTERPOSE__C_GENERIC__(RETURN_TYPE, NAME, ARG_TYPE_AND_NAME_LIST, CALL_OLD_FUNC)
          
#elif defined(__APPLE__)

/// Structure exposed to the linker for interposition
struct __osx_interpose {
	const void* new_func;
	const void* orig_func;
};

/**
 * Generate a macOS interpose struct
 * Types from: http://opensource.apple.com/source/dyld/dyld-210.2.3/include/mach-o/dyld-interposing.h
 */
#define OSX_INTERPOSE_STRUCT(NEW, OLD) \
  static const __osx_interpose __osx_interpose_##OLD \
    __attribute__((used, section("__DATA, __interpose"))) = \
    { reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(&(NEW))), \
      reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(&(OLD))) }

/**
  * The OSX interposition process is much simpler. Just create an OSX interpose struct,
  * include the actual function in the `real` namespace, and declare the beginning of the
  * replacement function with the appropriate return type.
  */
#define INTERPOSE(NAME) \
  namespace real { \
    using ::NAME; \
  } \
  extern "C" decltype(::NAME) __interpose_##NAME; \
  OSX_INTERPOSE_STRUCT(__interpose_##NAME, NAME); \
  extern "C" fn_info<decltype(::NAME)>::ret_type __interpose_##NAME

#define INTERPOSE__C_GENERIC__(RETURN_TYPE, NAME, ARG_TYPE_AND_NAME_LIST, ...) \
  static RETURN_TYPE Real__##NAME ARG_TYPE_AND_NAME_LIST { \
    __VA_ARGS__; \
  } \
  extern "C" RETURN_TYPE __interpose_##NAME ARG_TYPE_AND_NAME_LIST; \
  OSX_INTERPOSE_STRUCT(__interpose_##NAME, NAME); \
  extern "C" RETURN_TYPE __interpose_##NAME ARG_TYPE_AND_NAME_LIST

#define INTERPOSE_C(RETURN_TYPE, NAME, ARG_TYPE_AND_NAME_LIST, ARG_NAME_LIST) \
  INTERPOSE__C_GENERIC__(RETURN_TYPE, NAME, ARG_TYPE_AND_NAME_LIST, return NAME ARG_NAME_LIST)
  
#define INTERPOSE_C_LAMBDA(RETURN_TYPE, NAME, ARG_TYPE_AND_NAME_LIST, CALL_OLD_FUNC) \
  INTERPOSE__C_GENERIC__(RETURN_TYPE, NAME, ARG_TYPE_AND_NAME_LIST, CALL_OLD_FUNC)

#endif

#endif

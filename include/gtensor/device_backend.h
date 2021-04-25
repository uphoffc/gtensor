#ifndef GTENSOR_DEVICE_BACKEND_H
#define GTENSOR_DEVICE_BACKEND_H

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <type_traits>

#ifdef GTENSOR_HAVE_DEVICE
#include "device_runtime.h"

#if defined(GTENSOR_DEVICE_CUDA) || defined(GTENSOR_USE_THRUST)
#include "thrust_ext.h"
#endif

#ifdef GTENSOR_DEVICE_SYCL
#include "sycl_backend.h"
#endif

#endif // GTENSOR_HAVE_DEVICE

#include "defs.h"
#include "macros.h"

namespace gt
{

namespace space
{

struct host;

#ifdef GTENSOR_HAVE_DEVICE
struct device;
#else
using device = host;
#endif

} // namespace space

namespace backend
{

#ifdef GTENSOR_DEVICE_CUDA

inline void device_synchronize()
{
  gtGpuCheck(cudaStreamSynchronize(0));
}

inline int device_get_count()
{
  int device_count;
  gtGpuCheck(cudaGetDeviceCount(&device_count));
  return device_count;
}

inline void device_set(int device_id)
{
  gtGpuCheck(cudaSetDevice(device_id));
}

inline int device_get()
{
  int device_id;
  gtGpuCheck(cudaGetDevice(&device_id));
  return device_id;
}

inline uint32_t device_get_vendor_id(int device_id)
{
  cudaDeviceProp prop;
  uint32_t packed = 0;

  gtGpuCheck(cudaGetDeviceProperties(&prop, device_id));

  packed |= (0x000000FF & ((uint32_t)prop.pciDeviceID));
  packed |= (0x0000FF00 & (((uint32_t)prop.pciBusID) << 8));
  packed |= (0xFFFF0000 & (((uint32_t)prop.pciDomainID) << 16));

  return packed;
}

template <typename T>
inline void device_copy_async_dd(const T* src, T* dst, gt::size_type count)
{
  gtGpuCheck(
    cudaMemcpyAsync(dst, src, sizeof(T) * count, cudaMemcpyDeviceToDevice));
}

inline void device_memset(void* dst, int value, gt::size_type nbytes)
{
  gtGpuCheck(cudaMemset(dst, value, nbytes));
}

#elif defined(GTENSOR_DEVICE_HIP)

inline void device_synchronize()
{
  gtGpuCheck(hipStreamSynchronize(0));
}

inline int device_get_count()
{
  int device_count;
  gtGpuCheck(hipGetDeviceCount(&device_count));
  return device_count;
}

inline void device_set(int device_id)
{
  gtGpuCheck(hipSetDevice(device_id));
}

inline int device_get()
{
  int device_id;
  gtGpuCheck(hipGetDevice(&device_id));
  return device_id;
}

inline uint32_t device_get_vendor_id(int device_id)
{
  hipDeviceProp_t prop;
  uint32_t packed = 0;

  gtGpuCheck(hipGetDeviceProperties(&prop, device_id));

  packed |= (0x000000FF & ((uint32_t)prop.pciDeviceID));
  packed |= (0x0000FF00 & (((uint32_t)prop.pciBusID) << 8));
  packed |= (0xFFFF0000 & (((uint32_t)prop.pciDomainID) << 16));

  return packed;
}

template <typename T>
inline void device_copy_async_dd(const T* src, T* dst, gt::size_type count)
{
  gtGpuCheck(
    hipMemcpyAsync(dst, src, sizeof(T) * count, hipMemcpyDeviceToDevice));
}

inline void device_memset(void* dst, int value, gt::size_type nbytes)
{
  gtGpuCheck(hipMemset(dst, value, nbytes));
}

#elif defined(GTENSOR_DEVICE_SYCL)

inline void device_synchronize()
{
  gt::backend::sycl::get_queue().wait();
}

template <typename T>
inline void device_copy_async_dd(const T* src, T* dst, gt::size_type count)
{
  cl::sycl::queue& q = gt::backend::sycl::get_queue();
  q.memcpy(dst, src, sizeof(T) * count);
}

inline void device_memset(void* dst, int value, gt::size_type nbytes)
{
  cl::sycl::queue& q = gt::backend::sycl::get_queue();
  q.memset(dst, value, nbytes);
}

#endif // GTENSOR_DEVICE_{CUDA,HIP,SYCL}

#ifdef GTENSOR_DEVICE_HOST

inline void device_synchronize()
{
  // no need to synchronize on host
}

#endif

#ifdef GTENSOR_USE_THRUST

template <typename Pointer>
inline auto raw_pointer_cast(Pointer p)
{
  return thrust::raw_pointer_cast(p);
}

template <typename Pointer>
inline auto device_pointer_cast(Pointer p)
{
  return thrust::device_pointer_cast(p);
}

#else // using gt::backend::device_storage

// define no-op device_pointer/raw ponter casts
template <typename Pointer>
inline Pointer raw_pointer_cast(Pointer p)
{
  return p;
}

template <typename Pointer>
inline Pointer device_pointer_cast(Pointer p)
{
  return p;
}

#endif // GTENSOR_USE_THRUST

// ======================================================================

template <typename T, typename A>
struct wrap_allocator
{
  using value_type = T;
  using size_type = gt::size_type;

  T* allocate(size_type n) { return A::template allocate<T>(n); }
  void deallocate(T* p, size_type n) { A::deallocate(p); }
};

// ======================================================================
// backend::cuda

#ifdef GTENSOR_DEVICE_CUDA

namespace cuda
{

namespace detail
{

template <typename S_src, typename S_to>
struct copy;

template <>
struct copy<space::device, space::device>
{
  template <typename T>
  static void run(const T* src, T* dst, size_type count)
  {
    gtGpuCheck(
      cudaMemcpy(dst, src, sizeof(T) * count, cudaMemcpyDeviceToDevice));
  }
};

template <>
struct copy<space::device, space::host>
{
  template <typename T>
  static void run(const T* src, T* dst, size_type count)
  {
    gtGpuCheck(cudaMemcpy(dst, src, sizeof(T) * count, cudaMemcpyDeviceToHost));
  }
};

template <>
struct copy<space::host, space::device>
{
  template <typename T>
  static void run(const T* src, T* dst, size_type count)
  {
    gtGpuCheck(cudaMemcpy(dst, src, sizeof(T) * count, cudaMemcpyHostToDevice));
  }
};

template <>
struct copy<space::host, space::host>
{
  template <typename T>
  static void run(const T* src, T* dst, size_type count)
  {
    gtGpuCheck(cudaMemcpy(dst, src, sizeof(T) * count, cudaMemcpyHostToHost));
  }
};

} // namespace detail

struct ops
{
  using size_type = gt::size_type;

  struct device
  {
    template <typename T>
    static T* allocate(size_type n)
    {
      T* p;
      gtGpuCheck(cudaMalloc(&p, sizeof(T) * n));
      return p;
    }

    template <typename T>
    static void deallocate(T* p)
    {
      gtGpuCheck(cudaFree(p));
    }
  };

  struct managed
  {
    template <typename T>
    static T* allocate(size_t n)
    {
      T* p;
      gtGpuCheck(cudaMallocManaged(&p, sizeof(T) * n));
      return p;
    }

    template <typename T>
    static void deallocate(T* p)
    {
      gtGpuCheck(cudaFree(p));
    }
  };

  struct host
  {
    template <typename T>
    static T* allocate(size_type n)
    {
      T* p;
      gtGpuCheck(cudaMallocHost(&p, sizeof(T) * n));
      return p;
    }

    template <typename T>
    static void deallocate(T* p)
    {
      gtGpuCheck(cudaFreeHost(p));
    }
  };

  template <typename S_src, typename S_to, typename T>
  static void copy(const T* src, T* dst, gt::size_type count)
  {
    return detail::copy<S_src, S_to>::run(src, dst, count);
  }
};

template <typename T>
using device_allocator = wrap_allocator<T, typename ops::device>;

template <typename T>
using host_allocator = wrap_allocator<T, typename ops::host>;

} // namespace cuda

#endif

// ======================================================================
// backend::hip

#ifdef GTENSOR_DEVICE_HIP

namespace hip
{

namespace detail
{

template <typename S_src, typename S_to>
struct copy;

template <>
struct copy<space::device, space::device>
{
  template <typename T>
  static void run(const T* src, T* dst, size_type count)
  {
    gtGpuCheck(hipMemcpy(dst, src, sizeof(T) * count, hipMemcpyDeviceToDevice));
  }
};

template <>
struct copy<space::device, space::host>
{
  template <typename T>
  static void run(const T* src, T* dst, size_type count)
  {
    gtGpuCheck(hipMemcpy(dst, src, sizeof(T) * count, hipMemcpyHostToDevice));
  }
};

template <>
struct copy<space::host, space::device>
{
  template <typename T>
  static void run(const T* src, T* dst, size_type count)
  {
    gtGpuCheck(hipMemcpy(dst, src, sizeof(T) * count, hipMemcpyHostToDevice));
  }
};

template <>
struct copy<space::host, space::host>
{
  template <typename T>
  static void run(const T* src, T* dst, size_type count)
  {
    gtGpuCheck(hipMemcpy(dst, src, sizeof(T) * count, hipMemcpyHostToHost));
  }
};

} // namespace detail

struct ops
{
  struct device
  {
    template <typename T>
    static T* allocate(size_type n)
    {
      T* p;
      gtGpuCheck(hipMalloc(&p, sizeof(T) * n));
      return p;
    }

    template <typename T>
    static void deallocate(T* p)
    {
      gtGpuCheck(hipFree(p));
    }
  };

  struct managed
  {
    template <typename T>
    static T* allocate(size_t n)
    {
      T* p;
      gtGpuCheck(hipMallocManaged(&p, sizeof(T) * n));
      return p;
    }

    template <typename T>
    static void deallocate(T* p)
    {
      gtGpuCheck(hipFree(p));
    }
  };

  struct host
  {
    template <typename T>
    static T* allocate(size_type n)
    {
      T* p;
      gtGpuCheck(hipHostMalloc(&p, sizeof(T) * n));
      return p;
    }

    template <typename T>
    static void deallocate(T* p)
    {
      gtGpuCheck(hipHostFree(p));
    }
  };

  template <typename S_src, typename S_to, typename T>
  static void copy(const T* src, T* dst, gt::size_type count)
  {
    return detail::copy<S_src, S_to>::run(src, dst, count);
  }
};

template <typename T>
using device_allocator = wrap_allocator<T, typename ops::device>;

template <typename T>
using host_allocator = wrap_allocator<T, typename ops::host>;

} // namespace hip

#endif

// ======================================================================
// backend::sycl

#ifdef GTENSOR_DEVICE_SYCL

namespace sycl
{

namespace detail
{

template <typename S_src, typename S_to>
struct copy
{
  // TODO: SYCL exception handler
  template <typename T>
  static void run(const T* src, T* dst, size_type count)
  {
    cl::sycl::queue& q = gt::backend::sycl::get_queue();
    q.memcpy(dst, src, sizeof(T) * count);
    q.wait();
  }
};

} // namespace detail

struct ops
{
  struct device
  {
    template <typename T>
    static T* allocate(size_type n)
    {
      return cl::sycl::malloc_shared<T>(n, gt::backend::sycl::get_queue());
    }

    template <typename T>
    static void deallocate(T* p)
    {
      cl::sycl::free(p, gt::backend::sycl::get_queue());
    }
  };

  struct managed
  {
    template <typename T>
    static T* allocate(size_t n)
    {
      return cl::sycl::malloc_shared<T>(n, gt::backend::sycl::get_queue());
    }

    template <typename T>
    static void deallocate(T* p)
    {
      cl::sycl::free(p, gt::backend::sycl::get_queue());
    }
  };

  // The host allocation type in SYCL allows device code to directly access
  // the data. This is generally not necessary or effecient for gtensor, so
  // we opt for the same implementation as for the HOST device below.

  struct host
  {
    template <typename T>
    static T* allocate(size_type n)
    {
      T* p = static_cast<T*>(malloc(sizeof(T) * n));
      if (!p) {
        std::cerr << "host allocate failed" << std::endl;
        std::abort();
      }
      return p;
    }

    template <typename T>
    static void deallocate(T* p)
    {
      free(p);
    }
  };

  // template <typename T>
  // struct host
  // {
  //   static T* allocate( : size_type count)
  //   {
  //     return cl::sycl::malloc_host<T>(count, gt::backend::sycl::get_queue());
  //   }

  //   static void deallocate(T* p)
  //   {
  //     cl::sycl::free(p, gt::backend::sycl::get_queue());
  //   }
  // };

  template <typename S_src, typename S_to, typename T>
  static void copy(const T* src, T* dst, gt::size_type count)
  {
    return detail::copy<S_src, S_to>::run(src, dst, count);
  }
};

template <typename T>
using device_allocator = wrap_allocator<T, typename ops::device>;

template <typename T>
using host_allocator = wrap_allocator<T, typename ops::host>;

} // namespace sycl

#endif // GTENSOR_DEVICE_SYCL

// ======================================================================
// backend::host

namespace host
{

struct ops
{
  using size_type = gt::size_type;

  template <typename S_src, typename S_to, typename T>
  static void copy(const T* src, T* dst, size_type count)
  {
    std::memcpy(dst, src, sizeof(T) * count);
  }
};

template <typename T>
using host_allocator = std::allocator<T>;

}; // namespace host

// ======================================================================
// select backend

#ifdef GTENSOR_DEVICE_CUDA
using namespace cuda;
#elif GTENSOR_DEVICE_HIP
using namespace hip;
#elif GTENSOR_DEVICE_SYCL
using namespace sycl;
#elif GTENSOR_DEVICE_HOST
using namespace host;
#endif

} // namespace backend

// ======================================================================
// synchronize

void inline synchronize()
{
  gt::backend::device_synchronize();
}

} // namespace gt

#endif // GENSOR_DEVICE_BACKEND_H

#ifndef GTENSOR_DEVICE_BACKEND_H
#define GTENSOR_DEVICE_BACKEND_H

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <type_traits>

#ifdef GTENSOR_HAVE_DEVICE
#include "device_runtime.h"

#ifdef GTENSOR_USE_THRUST
#include "thrust_ext.h"
#endif

#ifdef GTENSOR_DEVICE_SYCL
#include "sycl_backend.h"
#endif

#include "macros.h"

#endif // GTENSOR_HAVE_DEVICE

namespace gt
{

namespace backend
{

#ifdef GTENSOR_DEVICE_CUDA

void inline device_synchronize()
{
  gtGpuCheck(cudaStreamSynchronize(0));
}

template <typename T>
void device_copy_hh(const T* src, T* dst, size_t count)
{
  gtGpuCheck(cudaMemcpy(dst, src, sizeof(T) * count, cudaMemcpyHostToHost));
}

template <typename T>
void device_copy_dd(const T* src, T* dst, size_t count)
{
  gtGpuCheck(cudaMemcpy(dst, src, sizeof(T) * count, cudaMemcpyDeviceToDevice));
}

template <typename T>
void device_copy_dh(const T* src, T* dst, size_t count)
{
  gtGpuCheck(cudaMemcpy(dst, src, sizeof(T) * count, cudaMemcpyDeviceToHost));
}

template <typename T>
void device_copy_hd(const T* src, T* dst, size_t count)
{
  gtGpuCheck(cudaMemcpy(dst, src, sizeof(T) * count, cudaMemcpyHostToDevice));
}

template <typename T>
struct device_allocator
{
  static T* allocate(int count)
  {
    T* p;
    gtGpuCheck(cudaMalloc(&p, sizeof(T) * count));
    return p;
  }

  static void deallocate(T* p)
  {
    if (p != nullptr) {
      gtGpuCheck(cudaFree(p));
    }
  }

  static void copy(const T* src, T* dst, std::size_t bytes)
  {
    device_copy_dd(src, dst, bytes);
  }
};

template <typename T>
struct host_allocator
{
  static T* allocate(int count)
  {
    T* p;
    gtGpuCheck(cudaMallocHost(&p, sizeof(T) * count));
    return p;
  }

  static void deallocate(T* p)
  {
    if (p != nullptr) {
      gtGpuCheck(cudaFreeHost(p));
    }
  }

  static void copy(const T* src, T* dst, std::size_t bytes)
  {
    device_copy_hh(src, dst, bytes);
  }
};

#elif defined(GTENSOR_DEVICE_HIP)

void inline device_synchronize()
{
  gtGpuCheck(hipStreamSynchronize(0));
}

template <typename T>
void device_copy_hh(const T* src, T* dst, size_t count)
{
  gtGpuCheck(hipMemcpy(dst, src, sizeof(T) * count, hipMemcpyHostToHost));
}

template <typename T>
void device_copy_dd(const T* src, T* dst, size_t count)
{
  gtGpuCheck(hipMemcpy(dst, src, sizeof(T) * count, hipMemcpyDeviceToDevice));
}

template <typename T>
void device_copy_dh(const T* src, T* dst, size_t count)
{
  gtGpuCheck(hipMemcpy(dst, src, sizeof(T) * count, hipMemcpyDeviceToHost));
}

template <typename T>
void device_copy_hd(const T* src, T* dst, size_t count)
{
  gtGpuCheck(hipMemcpy(dst, src, sizeof(T) * count, hipMemcpyHostToDevice));
}

template <typename T>
struct device_allocator
{
  static T* allocate(int count)
  {
    T* p;
    gtGpuCheck(hipMalloc(&p, sizeof(T) * count));
    return p;
  }

  static void deallocate(T* p)
  {
    if (p != nullptr) {
      gtGpuCheck(hipFree(p));
    }
  }

  static void copy(const T* src, T* dst, std::size_t count)
  {
    device_copy_dd(src, dst, count);
  }
};

template <typename T>
struct host_allocator
{
  static T* allocate(int count)
  {
    T* p;
    gtGpuCheck(hipHostMalloc(&p, sizeof(T) * count, hipHostMallocDefault));
    return p;
  }

  static void deallocate(T* p)
  {
    if (p != nullptr) {
      gtGpuCheck(hipHostFree(p));
    }
  }

  static void copy(const T* src, T* dst, std::size_t count)
  {
    device_copy_hh(src, dst, count);
  }
};

#elif defined(GTENSOR_DEVICE_SYCL)

void inline device_synchronize()
{
  gt::backend::sycl::get_queue().wait();
}

// TODO: SYCL exception handler
template <typename T>
void device_copy(const T* src, T* dst, size_t count)
{
  cl::sycl::queue& q = gt::backend::sycl::get_queue();
  q.memcpy(dst, src, sizeof(T) * count);
  q.wait();
}

template <typename T>
void device_copy_hh(const T* src, T* dst, size_t count)
{
  device_copy(src, dst, count);
}

template <typename T>
void device_copy_dd(const T* src, T* dst, size_t count)
{
  device_copy(src, dst, count);
}

template <typename T>
void device_copy_dh(const T* src, T* dst, size_t count)
{
  device_copy(src, dst, count);
}

template <typename T>
void device_copy_hd(const T* src, T* dst, size_t count)
{
  device_copy(src, dst, count);
}

template <typename T>
struct device_allocator
{
  static T* allocate(int count)
  {
    return cl::sycl::malloc_device<T>(count, gt::backend::sycl::get_queue());
  }

  static void deallocate(T* p)
  {
    if (p != nullptr) {
      cl::sycl::free(p, gt::backend::sycl::get_queue());
    }
  }

  static void copy(const T* src, T* dst, std::size_t count)
  {
    device_copy_dd(src, dst, count);
  }
};

// The host allocation type in SYCL allows device code to directly access
// the code. This is generally not necessary or effecient for gtensor, so
// we opt for the same implementation as for the HOST device below.
template <typename T>
struct host_allocator
{
  static T* allocate(int count)
  {
    T* p = static_cast<T*>(malloc(sizeof(T) * count));
    if (p == nullptr) {
      std::cerr << "host allocate failed" << std::endl;
      std::abort();
    }
    return p;
  }

  static void deallocate(T* p)
  {
    if (p != nullptr) {
      free(p);
    }
  }

  static void copy(const T* src, T* dst, std::size_t count)
  {
    std::memcpy(dst, src, sizeof(T) * count);
  }
};

/*
template<typename T>
struct host_allocator
{
  static T* allocate(int count)
  {
    return cl::sycl::malloc_host<T>(count, gt::backend::sycl::get_queue());
  }

  static void deallocate(T* p)
  {
    cl::sycl::free(p, gt::backend::sycl::get_queue());
  }

  static void copy(const void *src, void *dst, std::size_t count)
  {
    device_copy_hh(dst, src, count);
  }
};
*/

#endif // GTENSOR_DEVICE_{CUDA,HIP,SYCL}

#ifdef GTENSOR_DEVICE_HOST

void device_synchronize()
{
  // no need to synchronize on host
}

template <typename T>
struct host_allocator
{
  static T* allocate(int count)
  {
    T* p = static_cast<T*>(malloc(sizeof(T) * count));
    if (p == nullptr) {
      std::cerr << "host allocate failed" << std::endl;
      std::abort();
    }
    return p;
  }

  static void deallocate(T* p)
  {
    if (p != nullptr) {
      free(p);
    }
  }

  static void copy(const T* src, T* dst, std::size_t count)
  {
    std::memcpy(dst, src, count * sizeof(T));
  }
};

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

} // end namespace backend

} // end namespace gt

#endif // GENSOR_DEVICE_BACKEND_H
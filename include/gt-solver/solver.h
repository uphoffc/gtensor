#ifndef GTENSOR_SOLVE_H
#define GTENSOR_SOLVE_H

#include "gtensor/gtensor.h"
#include "gtensor/sparse.h"

#include "gt-blas/blas.h"

#ifdef GTENSOR_DEVICE_CUDA
#if CUDART_VERSION >= 12000
#ifdef GTENSOR_DEVICE_CUDA_CUSPARSE_GENERIC
// New generic API; available since 11.3.1, but performance
// is worse than old csrsm2 API in many cases pre-12.
#include "gt-solver/backend/cuda-generic.h"
#else
// bsrsm2 API, which can work with csr format by setting
// block size 1. In CUDA 12, appears to use less memory and
// often be faster than generic API. Exists even in 8.0,
// but not clear it has advantage over csrsm2 API for older
// cuda versions where csrsm2 is still available.
#include "gt-solver/backend/cuda-bsrsm2.h"
#endif // GTENSOR_DEVICE_CUDA_CUSPARSE_GENERIC
#else
// legacy API, deprecated since 11.3.1 but still supported until 12
#include "gt-solver/backend/cuda-csrsm2.h"
#endif // CUDA_VERSION

#elif defined(GTENSOR_DEVICE_HIP)
#include "gt-solver/backend/hip.h"

#elif defined(GTENSOR_DEVICE_SYCL)
#include "gt-solver/backend/sycl.h"
#endif

namespace gt
{

namespace solver
{

template <typename T>
class solver
{
public:
  using value_type = T;

  virtual void solve(T* rhs, T* result) = 0;
  virtual std::size_t get_device_memory_usage() = 0;
};

#ifdef GTENSOR_DEVICE_SYCL

// use contiguous strided dense API for SYCL, it has been better
// optimized

template <typename T>
class solver_dense : public solver<T>
{
public:
  using base_type = solver<T>;
  using typename base_type::value_type;

  solver_dense(gt::blas::handle_t& h, int n, int nbatches, int nrhs,
               T* const* matrix_batches);

  virtual void solve(T* rhs, T* result);
  virtual std::size_t get_device_memory_usage();

protected:
  gt::blas::handle_t& h_;
  int n_;
  int nbatches_;
  int nrhs_;
  gt::gtensor_device<T, 3> matrix_data_;
  gt::gtensor_device<gt::blas::index_t, 2> pivot_data_;
  gt::gtensor_device<T, 3> rhs_data_;
  gt::blas::index_t scratch_count_;
  gt::space::device_vector<T> scratch_;
};

#else // HIP and CUDA

template <typename T>
class solver_dense : public solver<T>
{
public:
  using base_type = solver<T>;
  using typename base_type::value_type;

  solver_dense(gt::blas::handle_t& h, int n, int nbatches, int nrhs,
               T* const* matrix_batches);

  virtual void solve(T* rhs, T* result);
  virtual std::size_t get_device_memory_usage();

protected:
  gt::blas::handle_t& h_;
  int n_;
  int nbatches_;
  int nrhs_;
  gt::gtensor_device<T, 3> matrix_data_;
  gt::gtensor_device<T*, 1> matrix_pointers_;
  gt::gtensor_device<gt::blas::index_t, 2> pivot_data_;
  gt::gtensor_device<int, 1> info_;
  gt::gtensor_device<T, 3> rhs_data_;
  gt::gtensor_device<T*, 1> rhs_pointers_;
};

#endif

template <typename T>
class solver_invert : solver<T>
{
public:
  using base_type = solver<T>;
  using typename base_type::value_type;

  solver_invert(gt::blas::handle_t& h, int n, int nbatches, int nrhs,
                T* const* matrix_batches);

  virtual void solve(T* rhs, T* result);
  virtual std::size_t get_device_memory_usage();

protected:
  gt::blas::handle_t& h_;
  int n_;
  int nbatches_;
  int nrhs_;
  gt::gtensor_device<T, 3> matrix_data_;
  gt::gtensor_device<T*, 1> matrix_pointers_;
  gt::gtensor_device<gt::blas::index_t, 2> pivot_data_;
  gt::gtensor_device<int, 1> info_;
  gt::gtensor_device<T, 3> rhs_data_;
  gt::gtensor_device<T*, 1> rhs_pointers_;
  gt::gtensor_device<T, 3> rhs_input_data_;
  gt::gtensor_device<T*, 1> rhs_input_pointers_;
};

template <typename T>
class solver_sparse : public solver<T>
{
public:
  using base_type = solver<T>;
  using typename base_type::value_type;

  solver_sparse(gt::blas::handle_t& blas_h, int n, int nbatches, int nrhs,
                T* const* matrix_batches);

  virtual void solve(T* rhs, T* result);
  virtual std::size_t get_device_memory_usage();

protected:
  int n_;
  int nbatches_;
  int nrhs_;
  gt::sparse::csr_matrix<T, gt::space::device> csr_mat_;
  csr_matrix_lu<T> csr_mat_lu_;

private:
  static gt::sparse::csr_matrix<T, gt::space::device> lu_factor_batches_to_csr(
    gt::blas::handle_t& h, int n, int nbatches, T* const* matrix_batches);
};

} // namespace solver

} // namespace gt

#endif

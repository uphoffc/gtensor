#include <gtest/gtest.h>

#include <cmath>
#include <iostream>
#include <stdexcept>

#include "gtensor/gtensor.h"

#include "gt-fft/fft.h"

#include "gtest_predicates.h"

constexpr double PI = 3.141592653589793;

template <typename E>
void fft_r2c_1d()
{
  constexpr int N = 4;
  constexpr int Nout = N / 2 + 1;
  constexpr int batch_size = 2;
  using T = gt::complex<E>;

  gt::gtensor<E, 2> h_A(gt::shape(N, batch_size));
  gt::gtensor_device<E, 2> d_A(gt::shape(N, batch_size));

  gt::gtensor<E, 2> h_A2(gt::shape(N, batch_size));
  gt::gtensor_device<E, 2> d_A2(gt::shape(N, batch_size));

  gt::gtensor<T, 2> h_B(gt::shape(Nout, batch_size));
  gt::gtensor_device<T, 2> d_B(gt::shape(Nout, batch_size));

  // x = [2 3 -1 4];
  h_A(0, 0) = 2;
  h_A(1, 0) = 3;
  h_A(2, 0) = -1;
  h_A(3, 0) = 4;

  // y = [7 -21 11 1];
  h_A(0, 1) = 7;
  h_A(1, 1) = -21;
  h_A(2, 1) = 11;
  h_A(3, 1) = 1;

  // zero output arrays
  gt::fill(d_B.data(), d_B.data() + batch_size * Nout, 0);
  gt::fill(d_A2.data(), d_A2.data() + batch_size * N, 0);

  gt::copy(h_A, d_A);

  // fft(x) -> [8+0i 3+1i -6+0i 3-1i]
  // but with fftw convention for real transforms, the last term is
  // conjugate of second and set to 0 to save storage / computation
  gt::fft::FFTPlanMany<gt::fft::Domain::REAL, E> plan({N}, batch_size);
  std::cout << "plan work bytes: " << plan.get_work_buffer_bytes() << std::endl;

  // plan.exec_forward(gt::raw_pointer_cast(d_A.data()),
  //                  gt::raw_pointer_cast(d_B.data()));
  plan(d_A, d_B);
  gt::copy(d_B, h_B);

  // test roundtripping data
  plan.inverse(d_B, d_A2);
  gt::copy(d_A2, h_A2);

  GT_EXPECT_EQ(h_A, h_A2 / N);

  GT_EXPECT_NEAR(h_B(0, 0), T(8, 0));
  GT_EXPECT_NEAR(h_B(1, 0), T(3, 1));
  GT_EXPECT_NEAR(h_B(2, 0), T(-6, 0));

  GT_EXPECT_NEAR(h_B(0, 1), T(-2, 0));
  GT_EXPECT_NEAR(h_B(1, 1), T(-4, 22));
  GT_EXPECT_NEAR(h_B(2, 1), T(38, 0));

  // reset input and output arrays and repeat with alternate ctor
  gt::copy(h_A, d_A);
  gt::fill(d_B.data(), d_B.data() + batch_size * Nout, 0);
  gt::fill(d_A2.data(), d_A2.data() + batch_size * N, 0);
  gt::fft::FFTPlanMany<gt::fft::Domain::REAL, E> plan2({N}, 1, N, 1, Nout,
                                                       batch_size);

  plan2(d_A, d_B);
  gt::copy(d_B, h_B);

  // test roundtripping data
  plan2.inverse(d_B, d_A2);
  gt::copy(d_A2, h_A2);

  GT_EXPECT_EQ(h_A, h_A2 / N);

  GT_EXPECT_NEAR(h_B(0, 0), T(8, 0));
  GT_EXPECT_NEAR(h_B(1, 0), T(3, 1));
  GT_EXPECT_NEAR(h_B(2, 0), T(-6, 0));

  GT_EXPECT_NEAR(h_B(0, 1), T(-2, 0));
  GT_EXPECT_NEAR(h_B(1, 1), T(-4, 22));
  GT_EXPECT_NEAR(h_B(2, 1), T(38, 0));
}

TEST(fft, d2z_1d) { fft_r2c_1d<double>(); }

TEST(fft, r2c_1d) { fft_r2c_1d<float>(); }

template <typename E>
void fft_r2c_1d_strided()
{
  constexpr int N = 4;
  constexpr int Nout = N / 2 + 1;
  constexpr int rstride = 2;
  constexpr int cstride = 3;
  constexpr int rdist = N * rstride;
  constexpr int cdist = Nout * cstride;
  constexpr int batch_size = 2;
  using T = gt::complex<E>;

  auto h_A = gt::zeros<E>({rdist, batch_size});
  auto d_A = gt::empty_device<E>(h_A.shape());

  auto h_A2 = gt::empty<E>(h_A.shape());
  auto d_A2 = gt::zeros_device<E>(h_A.shape());

  auto h_B = gt::empty<T>({cdist, batch_size});
  auto h_B_expected = gt::zeros<T>(h_B.shape());
  auto d_B = gt::zeros_device<T>(h_B.shape());

  // x = [2 3 -1 4];
  h_A(0 * rstride, 0) = 2;
  h_A(1 * rstride, 0) = 3;
  h_A(2 * rstride, 0) = -1;
  h_A(3 * rstride, 0) = 4;

  // y = [7 -21 11 1];
  h_A(0 * rstride, 1) = 7;
  h_A(1 * rstride, 1) = -21;
  h_A(2 * rstride, 1) = 11;
  h_A(3 * rstride, 1) = 1;

  h_B_expected(0 * cstride, 0) = T(8, 0);
  h_B_expected(1 * cstride, 0) = T(3, 1);
  h_B_expected(2 * cstride, 0) = T(-6, 0);

  h_B_expected(0 * cstride, 1) = T(-2, 0);
  h_B_expected(1 * cstride, 1) = T(-4, 22);
  h_B_expected(2 * cstride, 1) = T(38, 0);

  // zero output arrays

  gt::copy(h_A, d_A);

  // fft(x) -> [8+0i 3+1i -6+0i 3-1i]
  // but with fftw convention for real transforms, the last term is
  // conjugate of second and set to 0 to save storage / computation
  gt::fft::FFTPlanMany<gt::fft::Domain::REAL, E> plan(
    {N}, rstride, rdist, cstride, cdist, batch_size);
  std::cout << "plan work bytes: " << plan.get_work_buffer_bytes() << std::endl;

  // plan.exec_forward(gt::raw_pointer_cast(d_A.data()),
  //                  gt::raw_pointer_cast(d_B.data()));
  plan(d_A, d_B);
  gt::copy(d_B, h_B);

  // test roundtripping data
  plan.inverse(d_B, d_A2);
  gt::copy(d_A2, h_A2);

  GT_EXPECT_EQ(h_B_expected, h_B);
  GT_EXPECT_EQ(h_A, h_A2 / N);
}

TEST(fft, d2z_1d_strided) { fft_r2c_1d_strided<double>(); }

TEST(fft, r2c_1d_strided) { fft_r2c_1d_strided<float>(); }

template <typename E>
void fft_r2c_1d_strided_dist()
{
  constexpr int N = 4;
  constexpr int Nout = N / 2 + 1;
  constexpr int rstride = 2;
  constexpr int cstride = 3;
  constexpr int rdist = N * rstride + 7;
  constexpr int cdist = Nout * cstride + 2;
  constexpr int batch_size = 2;
  using T = gt::complex<E>;

  auto h_A = gt::zeros<E>({rdist, batch_size});
  auto d_A = gt::empty_device<E>(h_A.shape());

  auto h_A2 = gt::empty<E>(h_A.shape());
  auto d_A2 = gt::zeros_device<E>(h_A.shape());

  auto h_B = gt::zeros<T>({cdist, batch_size});
  auto h_B_expected = gt::zeros<T>(h_B.shape());
  auto d_B = gt::zeros_device<T>(h_B.shape());

  // x = [2 3 -1 4];
  h_A(0 * rstride, 0) = 2;
  h_A(1 * rstride, 0) = 3;
  h_A(2 * rstride, 0) = -1;
  h_A(3 * rstride, 0) = 4;

  // y = [7 -21 11 1];
  h_A(0 * rstride, 1) = 7;
  h_A(1 * rstride, 1) = -21;
  h_A(2 * rstride, 1) = 11;
  h_A(3 * rstride, 1) = 1;

  h_B_expected(0 * cstride, 0) = T(8, 0);
  h_B_expected(1 * cstride, 0) = T(3, 1);
  h_B_expected(2 * cstride, 0) = T(-6, 0);

  h_B_expected(0 * cstride, 1) = T(-2, 0);
  h_B_expected(1 * cstride, 1) = T(-4, 22);
  h_B_expected(2 * cstride, 1) = T(38, 0);

  gt::copy(h_A, d_A);

  // fft(x) -> [8+0i 3+1i -6+0i 3-1i]
  // but with fftw convention for real transforms, the last term is
  // conjugate of second and set to 0 to save storage / computation
  gt::fft::FFTPlanMany<gt::fft::Domain::REAL, E> plan(
    {N}, rstride, rdist, cstride, cdist, batch_size);
  std::cout << "plan work bytes: " << plan.get_work_buffer_bytes() << std::endl;

  // plan.exec_forward(gt::raw_pointer_cast(d_A.data()),
  //                  gt::raw_pointer_cast(d_B.data()));
  plan(d_A, d_B);
  gt::copy(d_B, h_B);

  // test roundtripping data
  plan.inverse(d_B, d_A2);
  gt::copy(d_A2, h_A2);

  GT_EXPECT_EQ(h_B_expected, h_B);
  GT_EXPECT_EQ(h_A, h_A2 / N);
}

TEST(fft, d2z_1d_strided_dist) { fft_r2c_1d_strided_dist<double>(); }

TEST(fft, r2c_1d_strided_dist) { fft_r2c_1d_strided_dist<float>(); }

template <typename E>
void fft_c2r_1d()
{
  constexpr int N = 4;
  constexpr int Ncomplex = N / 2 + 1;
  constexpr int batch_size = 2;
  using T = gt::complex<E>;

  gt::gtensor<E, 2> h_A(gt::shape(N, batch_size));
  gt::gtensor_device<E, 2> d_A(gt::shape(N, batch_size));

  gt::gtensor<T, 2> h_B(gt::shape(Ncomplex, batch_size));
  gt::gtensor_device<T, 2> d_B(gt::shape(Ncomplex, batch_size));

  h_B(0, 0) = T(8, 0);
  h_B(1, 0) = T(3, 1);
  h_B(2, 0) = T(-6, 0);

  h_B(0, 1) = T(-2, 0);
  h_B(1, 1) = T(-4, 22);
  h_B(2, 1) = T(38, 0);

  gt::copy(h_B, d_B);

  // ifft(x) -> [8+0i 3+1i -6+0i 3-1i]
  gt::fft::FFTPlanMany<gt::fft::Domain::REAL, E> plan({N}, batch_size);
  std::cout << "plan work bytes: " << plan.get_work_buffer_bytes() << std::endl;

  // plan.exec_inverse(gt::raw_pointer_cast(d_B.data()),
  //                  gt::raw_pointer_cast(d_A.data()));
  plan.inverse(d_B, d_A);

  gt::copy(d_A, h_A);

  EXPECT_EQ(h_A(0, 0) / N, 2);
  EXPECT_EQ(h_A(1, 0) / N, 3);
  EXPECT_EQ(h_A(2, 0) / N, -1);
  EXPECT_EQ(h_A(3, 0) / N, 4);

  EXPECT_EQ(h_A(0, 1) / N, 7);
  EXPECT_EQ(h_A(1, 1) / N, -21);
  EXPECT_EQ(h_A(2, 1) / N, 11);
  EXPECT_EQ(h_A(3, 1) / N, 1);
}

TEST(fft, z2d_1d) { fft_c2r_1d<double>(); }

TEST(fft, c2r_1d) { fft_c2r_1d<float>(); }

template <typename E>
void fft_c2c_1d_forward()
{
  constexpr int N = 4;
  constexpr int batch_size = 2;
  using T = gt::complex<E>;

  gt::gtensor<T, 2> h_A(gt::shape(N, batch_size));
  gt::gtensor_device<T, 2> d_A(gt::shape(N, batch_size));

  gt::gtensor<T, 2> h_A2(h_A.shape());
  gt::gtensor_device<T, 2> d_A2(h_A.shape());

  gt::gtensor<T, 2> h_B(gt::shape(N, batch_size));
  gt::gtensor_device<T, 2> d_B(gt::shape(N, batch_size));

  // x = [2 3 -1 4];
  h_A(0, 0) = 2;
  h_A(1, 0) = 3;
  h_A(2, 0) = -1;
  h_A(3, 0) = 4;

  // y = [7 -21 11 1];
  h_A(0, 1) = 7;
  h_A(1, 1) = -21;
  h_A(2, 1) = 11;
  h_A(3, 1) = 1;

  gt::copy(h_A, d_A);

  gt::fft::FFTPlanMany<gt::fft::Domain::COMPLEX, E> plan({N}, batch_size);
  std::cout << "plan work bytes: " << plan.get_work_buffer_bytes() << std::endl;

  // ifft(x) -> [8+0i 3+1i -6+0i 3-1i]
  // plan.exec_forward(gt::raw_pointer_cast(d_A.data()),
  //                  gt::raw_pointer_cast(d_B.data()));
  plan(d_A, d_B);

  gt::copy(d_B, h_B);

  GT_EXPECT_NEAR(h_B(0, 0), T(8, 0));
  GT_EXPECT_NEAR(h_B(1, 0), T(3, 1));
  GT_EXPECT_NEAR(h_B(2, 0), T(-6, 0));
  GT_EXPECT_NEAR(h_B(3, 0), T(3, -1));

  GT_EXPECT_NEAR(h_B(0, 1), T(-2, 0));
  GT_EXPECT_NEAR(h_B(1, 1), T(-4, 22));
  GT_EXPECT_NEAR(h_B(2, 1), T(38, 0));
  GT_EXPECT_NEAR(h_B(3, 1), T(-4, -22));

  // test round trip
  plan.inverse(d_B, d_A2);
  gt::copy(d_A2, h_A2);

  for (int i = 0; i < h_A.shape(1); i++) {
    for (int j = 0; j < h_A.shape(0); j++) {
      GT_EXPECT_NEAR(h_A(j, i), h_A2(j, i) / T(N, 0));
    }
  }
}

TEST(fft, z2z_1d_forward) { fft_c2c_1d_forward<double>(); }

TEST(fft, c2c_1d_forward) { fft_c2c_1d_forward<float>(); }

template <typename E>
void fft_c2c_1d_forward_strided()
{
  constexpr int N = 4;
  constexpr int istride = 2;
  constexpr int ostride = 3;
  constexpr int idist = N * istride;
  constexpr int odist = N * ostride;
  constexpr int batch_size = 2;
  using T = gt::complex<E>;

  auto h_A = gt::zeros<T>({idist, batch_size});
  auto d_A = gt::empty_device<T>(h_A.shape());

  auto h_A2 = gt::empty<T>(h_A.shape());
  auto d_A2 = gt::zeros_device<T>(h_A.shape());

  auto d_B = gt::zeros_device<T>({odist, batch_size});
  auto h_B = gt::empty<T>(d_B.shape());
  auto h_B_expected = gt::zeros<T>(d_B.shape());

  // x = [2 3 -1 4];
  h_A(0 * istride, 0) = 2;
  h_A(1 * istride, 0) = 3;
  h_A(2 * istride, 0) = -1;
  h_A(3 * istride, 0) = 4;

  // y = [7 -21 11 1];
  h_A(0 * istride, 1) = 7;
  h_A(1 * istride, 1) = -21;
  h_A(2 * istride, 1) = 11;
  h_A(3 * istride, 1) = 1;

  h_B_expected(0 * ostride, 0) = T(8, 0);
  h_B_expected(1 * ostride, 0) = T(3, 1);
  h_B_expected(2 * ostride, 0) = T(-6, 0);
  h_B_expected(3 * ostride, 0) = T(3, -1);

  h_B_expected(0 * ostride, 1) = T(-2, 0);
  h_B_expected(1 * ostride, 1) = T(-4, 22);
  h_B_expected(2 * ostride, 1) = T(38, 0);
  h_B_expected(3 * ostride, 1) = T(-4, -22);

  gt::copy(h_A, d_A);

  gt::fft::FFTPlanMany<gt::fft::Domain::COMPLEX, E> plan(
    {N}, istride, idist, ostride, odist, batch_size);
  std::cout << "plan work bytes: " << plan.get_work_buffer_bytes() << std::endl;

  // ifft(x) -> [8+0i 3+1i -6+0i 3-1i]
  // plan.exec_forward(gt::raw_pointer_cast(d_A.data()),
  //                  gt::raw_pointer_cast(d_B.data()));
  plan(d_A, d_B);

  gt::copy(d_B, h_B);

  // test round trip
  plan.inverse(d_B, d_A2);
  gt::copy(d_A2, h_A2);

  GT_EXPECT_NEAR(h_B_expected, h_B);
  GT_EXPECT_NEAR(h_A, h_A2 / T(N, 0));
}

TEST(fft, z2z_1d_forward_strided) { fft_c2c_1d_forward_strided<double>(); }

TEST(fft, c2c_1d_forward_strided) { fft_c2c_1d_forward_strided<float>(); }

template <typename E>
void fft_c2c_1d_forward_strided_dist()
{
  constexpr int N = 4;
  constexpr int istride = 2;
  constexpr int ostride = 3;
  constexpr int idist = N * istride + 1;
  constexpr int odist = N * ostride + 11;
  constexpr int batch_size = 2;
  using T = gt::complex<E>;

  auto h_A = gt::zeros<T>({idist, batch_size});
  auto d_A = gt::empty_device<T>(h_A.shape());

  auto h_A2 = gt::empty<T>(h_A.shape());
  auto d_A2 = gt::zeros_device<T>(h_A.shape());

  auto d_B = gt::zeros_device<T>({odist, batch_size});
  auto h_B = gt::empty<T>(d_B.shape());
  auto h_B_expected = gt::zeros<T>(d_B.shape());

  // x = [2 3 -1 4];
  h_A(0 * istride, 0) = 2;
  h_A(1 * istride, 0) = 3;
  h_A(2 * istride, 0) = -1;
  h_A(3 * istride, 0) = 4;

  // y = [7 -21 11 1];
  h_A(0 * istride, 1) = 7;
  h_A(1 * istride, 1) = -21;
  h_A(2 * istride, 1) = 11;
  h_A(3 * istride, 1) = 1;

  h_B_expected(0 * ostride, 0) = T(8, 0);
  h_B_expected(1 * ostride, 0) = T(3, 1);
  h_B_expected(2 * ostride, 0) = T(-6, 0);
  h_B_expected(3 * ostride, 0) = T(3, -1);

  h_B_expected(0 * ostride, 1) = T(-2, 0);
  h_B_expected(1 * ostride, 1) = T(-4, 22);
  h_B_expected(2 * ostride, 1) = T(38, 0);
  h_B_expected(3 * ostride, 1) = T(-4, -22);

  gt::copy(h_A, d_A);

  gt::fft::FFTPlanMany<gt::fft::Domain::COMPLEX, E> plan(
    {N}, istride, idist, ostride, odist, batch_size);
  std::cout << "plan work bytes: " << plan.get_work_buffer_bytes() << std::endl;

  // ifft(x) -> [8+0i 3+1i -6+0i 3-1i]
  // plan.exec_forward(gt::raw_pointer_cast(d_A.data()),
  //                  gt::raw_pointer_cast(d_B.data()));
  plan(d_A, d_B);

  gt::copy(d_B, h_B);

  // test round trip
  plan.inverse(d_B, d_A2);
  gt::copy(d_A2, h_A2);

  GT_EXPECT_NEAR(h_B_expected, h_B);
  GT_EXPECT_NEAR(h_A, h_A2 / T(N, 0));
}

TEST(fft, z2z_1d_forward_strided_dist)
{
  fft_c2c_1d_forward_strided_dist<double>();
}

TEST(fft, c2c_1d_forward_strided_dist)
{
  fft_c2c_1d_forward_strided_dist<float>();
}

template <typename E>
void fft_c2c_1d_inverse()
{
  constexpr int N = 4;
  constexpr int batch_size = 2;
  using T = gt::complex<E>;

  gt::gtensor<T, 2> h_A(gt::shape(N, batch_size));
  gt::gtensor_device<T, 2> d_A(gt::shape(N, batch_size));

  gt::gtensor<T, 2> h_B(gt::shape(N, batch_size));
  gt::gtensor_device<T, 2> d_B(gt::shape(N, batch_size));

  h_A(0, 0) = T(8, 0);
  h_A(1, 0) = T(3, 1);
  h_A(2, 0) = T(-6, 0);
  h_A(3, 0) = T(3, -1);

  h_A(0, 1) = T(-2, 0);
  h_A(1, 1) = T(-4, 22);
  h_A(2, 1) = T(38, 0);
  h_A(3, 1) = T(-4, -22);

  gt::copy(h_A, d_A);

  gt::fft::FFTPlanMany<gt::fft::Domain::COMPLEX, E> plan({N}, batch_size);
  std::cout << "plan work bytes: " << plan.get_work_buffer_bytes() << std::endl;

  // ifft(x) -> [8+0i 3+1i -6+0i 3-1i]
  // plan.exec_inverse(gt::raw_pointer_cast(d_A.data()),
  //                  gt::raw_pointer_cast(d_B.data()));
  plan.inverse(d_A, d_B);

  gt::copy(d_B, h_B);

  // required when using std::complex, int multiply is not defined
  auto dN = static_cast<E>(N);
  GT_EXPECT_NEAR(h_B(0, 0), dN * T(2, 0));
  GT_EXPECT_NEAR(h_B(1, 0), dN * T(3, 0));
  GT_EXPECT_NEAR(h_B(2, 0), dN * T(-1, 0));
  GT_EXPECT_NEAR(h_B(3, 0), dN * T(4, 0));

  GT_EXPECT_NEAR(h_B(0, 1), dN * T(7, 0));
  GT_EXPECT_NEAR(h_B(1, 1), dN * T(-21, 0));
  GT_EXPECT_NEAR(h_B(2, 1), dN * T(11, 0));
  GT_EXPECT_NEAR(h_B(3, 1), dN * T(1, 0));
}

TEST(fft, z2z_1d_inverse) { fft_c2c_1d_inverse<double>(); }

TEST(fft, c2c_1d_inverse) { fft_c2c_1d_inverse<float>(); }

TEST(fft, move_only)
{
  constexpr int N = 4;
  constexpr int Nout = N / 2 + 1;
  constexpr int batch_size = 1;
  using E = double;
  using T = gt::complex<E>;

  gt::gtensor<E, 2> h_A(gt::shape(N, batch_size));
  gt::gtensor_device<E, 2> d_A(gt::shape(N, batch_size));

  gt::gtensor<T, 2> h_B(gt::shape(Nout, batch_size));
  gt::gtensor_device<T, 2> d_B(gt::shape(Nout, batch_size));

  // x = [2 3 -1 4];
  h_A(0, 0) = 2;
  h_A(1, 0) = 3;
  h_A(2, 0) = -1;
  h_A(3, 0) = 4;

  // zero output array, rocfft at least does not zero padding elements
  // for real to complex transform
  gt::fill(d_B.data(), d_B.data() + batch_size * Nout, 0);

  gt::copy(h_A, d_A);

  // fft(x) -> [8+0i 3+1i -6+0i 3-1i]
  // but with fftw convention for real transforms, the last term is
  // conjugate of second and set to 0 to save storage / computation
  gt::fft::FFTPlanMany<gt::fft::Domain::REAL, E> plan({N}, batch_size);

  // Should not compile
  //  auto plan_copy = plan;
  //  gt::fft::FFTPlanMany<gt::fft::Domain::REAL, E> plan_copy2(plan);

  // do a move, then try to execute
  auto plan_moved = std::move(plan);

  // original plan is not valid, should throw error
  EXPECT_THROW(plan(d_A, d_B), std::runtime_error);

  plan_moved(d_A, d_B);

  gt::copy(d_B, h_B);

  GT_EXPECT_NEAR(h_B(0, 0), T(8, 0));
  GT_EXPECT_NEAR(h_B(1, 0), T(3, 1));
  GT_EXPECT_NEAR(h_B(2, 0), T(-6, 0));
}

template <typename E>
void fft_r2c_2d()
{
  constexpr int Nx = 64;
  constexpr int Ny = 16;
  constexpr int batch_size = 1;
  using T = gt::complex<E>;

  auto h_A = gt::zeros<E>({Nx, Ny, batch_size});
  auto d_A = gt::empty_device<E>(h_A.shape());

  auto h_A2 = gt::zeros<E>(h_A.shape());
  auto d_A2 = gt::empty_device<E>(h_A.shape());

  auto h_B = gt::empty<T>({Nx / 2 + 1, Ny, batch_size});
  auto h_B_expected = gt::empty<T>(h_B.shape());
  auto d_B = gt::empty_device<T>(h_B.shape());

  // Set up periodic domain with frequencies 4 and 2
  // m = [sin(2*pi*x+4*pi*y) for x in -2:1/16:2-1/16, y in 0:1/16:1-1/16]
  double x, y;
  for (int j = 0; j < Ny; j++) {
    for (int i = 0; i < Nx; i++) {
      x = -2.0 + i / 16.0;
      y = j / 16.0;
      h_A(i, j, 0) = sin(2 * PI * x + 4 * PI * y);
    }
  }

  gt::copy(h_A, d_A);

  gt::fft::FFTPlanMany<gt::fft::Domain::REAL, E> plan({Ny, Nx}, batch_size);
  std::cout << "plan work bytes: " << plan.get_work_buffer_bytes() << std::endl;

  plan(d_A, d_B);
  gt::copy(d_B, h_B);

  // NB: allow greater error than for other tests
  double max_err = 20.0 * gt::test::detail::max_err<E>::value;

  // Expect denormalized -5i at (4,2) and 0 elsewhere
  for (int j = 0; j < h_B.shape(1); j++) {
    for (int i = 0; i < h_B.shape(0); i++) {
      if (i == 4 && j == 2) {
        h_B_expected(i, j, 0) = T(0, -0.5) * T(Nx * Ny, 0);
      } else {
        h_B_expected(i, j, 0) = T(0, 0);
      }
    }
  }

  GT_EXPECT_NEAR_MAXERR(h_B_expected, h_B, max_err);

  // test roundtripping data, with normalization
  plan.inverse(d_B, d_A2);
  gt::copy(d_A2, h_A2);
  GT_EXPECT_NEAR(h_A, h_A2 / (Nx * Ny));
}

TEST(fft, r2c_2d) { fft_r2c_2d<float>(); }

TEST(fft, d2z_2d) { fft_r2c_2d<double>(); }

template <typename E>
void fft_c2c_2d()
{
  constexpr int Nx = 17;
  constexpr int Ny = 5;
  constexpr int batch_size = 1;
  using T = gt::complex<E>;

  auto h_A = gt::zeros<T>({Nx, Ny, batch_size});
  auto d_A = gt::empty_device<T>(h_A.shape());

  auto h_A2 = gt::empty<T>(h_A.shape());
  auto d_A2 = gt::empty_device<T>(h_A.shape());

  auto h_B = gt::empty<T>({Nx, Ny, batch_size});
  auto d_B = gt::empty_device<T>(h_B.shape());

  // origin at center of domain has value 1, to model delta function
  h_A(Nx / 2 + 1, Ny / 2 + 1, 0) = T(1.0, 0.0);

  gt::copy(h_A, d_A);

  gt::fft::FFTPlanMany<gt::fft::Domain::COMPLEX, E> plan({Ny, Nx}, batch_size);
  std::cout << "plan work bytes: " << plan.get_work_buffer_bytes() << std::endl;

  plan(d_A, d_B);
  gt::copy(d_B, h_B);

  // FFT of delta function is all ones in magnitude
  auto h_B_flat = gt::flatten(h_B);
  double max_err = gt::test::detail::max_err<E>::value;
  for (int i = 0; i < h_B_flat.shape(0); i++) {
    ASSERT_NEAR(gt::abs(h_B_flat(i)), 1.0, max_err);
  }

  // test roundtripping data, with normalization
  plan.inverse(d_B, d_A2);
  gt::copy(d_A2, h_A2);
  for (int i = 0; i < h_A.shape(0); i++) {
    for (int j = 0; j < h_A.shape(1); j++) {
      GT_EXPECT_NEAR(h_A(i, j, 0), h_A2(i, j, 0) / T(Nx * Ny, 0));
    }
  }
}

TEST(fft, c2c_2d) { fft_c2c_2d<float>(); }

TEST(fft, z2z_2d) { fft_c2c_2d<double>(); }

template <typename E>
void fft_r2c_3d()
{
  constexpr int Nx = 17;
  constexpr int Ny = 11;
  constexpr int Nz = 5;
  constexpr int batch_size = 1;
  using T = gt::complex<E>;

  auto h_A = gt::zeros<E>({Nx, Ny, Nz, batch_size});
  auto d_A = gt::empty_device<E>(h_A.shape());

  auto h_A2 = gt::empty<E>(h_A.shape());
  auto d_A2 = gt::empty_device<E>(h_A.shape());

  auto h_B = gt::empty<T>({Nx / 2 + 1, Ny, Nz, batch_size});
  auto d_B = gt::empty_device<T>(h_B.shape());

  // origin at center of domain has value 1, to model delta function
  h_A(Nx / 2 + 1, Ny / 2 + 1, Nz / 2 + 1, 0) = 1.0;

  gt::copy(h_A, d_A);

  gt::fft::FFTPlanMany<gt::fft::Domain::REAL, E> plan({Nz, Ny, Nx}, batch_size);
  std::cout << "plan work bytes: " << plan.get_work_buffer_bytes() << std::endl;

  plan(d_A, d_B);
  gt::copy(d_B, h_B);

  // FFT of delta function is all ones in magnitude
  GT_EXPECT_NEAR(h_B, 1.0);

  // test roundtripping data, with normalization
  plan.inverse(d_B, d_A2);
  gt::copy(d_A2, h_A2);
  GT_EXPECT_NEAR(h_A, h_A2 / (Nx * Ny * Nz));
}

TEST(fft, r2c_3d) { fft_r2c_3d<float>(); }

TEST(fft, d2z_3d) { fft_r2c_3d<double>(); }


#include <benchmark/benchmark.h>

#include <gtensor/gtensor.h>

using namespace gt::placeholders;

using real_t = double;
using complex_t = gt::complex<double>;

// ======================================================================
// BM_device_assign_4d

static void BM_device_assign_4d(benchmark::State& state)
{
  auto a = gt::zeros_device<real_t>(gt::shape(100, 100, 100, 100));
  auto b = gt::empty_like(a);

  // warmup, device compile
  b = a + 2 * a;
  gt::synchronize();

  for (auto _ : state) {
    b = a + 2 * a;
    gt::synchronize();
  }
}

BENCHMARK(BM_device_assign_4d)->Unit(benchmark::kMillisecond);

// ======================================================================
// BM_add_ij_sten

auto i_sten_6d_5(const gt::gtensor_span<const real_t, 1>& sten,
                 const gt::gtensor_span_device<const complex_t, 6>& f, int bnd)
{
  return sten(0) * f.view(_s(bnd - 2, -bnd - 2)) +
         sten(1) * f.view(_s(bnd - 1, -bnd - 1)) +
         sten(2) * f.view(_s(bnd + 0, -bnd + 0)) +
         sten(3) * f.view(_s(bnd + 1, -bnd + 1)) +
         sten(4) * f.view(_s(bnd + 2, -bnd + 2));
}

static void BM_add_ij_sten(benchmark::State& state)
{
  const int bnd = 2; // # of ghost points
  auto shape_rhs = gt::shape(70, 32, 24, 24, 32, 2);
  auto shape_dist = shape_rhs;
  shape_dist[0] += 2 * bnd;

  auto rhs = gt::zeros_device<complex_t>(shape_rhs);
  auto dist = gt::zeros_device<complex_t>(shape_dist);
  auto kj = gt::zeros_device<complex_t>({shape_rhs[1]});
  auto sten =
    gt::gtensor<real_t, 1>({1. / 12., -2. / 3., 0., 2. / 3., -1 / 12.});

  real_t facj = 2.;

  auto fn = [&]() {
    rhs = rhs +
          facj *
            kj.view(_newaxis, _all, _newaxis, _newaxis, _newaxis, _newaxis) *
            dist.view(_s(bnd, -bnd)) +
          i_sten_6d_5(sten, dist, bnd);
    gt::synchronize();
  };

  // warmup, device compile
  fn();

  for (auto _ : state) {
    fn();
  }
}

BENCHMARK(BM_add_ij_sten)->Unit(benchmark::kMillisecond);

// ======================================================================
// BM_add_dgdxy

// FIXME, almost same as i_sten_6d_f
template <typename E, typename E_sten>
auto x_deriv_5(const E& f, const E_sten& sten, int bnd)
{
  return sten(0) * f.view(_s(bnd - 2, -bnd - 2)) +
         sten(1) * f.view(_s(bnd - 1, -bnd - 1)) +
         sten(2) * f.view(_s(bnd + 0, -bnd + 0)) +
         sten(3) * f.view(_s(bnd + 1, -bnd + 1)) +
         sten(4) * f.view(_s(bnd + 2, -bnd + 2));
}

template <typename E, typename E_ikj>
auto y_deriv(const E& f, const E_ikj& ikj, int bnd)
{
  return ikj.view(_newaxis, _all, _newaxis) * f.view(_s(bnd, -bnd), _all, _all);
}

static void BM_add_dgdxy(benchmark::State& state)
{
  const int bnd = 2; // # of ghost points
  auto shape_rhs = gt::shape(70, 32, 24 * 24 * 32 * 2);
  auto shape_f = shape_rhs;
  shape_f[0] += 2 * bnd;

  auto sten =
    gt::gtensor<real_t, 1>({1. / 12., -2. / 3., 0., 2. / 3., -1 / 12.});

  auto rhs = gt::zeros_device<complex_t>(shape_rhs);
  auto f = gt::zeros_device<complex_t>(shape_f);
  auto ikj = gt::zeros_device<complex_t>({shape_rhs[1]});
  auto p1 = gt::zeros_device<complex_t>({shape_rhs[0], shape_rhs[2]});
  auto p2 = gt::zeros_device<complex_t>({shape_rhs[0], shape_rhs[2]});

  auto dij =
    gt::empty_device<complex_t>({shape_rhs[0], shape_rhs[1], shape_rhs[2], 2});

  auto fn = [&]() {
    dij.view(_all, _all, _all, 0) = x_deriv_5(f, sten, bnd);
    dij.view(_all, _all, _all, 1) = y_deriv(f, ikj, bnd);

    rhs = rhs + p1.view(_all, _newaxis) * dij.view(_all, _all, _all, 0) +
          p2.view(_all, _newaxis) * dij.view(_all, _all, _all, 1);
    gt::synchronize();
  };

  // warm up, device compile
  fn();

  for (auto _ : state) {
    fn();
  }
}

BENCHMARK(BM_add_dgdxy)->Unit(benchmark::kMillisecond);

// ======================================================================
// BM_add_dgdxy_fused

static void BM_add_dgdxy_fused(benchmark::State& state)
{
  const int bnd = 2; // # of ghost points
  auto shape_rhs = gt::shape(70, 32, 24 * 24 * 32 * 2);
  auto shape_f = shape_rhs;
  shape_f[0] += 2 * bnd;

  auto sten =
    gt::gtensor<real_t, 1>({1. / 12., -2. / 3., 0., 2. / 3., -1 / 12.});

  auto rhs = gt::zeros_device<complex_t>(shape_rhs);
  auto f = gt::zeros_device<complex_t>(shape_f);
  auto ikj = gt::zeros_device<complex_t>({shape_rhs[1]});
  auto p1 = gt::zeros_device<complex_t>({shape_rhs[0], shape_rhs[2]});
  auto p2 = gt::zeros_device<complex_t>({shape_rhs[0], shape_rhs[2]});

  auto fn = [&]() {
    auto dx_f = x_deriv_5(f, sten, bnd);
    auto dy_f = y_deriv(f, ikj, bnd);

    rhs = rhs + p1.view(_all, _newaxis) * dx_f + p2.view(_all, _newaxis) * dy_f;
    gt::synchronize();
  };

  // warm up, device compile
  fn();

  for (auto _ : state) {
    fn();
  }
}

BENCHMARK(BM_add_dgdxy_fused)->Unit(benchmark::kMillisecond);

// ======================================================================
// BM_add_dgdxy_6d
//
// same as above, but without collapsing dims 3-6

template <typename E, typename E_ikj>
auto y_deriv_6d(const E& f, const E_ikj& ikj, int bnd)
{
  return ikj.view(_newaxis, _all, _newaxis, _newaxis, _newaxis, _newaxis) *
         f.view(_s(bnd, -bnd));
}

static void BM_add_dgdxy_6d(benchmark::State& state)
{
  const int bnd = 2; // # of ghost points
  auto shape_rhs = gt::shape(70, 32, 24, 24, 32, 2);
  auto shape_f = shape_rhs;
  shape_f[0] += 2 * bnd;

  auto sten =
    gt::gtensor<real_t, 1>({1. / 12., -2. / 3., 0., 2. / 3., -1 / 12.});

  auto rhs = gt::zeros_device<complex_t>(shape_rhs);
  auto f = gt::zeros_device<complex_t>(shape_f);
  auto ikj = gt::zeros_device<complex_t>({shape_rhs[1]});
  auto p1 = gt::zeros_device<complex_t>(
    {shape_rhs[0], shape_rhs[2], shape_rhs[3], shape_rhs[4], shape_rhs[5]});
  auto p2 = gt::zeros_device<complex_t>(
    {shape_rhs[0], shape_rhs[2], shape_rhs[3], shape_rhs[4], shape_rhs[5]});

  auto dij =
    gt::empty_device<complex_t>({shape_rhs[0], shape_rhs[1], shape_rhs[2],
                                 shape_rhs[3], shape_rhs[4], shape_rhs[5], 2});

  auto fn = [&]() {
    dij.view(_all, _all, _all, _all, _all, _all, 0) = x_deriv_5(f, sten, bnd);
    dij.view(_all, _all, _all, _all, _all, _all, 1) = y_deriv_6d(f, ikj, bnd);

    rhs =
      rhs +
      p1.view(_all, _newaxis) *
        dij.view(_all, _all, _all, _all, _all, _all, 0) +
      p2.view(_all, _newaxis) * dij.view(_all, _all, _all, _all, _all, _all, 1);
    gt::synchronize();
  };

  // warm up, device compile
  fn();

  for (auto _ : state) {
    fn();
  }
}

BENCHMARK(BM_add_dgdxy_6d)->Unit(benchmark::kMillisecond);

// ======================================================================
// BM_add_dgdxy_fused_6d

static void BM_add_dgdxy_fused_6d(benchmark::State& state)
{
  const int bnd = 2; // # of ghost points
  auto shape_rhs = gt::shape(70, 32, 24, 24, 32, 2);
  auto shape_f = shape_rhs;
  shape_f[0] += 2 * bnd;

  auto sten =
    gt::gtensor<real_t, 1>({1. / 12., -2. / 3., 0., 2. / 3., -1 / 12.});

  auto rhs = gt::zeros_device<complex_t>(shape_rhs);
  auto f = gt::zeros_device<complex_t>(shape_f);
  auto ikj = gt::zeros_device<complex_t>({shape_rhs[1]});
  auto p1 = gt::zeros_device<complex_t>(
    {shape_rhs[0], shape_rhs[2], shape_rhs[3], shape_rhs[4], shape_rhs[5]});
  auto p2 = gt::zeros_device<complex_t>(
    {shape_rhs[0], shape_rhs[2], shape_rhs[3], shape_rhs[4], shape_rhs[5]});

  auto fn = [&]() {
    auto dx_f = x_deriv_5(f, sten, bnd);
    auto dy_f = y_deriv_6d(f, ikj, bnd);

    rhs = rhs + p1.view(_all, _newaxis) * dx_f + p2.view(_all, _newaxis) * dy_f;

    gt::synchronize();
  };

  // warm up, device compile
  fn();

  for (auto _ : state) {
    fn();
  }
}

BENCHMARK(BM_add_dgdxy_fused_6d)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();

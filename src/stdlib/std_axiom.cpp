#include "clpp/stdlib.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1e-12;

enum Id : std::uint8_t {
  Clamp = 0,
  Map,
  Wrap,
  Sign,
  Round,
  RoundDigits,
  Snap,
  PingPong,
  Saturate,
  Fract,
  InvLerp,
  Approach,
  IsFinite,
  Abs,
  Min,
  Max,
  Pow,
  Sqrt,
  Cbrt,
  Hypot,
  Log,
  Exp,
  Smoothstep,
  Smootherstep,
  Gcd,
  Lcm,
  IsEven,
  IsOdd,
  Factorial,
  Scale,
  Lerp,
  LerpClamped,
  LerpVector2,
  LerpVector3,
  LerpVector,
  LerpColor3,
  LerpCFrame,
  LerpUDim2,
  LerpAngle,
  Inverse,
  Project,
  Reject,
  Reflect,
  Angle,
  Distance,
  Distance2,
  Orthonormal,
  OrthonormalUp,
  Slerp,
  SlerpCFrame,
  LookAt,
  Flat,
  InSine,
  OutSine,
  InOutSine,
  InQuad,
  OutQuad,
  InOutQuad,
  InCubic,
  OutCubic,
  InOutCubic,
  InQuart,
  OutQuart,
  InOutQuart,
  InQuint,
  OutQuint,
  InOutQuint,
  InExpo,
  OutExpo,
  InOutExpo,
  InCirc,
  OutCirc,
  InOutCirc,
  InBack,
  OutBack,
  InOutBack,
  InElastic,
  OutElastic,
  InOutElastic,
  InBounce,
  OutBounce,
  InOutBounce,
  Linear,
  CubicBezier,
  QuadraticBezier,
  Hover,
  Float,
  FloatSpin,
  AabbContains,
  SphereContains,
  RayPlane,
  Barycentric,
  ClosestPointOnSegment,
  Deg,
  Rad,
  DeltaAngle,
  AngleDiff,
  DeltaAngleDegrees,
  NormalizeAngle,
  Sin,
  Cos,
  Tan,
  Asin,
  Acos,
  Atan2,
  RandomRange,
  Random,
  Weighted,
  Gaussian,
  GaussianMS,
  Average,
  Sum,
  HashU32,
  Value1,
  Value2,
  Value3,
  FromHSV,
  Contrast,
  LerpHSV,
  Floor,
  Ceil,
  Trunc,
  Log2,
  Log10,
  IsNaN,
  Pi,
  Dot,
  Cross,
  Length,
  Normalize,
  IdCount
};

static_assert(Linear == 82, "easing id drift");
static_assert(LerpHSV == 118, "axiom id drift");
static_assert(IdCount == 130, "axiom count drift");

struct Spec {
  const char* name;
  Id id;
  std::uint8_t arity;
  const char* types[5];
};

constexpr Spec kSpecs[] = {
    {"Clamp", Clamp, 3, {"double", "double", "double", nullptr, nullptr}},
    {"Map", Map, 5, {"double", "double", "double", "double", "double"}},
    {"Wrap", Wrap, 3, {"double", "double", "double", nullptr, nullptr}},
    {"Sign", Sign, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Round", Round, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Round", RoundDigits, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"Snap", Snap, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"PingPong", PingPong, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"Saturate", Saturate, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Fract", Fract, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InvLerp", InvLerp, 3, {"double", "double", "double", nullptr, nullptr}},
    {"Approach", Approach, 3, {"double", "double", "double", nullptr, nullptr}},
    {"IsFinite", IsFinite, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Abs", Abs, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Min", Min, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"Max", Max, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"Pow", Pow, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"Sqrt", Sqrt, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Cbrt", Cbrt, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Hypot", Hypot, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"Log", Log, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Exp", Exp, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Smoothstep", Smoothstep, 3, {"double", "double", "double", nullptr, nullptr}},
    {"Smootherstep", Smootherstep, 3, {"double", "double", "double", nullptr, nullptr}},
    {"Gcd", Gcd, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"Lcm", Lcm, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"IsEven", IsEven, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"IsOdd", IsOdd, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Factorial", Factorial, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Scale", Scale, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"Lerp", Lerp, 3, {"double", "double", "double", nullptr, nullptr}},
    {"LerpClamped", LerpClamped, 3, {"double", "double", "double", nullptr, nullptr}},
    {"LerpVector2", LerpVector2, 3, {"Vector2", "Vector2", "double", nullptr, nullptr}},
    {"LerpVector3", LerpVector3, 3, {"Vector3", "Vector3", "double", nullptr, nullptr}},
    {"LerpVector", LerpVector, 3, {"Vector3", "Vector3", "double", nullptr, nullptr}},
    {"LerpColor3", LerpColor3, 3, {"Vector3", "Vector3", "double", nullptr, nullptr}},
    {"LerpCFrame", LerpCFrame, 3, {"Vector3", "Vector3", "double", nullptr, nullptr}},
    {"LerpUDim2", LerpUDim2, 3, {"Vector4", "Vector4", "double", nullptr, nullptr}},
    {"LerpAngle", LerpAngle, 3, {"double", "double", "double", nullptr, nullptr}},
    {"Inverse", Inverse, 3, {"double", "double", "double", nullptr, nullptr}},
    {"Project", Project, 2, {"Vector3", "Vector3", nullptr, nullptr, nullptr}},
    {"Reject", Reject, 2, {"Vector3", "Vector3", nullptr, nullptr, nullptr}},
    {"Reflect", Reflect, 2, {"Vector3", "Vector3", nullptr, nullptr, nullptr}},
    {"Angle", Angle, 2, {"Vector3", "Vector3", nullptr, nullptr, nullptr}},
    {"Distance", Distance, 2, {"Vector3", "Vector3", nullptr, nullptr, nullptr}},
    {"Distance2", Distance2, 2, {"Vector2", "Vector2", nullptr, nullptr, nullptr}},
    {"Orthonormal", Orthonormal, 1, {"Vector3", nullptr, nullptr, nullptr, nullptr}},
    {"Orthonormal", OrthonormalUp, 2, {"Vector3", "Vector3", nullptr, nullptr, nullptr}},
    {"Slerp", Slerp, 3, {"Vector3", "Vector3", "double", nullptr, nullptr}},
    {"SlerpCFrame", SlerpCFrame, 3, {"Vector3", "Vector3", "double", nullptr, nullptr}},
    {"LookAt", LookAt, 2, {"Vector3", "Vector3", nullptr, nullptr, nullptr}},
    {"Flat", Flat, 1, {"Vector3", nullptr, nullptr, nullptr, nullptr}},
    {"InSine", InSine, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"OutSine", OutSine, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InOutSine", InOutSine, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InQuad", InQuad, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"OutQuad", OutQuad, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InOutQuad", InOutQuad, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InCubic", InCubic, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"OutCubic", OutCubic, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InOutCubic", InOutCubic, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InQuart", InQuart, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"OutQuart", OutQuart, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InOutQuart", InOutQuart, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InQuint", InQuint, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"OutQuint", OutQuint, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InOutQuint", InOutQuint, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InExpo", InExpo, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"OutExpo", OutExpo, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InOutExpo", InOutExpo, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InCirc", InCirc, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"OutCirc", OutCirc, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InOutCirc", InOutCirc, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InBack", InBack, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"OutBack", OutBack, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InOutBack", InOutBack, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InElastic", InElastic, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"OutElastic", OutElastic, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InOutElastic", InOutElastic, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InBounce", InBounce, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"OutBounce", OutBounce, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"InOutBounce", InOutBounce, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Linear", Linear, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"CubicBezier", CubicBezier, 5, {"double", "Vector3", "Vector3", "Vector3", "Vector3"}},
    {"QuadraticBezier", QuadraticBezier, 4, {"double", "Vector3", "Vector3", "Vector3", nullptr}},
    {"Hover", Hover, 3, {"double", "Vector3", "Vector3", nullptr, nullptr}},
    {"Float", Float, 3, {"double", "Vector3", "Vector3", nullptr, nullptr}},
    {"FloatSpin", FloatSpin, 2, {"double", "Vector3", nullptr, nullptr, nullptr}},
    {"AabbContains", AabbContains, 3, {"Vector3", "Vector3", "Vector3", nullptr, nullptr}},
    {"SphereContains", SphereContains, 3, {"Vector3", "Vector3", "double", nullptr, nullptr}},
    {"RayPlane", RayPlane, 4, {"Vector3", "Vector3", "Vector3", "Vector3", nullptr}},
    {"Barycentric", Barycentric, 4, {"Vector3", "Vector3", "Vector3", "Vector3", nullptr}},
    {"ClosestPointOnSegment", ClosestPointOnSegment, 3, {"Vector3", "Vector3", "Vector3", nullptr, nullptr}},
    {"Deg", Deg, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Rad", Rad, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"DeltaAngle", DeltaAngle, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"AngleDiff", AngleDiff, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"DeltaAngleDegrees", DeltaAngleDegrees, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"NormalizeAngle", NormalizeAngle, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Sin", Sin, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Cos", Cos, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Tan", Tan, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Asin", Asin, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Acos", Acos, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Atan2", Atan2, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"RandomRange", RandomRange, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"Random", Random, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"Weighted", Weighted, 1, {"any", nullptr, nullptr, nullptr, nullptr}},
    {"Gaussian", Gaussian, 0, {nullptr, nullptr, nullptr, nullptr, nullptr}},
    {"Gaussian", GaussianMS, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"Average", Average, 1, {"any", nullptr, nullptr, nullptr, nullptr}},
    {"Sum", Sum, 1, {"any", nullptr, nullptr, nullptr, nullptr}},
    {"HashU32", HashU32, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Value1", Value1, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Value2", Value2, 2, {"double", "double", nullptr, nullptr, nullptr}},
    {"Value3", Value3, 3, {"double", "double", "double", nullptr, nullptr}},
    {"FromHSV", FromHSV, 3, {"double", "double", "double", nullptr, nullptr}},
    {"Contrast", Contrast, 2, {"Vector3", "double", nullptr, nullptr, nullptr}},
    {"LerpHSV", LerpHSV, 3, {"Vector3", "Vector3", "double", nullptr, nullptr}},
    {"Floor", Floor, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Ceil", Ceil, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Trunc", Trunc, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Log2", Log2, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Log10", Log10, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"IsNaN", IsNaN, 1, {"double", nullptr, nullptr, nullptr, nullptr}},
    {"Pi", Pi, 0, {nullptr, nullptr, nullptr, nullptr, nullptr}},
    {"Dot", Dot, 2, {"Vector3", "Vector3", nullptr, nullptr, nullptr}},
    {"Cross", Cross, 2, {"Vector3", "Vector3", nullptr, nullptr, nullptr}},
    {"Length", Length, 1, {"Vector3", nullptr, nullptr, nullptr, nullptr}},
    {"Normalize", Normalize, 1, {"Vector3", nullptr, nullptr, nullptr, nullptr}},
};

static_assert(std::size(kSpecs) == static_cast<std::size_t>(IdCount), "axiom table drift");

struct Vec {
  double x{0};
  double y{0};
  double z{0};
  double w{0};
};

[[nodiscard]] double clamp_num(const double value, const double lo, const double hi) {
  const double top = value < lo ? lo : value;
  return top > hi ? hi : top;
}

[[nodiscard]] double mod_positive(const double value, const double span) {
  if (span == 0.0) {
    return 0.0;
  }
  double wrapped = std::fmod(value, span);
  if (wrapped < 0.0) {
    wrapped += span;
  }
  return wrapped;
}

[[nodiscard]] double saturate_num(const double value) { return clamp_num(value, 0.0, 1.0); }

[[nodiscard]] bool num_of(const clpp::Value& value, double& out) {
  if (!value.is_number()) {
    return false;
  }
  out = value.number;
  return true;
}

[[nodiscard]] bool vec_of(const clpp::Value& value, Vec& out) {
  if (!value.is_vector()) {
    return false;
  }
  out.x = value.number;
  out.y = value.y;
  out.z = value.z;
  out.w = value.w;
  return true;
}

[[nodiscard]] clpp::Value vec_value(const Vec& value) { return clpp::Value::vector_of(value.x, value.y, value.z, value.w); }

[[nodiscard]] double dot3(const Vec& a, const Vec& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

[[nodiscard]] double len3(const Vec& value) { return std::sqrt(dot3(value, value)); }

[[nodiscard]] Vec add3(const Vec& a, const Vec& b) { return Vec{a.x + b.x, a.y + b.y, a.z + b.z, 0}; }

[[nodiscard]] Vec sub3(const Vec& a, const Vec& b) { return Vec{a.x - b.x, a.y - b.y, a.z - b.z, 0}; }

[[nodiscard]] Vec scale3(const Vec& value, const double factor) {
  return Vec{value.x * factor, value.y * factor, value.z * factor, 0};
}

[[nodiscard]] Vec lerp3(const Vec& a, const Vec& b, const double t, const bool with_w) {
  Vec out{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, 0};
  if (with_w) {
    out.w = a.w + (b.w - a.w) * t;
  }
  return out;
}

[[nodiscard]] Vec unit3(const Vec& value, const Vec& fallback) {
  const double length = len3(value);
  if (length < kEps) {
    return fallback;
  }
  // Divide rather than multiply by 1/length: (3, 0, 4) normalizes to exactly (0.6, 0, 0.8).
  return Vec{value.x / length, value.y / length, value.z / length, 0};
}

[[nodiscard]] Vec zero_vec() { return Vec{}; }

[[nodiscard]] Vec back_vec() { return Vec{0, 0, -1, 0}; }

[[nodiscard]] double unit_random() {
  thread_local std::mt19937 generator{std::random_device{}()};
  std::uniform_real_distribution<double> distribution(0.0, 1.0);
  const double sample = distribution(generator);
  return sample >= 1.0 ? std::nextafter(1.0, 0.0) : sample;
}

[[nodiscard]] std::uint32_t mix32(std::uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352du;
  value ^= value >> 15;
  value *= 0x846ca68bu;
  value ^= value >> 16;
  return value;
}

[[nodiscard]] std::uint32_t hash_double(const double number) {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &number, sizeof(bits));
  const auto lo = static_cast<std::uint32_t>(bits);
  const auto hi = static_cast<std::uint32_t>(bits >> 32);
  return mix32(lo ^ mix32(hi));
}

[[nodiscard]] double hash_unit(const std::uint32_t hash) { return static_cast<double>(hash) / 4294967296.0; }

[[nodiscard]] double delta_radians(const double from, const double to) {
  return mod_positive(to - from + kPi, kPi * 2.0) - kPi;
}

[[nodiscard]] double round_places(const double value, const double digits) {
  const double places = clamp_num(std::floor(digits), 0.0, 12.0);
  const double scale = std::pow(10.0, places);
  return std::floor(value * scale + 0.5) / scale;
}

[[nodiscard]] double smooth_t(const double edge0, const double edge1, const double x) {
  if (edge0 == edge1) {
    return x < edge0 ? 0.0 : 1.0;
  }
  return saturate_num((x - edge0) / (edge1 - edge0));
}

[[nodiscard]] double gcd_num(double a, double b) {
  a = std::fabs(a);
  b = std::fabs(b);
  for (int step = 0; step < 128 && b > 1e-9; ++step) {
    const double next = std::fabs(std::fmod(a, b));
    a = b;
    b = next;
  }
  return a;
}

[[nodiscard]] bool even_num(const double value) {
  if (!std::isfinite(value)) {
    return false;
  }
  const double remainder = std::fmod(std::fabs(value), 2.0);
  return remainder < 1e-9 || std::fabs(remainder - 2.0) < 1e-9;
}

[[nodiscard]] double out_bounce(double t) {
  constexpr double n1 = 7.5625;
  constexpr double d1 = 2.75;
  if (t < 1.0 / d1) {
    return n1 * t * t;
  }
  if (t < 2.0 / d1) {
    t -= 1.5 / d1;
    return n1 * t * t + 0.75;
  }
  if (t < 2.5 / d1) {
    t -= 2.25 / d1;
    return n1 * t * t + 0.9375;
  }
  t -= 2.625 / d1;
  return n1 * t * t + 0.984375;
}

[[nodiscard]] double ease_of(const Id id, double t) {
  t = saturate_num(t);
  constexpr double kBack = 1.70158;
  constexpr double kBack3 = 2.70158;
  const double c4 = (2.0 * kPi) / 3.0;
  const double c5 = (2.0 * kPi) / 4.5;
  switch (id) {
    case InSine:
      return 1.0 - std::cos((t * kPi) / 2.0);
    case OutSine:
      return std::sin((t * kPi) / 2.0);
    case InOutSine:
      return t == 0.5 ? 0.5 : -(std::cos(kPi * t) - 1.0) / 2.0;
    case InQuad:
      return t * t;
    case OutQuad:
      return 1.0 - (1.0 - t) * (1.0 - t);
    case InOutQuad:
      return t < 0.5 ? 2.0 * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 2.0) / 2.0;
    case InCubic:
      return t * t * t;
    case OutCubic:
      return 1.0 - std::pow(1.0 - t, 3.0);
    case InOutCubic:
      return t < 0.5 ? 4.0 * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0;
    case InQuart:
      return t * t * t * t;
    case OutQuart:
      return 1.0 - std::pow(1.0 - t, 4.0);
    case InOutQuart:
      return t < 0.5 ? 8.0 * t * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 4.0) / 2.0;
    case InQuint:
      return t * t * t * t * t;
    case OutQuint:
      return 1.0 - std::pow(1.0 - t, 5.0);
    case InOutQuint:
      return t < 0.5 ? 16.0 * t * t * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 5.0) / 2.0;
    case InExpo:
      return t == 0.0 ? 0.0 : std::pow(2.0, 10.0 * t - 10.0);
    case OutExpo:
      return t == 1.0 ? 1.0 : 1.0 - std::pow(2.0, -10.0 * t);
    case InOutExpo:
      if (t == 0.0) {
        return 0.0;
      }
      if (t == 1.0) {
        return 1.0;
      }
      return t < 0.5 ? std::pow(2.0, 20.0 * t - 10.0) / 2.0 : (2.0 - std::pow(2.0, -20.0 * t + 10.0)) / 2.0;
    case InCirc:
      return 1.0 - std::sqrt(1.0 - t * t);
    case OutCirc:
      return std::sqrt(1.0 - (t - 1.0) * (t - 1.0));
    case InOutCirc:
      return t < 0.5 ? (1.0 - std::sqrt(1.0 - std::pow(2.0 * t, 2.0))) / 2.0
                     : (std::sqrt(1.0 - std::pow(-2.0 * t + 2.0, 2.0)) + 1.0) / 2.0;
    case InBack:
      return kBack3 * t * t * t - kBack * t * t;
    case OutBack:
      return 1.0 + kBack3 * std::pow(t - 1.0, 3.0) + kBack * std::pow(t - 1.0, 2.0);
    case InOutBack: {
      const double c2 = kBack * 1.525;
      return t < 0.5 ? (std::pow(2.0 * t, 2.0) * ((c2 + 1.0) * 2.0 * t - c2)) / 2.0
                     : (std::pow(2.0 * t - 2.0, 2.0) * ((c2 + 1.0) * (t * 2.0 - 2.0) + c2) + 2.0) / 2.0;
    }
    case InElastic:
      if (t == 0.0 || t == 1.0) {
        return t;
      }
      return -std::pow(2.0, 10.0 * t - 10.0) * std::sin((10.0 * t - 10.75) * c4);
    case OutElastic:
      if (t == 0.0 || t == 1.0) {
        return t;
      }
      return std::pow(2.0, -10.0 * t) * std::sin((10.0 * t - 0.75) * c4) + 1.0;
    case InOutElastic:
      if (t == 0.0 || t == 1.0) {
        return t;
      }
      return t < 0.5 ? -(std::pow(2.0, 20.0 * t - 10.0) * std::sin((20.0 * t - 11.125) * c5)) / 2.0
                     : (std::pow(2.0, -20.0 * t + 10.0) * std::sin((20.0 * t - 11.125) * c5)) / 2.0 + 1.0;
    case OutBounce:
      return out_bounce(t);
    case InBounce:
      return 1.0 - out_bounce(1.0 - t);
    case InOutBounce:
      return t < 0.5 ? (1.0 - out_bounce(1.0 - 2.0 * t)) / 2.0 : (1.0 + out_bounce(2.0 * t - 1.0)) / 2.0;
    case Linear:
      return t;
    default:
      return t;
  }
}

void hsv_to_rgb(double h, double s, double v, double& r, double& g, double& b) {
  h -= std::floor(h);
  s = saturate_num(s);
  v = saturate_num(v);
  const double sector = h * 6.0;
  const double fraction = sector - std::floor(sector);
  const double p = v * (1.0 - s);
  const double q = v * (1.0 - fraction * s);
  const double t = v * (1.0 - (1.0 - fraction) * s);
  switch (static_cast<int>(std::floor(sector)) % 6) {
    case 0:
      r = v;
      g = t;
      b = p;
      break;
    case 1:
      r = q;
      g = v;
      b = p;
      break;
    case 2:
      r = p;
      g = v;
      b = t;
      break;
    case 3:
      r = p;
      g = q;
      b = v;
      break;
    case 4:
      r = t;
      g = p;
      b = v;
      break;
    default:
      r = v;
      g = p;
      b = q;
      break;
  }
}

void rgb_to_hsv(const double r, const double g, const double b, double& h, double& s, double& v) {
  const double peak = r > g ? (r > b ? r : b) : (g > b ? g : b);
  const double floor_channel = r < g ? (r < b ? r : b) : (g < b ? g : b);
  const double delta = peak - floor_channel;
  v = peak;
  s = peak == 0.0 ? 0.0 : delta / peak;
  if (delta == 0.0) {
    h = 0.0;
  } else if (peak == r) {
    h = std::fmod((g - b) / delta, 6.0);
  } else if (peak == g) {
    h = (b - r) / delta + 2.0;
  } else {
    h = (r - g) / delta + 4.0;
  }
  h /= 6.0;
  if (h < 0.0) {
    h += 1.0;
  }
}

[[nodiscard]] bool read_list(const clpp::Value& value, std::vector<double>& out) {
  if (!value.is_struct()) {
    return false;
  }
  out.clear();
  out.reserve(value.fields.size());
  for (const clpp::Value& field : value.fields) {
    if (!field.is_number()) {
      return false;
    }
    out.push_back(field.number);
  }
  return true;
}

[[nodiscard]] std::string build_source() {
  std::string source;
  source.reserve(24000);
  for (const Spec& spec : kSpecs) {
    source += "func ";
    source += spec.name;
    source += "(";
    for (std::uint8_t index = 0; index < spec.arity; ++index) {
      if (index != 0) {
        source += ", ";
      }
      source += spec.types[index];
      source += " p";
      source += static_cast<char>('0' + index);
    }
    source += ")";
    const std::string_view name = spec.name;
    if (name.rfind("Is", 0) == 0 || name == "AabbContains" || name == "SphereContains") {
      source += " -> bool";  // predicates print as true/false
    }
    source += " {\n  return axiom::";
    source += spec.name;
    source += "(";
    for (std::uint8_t index = 0; index < spec.arity; ++index) {
      if (index != 0) {
        source += ", ";
      }
      source += "p";
      source += static_cast<char>('0' + index);
    }
    source += ");\n}\n";
  }
  return source;
}

[[nodiscard]] bool bad(std::string& error) {
  error = "type error";
  return false;
}

}  // namespace

namespace clpp::stdlib {

std::string_view axiom_source() {
  static const std::string source = build_source();
  return source;
}

int axiom_native(const std::string_view name, const std::size_t arity) {
  constexpr std::string_view prefix = "axiom::";
  if (name.size() <= prefix.size() || name.substr(0, prefix.size()) != prefix) {
    return -1;
  }
  const std::string_view leaf = name.substr(prefix.size());
  for (const Spec& spec : kSpecs) {
    if (leaf == spec.name && arity == spec.arity) {
      return 1000 + static_cast<int>(spec.id);
    }
  }
  return -1;
}

bool axiom_apply(const std::uint8_t id, const Value* args, const std::uint8_t arity, Value& out, std::string& error) {
  const auto which = static_cast<Id>(id);
  const auto n1 = [&](double& a) { return arity == 1 && args != nullptr && num_of(args[0], a); };
  const auto n2 = [&](double& a, double& b) {
    return arity == 2 && args != nullptr && num_of(args[0], a) && num_of(args[1], b);
  };
  const auto n3 = [&](double& a, double& b, double& c) {
    return arity == 3 && args != nullptr && num_of(args[0], a) && num_of(args[1], b) && num_of(args[2], c);
  };
  const auto v1 = [&](Vec& a) { return arity == 1 && args != nullptr && vec_of(args[0], a); };
  const auto v2 = [&](Vec& a, Vec& b) {
    return arity == 2 && args != nullptr && vec_of(args[0], a) && vec_of(args[1], b);
  };

  if (which >= InSine && which <= Linear) {
    double t = 0;
    if (!n1(t)) {
      return bad(error);
    }
    out = Value::number_of(ease_of(which, t));
    return true;
  }

  switch (which) {
    case Clamp: {
      double value = 0;
      double lo = 0;
      double hi = 0;
      if (!n3(value, lo, hi)) {
        return bad(error);
      }
      out = Value::number_of(clamp_num(value, lo, hi));
      return true;
    }
    case Map: {
      if (arity != 5 || args == nullptr) {
        return bad(error);
      }
      double value = 0;
      double in_min = 0;
      double in_max = 0;
      double out_min = 0;
      double out_max = 0;
      if (!num_of(args[0], value) || !num_of(args[1], in_min) || !num_of(args[2], in_max) || !num_of(args[3], out_min) ||
          !num_of(args[4], out_max)) {
        return bad(error);
      }
      if (in_max == in_min) {
        out = Value::number_of(out_min);
        return true;
      }
      out = Value::number_of(out_min + (out_max - out_min) * (value - in_min) / (in_max - in_min));
      return true;
    }
    case Wrap: {
      double value = 0;
      double lo = 0;
      double hi = 0;
      if (!n3(value, lo, hi)) {
        return bad(error);
      }
      const double span = hi - lo;
      out = Value::number_of(span == 0.0 ? lo : lo + mod_positive(value - lo, span));
      return true;
    }
    case Sign: {
      double value = 0;
      if (!n1(value)) {
        return bad(error);
      }
      out = Value::number_of(value > 0.0 ? 1.0 : value < 0.0 ? -1.0 : 0.0);
      return true;
    }
    case Round:
    case RoundDigits: {
      double value = 0;
      double digits = 0;
      if (which == Round) {
        if (!n1(value)) {
          return bad(error);
        }
      } else if (!n2(value, digits)) {
        return bad(error);
      }
      out = Value::number_of(round_places(value, digits));
      return true;
    }
    case Snap: {
      double value = 0;
      double step = 0;
      if (!n2(value, step)) {
        return bad(error);
      }
      out = Value::number_of(step == 0.0 ? value : std::floor(value / step + 0.5) * step);
      return true;
    }
    case PingPong: {
      double t = 0;
      double length = 0;
      if (!n2(t, length)) {
        return bad(error);
      }
      if (length == 0.0) {
        out = Value::number_of(0);
        return true;
      }
      const double wrapped = mod_positive(t, length * 2.0);
      out = Value::number_of(length - std::fabs(wrapped - length));
      return true;
    }
    case Saturate: {
      double value = 0;
      if (!n1(value)) {
        return bad(error);
      }
      out = Value::number_of(saturate_num(value));
      return true;
    }
    case Fract: {
      double value = 0;
      if (!n1(value)) {
        return bad(error);
      }
      out = Value::number_of(value - std::floor(value));
      return true;
    }
    case InvLerp:
    case Inverse: {
      double from = 0;
      double to = 0;
      double value = 0;
      if (!n3(from, to, value)) {
        return bad(error);
      }
      out = Value::number_of(to == from ? 0.0 : (value - from) / (to - from));
      return true;
    }
    case Approach: {
      double current = 0;
      double target = 0;
      double max_delta = 0;
      if (!n3(current, target, max_delta)) {
        return bad(error);
      }
      const double limit = std::fabs(max_delta);
      out = Value::number_of(current + clamp_num(target - current, -limit, limit));
      return true;
    }
    case IsFinite: {
      double value = 0;
      if (!n1(value)) {
        out = Value::number_of(0);
        return true;
      }
      out = Value::number_of(std::isfinite(value) ? 1.0 : 0.0);
      return true;
    }
    case Abs: {
      double value = 0;
      if (!n1(value)) {
        return bad(error);
      }
      out = Value::number_of(std::fabs(value));
      return true;
    }
    case Min:
    case Max: {
      double a = 0;
      double b = 0;
      if (!n2(a, b)) {
        return bad(error);
      }
      out = Value::number_of(which == Min ? (a < b ? a : b) : (a > b ? a : b));
      return true;
    }
    case Pow: {
      double base = 0;
      double exponent = 0;
      if (!n2(base, exponent)) {
        return bad(error);
      }
      out = Value::number_of(std::pow(base, exponent));
      return true;
    }
    case Sqrt: {
      double value = 0;
      if (!n1(value)) {
        return bad(error);
      }
      out = Value::number_of(std::sqrt(value));
      return true;
    }
    case Cbrt: {
      double value = 0;
      if (!n1(value)) {
        return bad(error);
      }
      out = Value::number_of(std::cbrt(value));
      return true;
    }
    case Hypot: {
      double a = 0;
      double b = 0;
      if (!n2(a, b)) {
        return bad(error);
      }
      out = Value::number_of(std::hypot(a, b));
      return true;
    }
    case Log: {
      double value = 0;
      if (!n1(value)) {
        return bad(error);
      }
      out = Value::number_of(std::log(value));
      return true;
    }
    case Exp: {
      double value = 0;
      if (!n1(value)) {
        return bad(error);
      }
      out = Value::number_of(std::exp(value));
      return true;
    }
    case Smoothstep:
    case Smootherstep: {
      double edge0 = 0;
      double edge1 = 0;
      double x = 0;
      if (!n3(edge0, edge1, x)) {
        return bad(error);
      }
      const double t = smooth_t(edge0, edge1, x);
      out = Value::number_of(which == Smoothstep ? t * t * (3.0 - 2.0 * t) : t * t * t * (t * (t * 6.0 - 15.0) + 10.0));
      return true;
    }
    case Gcd: {
      double a = 0;
      double b = 0;
      if (!n2(a, b)) {
        return bad(error);
      }
      out = Value::number_of(gcd_num(a, b));
      return true;
    }
    case Lcm: {
      double a = 0;
      double b = 0;
      if (!n2(a, b)) {
        return bad(error);
      }
      const double divisor = gcd_num(a, b);
      out = Value::number_of(divisor == 0.0 ? 0.0 : std::fabs(a * b) / divisor);
      return true;
    }
    case IsEven:
    case IsOdd: {
      double value = 0;
      if (!n1(value)) {
        return bad(error);
      }
      const bool even = even_num(value);
      out = Value::number_of((which == IsEven ? even : !even) ? 1.0 : 0.0);
      return true;
    }
    case Factorial: {
      double n = 0;
      if (!n1(n)) {
        return bad(error);
      }
      if (!std::isfinite(n)) {
        out = Value::number_of(n);
        return true;
      }
      const double count = std::floor(n);
      if (count < 2.0) {
        out = Value::number_of(1);
        return true;
      }
      if (count > 170.0) {
        out = Value::number_of(std::numeric_limits<double>::infinity());
        return true;
      }
      double result = 1.0;
      for (double step = 2.0; step <= count; step += 1.0) {
        result *= step;
      }
      out = Value::number_of(result);
      return true;
    }
    case Scale: {
      double value = 0;
      double index = 0;
      if (!n2(value, index)) {
        return bad(error);
      }
      out = Value::number_of(value * (mod_positive(index, 50.0) + 1.0) / 50.0);
      return true;
    }
    case Lerp:
    case LerpClamped: {
      double from = 0;
      double to = 0;
      double alpha = 0;
      if (!n3(from, to, alpha)) {
        return bad(error);
      }
      if (which == LerpClamped) {
        alpha = saturate_num(alpha);
      }
      out = Value::number_of(from + (to - from) * alpha);
      return true;
    }
    case LerpAngle: {
      double from = 0;
      double to = 0;
      double alpha = 0;
      if (!n3(from, to, alpha)) {
        return bad(error);
      }
      out = Value::number_of(from + delta_radians(from, to) * alpha);
      return true;
    }
    case LerpVector2:
    case LerpVector3:
    case LerpVector:
    case LerpColor3:
    case LerpCFrame:
    case SlerpCFrame:
    case LerpUDim2: {
      Vec a;
      Vec b;
      double alpha = 0;
      if (arity != 3 || args == nullptr || !vec_of(args[0], a) || !vec_of(args[1], b) || !num_of(args[2], alpha)) {
        return bad(error);
      }
      const bool with_w = which == LerpUDim2 || which == LerpCFrame || which == SlerpCFrame;
      Vec mixed = lerp3(a, b, alpha, with_w);
      if (which == LerpVector2) {
        mixed.z = 0;
        mixed.w = 0;
      }
      out = vec_value(mixed);
      return true;
    }
    case Project:
    case Reject:
    case Reflect: {
      Vec a;
      Vec b;
      if (!v2(a, b)) {
        return bad(error);
      }
      if (which == Reflect) {
        out = vec_value(sub3(a, scale3(b, 2.0 * dot3(a, b))));
        return true;
      }
      const double span = dot3(b, b);
      const Vec projected = span < kEps ? zero_vec() : scale3(b, dot3(a, b) / span);
      out = vec_value(which == Project ? projected : sub3(a, projected));
      return true;
    }
    case Angle: {
      Vec a;
      Vec b;
      if (!v2(a, b)) {
        return bad(error);
      }
      const double left = len3(a);
      const double right = len3(b);
      if (left < kEps || right < kEps) {
        out = Value::number_of(0);
        return true;
      }
      out = Value::number_of(std::acos(clamp_num(dot3(a, b) / (left * right), -1.0, 1.0)));
      return true;
    }
    case Distance:
    case Distance2: {
      Vec a;
      Vec b;
      if (!v2(a, b)) {
        return bad(error);
      }
      const double dx = b.x - a.x;
      const double dy = b.y - a.y;
      const double dz = which == Distance ? b.z - a.z : 0.0;
      out = Value::number_of(std::sqrt(dx * dx + dy * dy + dz * dz));
      return true;
    }
    case Orthonormal:
    case OrthonormalUp: {
      Vec forward;
      Vec up;
      if (which == Orthonormal) {
        if (!v1(forward)) {
          return bad(error);
        }
      } else if (!v2(forward, up)) {
        return bad(error);
      }
      (void)up;
      out = vec_value(unit3(forward, back_vec()));
      return true;
    }
    case Slerp: {
      Vec a;
      Vec b;
      double t = 0;
      if (arity != 3 || args == nullptr || !vec_of(args[0], a) || !vec_of(args[1], b) || !num_of(args[2], t)) {
        return bad(error);
      }
      const double mag_a = len3(a);
      const double mag_b = len3(b);
      if (mag_a < kEps || mag_b < kEps) {
        out = vec_value(lerp3(a, b, t, false));
        return true;
      }
      const Vec na = scale3(a, 1.0 / mag_a);
      const Vec nb = scale3(b, 1.0 / mag_b);
      const double cosine = clamp_num(dot3(na, nb), -1.0, 1.0);
      if (cosine > 0.9995) {
        out = vec_value(lerp3(a, b, t, false));
        return true;
      }
      const double theta = std::acos(cosine);
      const double sine = std::sin(theta);
      const Vec direction =
          add3(scale3(na, std::sin((1.0 - t) * theta) / sine), scale3(nb, std::sin(t * theta) / sine));
      out = vec_value(scale3(direction, mag_a + (mag_b - mag_a) * t));
      return true;
    }
    case LookAt: {
      Vec from;
      Vec look;
      if (!v2(from, look)) {
        return bad(error);
      }
      out = vec_value(unit3(sub3(look, from), back_vec()));
      return true;
    }
    case Flat: {
      Vec value;
      if (!v1(value)) {
        return bad(error);
      }
      value.y = 0;
      out = vec_value(unit3(value, back_vec()));
      return true;
    }
    case CubicBezier:
    case QuadraticBezier: {
      const bool cubic = which == CubicBezier;
      if (args == nullptr || arity != (cubic ? 5 : 4)) {
        return bad(error);
      }
      double t = 0;
      Vec p0;
      Vec p1;
      Vec p2;
      Vec p3;
      if (!num_of(args[0], t) || !vec_of(args[1], p0) || !vec_of(args[2], p1) || !vec_of(args[3], p2) ||
          (cubic && !vec_of(args[4], p3))) {
        return bad(error);
      }
      const double u = 1.0 - t;
      if (cubic) {
        out = vec_value(add3(add3(scale3(p0, u * u * u), scale3(p1, 3.0 * u * u * t)),
                             add3(scale3(p2, 3.0 * u * t * t), scale3(p3, t * t * t))));
      } else {
        out = vec_value(add3(add3(scale3(p0, u * u), scale3(p1, 2.0 * u * t)), scale3(p2, t * t)));
      }
      return true;
    }
    case Hover:
    case Float: {
      double elapsed = 0;
      Vec anchor;
      Vec look;
      if (arity != 3 || args == nullptr || !num_of(args[0], elapsed) || !vec_of(args[1], anchor) || !vec_of(args[2], look)) {
        return bad(error);
      }
      (void)look;
      const double bob = which == Hover ? std::sin(elapsed * 1.6) * 0.22 : std::sin(elapsed * 1.1) * 0.18;
      anchor.y += bob;
      out = vec_value(anchor);
      return true;
    }
    case FloatSpin: {
      double elapsed = 0;
      Vec anchor;
      if (arity != 2 || args == nullptr || !num_of(args[0], elapsed) || !vec_of(args[1], anchor)) {
        return bad(error);
      }
      anchor.y += std::sin(elapsed * 1.1) * 0.18;
      anchor.w = 50.0 * elapsed;
      out = vec_value(anchor);
      return true;
    }
    case AabbContains: {
      Vec point;
      Vec lo;
      Vec hi;
      if (arity != 3 || args == nullptr || !vec_of(args[0], point) || !vec_of(args[1], lo) || !vec_of(args[2], hi)) {
        return bad(error);
      }
      const bool inside = point.x >= lo.x && point.x <= hi.x && point.y >= lo.y && point.y <= hi.y && point.z >= lo.z &&
                          point.z <= hi.z;
      out = Value::number_of(inside ? 1.0 : 0.0);
      return true;
    }
    case SphereContains: {
      Vec point;
      Vec center;
      double radius = 0;
      if (arity != 3 || args == nullptr || !vec_of(args[0], point) || !vec_of(args[1], center) || !num_of(args[2], radius)) {
        return bad(error);
      }
      out = Value::number_of(radius >= 0.0 && len3(sub3(point, center)) <= radius ? 1.0 : 0.0);
      return true;
    }
    case RayPlane: {
      Vec origin;
      Vec dir;
      Vec point;
      Vec normal;
      if (arity != 4 || args == nullptr || !vec_of(args[0], origin) || !vec_of(args[1], dir) || !vec_of(args[2], point) ||
          !vec_of(args[3], normal)) {
        return bad(error);
      }
      const double denom = dot3(dir, normal);
      if (std::fabs(denom) < kEps) {
        out = vec_value(zero_vec());
        return true;
      }
      const double t = dot3(sub3(point, origin), normal) / denom;
      out = vec_value(t < 0.0 ? zero_vec() : add3(origin, scale3(dir, t)));
      return true;
    }
    case Barycentric: {
      Vec p;
      Vec a;
      Vec b;
      Vec c;
      if (arity != 4 || args == nullptr || !vec_of(args[0], p) || !vec_of(args[1], a) || !vec_of(args[2], b) ||
          !vec_of(args[3], c)) {
        return bad(error);
      }
      const Vec edge_b = sub3(b, a);
      const Vec edge_c = sub3(c, a);
      const Vec edge_p = sub3(p, a);
      const double d00 = dot3(edge_b, edge_b);
      const double d01 = dot3(edge_b, edge_c);
      const double d11 = dot3(edge_c, edge_c);
      const double d20 = dot3(edge_p, edge_b);
      const double d21 = dot3(edge_p, edge_c);
      const double denom = d00 * d11 - d01 * d01;
      if (std::fabs(denom) < kEps) {
        out = vec_value(Vec{1, 0, 0, 0});
        return true;
      }
      const double v = (d11 * d20 - d01 * d21) / denom;
      const double w = (d00 * d21 - d01 * d20) / denom;
      out = vec_value(Vec{1.0 - v - w, v, w, 0});
      return true;
    }
    case ClosestPointOnSegment: {
      Vec p;
      Vec a;
      Vec b;
      if (arity != 3 || args == nullptr || !vec_of(args[0], p) || !vec_of(args[1], a) || !vec_of(args[2], b)) {
        return bad(error);
      }
      const Vec ab = sub3(b, a);
      const double span = dot3(ab, ab);
      if (span < kEps) {
        out = vec_value(a);
        return true;
      }
      const double t = clamp_num(dot3(sub3(p, a), ab) / span, 0.0, 1.0);
      out = vec_value(add3(a, scale3(ab, t)));
      return true;
    }
    case Deg: {
      double rad = 0;
      if (!n1(rad)) {
        return bad(error);
      }
      out = Value::number_of(rad * 180.0 / kPi);
      return true;
    }
    case Rad: {
      double deg = 0;
      if (!n1(deg)) {
        return bad(error);
      }
      out = Value::number_of(deg * kPi / 180.0);
      return true;
    }
    case DeltaAngle:
    case AngleDiff: {
      double from = 0;
      double to = 0;
      if (!n2(from, to)) {
        return bad(error);
      }
      out = Value::number_of(delta_radians(from, to));
      return true;
    }
    case DeltaAngleDegrees: {
      double from = 0;
      double to = 0;
      if (!n2(from, to)) {
        return bad(error);
      }
      out = Value::number_of(mod_positive(to - from + 180.0, 360.0) - 180.0);
      return true;
    }
    case NormalizeAngle: {
      double angle = 0;
      if (!n1(angle)) {
        return bad(error);
      }
      out = Value::number_of(mod_positive(angle + kPi, kPi * 2.0) - kPi);
      return true;
    }
    case Sin:
    case Cos:
    case Tan:
    case Asin:
    case Acos: {
      double value = 0;
      if (!n1(value)) {
        return bad(error);
      }
      double result = 0;
      if (which == Sin) {
        result = std::sin(value);
      } else if (which == Cos) {
        result = std::cos(value);
      } else if (which == Tan) {
        result = std::tan(value);
      } else if (which == Asin) {
        result = std::asin(value);
      } else {
        result = std::acos(value);
      }
      out = Value::number_of(result);
      return true;
    }
    case Atan2: {
      double y = 0;
      double x = 0;
      if (!n2(y, x)) {
        return bad(error);
      }
      out = Value::number_of(std::atan2(y, x));
      return true;
    }
    case RandomRange:
    case Random: {
      double lo = 0;
      double hi = 0;
      if (!n2(lo, hi)) {
        return bad(error);
      }
      out = Value::number_of(lo + (hi - lo) * unit_random());
      return true;
    }
    case Weighted: {
      if (arity != 1 || args == nullptr) {
        return bad(error);
      }
      std::vector<double> weights;
      if (!read_list(args[0], weights)) {
        return bad(error);
      }
      if (weights.empty()) {
        out = Value::number_of(0);
        return true;
      }
      double total = 0;
      for (const double weight : weights) {
        if (weight > 0.0) {
          total += weight;
        }
      }
      if (total <= 0.0) {
        out = Value::number_of(1);
        return true;
      }
      double cursor = unit_random() * total;
      for (std::size_t index = 0; index < weights.size(); ++index) {
        if (weights[index] <= 0.0) {
          continue;
        }
        cursor -= weights[index];
        if (cursor < 0.0) {
          out = Value::number_of(static_cast<double>(index + 1));
          return true;
        }
      }
      out = Value::number_of(static_cast<double>(weights.size()));
      return true;
    }
    case Gaussian:
    case GaussianMS: {
      double mean = 0;
      double deviation = 1;
      if (which == GaussianMS && !n2(mean, deviation)) {
        return bad(error);
      }
      double u = unit_random();
      if (u <= 0.0) {
        u = 1e-12;
      }
      const double v = unit_random();
      out = Value::number_of(std::sqrt(-2.0 * std::log(u)) * std::cos(2.0 * kPi * v) * deviation + mean);
      return true;
    }
    case Average:
    case Sum: {
      if (arity != 1 || args == nullptr) {
        return bad(error);
      }
      std::vector<double> values;
      if (!read_list(args[0], values)) {
        return bad(error);
      }
      double total = 0;
      for (const double value : values) {
        total += value;
      }
      if (which == Sum || values.empty()) {
        out = Value::number_of(total);
        return true;
      }
      out = Value::number_of(total / static_cast<double>(values.size()));
      return true;
    }
    case HashU32: {
      double value = 0;
      if (!n1(value)) {
        return bad(error);
      }
      out = Value::number_of(static_cast<double>(hash_double(value)));
      return true;
    }
    case Value1:
    case Value2:
    case Value3: {
      double x = 0;
      double y = 0;
      double z = 0;
      if (which == Value1 && !n1(x)) {
        return bad(error);
      }
      if (which == Value2 && !n2(x, y)) {
        return bad(error);
      }
      if (which == Value3 && !n3(x, y, z)) {
        return bad(error);
      }
      std::uint32_t hash = hash_double(x);
      if (which != Value1) {
        const auto hy = hash_double(y);
        hash = mix32(hash ^ (hy + 0x9e3779b9u + (hash << 6) + (hash >> 2)));
      }
      if (which == Value3) {
        const auto hz = hash_double(z);
        hash = mix32(hash ^ (hz + 0x9e3779b9u + (hash << 6) + (hash >> 2)));
      }
      out = Value::number_of(hash_unit(hash));
      return true;
    }
    case FromHSV: {
      double h = 0;
      double s = 0;
      double v = 0;
      if (!n3(h, s, v)) {
        return bad(error);
      }
      double r = 0;
      double g = 0;
      double b = 0;
      hsv_to_rgb(h, s, v, r, g, b);
      out = Value::vector_of(r, g, b, 0);
      return true;
    }
    case Contrast: {
      Vec color;
      double amount = 0;
      if (arity != 2 || args == nullptr || !vec_of(args[0], color) || !num_of(args[1], amount)) {
        return bad(error);
      }
      const auto kick = [&](const double channel) { return clamp_num((channel - 0.5) * amount + 0.5, 0.0, 1.0); };
      out = Value::vector_of(kick(color.x), kick(color.y), kick(color.z), color.w);
      return true;
    }
    case Floor:
    case Ceil:
    case Trunc:
    case Log2:
    case Log10:
    case IsNaN: {
      double value = 0;
      if (!n1(value)) {
        return bad(error);
      }
      double result = 0;
      if (which == Floor) {
        result = std::floor(value);
      } else if (which == Ceil) {
        result = std::ceil(value);
      } else if (which == Trunc) {
        result = std::trunc(value);
      } else if (which == Log2) {
        result = std::log2(value);
      } else if (which == Log10) {
        result = std::log10(value);
      } else {
        result = std::isnan(value) ? 1.0 : 0.0;
      }
      out = Value::number_of(result);
      return true;
    }
    case Pi:
      if (arity != 0) {
        return bad(error);
      }
      out = Value::number_of(kPi);
      return true;
    case LerpHSV: {
      Vec a;
      Vec b;
      double t = 0;
      if (arity != 3 || args == nullptr || !vec_of(args[0], a) || !vec_of(args[1], b) || !num_of(args[2], t)) {
        return bad(error);
      }
      double ha = 0;
      double sa = 0;
      double va = 0;
      double hb = 0;
      double sb = 0;
      double vb = 0;
      rgb_to_hsv(a.x, a.y, a.z, ha, sa, va);
      rgb_to_hsv(b.x, b.y, b.z, hb, sb, vb);
      double dh = hb - ha;
      dh -= std::floor(dh + 0.5);
      double h = ha + dh * t;
      h -= std::floor(h);
      double r = 0;
      double g = 0;
      double blue = 0;
      hsv_to_rgb(h, sa + (sb - sa) * t, va + (vb - va) * t, r, g, blue);
      out = Value::vector_of(r, g, blue, 0);
      return true;
    }
    case InSine:
    case OutSine:
    case InOutSine:
    case InQuad:
    case OutQuad:
    case InOutQuad:
    case InCubic:
    case OutCubic:
    case InOutCubic:
    case InQuart:
    case OutQuart:
    case InOutQuart:
    case InQuint:
    case OutQuint:
    case InOutQuint:
    case InExpo:
    case OutExpo:
    case InOutExpo:
    case InCirc:
    case OutCirc:
    case InOutCirc:
    case InBack:
    case OutBack:
    case InOutBack:
    case InElastic:
    case OutElastic:
    case InOutElastic:
    case InBounce:
    case OutBounce:
    case InOutBounce:
    case Linear:
      break;
    case Dot:
    case Cross: {
      Vec a;
      Vec b;
      if (!v2(a, b)) {
        return bad(error);
      }
      if (which == Dot) {
        out = Value::number_of(dot3(a, b));
      } else {
        out = vec_value(Vec{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x, 0});
      }
      return true;
    }
    case Length:
    case Normalize: {
      Vec a;
      if (!v1(a)) {
        return bad(error);
      }
      if (which == Length) {
        out = Value::number_of(len3(a));
      } else {
        out = vec_value(unit3(a, zero_vec()));  // a zero vector stays zero
      }
      return true;
    }
    case IdCount:
      break;
  }
  error = "runtime error";
  return false;
}

}  // namespace clpp::stdlib

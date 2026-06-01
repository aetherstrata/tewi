export module tewi:number_types;

import std;

// ============================================================================
// NumberTypes - rust-like type aliases for numbers
// ============================================================================

export namespace tewi
{
/// Unsigned integer type capable of holding any array index.
using usize = std::size_t;

/// Signed integer type corresponding to `size_t`.
using isize = std::make_signed_t<std::size_t>;

using u8  = std::uint8_t;  ///< 8-bit unsigned integer.
using u16 = std::uint16_t; ///< 16-bit unsigned integer.
using u32 = std::uint32_t; ///< 32-bit unsigned integer.
using u64 = std::uint64_t; ///< 64-bit unsigned integer.

using i8  = std::int8_t;  ///< 8-bit signed integer.
using i16 = std::int16_t; ///< 16-bit signed integer.
using i32 = std::int32_t; ///< 32-bit signed integer.
using i64 = std::int64_t; ///< 64-bit signed integer.

using f16 = std::float16_t; ///< 16-bit floating-point type (half-precision).
using f32 = std::float32_t; ///< 32-bit floating-point type (single-precision).
using f64 = std::float64_t; ///< 64-bit floating-point type (double-precision).
} // namespace tewi

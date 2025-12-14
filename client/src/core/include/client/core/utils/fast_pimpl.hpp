#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace client::utils {

/**
 * @brief Implements pimpl without dynamic allocation.
 * @details Stores the implementation object in an in-place buffer with a configurable size and alignment.
 * @note Validation is enforced at compile time via static_assert in ValidateConstraints().
 * @tparam T Implementation type
 * @tparam Size Buffer size in bytes
 * @tparam Alignment Buffer alignment
 * @tparam RequireStrictMatch If true, requires exact size and alignment match
 */
template <class T, size_t Size, size_t Alignment, bool RequireStrictMatch = false>
class FastPimpl {
public:
  template <typename... Args>
  constexpr explicit(sizeof...(Args) == 1)
      FastPimpl(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    requires((sizeof...(Args) != 1 && std::constructible_from<T, Args...>) ||
             (!std::same_as<std::remove_cvref_t<Args>, FastPimpl> && ...))
  {
    std::construct_at(Impl(), std::forward<Args>(args)...);
  }

  constexpr FastPimpl(const FastPimpl& other) noexcept(std::is_nothrow_copy_constructible_v<T>) : FastPimpl(*other) {}
  constexpr FastPimpl(FastPimpl&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
      : FastPimpl(std::move(*other)) {}

  constexpr ~FastPimpl() noexcept(noexcept(std::destroy_at(Impl())));

  constexpr FastPimpl& operator=(const FastPimpl& rhs) noexcept(std::is_nothrow_copy_assignable_v<T>);
  constexpr FastPimpl& operator=(FastPimpl&& rhs) noexcept(std::is_nothrow_move_assignable_v<T>);

  /**
   * @brief Copy-assigns from a T instance.
   * @param value Source value
   * @return Reference to this instance
   */
  constexpr FastPimpl& operator=(const T& value) noexcept(std::is_nothrow_copy_assignable_v<T>)
    requires std::is_copy_assignable_v<T>;

  /**
   * @brief Move-assigns from a T instance.
   * @param value Source value
   * @return Reference to this instance
   */
  constexpr FastPimpl& operator=(T&& value) noexcept(std::is_nothrow_move_assignable_v<T>)
    requires std::is_move_assignable_v<T>;

  [[nodiscard]] constexpr T* operator->() noexcept { return Impl(); }
  [[nodiscard]] constexpr const T* operator->() const noexcept { return Impl(); }

  [[nodiscard]] constexpr T& operator*() noexcept { return *Impl(); }
  [[nodiscard]] constexpr const T& operator*() const noexcept { return *Impl(); }

private:
  static consteval void ValidateConstraints() noexcept;

  [[nodiscard]] constexpr T* Impl() noexcept { return std::bit_cast<T*>(storage_.data()); }
  [[nodiscard]] constexpr const T* Impl() const noexcept { return std::bit_cast<const T*>(storage_.data()); }

  alignas(Alignment) std::array<std::byte, Size> storage_{};
};

template <class T, size_t Size, size_t Alignment, bool RequireStrictMatch>
constexpr FastPimpl<T, Size, Alignment, RequireStrictMatch>::~FastPimpl() noexcept(noexcept(std::destroy_at(Impl()))) {
  ValidateConstraints();
  std::destroy_at(Impl());
}

template <class T, size_t Size, size_t Alignment, bool RequireStrictMatch>
constexpr auto FastPimpl<T, Size, Alignment, RequireStrictMatch>::operator=(const FastPimpl& rhs) noexcept(
    std::is_nothrow_copy_assignable_v<T>) -> FastPimpl& {
  if (this != &rhs) [[likely]] {
    *Impl() = *rhs;
  }
  return *this;
}

template <class T, size_t Size, size_t Alignment, bool RequireStrictMatch>
constexpr auto FastPimpl<T, Size, Alignment, RequireStrictMatch>::operator=(FastPimpl&& rhs) noexcept(
    std::is_nothrow_move_assignable_v<T>) -> FastPimpl& {
  if (this != &rhs) [[likely]] {
    *Impl() = std::move(*rhs);
  }
  return *this;
}

template <class T, size_t Size, size_t Alignment, bool RequireStrictMatch>
constexpr auto FastPimpl<T, Size, Alignment, RequireStrictMatch>::operator=(const T& value) noexcept(
    std::is_nothrow_copy_assignable_v<T>) -> FastPimpl&
  requires std::is_copy_assignable_v<T>
{
  if (Impl() != &value) [[likely]] {
    *Impl() = value;
  }
  return *this;
}

template <class T, size_t Size, size_t Alignment, bool RequireStrictMatch>
constexpr auto FastPimpl<T, Size, Alignment, RequireStrictMatch>::operator=(T&& value) noexcept(
    std::is_nothrow_move_assignable_v<T>) -> FastPimpl&
  requires std::is_move_assignable_v<T>
{
  if (Impl() != &value) [[likely]] {
    *Impl() = std::move(value);
  }
  return *this;
}

template <class T, size_t Size, size_t Alignment, bool RequireStrictMatch>
consteval void FastPimpl<T, Size, Alignment, RequireStrictMatch>::ValidateConstraints() noexcept {
  static_assert(Size >= sizeof(T), "FastPimpl: Size must be >= sizeof(T)");
  static_assert(!RequireStrictMatch || Size == sizeof(T), "FastPimpl: Strict size match required");
  static_assert(Alignment % alignof(T) == 0, "FastPimpl: Alignment must be a multiple of alignof(T)");
  static_assert(!RequireStrictMatch || Alignment == alignof(T), "FastPimpl: Strict alignment match required");
}

}  // namespace client::utils

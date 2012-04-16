#pragma once

#include <type_traits>

namespace sparkles {

// Yes, I'm doing this. It's ugly. Maybe I should do something different.
namespace mystd {

namespace priv {

template <typename T>
constexpr bool nothrow_move_p(typename ::std::remove_reference<T>::type &v2)
{
   typedef typename ::std::remove_reference<T>::type noref_T;
   return noexcept(v2 = static_cast< noref_T && >(v2) );
}

} // namespace priv

template <typename T>
struct is_nothrow_default_constructible
   : public ::std::has_nothrow_default_constructor<T>
{
};

template <typename T>
class is_nothrow_move_assignable {
 public:
   static constexpr bool value = priv::nothrow_move_p<T>(*(reinterpret_cast<T *>(0)));
};

template <>
class is_nothrow_move_assignable<void> {
 public:
   static constexpr bool value = false;
};

} // namespace std

} // namespace sparkles

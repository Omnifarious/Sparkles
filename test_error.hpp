#pragma once

#include <system_error>
#include <stdexcept>

namespace sparkles {
namespace test {

enum class test_error : int
{
   no_error = 0,
   some_error
};

class test_error_category_impl : public ::std::error_category
{
 public:
   virtual const char *name() const { return __FILE__ " test error"; }
   virtual ::std::string message(int ev) const {
      if (static_cast<test_error>(ev) == test_error::no_error) {
         return "no error";
      } else {
         return "an error of some sort";
      }
   }
   ::std::error_condition default_error_condition(int ev) const
   {
      if (static_cast<test_error>(ev) == test_error::no_error) {
         return make_error_condition(static_cast< ::std::errc>(0));
      } else {
         return make_error_condition(::std::errc::function_not_supported);
      }
   }
};

inline const ::std::error_category &test_error_category()
{
   static const test_error_category_impl instance;
   return instance;
}

inline ::std::error_code make_error_code(test_error e)
{
   return ::std::error_code(static_cast<int>(e), test_error_category());
}

inline ::std::error_code make_error_condition(test_error e)
{
   return ::std::error_code(static_cast<int>(e), test_error_category());
}

class test_exception : public ::std::runtime_error {
 public:
   test_exception(const ::std::string &reason) : runtime_error(reason) { }
};

} // namespace test
} // namespace sparkles

namespace std
{
template <>
struct is_error_code_enum< ::sparkles::test::test_error> : public true_type
{
};
} // namespace std

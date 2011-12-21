#include <sparkles/operation.hpp>
#include <sparkles/errors.hpp>
#include <exception>
#include <system_error>

namespace sparkles {

const ::std::error_code operation_with_error::no_error;

void operation_with_error::result() const
{
   if (!is_valid()) {
      throw invalid_result("attempt to fetch a non-existent result.");
   } else if (is_error()) {
      throw ::std::system_error(error_, "Result was an error code");
   } else if (is_exception()) {
      ::std::rethrow_exception(exception_);
   }
}

::std::error_code operation_with_error::error() const
{
   if (!is_valid()) {
      throw invalid_result("attempt to fetch a non-existent result.");
   } else if (!is_error()) {
      throw invalid_result("Tried to fetch error code when result isn't an error code.");
   } else {
      return error_;
   }
}

::std::exception_ptr operation_with_error::exception() const
{
   if (!is_valid()) {
      throw invalid_result("attempt to fetch a non-existent result.");
   } else if (is_error() || !is_exception()) {
      throw invalid_result("Tried to fetch an exception when result isn't an exception.");
   } else {
      return exception_;
   }
}

void operation_with_error::set_result()
{
   if (is_valid() || finished()) {
      throw invalid_result("Attempt to set a result that's already been set.");
   } else {
      is_valid_ = true;
      error_ = no_error;
      exception_ = nullptr;
      set_finished();
   }
}

void operation_with_error::set_bad_result(::std::exception_ptr exception)
{
   if (exception == nullptr) {
      throw ::std::invalid_argument("Cannot set a null exception result.");
   } else if (is_valid() || finished()) {
      throw invalid_result("Attempt to set a result that's already been set.");
   } else {
      exception_ = exception;
      is_valid_ = true;
      error_ = no_error;
      set_finished();
   }
}

void operation_with_error::set_bad_result(::std::error_code error)
{
   if (error == no_error) {
      throw ::std::invalid_argument("Cannot set a no-error error result.");
   } else if (is_valid() || finished()) {
      throw invalid_result("Attempt to set a result that's already been set.");
   } else {
      error_ = error;
      is_valid_ = true;
      exception_ = nullptr;
      set_finished();
   }
}

} // namespace sparkles

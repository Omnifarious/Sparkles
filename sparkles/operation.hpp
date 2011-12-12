#pragma once

#include <sparkles/errors.hpp>
#include <sparkles/operation_base.hpp>
#include <exception>
#include <system_error>

namespace sparkles {

//! An operation that returns a result.
//
// ResultType must be copyable and default constructable.
//
// A result that is an exception can only be fetched once. After it is fetched
// the first time, the result is rendered invalid.
//
// An invalid result can happen even after the operation is finished in two
// cases. First, if the result was an exception and it was already
// fetched. Secondly, if the operation was finished without setting a
// result. The second case is much rarer, and likely represents a programming
// error.
template <class ResultType>
class operation : public operation_base
{
 public:
   typedef typename ::std::remove_const<ResultType>::type result_t;

   //! Is there a valid result of any kind?
   bool is_valid() const { return is_valid_; }
   //! Is the result an exception?
   bool is_exception() const { return is_valid_ && (exception_ != nullptr); }
   //! Is the result an error?
   bool is_error() const { return is_valid_ && is_error_; }

   //! Fetch the result.
   //
   // This results in the exception being re-thrown if the result is an
   // exception and it hasn't already been re-thrown by a prior fetch.
   //
   // This will also result in a throw of ::std::system_error when the result is
   // an error code.
   //
   // And if there is no result (i.e. is_valid() is false) an invalid_result
   // exception will be thrown.
   result_t result();

   //! Fetch the error code.
   //
   // If is_error() is true, this will fetch the error code, otherwise
   // invalid_result will be thrown.
   ::std::error_code error() const;

   //! Fetch the ::std::exception_ptr of the exception (if any).
   //
   // This returns the ::std::exception_ptr for the exception being held if
   // there is one. If is_exception() returns false, this will throw
   // invalid_result.
   //
   // This is needed in order to implement forwarding or wrapping operations.
   ::std::exception_ptr exception() const;

 protected:
   //! Construct an operation<ResultType> with the given set of dependencies
   template <class InputIterator>
   operation(InputIterator dependencies_begin,
             const InputIterator &dependencies_end)
        : operation_base(dependencies_begin, dependencies_end),
          is_valid_(false), is_error_(false), exception_(nullptr)
   {
   }

   void set_result(result_t result);
   void set_result(::std::exception_ptr exception);
   void set_result(::std::error_code error);

 private:
   bool is_valid_, is_error_;
   result_t result_;
   ::std::exception_ptr exception_;
   ::std::error_code error_;

   using operation_base::set_finished;

   virtual void i_dependency_finished(const opbase_ptr_t &dependency) = 0;
};

template <class ResultType>
typename operation<ResultType>::result_t operation<ResultType>::result()
{
   if (!is_valid_) {
      throw invalid_result("attempt to fetch a non-existent result.");
   } else if (is_error_) {
      throw ::std::system_error(error_, "Result was an error code");
   } else if (exception_ != nullptr) {
      ::std::exception_ptr local;
      local.swap(exception_);
      // Exceptions can only be fetched once!
      is_valid_ = false;
      ::std::rethrow_exception(local);
   } else {
      return result_;
   }
}

template <class ResultType>
::std::error_code operation<ResultType>::error() const
{
   if (!is_valid_) {
      throw invalid_result("attempt to fetch a non-existent result.");
   } else if (!is_error_) {
      throw invalid_result("Tried to fetch error code when result isn't an error code.");
   } else {
      return error_;
   }
}

template <class ResultType>
::std::exception_ptr operation<ResultType>::exception() const
{
   if (!is_valid_) {
      throw invalid_result("attempt to fetch a non-existent result.");
   } else if (is_error_ || (exception_ == nullptr)) {
      throw invalid_result("Tried to fetch an exception when result isn't an exception.");
   } else {
      return exception_;
   }
}

template <class ResultType>
void operation<ResultType>::set_result(operation<ResultType>::result_t result)
{
   if (is_valid_ || finished()) {
      throw invalid_result("Attempt to set a result that's already been set.");
   } else {
      result_ = ::std::move(result);
      is_valid_ = true;
      is_error_ = false;
      delete exception_;
      exception_ = nullptr;
      set_finished();
   }
}

template <class ResultType>
void operation<ResultType>::set_result(::std::exception_ptr exception)
{
   if (exception == nullptr) {
      throw ::std::invalid_argument("Cannot set a null exception result.");
   } else if (is_valid_ || finished()) {
      throw invalid_result("Attempt to set a result that's already been set.");
   } else {
      exception_ = exception;
      is_valid_ = true;
      is_error_ = false;
      set_finished();
   }
}

template <class ResultType>
void operation<ResultType>::set_result(::std::error_code error)
{
   if (error == ::std::error_code()) {
      throw ::std::invalid_argument("Cannot set a no-error error result.");
   } else if (is_valid_ || finished()) {
      throw invalid_result("Attempt to set a result that's already been set.");
   } else {
      error_ = error;
      is_valid_ = true;
      is_error_ = true;
      delete exception_;
      exception_ = nullptr;
      set_finished();
   }
}

} // namespace teleotask

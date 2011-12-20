#pragma once

#include <sparkles/operation_base.hpp>
#include <exception>
#include <system_error>

namespace sparkles {

template <class ResultType> class operation;

//! An operation that returns nothing.
template<>
class operation<void> : public operation_base
{
 public:
   typedef ::std::shared_ptr<operation<void> > ptr_t;

   //! Is there a valid result of any kind?
   bool is_valid() const { return is_valid_; }
   //! Is the result an exception?
   bool is_exception() const { return is_valid_ && (exception_ != nullptr); }
   //! Is the result an error?
   bool is_error() const { return is_valid_ && (error_ != ::std::error_code()); }

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
   //
   // When the expected result is void, the function simply returns unless it
   // would throw an exception for one of the reasons given above.
   void result();

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
   //! Construct an operation<void> with the given set of dependencies
   template <class InputIterator>
   operation(InputIterator dependencies_begin,
             const InputIterator &dependencies_end)
        : operation_base(dependencies_begin, dependencies_end),
          is_valid_(false), exception_(nullptr)
   {
   }

   void set_result();
   void set_bad_result(::std::exception_ptr exception);
   void set_bad_result(::std::error_code error);

 private:
   bool is_valid_;
   ::std::exception_ptr exception_;
   ::std::error_code error_;

   void set_finished() { operation_base::set_finished(); }

   virtual void i_dependency_finished(const opbase_ptr_t &dependency) = 0;
};

//! An operation that returns a result.
//
// ResultType must be copyable and default constructable.
template <class ResultType>
class operation : public operation<void>
{
 public:
   typedef typename ::std::remove_const<ResultType>::type result_t;
   typedef ::std::shared_ptr<operation<ResultType> > ptr_t;

   result_t result() {
      operation<void>::result();
      return result_;
   }

 protected:
   //! Construct an operation<ResultType> with the given set of dependencies
   template <class InputIterator>
   operation(InputIterator dependencies_begin,
             const InputIterator &dependencies_end)
        : operation<void>(dependencies_begin, dependencies_end)
   {
   }

   void set_result(result_t result) {
      if (!(is_valid() || finished())) {
         result_ = ::std::move(result);
      }
      operation<void>::set_result();
   }

 private:
   result_t result_;

   virtual void i_dependency_finished(const opbase_ptr_t &dependency) = 0;
};

} // namespace teleotask

#pragma once

#include <sparkles/operation_base.hpp>
#include <exception>
#include <system_error>

namespace sparkles {

//! Implementation details for classes in the sparkles namespace.
namespace priv {

class result_base
{
 public:
   //! Is there a valid result of any kind?
   bool is_valid() const { return is_valid_; }
   //! Is the result an exception?
   bool is_exception() const { return is_valid_ && (exception_ != nullptr); }
   //! Is the result an error?
   bool is_error() const { return is_valid_ && (error_ != ::std::error_code()); }

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
   //! Construct a result_base.
   //
   // Since this is protected, only derived classes can create a result_base.
   // And this is intended to always be used with private inheritance. So no
   // virtual destructor is needed.
   result_base()
        : is_valid_(false), exception_(nullptr)
   {
   }
   result_base(const result_base &) = default;
   result_base(result_base &&) = default;
   result_base &operator =(const result_base &) = default;
   result_base &operator =(result_base &&) = default;

   //! Set this as having been completed without error.
   //
   // Throws an exception if this can't be accomplished.
   void set_result(bool finished);

   //! Set this as having been completed with an exception.
   //
   // Throws an exception if this can't be accomplished.
   void set_bad_result(::std::exception_ptr exception, bool finished);

   //! Set this as having been completed with an ::std::error_code.
   //
   // Throws an exception if this can't be accomplished.
   void set_bad_result(::std::error_code error, bool finished);

   //! Throws an appropriate result exception if warranted.
   //
   // This is a helper function so that derived classes can call this, and if no
   // exception is thrown, return the appropriate result.
   //
   // Throws invalid_result if no set*result function has been called.
   //
   // Throws the passed in exception if set_bad_result has been called with an
   // ::std::exception_ptr.
   //
   // Throws ::std::system_error if set_bad_result has been called with an
   // ::std::error_code.
   //
   // Otherwise, returns.
   void maybe_throw_result_exception();

 private:
   bool is_valid_;
   ::std::exception_ptr exception_;
   ::std::error_code error_;
};

} // namespace priv

template <class ResultType> class operation;

//! A specialization for an operation that returns nothing.
template<>
class operation<void> : public operation_base, private priv::result_base
{
 public:
   typedef ::std::shared_ptr<operation<void> > ptr_t;

   using priv::result_base::is_valid;
   using priv::result_base::is_exception;
   using priv::result_base::is_error;

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
   void result() { maybe_throw_result_exception(); }

   using priv::result_base::error;
   using priv::result_base::exception;

 protected:
   //! Construct an operation<void> with the given set of dependencies
   template <class InputIterator>
   operation(InputIterator dependencies_begin,
             const InputIterator &dependencies_end)
        : operation_base(dependencies_begin, dependencies_end),
          result_base()
   {
   }

   void set_result() {
      // Throws an exception if anything goes wrong.
      result_base::set_result(finished());
      set_finished();
   }
   void set_bad_result(::std::exception_ptr exception) {
      // Throws an exception if anything goes wrong.
      result_base::set_bad_result(::std::move(exception), finished());
      set_finished();
   }
   void set_bad_result(::std::error_code error) {
      // Throws an exception if anything goes wrong.
      result_base::set_bad_result(::std::move(error), finished());
      set_finished();
   }

 private:
   void set_finished() { operation_base::set_finished(); }

   virtual void i_dependency_finished(const opbase_ptr_t &dependency) = 0;
};

//! An operation that returns a result.
//
// ResultType must be copyable and default constructable.
template <class ResultType>
class operation : public operation_base, private priv::result_base
{
 public:
   typedef typename ::std::remove_const<ResultType>::type result_t;
   typedef ::std::shared_ptr<operation<ResultType> > ptr_t;

   using priv::result_base::is_valid;
   using priv::result_base::is_exception;
   using priv::result_base::is_error;

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
   result_t result() {
      maybe_throw_result_exception();
      return result_;
   }

   using priv::result_base::error;
   using priv::result_base::exception;

 protected:
   //! Construct an operation<ResultType> with the given set of dependencies
   template <class InputIterator>
   operation(InputIterator dependencies_begin,
             const InputIterator &dependencies_end)
        : operation_base(dependencies_begin, dependencies_end)
   {
   }

   void set_result(result_t result) {
      // Throws an exception if anything goes wrong.
      result_base::set_result(finished());
      result_ = ::std::move(result);
      set_finished();
   }
   void set_bad_result(::std::exception_ptr exception) {
      // Throws an exception if anything goes wrong.
      result_base::set_bad_result(::std::move(exception), finished());
      set_finished();
   }
   void set_bad_result(::std::error_code error) {
      // Throws an exception if anything goes wrong.
      result_base::set_bad_result(::std::move(error), finished());
      set_finished();
   }

 private:
   result_t result_;

   void set_finished() { operation_base::set_finished(); }

   virtual void i_dependency_finished(const opbase_ptr_t &dependency) = 0;
};

} // namespace sparkles

#pragma once

#include <sparkles/operation_base.hpp>
#include <exception>
#include <system_error>
#include <utility>
#include <memory>

namespace sparkles {

namespace priv {

//! An operation that can fail with an error or exception.
//
// By design this class cannot be directly instantiated. It's meant to be a
// helper abstract base class and is an implementation detail that may change in
// the future.  You should use operation<void> instead.
class operation_with_error : public operation_base
{
 public:
   //! Is there a valid result of any kind?
   bool is_valid() const { return is_valid_; }
   //! Is the result an exception?
   bool is_exception() const { return is_valid_ && (exception_ != nullptr); }
   //! Is the result an error?
   bool is_error() const {
      return is_valid_ && (error_ != no_error);
   }

   //! Fetch the result.
   //
   // This results in the exception being re-thrown if the result is an
   // exception.
   //
   // This will also result in a throw of ::std::system_error when the result
   // is an error code.
   //
   // And if there is no result (i.e. is_valid() is false) an invalid_result
   // exception will be thrown.
   void result() const;

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
   static const ::std::error_code no_error;

   //! Construct an operation_with_error with the given set of dependencies
   template <class InputIterator>
   operation_with_error(InputIterator dependencies_begin,
                        const InputIterator &dependencies_end)
        : operation_base(dependencies_begin, dependencies_end),
          is_valid_(false), exception_(nullptr)
   {
   }

   //! Set this as having been completed without error.
   //
   // Throws an exception if this can't be accomplished.
   void set_result();

   //! Static version of set_result that can be passed as a function pointer.
   static void set_result_on(operation_with_error &op) { op.set_result(); }


   //! Set this as having been completed with an exception.
   //
   // Throws an exception if this can't be accomplished.
   void set_bad_result(::std::exception_ptr exception);

   //! Static version of set_bad_result that can be passed as a function pointer.
   static void set_bad_result_on(operation_with_error &op,
                                 ::std::exception_ptr exception)
   {
      op.set_bad_result(::std::move(exception));
   }

   //! Set this as having been completed with an ::std::error_code.
   //
   // Throws an exception if this can't be accomplished.
   void set_bad_result(::std::error_code error);

   //! Static version of set_bad_result that can be passed as a function pointer.
   static void set_bad_result_on(operation_with_error &op,
                                 ::std::error_code error)
   {
      op.set_bad_result(::std::move(error));
   }

 private:
   bool is_valid_;
   ::std::exception_ptr exception_;
   ::std::error_code error_;

   void set_finished() { operation_base::set_finished(); }

   virtual void i_dependency_finished(const opbase_ptr_t &dependency) = 0;
};

} // namespace priv

template <class ResultType> class operation;

//! A specialization for an operation that returns nothing.
template<>
class operation<void> : public priv::operation_with_error
{
 public:
   typedef ::std::shared_ptr<operation<void> > ptr_t;
   typedef void result_t;

 protected:
   //! Construct an operation<void> with the given set of dependencies
   template <class InputIterator>
   operation(InputIterator dependencies_begin,
             const InputIterator &dependencies_end)
        : operation_with_error(dependencies_begin, dependencies_end)
   {
   }

 private:
   virtual void i_dependency_finished(const opbase_ptr_t &dependency) = 0;
};

//! An operation that returns a result.
//
// ResultType must be copyable and default constructable.
template <class ResultType>
class operation : public priv::operation_with_error
{
 public:
   typedef typename ::std::remove_const<ResultType>::type result_t;
   typedef ::std::shared_ptr<operation<ResultType> > ptr_t;

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
   result_t result() const {
      operation_with_error::result();
      return result_;
   }

 protected:
   //! Construct an operation<ResultType> with the given set of dependencies
   template <class InputIterator>
   operation(InputIterator dependencies_begin,
             const InputIterator &dependencies_end)
        : operation_with_error(dependencies_begin, dependencies_end)
   {
   }

   //! Construct an operation<ResultType> with the given set of dependencies
   operation(const ::std::initializer_list<opbase_ptr_t> &lst)
        : operation_with_error(lst.begin(), lst.end())
   {
   }

   void set_result(result_t result) {
      if (!is_valid()) {
         result_ = ::std::move(result);
      }
      set_result();
   }

 private:
   result_t result_;

   void set_result() { operation_with_error::set_result(); }

   virtual void i_dependency_finished(const opbase_ptr_t &dependency) = 0;
};

} // namespace sparkles

#pragma once

#include <sparkles/operation_base.hpp>
#include <sparkles/op_result.hpp>
#include <exception>
#include <system_error>
#include <utility>
#include <memory>

namespace sparkles {

/*! \brief An operation that can fail with an error or exception.
 *
 * By design this class cannot be directly instantiated. It's meant to be a
 * helper abstract base class and is an implementation detail that may change in
 * the future.  You should use operation<void> instead.
 */
template <typename ResultType>
class operation : public operation_base
{
 public:
   typedef typename operation_base::opbase_ptr_t opbase_ptr_t;
   typedef ::std::shared_ptr<operation<ResultType> > ptr_t;
   typedef ResultType result_t;

   //! Is there a valid result of any kind?
   bool is_valid() const { return result_.is_valid(); }
   //! Is the result an exception?
   bool is_exception() const { return result_.is_exception(); }
   //! Is the result an error?
   bool is_error() const { return result_.is_error(); }

   /*! \brief Fetch the result.
    *
    * This results in the exception being re-thrown if the result is an
    * exception.
    *
    * This will also result in a throw of ::std::system_error when the result
    * is an error code.
    *
    * And if there is no result (i.e. is_valid() is false) an invalid_result
    * exception will be thrown.
    *
    * \seealso op_result<ResultType>::result
    */
   ResultType result() const {
      return result_.result();
   }

   /*! \brief Fetch the error code.
    *
    * If is_error() is true, this will fetch the error code, otherwise
    * invalid_result will be thrown.
    */
   ::std::error_code error() const {
      return result_.error();
   }

   /*! \brief Fetch the ::std::exception_ptr of the exception (if any).
    *
    * This returns the ::std::exception_ptr for the exception being held if
    * there is one. If is_exception() returns false, this will throw
    * invalid_result.
    *
    * This is needed in order to implement forwarding or wrapping operations.
    */
   ::std::exception_ptr exception() const {
      return result_.exception();
   }

   /*! \brief Fetch a copy of the op_result<ResultType>. Guaranteed to hold
    *  nothing if the operation is not finished.
    *
    * Sometimes it's very convenient to manipulate a result object
    * directly. Especially if you're writing your own code that's intended to
    * derive from operation<T>.
    */
   priv::op_result<ResultType> raw_result() const {
      return result_;
   }

   /*! \brief Fetch an op_result<ResultType> holding the result value and
    *  destroy the internally held value. Guaranteed to hold nothing if the
    *  operation is not finished.
    *
    * This is intended for when you want to move a result out of an operation
    * instead of copying it. After this is done, the result will be invalid,
    * though the operation will still be finished. So use this with care.
    */
   priv::op_result<ResultType> destroy_raw_result() {
      return ::std::move(result_);
   }

 protected:
   //! Construct an operation_with_error with the given set of dependencies
   template <class InputIterator>
   operation(InputIterator dependencies_begin,
             const InputIterator &dependencies_end)
        : operation_base(dependencies_begin, dependencies_end)
   {
   }

   //! Construct an operation<void> with the given set of dependencies
   operation(const ::std::initializer_list<opbase_ptr_t> &lst)
        : operation_base(lst.begin(), lst.end())
   {
   }

   /*! \brief Set this non-void operation as having been completed without error
    * with the given result.
    *
    * If the result has been destructively read, the results of this are
    * unspecified. It may throw an exception, or it may silently fail to do
    * anything.
    *
    * Otherwise, throws an exception if this can't be accomplished.
    */
   template <typename U = ResultType>
   // The following fails template expansion if ResultType is void.
   typename ::std::enable_if<!::std::is_void<U>::value, void>::type
   set_result(U result) {
      if (result_.is_valid() || !finished()) {
         result_.set_result(::std::move(result));
         set_finished();
      }
   }

   /*! \brief Set this void operation as having been completed without error.
    *
    * If the result has been destructively read, the results of this are
    * unspecified. It may throw an exception, or it may silently fail to do
    * anything.
    *
    * Otherwise, throws an exception if this can't be accomplished.
    */
   template <typename U = ResultType>
   // The following fails template expansion if ResultType is not void.
   typename ::std::enable_if< ::std::is_void<U>::value, void>::type
   set_result() {
      if (result_.is_valid() || !finished()) {
         result_.set_result();
         set_finished();
      }
   }

   /*! \brief Set this as having been completed with an exception.
    *
    * Throws an exception if this can't be accomplished.
    */
   void set_bad_result(::std::exception_ptr exception) {
      if (result_.is_valid() || !finished()) {
         result_.set_bad_result(::std::move(exception));
         set_finished();
      }
   }

   /*! \brief Set this as having been completed with an ::std::error_code.
    *
    * If the result has been destructively read, the results of this are
    * unspecified. It may throw an exception, or it may silently fail to do
    * anything.
    *
    * Otherwise, throws an exception if this can't be accomplished.
    */
   void set_bad_result(::std::error_code error) {
      if (result_.is_valid() || !finished()) {
         result_.set_bad_result(::std::move(error));
         set_finished();
      }
   }

   /*! \brief Set a raw result value. Used with raw_result for copying results
    *  from another operation.
    *
    * If the result has been destructively read, the results of this are
    * unspecified. It may throw an exception, or it may silently fail to do
    * anything.
    *
    * Otherwise, throws an exception if this can't be accomplished.
    */
   void set_raw_result(const priv::op_result<ResultType> &result) {
      if (result_.is_valid() || !finished()) {
         result_ = result;
         set_finished();
      }
   }

   /*! \brief Set a raw result value. Used with destroy_raw_result for moving
    *  results from another operation.
    *
    * If the result has been destructively read, the results of this are
    * unspecified. It may throw an exception, or it may silently fail to do
    * anything.
    *
    * Otherwise, throws an exception if this can't be accomplished.
    */
   void set_raw_result(priv::op_result<ResultType> &&result) {
      if (result_.is_valid() || !finished()) {
         result_ = ::std::move(result);
         set_finished();
      }
   }

 private:
   priv::op_result<ResultType> result_;

   void set_finished() { operation_base::set_finished(); }

   virtual void i_dependency_finished(const opbase_ptr_t &dependency) = 0;
};

} // namespace sparkles

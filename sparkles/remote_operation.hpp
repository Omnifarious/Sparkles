#pragma once

#include <sparkles/forward_decls.hpp>
#include <sparkles/operation.hpp>
#include <sparkles/work_queue.hpp>
#include <memory>
#include <utility>
#include <system_error>
#include <exception>
#include <functional>

namespace sparkles {

//! Private classes and functions used in the implementation of sparkles.
namespace priv {

typedef ::std::weak_ptr<operation_with_error> weak_errop_ptr_t;

//! Queue up a functor that will propogate the result
//
// \return Returns false if a typed result must be propogated.
inline bool
enqueue_propogate_result(
   work_queue &wq,
   const ::std::shared_ptr<operation_with_error> &me,
   const ::std::shared_ptr<operation_with_error> &other_op,
   bool isvoid,
   void (operation_with_error::*set_result)(),
   void (operation_with_error::*set_error)(::std::error_code),
   void (operation_with_error::*set_exception)(::std::exception_ptr)
   )
{
   ::std::weak_ptr<operation_with_error> weak_me(me);
   if (other_op->is_error()) {
      auto weak_deref_error =
         [weak_me, set_error](::std::error_code error) {
            ::std::shared_ptr<operation_with_error> me(weak_me.lock());
            if (me != nullptr) {
               ((*me).*set_error)(::std::move(error));
            }
      };
      wq.enqueue(::std::bind(weak_deref_error, other_op->error()));
      return true;
   } else if (other_op->is_exception()) {
      auto weak_deref_exception =
         [weak_me, set_exception](::std::exception_ptr exception) {
            ::std::shared_ptr<operation_with_error> me(weak_me.lock());
            if (me != nullptr) {
               ((*me).*set_exception)(::std::move(exception));
            }
      };
      wq.enqueue(::std::bind(weak_deref_exception, other_op->exception()));
      return true;
   } else if (isvoid) {
      auto weak_deref_voidresult =
         [weak_me, set_result]() {
            ::std::shared_ptr<operation_with_error> me(weak_me.lock());
            if (me != nullptr) {
               ((*me).*set_result)();
            }
      };
      wq.enqueue(weak_deref_voidresult);
      return true;
   } else {
      return false;
   }
}

} // namespace priv

//************************
// The basic problem here is that this class has to be very careful about what
// it does in which thread. It can't, for example, do anything to it's
// depedency (like remove itself as a depedent) in the thread of this class'
// dependents.
//
// The main danger here as that the destructor is likely to be called in the
// dependents' thread because this result is no longer needed. The destructor
// automatically de-registers this class from its dependencies, but it's
// dependencies are in a different thread.
//************************

//! A local thread placeholder for an operation in another thread.
//
// The principle behind this is that the list of dependencies is 'owned' by the
// remote thread, and the list of dependents is 'owned' by the local thread.
// No operation that modifies the list of dependencies is ever called in the
// local thread (unless it's the destructor, and that's handled specially). And
// no operation that modifies the list of dependents is ever called in the
// remote thread.
//
// When the remote thread operation finishes, it signals this operation in the
// normal fashion.  This operation's i_dependency_finished function then puts a
// callback on the local thread's work queue that will then 'finish' this
// operation in the context of the local thread.
template <typename ResultType>
class remote_operation : public operation<ResultType>
{
   typedef operation<ResultType> baseclass_t;
   typedef typename baseclass_t::ptr_t baseptr_t;

 public:
   typedef typename baseclass_t::result_t result_t;
   typedef ::std::shared_ptr<remote_operation<ResultType> > ptr_t;
   typedef operation_base::opbase_ptr_t opbase_ptr_t;

 protected:
   //! Construct a remote_operation.
   //
   // \param[in] remote The operation from another thread who's value will be
   // captured.
   // \param[in] wq A reference to the work_queue of the thread this operation
   // will be operating in (must exist for lifetime of object).
   // \param[in] remote_wq A pointer to the work_queue for the thread remote is
   // in (used for cleanup).
   remote_operation(baseptr_t remote, work_queue &wq)
        : baseclass_t({remote}), remote_(remote), wq_(wq)
   {
      this->set_mulithreaded_dependencies(true);
   }

 private:
   baseptr_t remote_;
   work_queue &wq_;

   // Make this function private and inaccessible directly in derived classes.
   void remove_dependency(const opbase_ptr_t &dependency) {
      baseclass_t::remove_dependency(dependency);
   }
   void remote_finished();

   virtual void i_dependency_finished(
      const opbase_ptr_t &dependency
      );
};

//----------------------------template functions-------------------------------

template <class ResultType>
void remote_operation<ResultType>::remote_finished()
{
   typedef remote_operation<ResultType> me_t;
   const ptr_t me(::std::static_pointer_cast<me_t>(this->shared_from_this()));

   // This is safe to do because this function will always be called in the
   // context of the dependency's thread. And there is no way for the
   // dependency to be removed from the thread of this operation.
   remove_dependency(remote_);
   baseptr_t remote_ptr;
   remote_ptr.swap(remote_);
   if (!priv::enqueue_propogate_result(wq_, me, remote_ptr, false,
                                       &me_t::set_result,
                                       &me_t::set_bad_result,
                                       &me_t::set_bad_result))
   {
      ::std::weak_ptr<me_t> weakme(me);
      auto result_callback = [weakme](result_t result) -> void {
         const ptr_t me(weakme.lock());
         if (me != nullptr) {
            me->set_result(::std::move(result));
         }
      };
      wq_.enqueue(::std::bind(result_callback, remote_ptr->result()));
   }
}

template <>
void remote_operation<void>::remote_finished()
{
   typedef remote_operation<void> me_t;
   const ptr_t me(
      ::std::static_pointer_cast<remote_operation<void> >(
         this->shared_from_this()));

   // This is safe to do because this function will always be called in the
   // context of the dependency's thread. And there is no way for the
   // dependency to be removed from the thread of this operation.
   remove_dependency(remote_);
   baseptr_t remote_ptr;
   remote_ptr.swap(remote_);
   if (!priv::enqueue_propogate_result(wq_, me, remote_ptr, true,
                                       &me_t::set_result,
                                       &me_t::set_bad_result,
                                       &me_t::set_bad_result))
   {
      throw ::std::logic_error("This shouldn't ever have been reached.");
   }
}

template <typename ResultType>
void
remote_operation<ResultType>::i_dependency_finished(
   const opbase_ptr_t &dependency)
{
   if (dependency != remote_) {
      throw ::std::logic_error("Mysterious dependency informed me.");
   } else {
      remote_finished();
   }
}

} // namespace sparkles

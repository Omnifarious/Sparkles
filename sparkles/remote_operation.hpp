#pragma once

#include <sparkles/operation.hpp>
#include <sparkles/work_queue.hpp>
#include <sparkles/errors.hpp>
#include <exception>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <memory>

namespace sparkles {

namespace priv {

/*! \brief Private implementation detail, Do not use.
 *
 * Utility class to reduce template bloat by abstracting out non-type dependent
 * code.
 *
 * Uses the trick that you can have a pointer to a function as a template
 * parameter to allow an otherwise inaccessible (because of access specifiers)
 * function to be passed in and used.
 */
template <void (*func)(operation_with_error &)>
class deliver_nothing {
 public:
   typedef deliver_nothing<func> me_t;

   deliver_nothing(::std::weak_ptr<operation_with_error> wdest)
        : wdest_(::std::move(wdest))
   {
   }

   void operator()() {
      auto dest = wdest_.lock();
      if (dest) {
         func(*(dest.get()));
      }
   }

   static void enqueue_me(bool &used, ::sparkles::work_queue &wq,
                          ::std::weak_ptr<operation_with_error> wdest)
   {
      if (used) {
         throw invalid_result("Attempt to set a result that's "
                              "already been set.");
      } else {
         used = true;
         if (wdest.lock()) {
            wq.enqueue(me_t(wdest));
         }
      }
   }

 private:
   ::std::weak_ptr<operation_with_error> wdest_;
};

/*! \brief Private implementation detail, Do not use.
 *
 * Utility class to reduce template bloat by abstracting out non-type dependent
 * code.
 *
 * Uses the trick that you can have a pointer to a function as a template
 * parameter to allow an otherwise inaccessible (because of access specifiers)
 * function to be passed in and used.
 */
template <void (*func)(operation_with_error &, ::std::exception_ptr)>
class deliver_exception {
 public:
   typedef deliver_exception<func> me_t;

   deliver_exception(::std::weak_ptr<operation_with_error> wdest,
                     ::std::exception_ptr exception)
        : wdest_(::std::move(wdest)), exception_(::std::move(exception))
   {
   }

   void operator()() {
      auto dest = wdest_.lock();
      if (dest) {
         func(*(dest.get()), exception_);
      }
   }

   static void enqueue_me(bool &used, ::sparkles::work_queue &wq,
                          ::std::weak_ptr<operation_with_error> wdest,
                          ::std::exception_ptr exception)
   {
      if (exception == nullptr) {
         throw ::std::invalid_argument("Cannot set a null exception result.");
      } else if (used) {
         throw invalid_result("Attempt to set a result that's "
                              "already been set.");
      } else {
         used = true;
         if (wdest.lock()) {
            wq.enqueue(me_t(wdest, ::std::move(exception)));
         }
      }
   }

 private:
   ::std::weak_ptr<operation_with_error> wdest_;
   const ::std::exception_ptr exception_;
};

/*! \brief Private implementation detail, Do not use.
 *
 * Utility class to reduce template bloat by abstracting out non-type dependent
 * code.
 *
 * Uses the trick that you can have a pointer to a function as a template
 * parameter to allow an otherwise inaccessible (because of access specifiers)
 * function to be passed in and used.
 */
template <void (*func)(operation_with_error &, ::std::error_code)>
class deliver_errc {
 public:
   typedef deliver_errc<func> me_t;

   deliver_errc(::std::weak_ptr<operation_with_error> wdest,
                ::std::error_code err)
        : wdest_(::std::move(wdest)), err_(::std::move(err))
   {
   }

   void operator()() {
      auto dest = wdest_.lock();
      if (dest) {
         func(*(dest.get()), err_);
      }
   }

   static void enqueue_me(bool &used, ::sparkles::work_queue &wq,
                          ::std::weak_ptr<operation_with_error> wdest,
                          ::std::error_code err)
   {
      if (err == ::std::error_code()) {
         throw ::std::invalid_argument("Cannot set a no-error error result.");
      } else if (used) {
         throw invalid_result("Attempt to set a result that's "
                              "already been set.");
      } else {
         used = true;
         if (wdest.lock()) {
            wq.enqueue(me_t(wdest, ::std::move(err)));
         }
      }
   }

 private:
   ::std::weak_ptr<operation_with_error> wdest_;
   const ::std::error_code err_;
};

} // namespace priv

/*! \brief An operation that stands in for a result from a different thread.
 *
 * This class delivers its result when an object representing that result is
 * pulled from a work_queue and evaluated.
 *
 * That object makes it onto the queue because when a remote_operation is
 * instantiated a corresponding promise object is also instantiated.  This
 * promise object contains a reference to the work_queue it's supposed to
 * deliver its result too.  When the promise value is given to the promise
 * object, it queues a notification onto the corresponding work_queue.
 *
 * The promise object holds a weak reference to the remote_operation, so if the
 * operation is discarded it's possible that the code that's executing in the
 * other thread can be aborted early.
 */
template <typename ResultType>
class remote_operation : public operation<ResultType> {
   struct private_cookie {
   };
 public:
   //! The private_cookie ensures that you must use the create function.
   remote_operation(const private_cookie &) : operation<ResultType>({}) { }

   typedef ::std::shared_ptr<remote_operation<ResultType> > ptr_t;
   typedef typename operation<ResultType>::opbase_ptr_t opbase_ptr_t;
   typedef typename operation<ResultType>::result_t result_t;

   class promise;
   friend class promise;

   //! Create a remote_operation and its associated promise.
   static ::std::pair<ptr_t, ::std::shared_ptr<promise> >
   create(work_queue &answerq) {
      typedef remote_operation<ResultType> me_t;
      auto remop = ::std::make_shared<me_t>(private_cookie());
      auto prom = ::std::make_shared<promise>(private_cookie(), remop, answerq);
      return ::std::pair<ptr_t, ::std::shared_ptr<promise> >(remop, prom);
   }

 private:
   //! Oddly enough, this will never be called for this class.
   virtual void i_dependency_finished(const opbase_ptr_t &) {
      throw ::std::runtime_error("This object should have no dependencies.");
   }
};

/*! \brief The class that's used in the other thread to send back a result.
 *
 * When you have an answer for the other thread, use the set_result or
 * set_bad_result method to send it back.
 *
 * You may only call one of those methods once.  If you call them again you will
 * get a bad_result exception.
 *
 * \todo The destructor needs to force a broken_promise exception in the caller.
 */
template <typename ResultType>
class remote_operation<ResultType>::promise {
   friend class remote_operation<ResultType>;

 public:
   typedef ::std::weak_ptr<remote_operation<ResultType> > weak_op_ptr_t;
   typedef ::std::shared_ptr<promise> ptr_t;

   /*! \brief Construct a promise connected to the given remote_operation and
    * posting to the given work_queue.
    *
    * The private_cookie parameter is just to ensure that only the
    * remote_operation::create method creates these.
    */
   promise(const private_cookie &, const weak_op_ptr_t &dest,
           ::sparkles::work_queue &wq)
        : dest_(dest), wq_(wq), used_(false)
   {
   }

   //! Is something still expecting this promise to be fulfilled?
   bool still_needed() const { return !used_ && dest_.lock(); }

   //! Has this promise already been fulfilled?
   bool already_set() const { return used_; }

   //! Fulfill this promise with an error code.
   void set_bad_result(::std::error_code err) {
      typedef priv::deliver_errc<
         remote_operation<ResultType>::set_bad_result_on
         > delivery_func_t;
      delivery_func_t::enqueue_me(used_, wq_, dest_, ::std::move(err));
   }

   //! Fulfill this promise with an exception.
   void set_bad_result(::std::exception_ptr exception) {
      typedef priv::deliver_exception<
         remote_operation<ResultType>::set_bad_result_on
         > delivery_func_t;
      delivery_func_t::enqueue_me(used_, wq_, dest_, ::std::move(exception));
   }

   //! Fulfill this promise with an actual result.
   void set_result(ResultType result) {
      class deliver_result {
       public:
         deliver_result(weak_op_ptr_t dest, ResultType result)
              : dest_(dest), result_(::std::move(result))
         {}

         void operator ()() {
            auto dest = dest_.lock();
            if (dest) {
               dest->set_result(::std::move(result_));
            }
         }

       private:
         weak_op_ptr_t dest_;
         ResultType result_;
      };
      if (used_) {
         throw invalid_result("Attempt to set a result that's "
                              "already been set.");
      } else {
         used_ = true;
         wq_.enqueue(deliver_result(dest_, ::std::move(result)));
      }
   }

 private:
   weak_op_ptr_t dest_;
   ::sparkles::work_queue &wq_;
   bool used_;
};

/*! \brief A specialization of promise for void promises.
 *
 * See the documentation for the non-specialized promise type.
 *
 * There has GOT to be a better way to do this than all this duplicate code.
 *
 * \todo The destructor needs to force a broken_promise exception in the caller.
 */
template <>
class remote_operation<void>::promise {
   friend class remote_operation<void>;

 public:
   typedef ::std::weak_ptr<remote_operation<void> > weak_op_ptr_t;
   typedef ::std::shared_ptr<promise> ptr_t;

   promise(const private_cookie &, const weak_op_ptr_t &dest,
           ::sparkles::work_queue &wq)
        : dest_(dest), wq_(wq)
   {
   }

   bool still_needed() const { return !used_ && dest_.lock(); }

   bool already_set() const { return used_; }

   void set_bad_result(::std::error_code err) {
      typedef priv::deliver_errc<
         remote_operation<void>::set_bad_result_on
         > delivery_func_t;
      delivery_func_t::enqueue_me(used_, wq_, dest_, ::std::move(err));
   }

   void set_bad_result(::std::exception_ptr exception) {
      typedef priv::deliver_exception<
         remote_operation<void>::set_bad_result_on
         > delivery_func_t;
      delivery_func_t::enqueue_me(used_, wq_, dest_, ::std::move(exception));
   }

   void set_result() {
      typedef priv::deliver_nothing<
         remote_operation<void>::set_result_on
         > delivery_func_t;
      delivery_func_t::enqueue_me(used_, wq_, dest_);
   }

 private:
   weak_op_ptr_t dest_;
   ::sparkles::work_queue &wq_;
   bool used_;
};

} // namespace sparkles

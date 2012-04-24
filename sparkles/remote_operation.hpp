#pragma once

#include <sparkles/op_result.hpp>
#include <sparkles/operation.hpp>
#include <sparkles/work_queue.hpp>
#include <sparkles/errors.hpp>
#include <exception>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <memory>

namespace sparkles {

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

   //! A shared_ptr to me!
   typedef ::std::shared_ptr<remote_operation<ResultType> > ptr_t;
   //! A shared_ptr to the master base class, operation_base
   typedef typename operation<ResultType>::opbase_ptr_t opbase_ptr_t;
   //! The type of the result.
   typedef typename operation<ResultType>::result_t result_t;

   class promise;
   friend class promise;

   /*! Create a remote_operation and its associated promise.
    *
    * \param [in] answerq The work_queue the promise is expected to deliver it's
    * result to. It must stick around as long as the promise does.
    *
    * Actually, the promise will ignore the work_queue as soon as it's fulfilled
    * and it's delivered its result to the work_queue. And if you can prevent a
    * race condition between the remote_operation going away and the promise
    * being fulfilled, the promise will ignore the work_queue is soon as its
    * weak_ptr to the remote_operation disappears.
    */
   static ::std::pair<ptr_t, ::std::shared_ptr<promise> >
   create(work_queue &answerq) {
      typedef remote_operation<ResultType> me_t;
      auto remop = ::std::make_shared<me_t>(private_cookie{});
      auto prom = ::std::make_shared<promise>(private_cookie{}, remop, answerq);
      me_t::register_as_dependent(remop);
      return ::std::pair<ptr_t, ::std::shared_ptr<promise> >(remop, prom);
   }

 private:
   //! Oddly enough, this will never be called for this class.
   virtual void i_dependency_finished(const opbase_ptr_t &) {
      throw ::std::runtime_error("This object should have no dependencies.");
   }
};

/*! \brief This exception is thrown when a promise is destroyed without being
 * fulfilled.
 *
 * It is not thrown in the destructor of the promise because throwing things in
 * destructors is a very bad idea in general, and the cases in which you should
 * do so are even more limited than, say, the cases in which you should use a
 * bare pointer, or the cases in which you should make the data members of a
 * class with virtual functions public.
 *
 * It is, instead, posted to the work_queue of the thread that holds the
 * operation who's promise is being broken. So the promise breaker is not
 * informed via exception, the entity who's promise has been broken is.
 *
 * The constructors are private on purpose. This is to discourage people from
 * using the broken_promise exception to indicate some sort of random
 * failure. Only a promise object can easily create (and thus throw) a
 * broken_promise exception.
 */
class broken_promise : public ::std::runtime_error {
 private:
   //! Make sure only promise objects can easily throw these.
   template <typename ResultType>
   friend class remote_operation<ResultType>::promise;

   //! Construct with a C string explanation
   explicit broken_promise(const char *what_arg)
        : runtime_error(what_arg)
   {
   }
   //! Construct with a ::std::string explanation
   explicit broken_promise(const ::std::string &what_arg)
        : runtime_error(what_arg)
   {
   }
};

/*! \brief The class that's used in the other thread to send back a result.
 *
 * When you have an answer for the other thread, use the set_result or
 * set_bad_result method to send it back.
 *
 * You may only call one of those methods once.  If you call them again you will
 * get a bad_result exception.
 */
template <typename ResultType>
class remote_operation<ResultType>::promise {
   friend class remote_operation<ResultType>;
   class delivery;
   friend class delivery;

 public:
   typedef ::std::weak_ptr<remote_operation<ResultType> > weak_op_ptr_t;
   typedef ::std::shared_ptr<promise> ptr_t;

 private:
   class delivery : public op_result<ResultType> {
      typedef op_result<ResultType> parent_t;
    public:
      explicit delivery(weak_op_ptr_t dest) : dest_(dest) { }
      delivery(weak_op_ptr_t dest, const parent_t &result)
           : parent_t(result), dest_(::std::move(dest))
      {
      }
      delivery(weak_op_ptr_t dest, parent_t &&result)
           : parent_t(::std::move(result)), dest_(::std::move(dest))
      {
      }

      /*! \brief This may only be called once. Delivers the result.
       */
      void operator ()() {
         auto lockeddest = dest_.lock();
         if (lockeddest) {
            dest_.reset();
            promise::move_into(::std::move(*this), ::std::move(lockeddest));
         }
      }

    private:
      weak_op_ptr_t dest_;
   };

 public:
   /*! \brief Construct a promise connected to the given remote_operation and
    * posting to the given work_queue.
    *
    * The private_cookie parameter is just to ensure that only the
    * remote_operation::create method creates these.
    */
   promise(const private_cookie &, const weak_op_ptr_t &dest,
           ::sparkles::work_queue &wq)
        : dest_(dest), wq_(wq), fulfilled_(false)
   {
   }

   //! Destroy a promise and send a broken_promise if unfulfilled.
   virtual ~promise() noexcept(true) {
      if (still_needed()) {
         broken_promise exc{"Promised destroyed without being fulfilled."};
         try {
            delivery outbound(dest_);
            outbound.set_bad_result(::std::make_exception_ptr(::std::move(exc)));
            wq_.enqueue(::std::move(outbound));
         } catch (...) {
            // I don't think enqueue_me will ever throw an exception, but just
            // in case, it should be eaten before escaping the destructor.
         }
      }
   }

   //! Is something still expecting this promise to be fulfilled?
   bool still_needed() const { return !fulfilled_ && dest_.lock(); }

   //! Has this promise already been fulfilled?
   bool fulfilled() const { return fulfilled_; }

   //! Fulfill this promise with an error code.
   void set_bad_result(::std::error_code err) {
      if (still_needed()) {
         delivery outbound(dest_);
         outbound.set_bad_result(err);
         wq_.enqueue(::std::move(outbound));
      } else if (fulfilled()) {
         throw invalid_result("Attempt to set a result that's already been "
                              "set.");
      }
      fulfilled_ = true;
   }

   //! Fulfill this promise with an exception.
   void set_bad_result(::std::exception_ptr exception) {
      if (still_needed()) {
         delivery outbound(dest_);
         outbound.set_bad_result(exception);
         wq_.enqueue(::std::move(outbound));
      } else if (fulfilled()) {
         throw invalid_result("Attempt to set a result that's already been "
                              "set.");
      }
      fulfilled_ = true;
   }

   /*! \brief Fulfill this promise with a non-void result.
    *
    * This function does not exist for promise<void>.
    *
    * \return void
    */
   template<typename U = ResultType>
   // This causes an SFINAE failure in expansion if ResultType is void.
   typename ::std::enable_if< ::std::is_same<U, ResultType>::value
                              && !::std::is_void<U>::value, void>::type
   set_result(U res) {
      if (still_needed()) {
         delivery outbound(dest_);
         outbound.set_result(::std::move(res));
         wq_.enqueue(::std::move(outbound));
      } else if (fulfilled()) {
         throw invalid_result("Attempt to set a result that's already been "
                              "set.");
      }
      fulfilled_ = true;
   }

   /*! \brief Fulfill this promise with a void result.
    *
    * This function only exists for promise<void>.
    *
    * \return void
    */
   template<typename U = ResultType>
   // This causes an SFINAE failure in expansion if ResultType is not void.
   typename ::std::enable_if< ::std::is_same<U, ResultType>::value
                              && ::std::is_void<U>::value, void>::type
   set_result() {
      if (still_needed()) {
         delivery outbound(dest_);
         outbound.set_result();
         wq_.enqueue(::std::move(outbound));
      } else if (fulfilled()) {
         throw invalid_result("Attempt to set a result that's already been "
                              "set.");
      }
      fulfilled_ = true;
   }

   void set_raw_result(const op_result<ResultType> &result) {
      if (still_needed()) {
         wq_.enqueue(outbound(dest_, result));
      } else if (fulfilled()) {
         throw invalid_result("Attempt to set a result that's already been "
                              "set.");
      }
      fulfilled_ = true;
   }

   void set_raw_result(op_result<ResultType> &&result) {
      if (still_needed()) {
         wq_.enqueue(outbound(dest_, ::std::move(result)));
      } else if (fulfilled()) {
         throw invalid_result("Attempt to set a result that's already been "
                              "set.");
      }
      fulfilled_ = true;
   }
 private:
   weak_op_ptr_t dest_;
   ::sparkles::work_queue &wq_;
   bool fulfilled_;

   static void move_into(op_result<ResultType> &&result,
                         remote_operation<ResultType>::ptr_t lockeddest) {
      lockeddest->set_raw_result(::std::move(result));
   }
};

/*! \brief This represents an operation who's result is promised to another
 *  thread.
 *
 * Note that it is trivial to use this to create deadlock cycles in the
 * operation dependency graph:
 *
 * \code
 * work_queue wq;
 * auto rem_prom = remote_operation<int>::create(wq);
 * auto prom_op = promised_operation::create(rem_prom.second, rem_prom.first);
 * \endcode
 *
 * So don't do that. If you always hand off the promise to another thread and
 * you never share operation's between threads this should accomplish the goal.
 * You can, of course, create cycles between threads, and you need to be careful
 * to not do this either.
 */
template <typename ResultType>
class promised_operation : public operation<ResultType>
{
   //! Private cookie to make sure only the create method creates these things.
   struct priv_cookie {};
   typedef typename remote_operation<ResultType>::promise::ptr_t promise_ptr_t;
   typedef typename operation<ResultType>::ptr_t op_ptr_t;

 public:
   typedef typename operation<ResultType>::opbase_ptr_t opbase_ptr_t;
   typedef ::std::shared_ptr<promised_operation<ResultType> > ptr_t;

   /*! Construct a promised_operation.
    *
    * \param [in] promise The promise that will be fullfilled when local_op
    * finishes.
    * \param [in] local_op The operation that needs to finish before the promise
    * can be fulfilled.
    *
    * Because of the priv_cookie argument, these cannot be constructed
    * directly. They must be constructed by the create method.
    */
   promised_operation(const priv_cookie &,
                      promise_ptr_t promise, op_ptr_t local_op)
        : operation<ResultType>({local_op}),
          promise_(::std::move(promise)), local_op_(local_op)
   {
      if (local_op->finished()) {
         this->i_dependency_finished(local_op);
      }
   }

   //! Allocate a new promised_operation and return a shared_ptr to it.
   static ptr_t create(promise_ptr_t promise, op_ptr_t local_op)
   {
      typedef promised_operation<ResultType> me_t;
      auto newme = ::std::make_shared<me_t>(priv_cookie(),
                                            ::std::move(promise),
                                            ::std::move(local_op));
      me_t::register_as_dependent(newme);
      return newme;
   }

 private:
   promise_ptr_t promise_;
   op_ptr_t local_op_;

   virtual void i_dependency_finished(const opbase_ptr_t &op) {
      if (op != local_op_) {
         throw ::std::runtime_error("A dependency I don't have finished.");
      } else {
         op_ptr_t my_op;
         my_op.swap(local_op_);
         this->remove_dependency(op);
         this->set_raw_result(::std::move(my_op->raw_result()));
         const auto constthis = this;
         constthis->raw_result().copy_to(*promise_);
      }
   }
};

} // namespace sparkles

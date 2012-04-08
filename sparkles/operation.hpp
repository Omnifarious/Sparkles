#pragma once

#include <sparkles/operation_base.hpp>
#include <exception>
#include <system_error>
#include <utility>
#include <memory>
#include <cstdint>

namespace sparkles {

namespace priv {

/*! \brief Implementation detail... a class to hold the result of an operation.
 *
 * This exists to have a place to put all the non-type dependent
 * operations. Hopefully this will reduce the amount of generated code
 * significantly.
 *
 * Basically, this is a variant that can hold one of 4 different types. Either
 * it can hold 'nothing', it can hold 'success', it can hold an 'exception_ptr'
 * to a thrown exception, or it can hold an ::std::error_code.
 *
 * The template subclass op_result<T> can hold an arbitary type as the return
 * value of the operation instead of simply 'success'. That's why the 'success'
 * type is called 'value'.
 *
 * Another odd feature is that a value, once set, cannot be changed. It can move
 * from 'nothing' to holding one of the other three values, and after that,
 * attempts to mutate it through anything another than a 'move' operation result
 * in an exception being thrown.
 */
class op_result_base {
 public:
   //! The sort of value currently stored in this variant.
   enum class stored_type : ::std::uint8_t {
      nothing, value, exception, error
   };

   //! Defaults to holding nothing.
   op_result_base() : type_(stored_type::nothing) { }

   //! What type of value is stored here?
   stored_type get_type() const { return type_; }

   //! Does this contains a value, i.e. is the stored type not 'nothing'?
   bool is_valid() const     { return type_ != stored_type::nothing; }

   //! Does this hold an ::std::error_code value?
   bool is_error() const     { return type_ == stored_type::error; }
   //! Does this hold an ::std::exception_ptr value?
   bool is_exception() const { return type_ == stored_type::exception; }
   //! Does this hold a 'success' value?
   bool is_value() const     { return type_ == stored_type::value; }

   //! Set this object to contain an ::std::error_code result.
   void set_bad_result(::std::error_code err) {
      if (err == ::std::error_code{}) {
         throw ::std::invalid_argument("Cannot set a no-error error result.");
      }
      throw_on_set();
      type_ = stored_type::error;
      error_ = ::std::move(err);
   }
   //! Set this object to contain an ::std::exception_ptr result.
   void set_bad_result(::std::exception_ptr exception) {
      if (exception == ::std::exception_ptr{}) {
         throw ::std::invalid_argument("Cannot set a null exception result.");
      }
      throw_on_set();
      type_ = stored_type::exception;
      exception_ = ::std::move(exception);
   }

   /*! \brief Fetch the result.
    *
    * If the result hasn't been set (i.e. it's still 'nothing') this throws an
    * invalid_result exception.
    *
    * If the result is an ::std::exception_ptr this exception is rethrown.
    *
    * If the result is an ::std::error_code the error is thrown inside of a
    * ::std::system_error exception.
    *
    * If the result is 'success' this function merely returns.
    */
   void result() const {
      switch (type_) {
       case stored_type::nothing:
         throw invalid_result("attempt to fetch a non-existent result.");
         break;
       case stored_type::error:
         throw ::std::system_error(error_, "Result was an error code.");
         break;
       case stored_type::exception:
         ::std::rethrow_exception(exception_);
         break;
       case stored_type::value:
         break;
      }
   }
   //! Fetch the ::std::error_code or throw an error if it's not one.
   ::std::error_code error() const {
      throw_fetching_bad_result_type(stored_type::error);
      return error_;
   }
   //! Fetch the ::std::exception_ptr or throw an error if it's not one.
   ::std::exception_ptr exception() const {
      throw_fetching_bad_result_type(stored_type::exception);
      return exception_;
   }

   /*! \brief Just like result() except that it fetches the result destructively
    *  with a move operation. The type is 'nothing' afterwards.
    */
   void destroy_result() {
      const stored_type saved_type = type_;
      type_ = stored_type::nothing;
      switch (saved_type) {
       case stored_type::nothing:
         throw invalid_result("attempt to fetch a non-existent result.");
         break;
       case stored_type::error:
         throw ::std::system_error(::std::move(error_),
                                   "Result was an error code.");
         break;
       case stored_type::exception:
         ::std::rethrow_exception(::std::move(exception_));
         break;
       case stored_type::value:
         break;
      }
   }
   /*! \brief Just like error() except that it fetches the error destructively
    *  with a move operation. The type is 'nothing' afterwards.
    */
   ::std::error_code destroy_error() {
      throw_fetching_bad_result_type(stored_type::error);
      type_ = stored_type::nothing;
      return ::std::move(error_);
   }
   /*! \brief Just like exception() except that it fetches the exception
    *  destructively with a move operation. The type is 'nothing' afterwards.
    */
   ::std::exception_ptr destroy_exception() {
      throw_fetching_bad_result_type(stored_type::exception);
      type_ = stored_type::nothing;
      return ::std::move(exception_);
   }

   template <typename Copier>
   void copy_to(op_result_base &other, const Copier &value_copier) const
   {
      this->copy_to(other);
      value_copier(other);
   }
   /*! \brief Copy this result to another object that supports the 'set_result',
    * 'set_bad_result' interface that this class supports.
    */
   template <class Other, typename Copier>
   void copy_to(Other &other, const Copier &value_copier) const
   {
      if (type_ != stored_type::value) {
         this->copy_to(other);
      } else {
         value_copier(other);
      }
   }
   template <typename Mover>
   void move_to(op_result_base &other, const Mover &value_mover)
   {
      this->move_to(other);
      value_mover(other);
   }
   /*! \brief Destructively move this result to another object that supports the
    * 'set_result', 'set_bad_result' interface that this class supports.
    */
   template <class Other, typename Mover>
   void move_to(Other &other, const Mover &value_mover)
   {
      if (type_ != stored_type::value) {
         this->move_to(other);
      } else {
         value_mover(other);
      }
      type_ = stored_type::nothing;
   }

 protected:
   /*! \brief Copying is the unsurprising default. Protected to avoid slicing.
    */
   op_result_base(const op_result_base &) = default;
   /*! \brief Move constructing results in the moved from result being
    *  'nothing'.  Protected to avoid slicing.
    */
   op_result_base(op_result_base &&other)
        : type_(other.type_),
          error_(::std::move(other.error_)),
          exception_(::std::move(other.exception_))
   {
      other.type_ = stored_type::nothing;
   }
   /*! \brief Moving results in the moved from result being 'nothing'. Protected
    *  to avoid slicing.
    */
   const op_result_base &operator =(op_result_base &&other) {
      throw_on_set();
      type_ = other.type_;
      switch (type_) {
       case stored_type::error:
         error_ = ::std::move(other.error_);
         break;
       case stored_type::exception:
         exception_ = ::std::move(other.exception_);
         break;
       case stored_type::nothing:
       case stored_type::value:
         break;
      }
      other.type_ = stored_type::nothing;
      return *this;
   }
   /*! \brief Copying is basically the default except throwing if already
    *  set. Protected to avoid slicing.
    */
   const op_result_base &operator =(const op_result_base &other) {
      throw_on_set();
      type_ = other.type_;
      switch (type_) {
       case stored_type::error:
         error_ = ::std::move(other.error_);
         break;
       case stored_type::exception:
         exception_ = ::std::move(other.exception_);
         break;
       case stored_type::nothing:
       case stored_type::value:
         break;
      }
      return *this;
   }

   //! If the result stored isn't of the given type, throw an error.
   void throw_fetching_bad_result_type(const stored_type check) const
   {
      if (type_ == stored_type::nothing) {
         throw invalid_result("attempt to fetch a non-existent result.");
      } else if (type_ != check) {
         throw invalid_result(((check == stored_type::error) ?
                               "Tried to fetch error code when result "
                               "isn't an error code."
                               : "Tried to fetch an exception when result "
                               "isn't an exception."));
      }
   }
   //! If the result already holds a value other than 'nothing' throw an error.
   void throw_on_set() const {
      if (type_ != stored_type::nothing) {
         throw invalid_result("Attempt to set a result that's already been "
                              "set.");
      }
   }
   /*! \brief Set a 'success' result.
    *
    * This is a protected method because 'success' is typically associated with
    * some kind of additional value. Derived classes need to handle this value
    * but still need a way to set the 'success' result.
    */
   void set_result() {
      throw_on_set();
      type_ = stored_type::value;
   }

 private:
   stored_type type_;
   ::std::error_code error_;
   ::std::exception_ptr exception_;

   void copy_to(op_result_base &other) const
   {
      other.throw_on_set();
      other = *this;
   }

   template <class Other>
   void copy_to(Other &other) const
   {
      switch (type_) {
       case stored_type::nothing:
         throw invalid_result("Trying to copy a result that isn't there.");
         break;
       case stored_type::error:
         other.set_bad_result(error_);
         break;
       case stored_type::exception:
         other.set_bad_result(exception_);
         break;
       case stored_type::value:
         throw ::std::logic_error("Should never get here.");
         break;
      }
   }
   void move_to(op_result_base &other)
   {
      other.throw_on_set();
      other = ::std::move(*this);
   }
   template <class Other>
   void move_to(Other &other)
   {
      switch (type_) {
       case stored_type::nothing:
         throw invalid_result("Trying to move a result that isn't there.");
         break;
       case stored_type::error:
         other.set_bad_result(::std::move(error_));
         break;
       case stored_type::exception:
         other.set_bad_result(::std::move(exception_));
         break;
       case stored_type::value:
         throw ::std::logic_error("Should never get here.");
         break;
      }
   }
};

struct empty_type {};

template<typename T>
struct Store {
   typedef typename ::std::conditional< ::std::is_void<T>::value,
                                        empty_type, T>::type type;
};

template<typename T>
inline T &&restore(T &&t)
{
   return ::std::forward<T>(t);
}

inline void restore(empty_type)
{
}

template <class T, class Other>
void value_copier(const T &t, Other &other)
{
   other.set_result(t);
}
template <class Other>
void value_copier(const empty_type &t, Other &other)
{
   other.set_result();
}

template <class T, class Other>
void value_mover(T &&t, Other &other)
{
   other.set_result(::std::move(t));
}
template <class Other>
void value_mover(empty_type &&t, Other &other)
{
   other.set_result();
}

template <typename T>
class op_result : public op_result_base {
 public:
   /*! \brief Set a non-void, non-error result.
    *
    * Throws an exception if this object already contains a value.
    */
   template<typename U = T>
   // This causes an SFINAE failure in expansion if T is void.
   typename ::std::enable_if<!::std::is_void<U>::value, void>::type
   set_result(U res) {
      op_result_base::set_result();
      val_ = ::std::move(res);
   }

   /*! \brief Set a non-error, but void result.
    *
    * Throws an exception if this object already contains a value.
    */
   template<typename U = T>
   // This causes an SFINAE failure in expansion if T is not void.
   typename ::std::enable_if< ::std::is_void<U>::value, void>::type
   set_result() {
      op_result_base::set_result();
   }

   /*! \brief Fetch the result
    *
    * Throws invalid_result if result hasn't been set. If the result is an
    * exception, it rethrows that exception. If the result is an
    * ::std::error_code, it throws the error_code in a system_error exception.
    *
    * Otherwise, if the result is an ordinary old result, it just returns it
    * non-destructively (i.e. it makes a copy of the stored value).
    */
   T result() const {
      op_result_base::result();
      return restore(val_);
   }
   /*! \brief Fetch the result
    *
    * Throws invalid_result if result hasn't been set. If the result is an
    * exception, it rethrows that exception. If the result is an
    * ::std::error_code, it throws the error_code in a system_error exception.
    *
    * Otherwise, if the result is an ordinary old result, it just moves-returns
    * it destructively. After this, the object is considered to have no value
    * again.
    */
   T destroy_result() {
      op_result_base::destroy_result();
      return restore(::std::move(val_));
   }

   void copy_to(op_result<T> &other) const {
      op_result_base &baseother = other;
      op_result_base::copy_to(baseother,
                              [this, &other](op_result_base &) -> void {
                                 other.val_ = this->val_;
                              });
   }
   template <class Other>
   void copy_to(Other &other) const {
      op_result_base::copy_to(other, [this](Other &other) -> void {
            value_copier(this->val_, other);
         });
   }

   void move_to(op_result<T> &other) {
      op_result_base &baseother = other;
      op_result_base::move_to(baseother,
                              [this, &other](op_result_base &) -> void {
                                 other.val_ = ::std::move(this->val_);
                              });
   }
   template <class Other>
   void move_to(Other &other) {
      op_result_base::move_to(other, [this](Other &other) -> void {
            value_mover(::std::move(this->val_), other);
         });
   }

 private:
   typename Store<T>::type val_;
};

}

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
    * Throws an exception if this can't be accomplished.
    */
   template <typename U = ResultType>
   // The following fails template expansion if ResultType is void.
   typename ::std::enable_if<!::std::is_void<U>::value, void>::type
   set_result(U result) {
      result_.set_result(::std::move(result));
      set_finished();
   }

   /*! \brief Set this void operation as having been completed without error.
    *
    * Throws an exception if this can't be accomplished.
    */
   template <typename U = ResultType>
   // The following fails template expansion if ResultType is not void.
   typename ::std::enable_if< ::std::is_void<U>::value, void>::type
   set_result() {
      result_.set_result();
      set_finished();
   }

   /*! \brief Set this as having been completed with an exception.
    *
    * Throws an exception if this can't be accomplished.
    */
   void set_bad_result(::std::exception_ptr exception) {
      result_.set_bad_result(::std::move(exception));
      set_finished();
   }

   /*! \brief Set this as having been completed with an ::std::error_code.
    *
    * Throws an exception if this can't be accomplished.
    */
   void set_bad_result(::std::error_code error) {
      result_.set_bad_result(::std::move(error));
      set_finished();
   }

   void copy_from(const priv::op_result<ResultType> &other) {
      other.copy_to(result_);
      if (result_.is_valid()) {
         set_finished();
      }
   }
   void copy_to(priv::op_result<ResultType> &other) const {
      result_.copy_to(other);
   }
   static void copy_from_to(const operation<ResultType> &from,
                            operation<ResultType> &to)
   {
      to.copy_from(from.result_);
   }
   template <class T>
   void copy_to(T &other) const {
      result_.copy_to(other);
   }
   void move_from(priv::op_result<ResultType> &&other) {
      other.move_to(result_);
      if (result_.is_valid()) {
         set_finished();
      }
   }
   void move_to(priv::op_result<ResultType> &other) {
      result_.move_to(other);
   }
   static void move_from_to(operation<ResultType> &from,
                            operation<ResultType> &to)
   {
      to.move_from(from.result_);
   }
   template <class T>
   void move_to(T &other) const {
      result_.move_to(other);
   }

 private:
   priv::op_result<ResultType> result_;

   void set_finished() { operation_base::set_finished(); }

   virtual void i_dependency_finished(const opbase_ptr_t &dependency) = 0;
};

} // namespace sparkles

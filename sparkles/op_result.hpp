#pragma once
#include <sparkles/errors.hpp>
#include <exception>
#include <system_error>
#include <utility>
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
   //! Defaults to holding nothing.
   op_result_base() : type_(stored_type::nothing) { }
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
   ~op_result_base() = default;

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

//! The empty type standing in for 'void' when void can't be used.
struct fake_void_type {};

/*! \brief Given a typename T, return the actual stored type for that type.
 *
 * This is basically the identity for any type other than void, and
 * fake_void_type for void.
 */
template<typename T>
struct Store {
   typedef typename ::std::conditional< ::std::is_void<T>::value,
                                        fake_void_type, T>::type type;
};

//! Used in conjuction with Store<T> for returning a type that may be void.
template<typename T>
inline T &&restore(T &&t)
{
   return ::std::forward<T>(t);
}
//! Used in conjuction with Store<T> for returning a type that may be void.
inline void restore(fake_void_type)
{
}

//! Copy a non-void value by calling some other type's set_result method.
template <class T, class Other>
void value_copier(const T &t, Other &other)
{
   other.set_result(t);
}
//! Copy a void value by calling some other type's set_result method.
template <class Other>
void value_copier(const fake_void_type &t, Other &other)
{
   other.set_result();
}

//! Move a non-void value by calling some other type's set_result method.
template <class T, class Other>
void value_mover(T &&t, Other &other)
{
   other.set_result(::std::move(t));
}
//! 'Move' a void value by calling some other type's set_result method.
template <class Other>
void value_mover(fake_void_type &&t, Other &other)
{
   other.set_result();
}

} // namespace priv

/*! \brief Stores the result of an operation that may be one of three things.  A
 *  value of an arbitrary type, an exception, or an ::std::error_code.
 *
 * Value of arbitrary type includes the void type, in which case the result
 * simply means "it didn't have an exception or error.", but the operation
 * completed.
 *
 * An op_result is initially created in an 'empty' state, and any attempt to get
 * an actual value from an op_result in this state will result in an exception
 * being thrown.
 *
 * Additionally, once an op_result has acquired a value, this value may not be
 * changed. Subsequent attempts to set the value of an op_result result in an
 * exception being thrown.
 *
 * Lastly, destructive operations such as moving will cause the op_result to
 * reset back to the 'holds nothing' state.
 */
template <typename T>
class op_result : public priv::op_result_base {
 public:
   //! It's the default empty constructor, initializes to holding 'nothing'.
   op_result() = default;
   //! Construct this op_result as a copy of another.
   op_result(const op_result &other)
        : priv::op_result_base(other)
   {
      if (get_type() == stored_type::value) {
         val_ = other.val_;
      }
   }
   /*! \brief Construct this op_result as a destructive copy of another.
    *
    * This means that 'other' will contain nothing after this constructor
    * finishes.
    */
   op_result(op_result &&other)
        : priv::op_result_base(::std::move(other))
   {
      if (get_type() == stored_type::value) {
         val_ = ::std::move(other.val_);
      }
   }

   /*! \brief Set a non-void, non-error result.
    *
    * Throws an exception if this object already contains a value.
    */
   template<typename U = T>
   // This causes an SFINAE failure in expansion if T is void.
   typename ::std::enable_if<!::std::is_void<U>::value, void>::type
   set_result(U res) {
      priv::op_result_base::set_result();
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
      priv::op_result_base::set_result();
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
      priv::op_result_base::result();
      return priv::restore(val_);
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
      priv::op_result_base::destroy_result();
      return priv::restore(::std::move(val_));
   }

   /*! \brief Copy the stored value to some other type supporting the
    *  set_result, set_bad_result interface.
    *
    * This will use the priv::value_copier free function and relies on that
    * function's overload for priv::fake_void_type to handle void 'values'. This
    * means it will call 'set_result()' on the destination instead of
    * 'set_result(val)'.
    */
   template <class Other>
   void copy_to(Other &other) const {
      priv::op_result_base::copy_to(other, [this](Other &other) -> void {
            priv::value_copier(this->val_, other);
         });
   }

   /*! \brief Move the stored value (thereby destroying it) to some other type
    *  supporting the set_result, set_bad_result interface.
    *
    * This will use the priv::value_mover free function and relies on that
    * function's overload for priv::fake_void_type to handle void 'values'. This
    * means it will call 'set_result()' on the destination instead of
    * 'set_result(::std::move(val))'.
    */
   template <class Other>
   void move_to(Other &other) {
      priv::op_result_base::move_to(other, [this](Other &other) -> void {
            priv::value_mover(::std::move(this->val_), other);
         });
   }

 private:
   typename priv::Store<T>::type val_;
};

} // namespace sparkles

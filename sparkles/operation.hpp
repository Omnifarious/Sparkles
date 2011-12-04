#include <sparkles/errors.hpp>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <exception>
#include <system_error>
#include <string>
#include <utility>
#include <type_traits>
#include <algorithm>

namespace sparkles {

class operation_base : public ::std::enable_shared_from_this<operation_base>
{
 public:
   typedef ::std::shared_ptr<operation_base> opbase_ptr_t;

   operation_base(const operation_base &) = delete;
   operation_base &operator =(const operation_base &) = delete;

   operation_base(operation_base &&other) = delete;
   operation_base &operator =(operation_base &&other) = delete;

   //! Destroy and remove this from the list of dependents for its dependencies.
   virtual ~operation_base();

   //! Is this operation completed?
   bool finished() const { return finished_; }

   //! Register given object as a dependent with all its dependencies.
   //
   // This method should be harmless for anybody to call at any time.  You would
   // think the constructor should call this method, but there needs to be a
   // valid shared_ptr to this object before it can be registered as a
   // dependent.
   //
   // This means the constructing static method should call it instead.  And if
   // you don't have one of those, then you'll have to call it by hand once you
   // have a proper shared_ptr.
   static void register_as_dependent(const opbase_ptr_t &op) {
      op->for_each_dependency([op](const opbase_ptr_t &dep) {
            dep->add_dependent(op);
         });
   }

 protected:
   //! Construct from a batch of dependencies.
   //
   // These dependencies can only be specified at construction time. The list
   // can be subtracted from, but not added to during the lifetime of the
   // object. This makes it require significant effort to construct cycles of
   // dependencies.
   template <class InputIterator>
   operation_base(InputIterator begin,
                  const InputIterator &end)
        : finished_(false), dependencies_(begin, end)
   {
   }

   //! Set this operation as being finished.
   void set_finished();

   //! Execute a function repeatedly with a shared_ptr to each dependency.
   template <class UnaryFunction>
   UnaryFunction for_each_dependency(UnaryFunction f) const {
      return ::std::for_each(dependencies_.begin(),
                             dependencies_.end(),
                             f);
   }

   //! I no longer depend on this operation.
   //
   // \param[in] dependency The dependency to remove
   //
   // \throws bad_dependency if this operation does not currently depend on
   // dependency.
   //
   // Removes an operation from the list of operations this operation depends
   // on.  This is safe to do after the operation has been created (even though
   // adding isn't) because deleting edges cannot be used to create a cycle,
   // wheres adding them can.
   //
   // This will result in set_finished being called if finished_ isn't already
   // true and the last dependency has been removed.
   void remove_dependency(const opbase_ptr_t &dependency) {
      auto deppos = dependencies_.find(dependency);
      if (deppos == dependencies_.end()) {
         throw bad_dependency("Tried to remove a dependency I didn't have.");
      } else {
         remove_dependency(deppos);
      }
   }

   //! A dependency has gone from not finished to finished.
   virtual void i_dependency_finished(const opbase_ptr_t &dependency) = 0;

 private:
   typedef ::std::weak_ptr<operation_base> weak_opbase_ptr_t;

   bool finished_;
   ::std::unordered_set<opbase_ptr_t> dependencies_;
   ::std::unordered_map<operation_base *, weak_opbase_ptr_t> dependents_;

   void dependency_finished(const opbase_ptr_t &dependency);

   void remove_dependency(::std::unordered_set<opbase_ptr_t>::iterator &deppos);
   void add_dependent(const opbase_ptr_t &dependent);
   void remove_dependent(const operation_base *dependent);
};

//! An operation that returns a result.
//
// ResultType must be moveable or copyable. If it isn't moveable, it will be
// copied.
template <class ResultType>
class operation : public operation_base
{
 public:
   typedef typename ::std::remove_const<ResultType>::type result_t;

   bool is_exception() const { return exception_ != nullptr; }
   bool is_error() const { return is_error_; }

   result_t result();

 protected:
   //! Construct an operation<ResultType> with the given set of dependencies
   template <class InputIterator>
   operation(InputIterator dependencies_begin,
             const InputIterator &dependencies_end)
        : operation_base(dependencies_begin, dependencies_end),
          is_error_(false), exception_(nullptr)
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
      throw invalid_result("attempt to get an already fetched result.");
   } else if (is_error_) {
      is_valid_ = false;
      throw ::std::system_error(error_, "Result was an error code");
   } else if (exception_ != nullptr) {
      ::std::exception_ptr local;
      local.swap(exception_);
      is_valid_ = false;
      ::std::rethrow_exception(local);
   } else {
      is_valid_ = false;
      return ::std::move(result_);
   }
}

template <class ResultType>
void operation<ResultType>::set_result(operation<ResultType>::result_t result)
{
   if (is_valid_) {
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
   } else if (is_valid_) {
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
   } else if (is_valid_) {
      throw invalid_result("Attempt to set a result that's already been set.");
   } else {
      error_ = error;
      is_error_ = true;
      exception_ = nullptr;
      set_finished();
   }
}

} // namespace teleotask

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

namespace sparkles {

class operation_base : public ::std::enable_shared_from_this<operation_base>
{
 public:
   typedef ::std::shared_ptr<operation_base> opbase_ptr_t;

   operation_base(const operation_base &) = delete;
   operation_base &operator =(const operation_base &) = delete;

   operation_base(operation_base &&other) = default;
   operation_base &operator =(operation_base &&other) = default;

   virtual ~operation_base();

   template <class InputIterator>
   operation_base(InputIterator begin,
                  const InputIterator &end)
        : finished_(false), dependencies_(begin, end)
   {
      for (auto dependency : dependencies_) {
         dependency->add_dependent(shared_from_this());
      }
   }

   bool finished() const { return finished_; }

   void add_dependent(const opbase_ptr_t &dependent);
   void remove_dependent(const opbase_ptr_t &dependent);

   void dependency_finished(const opbase_ptr_t &dependency);

 protected:
   void set_finished();

   void cancel_dependency(const opbase_ptr_t &dependency);

 private:
   typedef ::std::weak_ptr<operation_base> weak_opbase_ptr_t;

   bool finished_;
   ::std::unordered_set<opbase_ptr_t> dependencies_;
   ::std::unordered_map<operation_base *, weak_opbase_ptr_t> dependents_;

   virtual
   void i_dependency_finished(const opbase_ptr_t &dependency) = 0;
};

inline operation_base::~operation_base() = default;

template <class ResultType>
class operation : public operation_base
{
 public:
   typedef typename ::std::remove_const<ResultType>::type result_t;

   operation(operation &&other) = default;
   operation &operator =(operation &&other) = default;

   template <class InputIterator>
   operation(InputIterator dependencies_begin,
             const InputIterator &dependencies_end)
        : operation_base(dependencies_begin, dependencies_end),
          is_error_(false), exception_(nullptr)
   {
   }
   operation(const ::std::initializer_list< opbase_ptr_t > &lst)
        : operation_base(lst.begin(), lst.end()),
          is_error_(false), exception_(nullptr)
   {
   }

   bool is_exception() const { return exception_ != nullptr; }
   bool is_error() const { return is_error_; }

   result_t &&result();

 protected:
   void set_result(result_t &&result);
   void set_result(const result_t &result);
   void set_result(::std::exception_ptr exception);
   void set_result(::std::error_code error);

 private:
   bool is_valid_, is_error_;
   result_t result_;
   ::std::exception_ptr exception_;
   ::std::error_code error_;
};

template <class ResultType>
typename operation<ResultType>::result_t &&operation<ResultType>::result()
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
void operation<ResultType>::set_result(operation<ResultType>::result_t &&result)
{
   if (is_valid_) {
      throw invalid_result("Attempt to set a result that's already been set.");
   } else {
      result_ = result;
      is_valid_ = true;
      is_error_ = false;
      delete exception_;
      exception_ = nullptr;
      set_finished();
   }
}

template <class ResultType>
void operation<ResultType>::set_result(
   const operation<ResultType>::result_t &result
   )
{
   if (is_valid_) {
      throw invalid_result("Attempt to set a result that's already been set.");
   } else {
      result_ = result;
      is_valid_ = true;
      is_error_ = false;
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

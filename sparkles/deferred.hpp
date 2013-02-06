#include <functional>
#include <memory>
#include <type_traits>
#include <tuple>
#include <exception>
#include <array>
#include <vector>
#include <iterator>
#include <sparkles/operation.hpp>
#include <sparkles/operation_base.hpp>
#include <cstddef>

namespace sparkles {

namespace priv {

template <typename T>
struct is_op_ptr {
 private:
   // Returns false_type, which has a ::value that is false.
   template <class AT>
   static constexpr std::false_type is_it_a_ptr(...);

   // Returns true_type (if enable_if allows it to exist).
   template <class AT>
   static constexpr typename ::std::enable_if<
      ::std::is_same<
         AT,
         typename operation<typename AT::element_type::result_type>::ptr_t>::value,
      std::true_type>::type  // note the true_type return
   is_it_a_ptr(int); // no definition needed

 public:
   // do everything unevaluated
   static constexpr bool value = decltype(is_it_a_ptr<T>(0))::value;
};

class wrapped_type_base
{
 protected:
   typedef operation_base::opbase_ptr_t opbase_ptr_t;

 public:
   virtual ~wrapped_type_base() noexcept(true) = default;
   virtual void throw_on_error(const opbase_ptr_t &) const = 0;
   virtual opbase_ptr_t wrapper() const = 0;
};

template <typename T>
class wrapped_type : public wrapped_type_base
{
 public:
   static constexpr bool passthrough = is_op_ptr<T>::value;
   typedef typename ::std::conditional< passthrough,
                                        T,
                                        typename operation<T>::ptr_t>::type type;
   typedef T orig_type;
   typedef decltype(::std::declval<type>()->result()) base_type;

   wrapped_type(const type &o) : wrapped_(o) { }
   wrapped_type(type &&o) : wrapped_(o) { }
   virtual ~wrapped_type() noexcept(true) = default;

   template <typename U = T>
   typename ::std::enable_if< ::std::is_same<U, T>::value && passthrough,
                              orig_type>::type
   result() {
      return wrapped_;
   }
   template <typename U = T>
   typename ::std::enable_if< ::std::is_same<U, T>::value && !passthrough,
                              orig_type>::type
   result() const {
      return wrapped_->result();
   }

   opbase_ptr_t wrapper() const override { return wrapped_; }

   void throw_on_error(const opbase_ptr_t &bp) const override {
      if (!passthrough && (bp == wrapped_) &&
          (wrapped_->is_exception() || wrapped_->is_error()))
      {
         wrapped_->result();
      }
   }

 private:
   type wrapped_;
};

template< ::std::size_t I = 0, typename Func, typename... Tp>
inline typename ::std::enable_if<I == sizeof...(Tp), void>::type
for_each(const ::std::tuple<Tp...> &, Func)
{
}

template< ::std::size_t I = 0, typename Func, typename... Tp>
inline typename ::std::enable_if<I < sizeof...(Tp), void>::type
for_each(const ::std::tuple<Tp...> &t, Func f)
{
   f(::std::get<I>(t));
   for_each<I + 1, Func, Tp...>(t, f);
}

template <typename ResultType>
class suspended_call_base {
 protected:
   typedef operation_base::opbase_ptr_t opbase_ptr_t;

 public:
   virtual ~suspended_call_base() noexcept(true) = default;

   typedef ::std::vector<opbase_ptr_t> deplist_t;
   typedef op_result<ResultType> op_result_t;

   virtual op_result_t operator()() = 0;
   virtual deplist_t fetch_deplist() const = 0;
   virtual void test_arg_throw(const opbase_ptr_t &arg) const = 0;
};

template <typename ResultType, typename FuncT, typename TupleT>
class suspended_call : public suspended_call_base<ResultType> {
 public:
   static constexpr unsigned int num_args = ::std::tuple_size<TupleT>::value;
   typedef typename suspended_call_base<ResultType>::op_result_t op_result_t;
   typedef typename suspended_call_base<ResultType>::deplist_t deplist_t;
   typedef typename suspended_call_base<ResultType>::opbase_ptr_t opbase_ptr_t;

   explicit suspended_call(FuncT func, TupleT args)
        : func_(::std::move(func)), args_(::std::move(args))
   {
   }
   ~suspended_call() noexcept(true) override { }

   // This can only be called once and will alter the state of the object so it
   // cannot be called again.
   op_result_t operator()() override {
      typedef call_helper< ::std::tuple_size<TupleT>::value> helper_t;
      return ::std::move(helper_t::engage(func_, args_));
   }
   deplist_t fetch_deplist() const override
   {
      deplist_t deplist;
      deplist.reserve(num_args);
      for_each(this->args_, [&deplist](const wrapped_type_base &w) -> void {
            deplist.emplace_back(w.wrapper());
         });
      return ::std::move(deplist);
   }
   void test_arg_throw(const opbase_ptr_t &arg) const override
   {
      for_each(this->args_, [&arg](const wrapped_type_base &w) -> void {
            w.throw_on_error(arg);
         });
   }

 private:
   FuncT func_;
   TupleT args_;

   template <unsigned int N, unsigned int... I>
   struct call_helper {
      static op_result_t engage(FuncT &func, TupleT &args) {
         return ::std::move(call_helper<N - 1, N - 1, I...>::engage(func, args));
      }
   };

   template <unsigned int... I>
   struct call_helper<0, I...> {
      template <typename U = ResultType>
      // This causes an SFINAE failure in expansion if T is void.
      // i.e. this is the version for non-void returns
      static
      typename ::std::enable_if< ::std::is_same<U, ResultType>::value
                                 && !::std::is_void<ResultType>::value,
                                 op_result_t>::type
      engage(FuncT &func, TupleT &args) {
         using ::std::move;
         op_result_t result;
         try {
            result.set_result(move(func(::std::get<I>(args).result()...)));
         } catch (...) {
            result.set_bad_result(::std::current_exception());
         }
         return move(result);
      }

      template <typename U = ResultType>
      // This causes an SFINAE failure in expansion if T is not void.
      // i.e. this is the version for void returns
      static
      typename ::std::enable_if< ::std::is_same<U, ResultType>::value
                                 && ::std::is_void<ResultType>::value,
                                 op_result_t>::type
      engage(FuncT &func, TupleT &args) {
         op_result_t result;
         try {
            func(::std::get<I>(args).result()...);
            result.set_result();
         } catch (...) {
            result.set_bad_result(::std::current_exception());
         }
         return ::std::move(result);
      }
   };
};

template <typename ResultType>
class op_deferred_func : public operation<ResultType>
{
   struct this_is_private {};
   typedef ::std::vector< ::std::reference_wrapper<const wrapped_type_base> > depvec_t;

 public:
   typedef typename operation<ResultType>::opbase_ptr_t opbase_ptr_t;
   typedef ::std::shared_ptr<op_deferred_func<ResultType>> ptr_t;
   typedef ::std::function<op_result<ResultType>(void)> deferred_func_t;
   typedef suspended_call_base<ResultType> suspended_call_t;

   template <class InputIterator>
   op_deferred_func(const this_is_private &,
                    ::std::unique_ptr<suspended_call_t> amber,
                    InputIterator dependencies_begin,
                    const InputIterator &dependencies_end)
        : operation<ResultType>(dependencies_begin, dependencies_end),
          amber_(::std::move(amber))
   {
   }

   static ptr_t create(::std::unique_ptr<suspended_call_t> amber)
   {
      typedef op_deferred_func<ResultType> me_t;
      typedef typename suspended_call_t::deplist_t deplist_t;
      deplist_t deplist{ ::std::move(amber->fetch_deplist()) };
      ptr_t newdeferred{
         ::std::make_shared<me_t>(this_is_private{}, ::std::move(amber),
                                  deplist.begin(), deplist.end())
            };
      me_t::register_as_dependent(newdeferred);
      return ::std::move(newdeferred);
   }

 private:
   ::std::unique_ptr<suspended_call_t> amber_;

   virtual void i_dependency_finished(const opbase_ptr_t &dep) {
      if (!this->finished()) {
         try {
            // This will throw if the newly finished dependency would throw.
            amber_->test_arg_throw(dep);
            // No throwing happend, are any dependencies left unfinished?
            if (
               this->find_dependency_if([](const opbase_ptr_t &dep) {
                     return !dep->finished();
                  }) == nullptr
               )
            {
               // If there are no unfinished depencies.
               this->set_raw_result((*amber_)());
               // The suspended call is no longer needed after it's called.
               amber_.reset();
            }
         } catch (...) {
            this->set_bad_result(::std::current_exception());
            // If an exception happened during argument evaluation, the
            // suspended call will never be called.
            amber_.reset();
         }
      }
   }
};

template <typename ResultType, typename... ArgTypes>
class deferred {
 public:
   typedef typename operation<ResultType>::ptr_t operation_t;
   typedef typename op_deferred_func<ResultType>::ptr_t deferred_t;
   typedef ::std::function<ResultType(ArgTypes...)> wrapped_func_t;

   explicit deferred(const wrapped_func_t &func)
        : func_(func)
   {
   }

   operation_t until(const typename wrapped_type<ArgTypes>::type &... args) {
      typedef ::std::tuple<wrapped_type<ArgTypes>...> argtuple_t;
      typedef suspended_call<ResultType, wrapped_func_t, argtuple_t> suspcall_t;
      argtuple_t saved_args = ::std::make_tuple(args...);
      ::std::unique_ptr<suspcall_t> amber{
         new suspcall_t(func_, ::std::move(saved_args))
            };

      return op_deferred_func<ResultType>::create(::std::move(amber));
   }

 private:
   const wrapped_func_t func_;
};

} // namespace priv

//!@{
/**
 * \brief Defer execution of the provided function until the arguments are
 * available, see group description.
 *
 * The syntax for this might be a bit overly clever. The basic idea is to do
 * something like this:
 *
 * ~~~~~~~~~~~~~~~~~~~{.cpp}
 * int run_me_later(double arg1);
 *
 * ....
 * {
 *    using ::sparkles;
 *    operation<double> later_double = compute_a_double();
 *    operation<int> later_int = defer(run_me_later).until(later_double);
 * ~~~~~~~~~~~~~~~~~~~
 */
template <typename ResultType, typename... ArgTypes>
priv::deferred<ResultType, ArgTypes...>
defer(::std::function<ResultType(ArgTypes...)> func)
{
   static_assert(sizeof...(ArgTypes) > 0, "Deferring a function with no "
                 "arguments until its arguments are ready is meaningless.");
   return priv::deferred<ResultType, ArgTypes...>(func);
}

template <typename ResultType, typename... ArgTypes>
priv::deferred<ResultType, ArgTypes...>
defer(ResultType (*func)(ArgTypes...))
{
   static_assert(sizeof...(ArgTypes) > 0, "Deferring a function with no "
                 "arguments until its arguments are ready is meaningless.");
   ::std::function<ResultType(ArgTypes...)> f = func;
   return priv::deferred<ResultType, ArgTypes...>(::std::move(f));
}
//!@}

} // namespace sparkles

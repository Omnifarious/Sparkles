#include <functional>
#include <memory>
#include <type_traits>
#include <tuple>
#include <exception>
#include <array>
#include <sparkles/operation.hpp>
#include <sparkles/operation_base.hpp>

namespace sparkles {

namespace priv {

template <unsigned int N, typename TupleT>
struct deplist_filler
{
   typedef ::std::array< ::sparkles::operation_base::opbase_ptr_t,
                         ::std::tuple_size<TupleT>::value> deplist_t;

   static void fill(deplist_t &deplist, const TupleT &args) {
      deplist[N - 1] = ::std::get<N - 1>(args).wrapper();
      deplist_filler<N-1, TupleT>::fill(deplist, args);
   }
};

template <typename TupleT>
struct deplist_filler<0, TupleT>
{
   typedef ::std::array< ::sparkles::operation_base::opbase_ptr_t,
                         ::std::tuple_size<TupleT>::value> deplist_t;

   static void fill(deplist_t &, const TupleT &) { }
};

template <typename ResultType, typename FuncT, typename TupleT>
class suspended_call {
 public:
   typedef op_result<ResultType> op_result_t;
   static constexpr unsigned int num_args = ::std::tuple_size<TupleT>::value;
   typedef ::std::array<operation_base::opbase_ptr_t, num_args> deplist_t;

   explicit suspended_call(FuncT func, TupleT args)
        : func_(::std::move(func)), args_(::std::move(args))
   {
   }

   // This can only be called once and will alter the state of the object so it
   // cannot be called again.
   op_result_t operator()() {
      typedef call_helper< ::std::tuple_size<TupleT>::value> helper_t;
      return ::std::move(helper_t::engage(func_, args_));
   }
   deplist_t fetch_deplist() {
      deplist_t deplist;
      deplist_filler<num_args, TupleT>::fill(deplist, args_);
      return ::std::move(deplist);
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

template <typename T>
class wrapped_type
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

   template <typename U = T>
   typename ::std::enable_if< ::std::is_same<U, T>::value && passthrough,
                              orig_type>::type
   result() {
      return wrapped_;
   }
   template <typename U = T>
   typename ::std::enable_if< ::std::is_same<U, T>::value && !passthrough,
                              orig_type>::type
   result() {
      return wrapped_->result();
   }

   const type &wrapper() const { return wrapped_; }

 private:
   type wrapped_;
};

template <typename ResultType>
class op_deferred_func : public operation<ResultType>
{
   struct this_is_private {};

 public:
   typedef typename operation<ResultType>::opbase_ptr_t opbase_ptr_t;
   typedef ::std::shared_ptr<op_deferred_func<ResultType>> ptr_t;
   typedef ::std::function<op_result<ResultType>(void)> deferred_func_t;

   template <class InputIterator>
   op_deferred_func(const this_is_private &, deferred_func_t func,
                    InputIterator dependencies_begin,
                    const InputIterator &dependencies_end)
        : suspended_call_(::std::move(func)),
          operation<ResultType>(dependencies_begin, dependencies_end)
   {
   }

   template <class InputIterator>
   static ptr_t
   create(deferred_func_t func,
          InputIterator dependencies_begin,
          const InputIterator &dependencies_end)
   {
      typedef op_deferred_func<ResultType> me_t;
      ptr_t newdeferred{
         ::std::make_shared<me_t>(this_is_private{}, ::std::move(func),
                                  dependencies_begin, dependencies_end)
            };
      me_t::register_as_dependent(newdeferred);
      return ::std::move(newdeferred);
   }

 private:
   ::std::function<op_result<ResultType>(void)> suspended_call_;

   virtual void i_dependency_finished(const opbase_ptr_t &dependency) {
      if (
         !this->finished() &&
         find_dependency_if([](const opbase_ptr_t &dep) {
               return !dep->finished();
            }) == nullptr
         )
      {
         set_raw_result(suspended_call_());
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
      auto amber = suspcall_t(func_, ::std::move(saved_args));
      typename suspcall_t::deplist_t deplist = amber.fetch_deplist();
      ::std::function<ResultType()> f{::std::move(amber)};
      return op_deferred_func<ResultType>::create(f,
                                                  deplist.begin(),
                                                  deplist.end());
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

#include <functional>
#include <memory>
#include <type_traits>
#include <tuple>
#include <exception>
#include <array>
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
   virtual bool this_one_has_error(const opbase_ptr_t &) const = 0;
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
   bool this_one_has_error(const opbase_ptr_t &bp) const override {
      return !passthrough && (bp == wrapped_) &&
         (wrapped_->is_exception() || wrapped_->is_error());
   }

   opbase_ptr_t wrapper() const override { return wrapped_; }

 private:
   type wrapped_;
};

typedef operation_base::opbase_ptr_t opbase_ptr_t;

template < ::std::size_t... Indices>
struct indices {};

template < ::std::size_t N, std::size_t... Is>
struct build_indices : build_indices<N-1, N-1, Is...> {};

template < ::std::size_t... Is>
struct build_indices<0, Is...> : indices<Is...> {};

template <typename Tuple>
using IndicesFor = build_indices< ::std::tuple_size<Tuple>::value>;

template <typename Tuple, ::std::size_t... Indices>
inline ::std::array<const opbase_ptr_t, ::std::tuple_size<Tuple>::value>
argtuple_to_array(const Tuple &t, indices<Indices...>)
{
   constexpr auto elements = ::std::tuple_size<Tuple>::value;
   typedef ::std::array<const opbase_ptr_t, elements> out_ary_t;
   out_ary_t foo{ ::std::get<Indices>(t).wrapper()... };
   return ::std::move(foo);
}

template <typename Tuple>
::std::array<const opbase_ptr_t, ::std::tuple_size<Tuple>::value >
argtuple_to_array(const Tuple &t)
{
   return argtuple_to_array(t, IndicesFor<Tuple> {});
}

template <typename ResultType, typename FuncT, typename TupleT>
class suspended_call {
 public:
   typedef op_result<ResultType> op_result_t;
   static constexpr unsigned int num_args = ::std::tuple_size<TupleT>::value;

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
   auto fetch_deplist() -> decltype(argtuple_to_array(::std::declval<TupleT>()))
   {
      return argtuple_to_array(this->args_);
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

 public:
   typedef typename operation<ResultType>::opbase_ptr_t opbase_ptr_t;
   typedef ::std::shared_ptr<op_deferred_func<ResultType>> ptr_t;
   typedef ::std::function<op_result<ResultType>(void)> deferred_func_t;

   template <class InputIterator>
   op_deferred_func(const this_is_private &, deferred_func_t func,
                    InputIterator dependencies_begin,
                    const InputIterator &dependencies_end)
        : operation<ResultType>(dependencies_begin, dependencies_end),
          suspended_call_(::std::move(func))
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

   virtual void i_dependency_finished(const opbase_ptr_t &) {
      if (
         !this->finished() &&
         this->find_dependency_if([](const opbase_ptr_t &dep) {
               return !dep->finished();
            }) == nullptr
         )
      {
         this->set_raw_result(suspended_call_());
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
      auto deplist = amber.fetch_deplist();
      typedef op_deferred_func<ResultType> deferred_op_t;
      typename deferred_op_t::deferred_func_t f{::std::move(amber)};
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

#include <functional>
#include <memory>
#include <type_traits>
#include <tuple>

struct base_stub_op {
   typedef ::std::shared_ptr<base_stub_op> ptr_t;

   virtual ~base_stub_op() noexcept(true) = default;
};

template <typename ResultType>
struct stub_op : public base_stub_op {
   typedef base_stub_op::ptr_t base_ptr_t;
   typedef ::std::shared_ptr<stub_op<ResultType> > ptr_t;
   typedef ResultType result_type;

   virtual ResultType result() const { return ResultType(); }

   static ptr_t create() {
      return ::std::make_shared<stub_op<ResultType>>();
   }
};

template <typename ResultType>
struct stub_const_op : public stub_op<ResultType> {
   typedef ::std::shared_ptr<stub_const_op<ResultType> > ptr_t;
   typedef ResultType result_type;

   explicit stub_const_op(ResultType &&val)
        : val_(::std::move(val))
   { }
   explicit stub_const_op(const ResultType &val)
        : val_(val)
   { }

   ResultType result() const { return val_; }

   static ptr_t create(ResultType &&val) {
      return ::std::make_shared<stub_const_op<ResultType>>(::std::move(val));
   }
   static ptr_t create(const ResultType &val) {
      return ::std::make_shared<stub_const_op<ResultType>>(val);
   }

 private:
   const ResultType val_;
};

template <typename ResultType>
struct stub_func_op : public stub_op<ResultType> {
   typedef ::std::shared_ptr<stub_func_op<ResultType> > ptr_t;
   typedef ResultType result_type;
   typedef ::std::function<ResultType()> func_t;

   explicit stub_func_op(func_t &&func)
        : func_(::std::move(func))
   { }
   explicit stub_func_op(const func_t &func)
        : func_(func)
   { }
   virtual ~stub_func_op() noexcept(true) { }

   ResultType result() const { return func_(); }

   static ptr_t create(func_t &&func) {
      return ::std::make_shared<stub_func_op<ResultType>>(::std::move(func));
   }
   static ptr_t create(const func_t &func) {
      return ::std::make_shared<stub_func_op<ResultType>>(func);
   }

 private:
   const func_t func_;
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
         typename stub_op<typename AT::element_type::result_type>::ptr_t>::value,
      std::true_type>::type  // note the true_type return
   is_it_a_ptr(int); // no definition needed

 public:
   // do everything unevaluated
   static constexpr bool value = decltype(is_it_a_ptr<T>(0))::value;
};

template <typename T>
class transform_type
{
 public:
   static constexpr bool passthrough = is_op_ptr<T>::value;
   typedef typename ::std::conditional< passthrough,
                                        T,
                                        typename stub_op<T>::ptr_t>::type type;
   typedef T orig_type;
   typedef decltype(::std::declval<type>()->result()) base_type;

   transform_type(const type &o) : wrapped_(o) { }
   transform_type(type &&o) : wrapped_(o) { }

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

 private:
   type wrapped_;
};

template <typename ResultType, typename FuncT, typename TupleT>
class suspended_call {
 public:
   explicit suspended_call(FuncT func, TupleT args)
        : func_(::std::move(func)), args_(::std::move(args))
   {
   }

   // This can only be called once and will alter the state of the object so it
   // cannot be called again.
   ResultType operator()() {
      typedef call_helper< ::std::tuple_size<TupleT>::value> helper_t;
      return ::std::move(helper_t::engage(func_, args_));
   }

 private:
   FuncT func_;
   TupleT args_;

   template <unsigned int N, unsigned int... I>
   struct call_helper {
      static ResultType engage(FuncT &func, TupleT &args) {
         return ::std::move(call_helper<N - 1, N - 1, I...>::engage(func, args));
      }
   };
   template <unsigned int... I>
   struct call_helper<0, I...> {
      static ResultType engage(FuncT &func, TupleT &args) {
         return ::std::move(func(::std::get<I>(args).result()...));
      }
   };
};

template <typename ResultType, typename... ArgTypes>
class deferred {
 public:
   typedef typename stub_op<ResultType>::ptr_t deferred_t;
   typedef ::std::function<ResultType(ArgTypes...)> wrapped_func_t;

   explicit deferred(const wrapped_func_t &func)
        : func_(func)
   {
   }

   deferred_t until(const typename transform_type<ArgTypes>::type &... args) {
      typedef ::std::tuple<transform_type<ArgTypes>...> argtuple_t;
      argtuple_t saved_args = ::std::make_tuple(args...);
      ::std::function<ResultType()> f{suspended_call<ResultType, wrapped_func_t, argtuple_t>(func_, ::std::move(saved_args))};
      return stub_func_op<ResultType>::create(f);
   }

 private:
   const wrapped_func_t func_;
};

template <typename ResultType, typename... ArgTypes>
deferred<ResultType, ArgTypes...>
defer(::std::function<ResultType(ArgTypes...)> func)
{
   return deferred<ResultType, ArgTypes...>(func);
}

template <typename ResultType, typename... ArgTypes>
deferred<ResultType, ArgTypes...>
defer(ResultType (*func)(ArgTypes...))
{
   ::std::function<ResultType(ArgTypes...)> f = func;
   return deferred<ResultType, ArgTypes...>(::std::move(f));
}

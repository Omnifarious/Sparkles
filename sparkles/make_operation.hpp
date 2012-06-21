#include <functional>
#include <memory>
#include <type_traits>

template <typename ResultType>
struct stub_op {
   typedef ::std::shared_ptr<stub_op<ResultType> > ptr_t;
   typedef ResultType result_type;

   ResultType result() const { return ResultType(); }
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
struct transform_type
   : ::std::conditional< is_op_ptr<T>::value, T, typename stub_op<T>::ptr_t>
{
};

template <bool PassThrough, typename T>
class wrapper_argument;

template <typename T>
class wrapper_argument<false, T> {
 public:
   static constexpr bool passthrough = false;
   typedef T type;
   typedef typename T::element_type::result_type orig_type;

   orig_type unwrap(T arg) { return arg->result(); }
};

template <typename T>
class wrapper_argument<true, T> {
 public:
   static constexpr bool passthrough = true;
   typedef T type;
   typedef typename T::element_type::result_type orig_type;

   T unwrap(T arg) { return arg; }
};

template <typename ResultType, typename... ArgTypes>
::std::function<typename transform_type<ResultType>::type(typename transform_type<ArgTypes...>::type)>
make_function(::std::function<ResultType(ArgTypes...)>)
{
   return nullptr;
}

template <typename ResultType, typename... ArgTypes>
::std::function<ResultType(ArgTypes...)>
from_funcptr(ResultType (*func)(ArgTypes...))
{
   return ::std::function<ResultType(ArgTypes...)>{func};
}

int foo(double fred)
{
   return int(fred) - 5;
}


int bar(stub_op<double>::ptr_t fred)
{
   return fred->result() - 5;
}

#if 0
void test()
{
   auto tfoo = make_function(from_funcptr(foo));
   auto tbar = make_function(from_funcptr(bar));
   int nothing = tfoo;
   int more_nothing = tbar;
   tfoo = tbar;
   foo(nothing);
   foo(more_nothing);
}
#endif

#include <functional>
#include <memory>
#include <type_traits>

struct base_stub_op {
   virtual ~base_stub_op() = default;
};

template <typename ResultType>
struct stub_op : public base_stub_op {
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
class transform_type
{
 public:
   static constexpr bool passthrough = is_op_ptr<T>::value;
   typedef typename ::std::conditional< passthrough,
                                        T,
                                        typename stub_op<T>::ptr_t>::type type;
   typedef T orig_type;

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

#if 0
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

template <typename Container_T>
void make_deplist(Container_T &output)
{
}

template <typename Container_T, typename FirstArg, typename... Args>
void make_deplist(
   Container_T &output,
   FirstArg &first_arg,
   Args... args
   )
{
   typedef decltype(first_arg) wrapper_t;
   if (!wrapper_t::passthrough) {
      output.insert(output.end(), first_arg);
   }
   make_deplist(output, args...);
}
#endif

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

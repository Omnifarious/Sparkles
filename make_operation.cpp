#include <sparkles/make_operation.hpp>
#include <iostream>

using ::std::cerr;

int a_function()
{
   cerr << "In a_function.\n";
   return 5;
}

int a_function2(int arg)
{
   cerr << "In a_function(" << arg << ").\n";
   return arg;
}

#if 0
int main()
{
   cerr << "Here 1\n";
   auto func1 = defer(a_function).until();
   cerr << "Here 2\n";
   auto func2 = defer(a_function2).until(func1);
   cerr << "Here 3\n";
   cerr << "func2->result() == " << func2->result() << '\n';
}
#endif

#include <stdexcept>

namespace sparkles {

//! Informed about a dependency I don't have being changed
class bad_dependency : public ::std::logic_error {
 public:
   bad_dependency(const ::std::string &arg)
        : logic_error(arg)
   {
   }
};

//! Tried to fetch a result twice (fetching destroys the result).
class invalid_result : public ::std::runtime_error {
 public:
   invalid_result(const ::std::string &arg)
        : runtime_error(arg)
   {
   }
};

} // namespace sparkles

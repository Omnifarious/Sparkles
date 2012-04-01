#pragma once

#include <stdexcept>

namespace sparkles {

//! Informed about a dependency being changed that's not a dependency of mine.
class bad_dependency : public ::std::logic_error {
 public:
   bad_dependency(const ::std::string &arg)
        : logic_error(arg)
   {
   }
};

/*! \brief Something is wrong with your attempt to fetch or set a result.
 *
 * Most likely, you tried to set a result twice on the same operation, or you
 * tried to fetch a result twice from the same operation.
 */
class invalid_result : public ::std::runtime_error {
 public:
   invalid_result(const ::std::string &arg)
        : runtime_error(arg)
   {
   }
};

} // namespace sparkles

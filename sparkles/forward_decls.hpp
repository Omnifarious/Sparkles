#pragma once

/*! \brief The namespace for the Sparkles library.
 *
 * This library is intended to implement a way to have one asynchronous result
 * depend on one or more other asynchronous results. This can be used to
 * normalize the inversion of control problems you get with asynchronous
 * event-diven code.
 */
namespace sparkles {

/*! \brief Things in this namespace are implementation details and should not be
 *  used directly.
 */
namespace priv {
} // namespace priv

class operation_base;

class semaphore;

class work_queue;

template <class ResultType>
class operation;

template <class ResultType>
class remote_operation;

class bad_dependency;

class invalid_result;

} // namespace sparkles

// Documentation for some C++11 STL stuff...

/*! \class std::exception_ptr
 * \brief An STL class that holds pointers to thrown exceptions.
 *
 * \sa http://en.cppreference.com/w/cpp/error/exception/exception_ptr
 */

/*! \class std::error_code
 * \brief A STL class for holding error codes. Part of the C++11 STL extensible
 * error code system.
 *
 * \sa http://en.cppreference.com/w/cpp/error/system_error/error_code
 */

/*! \class std::system_error
 * \extends ::std::runtime_error
 * \brief An STL exception class for throwing ::std::error_code values as
 * exceptions.
 *
 * \sa http://en.cppreference.com/w/cpp/error/system_error/system_error
 */

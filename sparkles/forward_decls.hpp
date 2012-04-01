#pragma once

/*! \brief The namespace for the Sparkles library.
 *
 * This library is intended to implement a way to have one asynchronous result
 * depend on one or more other asynchronous results. This can be used to
 * normalize the inversion of control problems you get with asynchronous
 * event-diven code.
 */
namespace sparkles {

/*! \brief Things in this namespace are implementation details and not to be
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

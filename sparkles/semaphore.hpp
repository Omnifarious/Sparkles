#pragma once

#include <semaphore.h>
#include <system_error>
#include <cerrno>

namespace sparkles {

/*! \brief A simple counting semaphore implemented with the Linux pthreads API.
 *
 * Unfortunately the base threading library doesn't have one of these.  It's
 * possible this could be implemented in terms of C++11s atomic operations, but
 * I didn't want to go through the effort.
 *
 * They can also be emulated with condition variables, but that seems very
 * inefficient.
 *
 * But, the current implementation renders this Linux specific.
*/
class semaphore
{
 public:
   //! Can't be copy constructed.
   semaphore(const semaphore &) = delete;
   //! Can't be move constructed.
   semaphore(semaphore &&) = delete;
   //! Can't be copy assigned.
   const semaphore &operator =(semaphore &) = delete;
   //! Can't be move assigned.
   const semaphore &operator =(semaphore &&) = delete;
   //! Construct from an integer initial count.
   explicit semaphore(unsigned int val = 0) {
      if (::sem_init(&sem_, 0, val) != 0) {
         throw ::std::system_error(errno, ::std::system_category(),
                                   "Unable to initialize semaphore.");
      }
   }
   //! Destroy the semaphore
   ~semaphore() { ::sem_destroy(&sem_); }

   //! Increase the count by 1 (aka release the semaphore).
   void release() {
      if (::sem_post(&sem_) != 0) {
         throw ::std::system_error(errno, ::std::system_category(),
                                   "Unable to release (or post to) semaphore.");
      }
   }

   //! Decrease the count by 1 and block if count is 0 until someome else posts.
   void acquire() {
      if (::sem_wait(&sem_) != 0) {
         throw ::std::system_error(errno, ::std::system_category(),
                                   "Infinite wait for semaphore failed.");
      }
   }
   //! Decrease the count by 1 if it's > 0, and return true else return false
   bool try_acquire() {
      if (::sem_trywait(&sem_) == 0) {
         return true;
      } else {
         if (errno == EAGAIN) {
            return false;
         } else {
            throw ::std::system_error(errno, ::std::system_category(),
                                      "Try wait for semaphore failed.");
         }
      }
   }

   /*! \brief What's the current count which may already be out-of-date.
    *
    * This value can change at any moment. It's useful for debugging and
    * informational purposes, but relying on this function for anything serious
    * will create race conditions.
    */
   int getvalue() {
      int retval;
      if (sem_getvalue(&sem_, &retval) != 0) {
         throw ::std::system_error(errno, ::std::system_category(),
                                   "Unable to get semaphore value.");
      }
      // Normalize allowed Posix behaviors to just returning 0 if the semaphore
      // is contended and never return a negative value indicating how many
      // waiters there are.
      return (retval <= 0) ? 0 : retval;
   }

 private:
   ::sem_t sem_;
};

}

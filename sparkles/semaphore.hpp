#pragma once

#include <semaphore.h>
#include <system_error>
#include <cerrno>

namespace sparkles {

class semaphore
{
 public:
   semaphore(const semaphore &) = delete;
   semaphore(semaphore &&) = delete;
   const semaphore &operator =(semaphore &) = delete;
   const semaphore &operator =(semaphore &&) = delete;
   explicit semaphore(unsigned int val = 0) {
      if (::sem_init(&sem_, 0, val) != 0) {
         throw ::std::system_error(errno, ::std::system_category(),
                                   "Unable to initialize semaphore.");
      }
   }
   ~semaphore() { ::sem_destroy(&sem_); }

   void release() {
      if (::sem_post(&sem_) != 0) {
         throw ::std::system_error(errno, ::std::system_category(),
                                   "Unable to release (or post to) semaphore.");
      }
   }

   void acquire() {
      if (::sem_wait(&sem_) != 0) {
         throw ::std::system_error(errno, ::std::system_category(),
                                   "Infinite wait for semaphore failed.");
      }
   }
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

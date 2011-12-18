#include <sparkles/work_queue.hpp>
#include <sparkles/semaphore.hpp>

#include <mutex>
#include <cassert>
#include <utility>
#include <memory>

using ::std::mutex;
typedef ::std::lock_guard<mutex> lock_guard;

namespace {


} // Anonymous namespace

namespace sparkles {

struct work_queue::node_t {
   node_t *next_;
   work_item_t item_;

   node_t() : next_(nullptr) {}
};

struct work_queue::impl_t {
   semaphore numitems_;
   ::std::mutex queue_mutex_;
   ::std::mutex oob_queue_mutex_;
   ::std::mutex deleted_queue_mutex_;
   node_t *queue_head_;
   node_t *queue_tail_;
   node_t *oob_queue_head_;
   node_t *oob_queue_tail_;
   node_t *deleted_head_;

   impl_t() : queue_head_(nullptr), queue_tail_(nullptr),
              oob_queue_head_(nullptr), oob_queue_tail_(nullptr),
              deleted_head_(nullptr)
   {
   }
};

class no_type {
 private:
   no_type();
};

template <unsigned int foo> struct fake {
   no_type joe;
};

inline work_queue::impl_t &work_queue::impl_()
{
   return *(reinterpret_cast<impl_t *>(&storage_));
}

inline const work_queue::impl_t &work_queue::impl_() const
{
   return *(reinterpret_cast<const impl_t *>(&storage_));
}

inline work_queue::node_t *work_queue::make_new_node(impl_t &impl)
{
   {
      node_t *newnode = nullptr;
      {
         lock_guard lock(impl.deleted_queue_mutex_);
         newnode = impl.deleted_head_;
         if (newnode != nullptr) {
            impl.deleted_head_ = newnode->next_;
         }
      }
      if (newnode != nullptr) {
         newnode->next_ = nullptr;
         return newnode;
      }
   }
   return new node_t;
}

inline void work_queue::free_queue(node_t *head)
{
   while (head != nullptr) {
      node_t *tmp = head;
      head = head->next_;
      delete tmp;
   }
}

inline work_queue::node_t *
work_queue::remove_from_queue(node_t *&head, node_t *&tail)
{
   if (head == nullptr) {
      return nullptr;
   } else {
      node_t *tmp = head;
      head = head->next_;
      if (head == nullptr) {
         tail = nullptr;
      }
      return tmp;
   }
}

work_queue::work_queue()
{
//   fake<sizeof(impl)> me;
   static_assert(alignof(impl_data) >= alignof(impl_t),
                 "Alignment too loose for impl_storage.");
   static_assert(sizeof(impl_data) >= sizeof(impl_t),
                 "impl_storage too small.");
   void *storage = &storage_;
   impl_t * const myimpl = new(storage) impl_t;
   if (myimpl != &impl_()) {
      throw ::std::logic_error("Addresses that must match do not!");
   }
}

work_queue::~work_queue()
{
   impl_t &impl = impl_();
   {
      lock_guard dlock(impl.deleted_queue_mutex_);
      lock_guard qlock(impl.queue_mutex_);
      lock_guard olock(impl.oob_queue_mutex_);
      free_queue(impl.deleted_head_);
      impl.deleted_head_ = nullptr;
      free_queue(impl.queue_head_);
      impl.queue_head_ = impl.queue_tail_ = nullptr;
      free_queue(impl.oob_queue_head_);
      impl.oob_queue_head_ = impl.oob_queue_tail_ = nullptr;
   }
   (&impl)->~impl_t();
}

void work_queue::enqueue(work_item_t item, bool out_of_band)
{
   impl_t &impl = impl_();
   ::std::unique_ptr<node_t> newnode(make_new_node(impl));
   {
      lock_guard lock(out_of_band ? impl.oob_queue_mutex_ : impl.queue_mutex_);
      node_t *&head = out_of_band ? impl.oob_queue_head_ : impl.queue_head_;
      node_t *&tail = out_of_band ? impl.oob_queue_tail_ : impl.queue_tail_;
      if (tail != nullptr) {
         tail->next_ = newnode.get();
         tail = newnode.release();
      } else {
         head = tail = newnode.release();
      }
      tail->item_ = ::std::move(item);
   }
   impl.numitems_.release();
}

work_queue::work_item_t work_queue::real_dequeue(impl_t &impl)
{
   ::std::unique_ptr<node_t> removednode;
   {
      lock_guard lock(impl.oob_queue_mutex_);
      removednode.reset(remove_from_queue(impl.oob_queue_head_,
                                          impl.oob_queue_tail_));
   }
   if (removednode == nullptr) {
      lock_guard lock(impl.queue_mutex_);
      removednode.reset(remove_from_queue(impl.queue_head_, impl.queue_tail_));
   }
   if (removednode != nullptr) {
      work_item_t dequeued_item{::std::move(removednode->item_)};
      {
         lock_guard lock(impl.deleted_queue_mutex_);
         removednode->next_ = impl.deleted_head_;
         impl.deleted_head_ = removednode.release();
      }
      return dequeued_item;
   } else {
      throw ::std::logic_error("A queue that claims to have items is empty.");
   }
}

work_queue::work_item_t work_queue::dequeue()
{
   impl_t &impl = impl_();
   impl.numitems_.acquire();
   return real_dequeue(impl);
}

bool work_queue::try_dequeue(work_item_t &dest)
{
   impl_t &impl = impl_();
   if (impl.numitems_.try_acquire()) {
      real_dequeue(impl).swap(dest);
      return true;
   } else {
      return false;
   }
}

} // namespace sparkles
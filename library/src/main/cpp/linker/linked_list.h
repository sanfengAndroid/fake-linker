//
// Created by beich on 2019/4/18.
//

#pragma once

#include <linker_macros.h>
#include <macros.h>

// https://cs.android.com/android/platform/superproject/+/master:bionic/linker/linked_list.h

template <typename T>
struct LinkedListEntry {
  LinkedListEntry<T> *next;
  T *element;
};

// ForwardInputIterator
template <typename T>
class LinkedListIterator {
public:
  LinkedListIterator() : entry_(nullptr) {}

  LinkedListIterator(const LinkedListIterator<T> &that) : entry_(that.entry_) {}

  explicit LinkedListIterator(LinkedListEntry<T> *entry) : entry_(entry) {}

  LinkedListIterator<T> &operator=(const LinkedListIterator<T> &that) {
    entry_ = that.entry_;
    return *this;
  }

  LinkedListIterator<T> &operator++() {
    entry_ = entry_->next;
    return *this;
  }

  T *const operator*() { return entry_->element; }

  bool operator==(const LinkedListIterator<T> &that) const { return entry_ == that.entry_; }

  bool operator!=(const LinkedListIterator<T> &that) const { return entry_ != that.entry_; }

private:
  LinkedListEntry<T> *entry_;
};

/*
 * Represents linked list of objects of type T
 */
template <typename T, typename Allocator>
class LinkedList {
public:
  typedef LinkedListIterator<T> iterator;
  typedef T *value_type;

  LinkedList() : head_(nullptr), tail_(nullptr) {}

  ~LinkedList() { clear(); }

  LinkedList(LinkedList &&that) noexcept {
    this->head_ = that.head_;
    this->tail_ = that.tail_;
    that.head_ = that.tail_ = nullptr;
  }

  void push_front(T *const element) {
    LinkedListEntry<T> *new_entry = Allocator::alloc();
    new_entry->next = head_;
    new_entry->element = element;
    head_ = new_entry;
    if (tail_ == nullptr) {
      tail_ = new_entry;
    }
  }

  void push_back(T *const element) {
    if (find(element) == end()) {
      LinkedListEntry<T> *new_entry = Allocator::alloc();
      new_entry->next = nullptr;
      new_entry->element = element;
      if (tail_ == nullptr) {
        tail_ = head_ = new_entry;
      } else {
        tail_->next = new_entry;
        tail_ = new_entry;
      }
    }
  }

  T *pop_front() {
    if (head_ == nullptr) {
      return nullptr;
    }
    LinkedListEntry<T> *entry = head_;
    T *element = entry->element;
    head_ = entry->next;
    Allocator::free(entry);

    if (head_ == nullptr) {
      tail_ = nullptr;
    }
    return element;
  }

  T *front() const {
    if (head_ == nullptr) {
      return nullptr;
    }
    return head_->element;
  }

  T *back() const {
    T *element = nullptr;
    for (LinkedListEntry<T> *e = head_; e != nullptr; e = e->next) {
      element = e->element;
    }
    return element;
  }

  void clear() {
    while (head_ != nullptr) {
      LinkedListEntry<T> *p = head_;
      head_ = head_->next;
      Allocator::free(p);
    }
    tail_ = nullptr;
  }

  bool empty() const { return (head_ == nullptr); }

  template <typename F>
  void for_each(F action) const {
    visit([&](T *si) {
      action(si);
      return true;
    });
  }

  template <typename F>
  bool visit(F action) const {
    for (LinkedListEntry<T> *e = head_; e != nullptr; e = e->next) {
      if (!action(e->element)) {
        return false;
      }
    }
    return true;
  }

  template <typename F>
  void remove_if(F predicate) {
    for (LinkedListEntry<T> *e = head_, *p = nullptr; e != nullptr;) {
      if (predicate(e->element)) {
        LinkedListEntry<T> *next = e->next;
        if (p == nullptr) {
          head_ = next;
        } else {
          p->next = next;
        }
        if (tail_ == e) {
          tail_ = p;
        }
        Allocator::free(e);
        e = next;
      } else {
        p = e;
        e = e->next;
      }
    }
  }

  void remove(T *element) {
    remove_if([&](T *e) {
      return e == element;
    });
  }

  template <typename F>
  T *find_if(F predicate) const {
    for (LinkedListEntry<T> *e = head_; e != nullptr; e = e->next) {
      if (predicate(e->element)) {
        return e->element;
      }
    }
    return nullptr;
  }

  iterator begin() const { return iterator(head_); }

  iterator end() const { return iterator(nullptr); }

  iterator find(T *value) const {
    for (LinkedListEntry<T> *e = head_; e != nullptr; e = e->next) {
      if (e->element == value) {
        return iterator(e);
      }
    }

    return end();
  }

  size_t copy_to_array(T *array[], size_t array_length) const {
    size_t sz = 0;
    for (LinkedListEntry<T> *e = head_; sz < array_length && e != nullptr; e = e->next) {
      array[sz++] = e->element;
    }
    return sz;
  }

  bool contains(const T *el) const {
    for (LinkedListEntry<T> *e = head_; e != nullptr; e = e->next) {
      if (e->element == el) {
        return true;
      }
    }
    return false;
  }

  static LinkedList make_list(T *const element) {
    LinkedList<T, Allocator> one_element_list;
    one_element_list.push_back(element);
    return one_element_list;
  }

  size_t size() const {
    size_t result = 0;
    for_each([&](T *) {
      ++result;
    });
    return result;
  }

protected:
  LinkedListEntry<T> *head_;
  LinkedListEntry<T> *tail_;
  DISALLOW_COPY_AND_ASSIGN(LinkedList);
};

template <typename T, typename Allocator>
ANDROID_GE_T class LinkedListT {
public:
  typedef LinkedListIterator<T> iterator;
  typedef T *value_type;

  // Allocating the head/tail fields separately from the LinkedList struct saves
  // memory in the Zygote (e.g. because adding an soinfo to a namespace doesn't
  // dirty the page containing the soinfo).
  struct LinkedListHeader {
    LinkedListEntry<T> *head;
    LinkedListEntry<T> *tail;
  };

  // The allocator returns a LinkedListEntry<T>* but we want to treat it as a
  // LinkedListHeader struct instead.
  static_assert(sizeof(LinkedListHeader) == sizeof(LinkedListEntry<T>), "");
  static_assert(alignof(LinkedListHeader) == alignof(LinkedListEntry<T>), "");

  constexpr LinkedListT() : header_(nullptr) {}

  ~LinkedListT() {
    clear();
    if (header_ != nullptr) {
      Allocator::free(reinterpret_cast<LinkedListEntry<T> *>(header_));
    }
  }

  LinkedListT(LinkedListT &&that) noexcept {
    this->header_ = that.header_;
    that.header_ = nullptr;
  }

  bool empty() const { return header_ == nullptr || header_->head == nullptr; }

  void push_front(T *const element) {
    alloc_header();
    LinkedListEntry<T> *new_entry = Allocator::alloc();
    new_entry->next = header_->head;
    new_entry->element = element;
    header_->head = new_entry;
    if (header_->tail == nullptr) {
      header_->tail = new_entry;
    }
  }

  void push_back(T *const element) {
    alloc_header();
    LinkedListEntry<T> *new_entry = Allocator::alloc();
    new_entry->next = nullptr;
    new_entry->element = element;
    if (header_->tail == nullptr) {
      header_->tail = header_->head = new_entry;
    } else {
      header_->tail->next = new_entry;
      header_->tail = new_entry;
    }
  }

  T *pop_front() {
    if (empty())
      return nullptr;

    LinkedListEntry<T> *entry = header_->head;
    T *element = entry->element;
    header_->head = entry->next;
    Allocator::free(entry);

    if (header_->head == nullptr) {
      header_->tail = nullptr;
    }

    return element;
  }

  T *front() const { return empty() ? nullptr : header_->head->element; }

  T *back() const {
    if (empty()) {
      return nullptr;
    }
    T *element = nullptr;

    for (LinkedListEntry<T> *e = head(); e != nullptr; e = e->next) {
      element = e->element;
    }
    return element;
  }

  void clear() {
    if (empty())
      return;

    while (header_->head != nullptr) {
      LinkedListEntry<T> *p = header_->head;
      header_->head = header_->head->next;
      Allocator::free(p);
    }

    header_->tail = nullptr;
  }

  template <typename F>
  void for_each(F action) const {
    visit([&](T *si) {
      action(si);
      return true;
    });
  }

  template <typename F>
  bool visit(F action) const {
    for (LinkedListEntry<T> *e = head(); e != nullptr; e = e->next) {
      if (!action(e->element)) {
        return false;
      }
    }
    return true;
  }

  template <typename F>
  void remove_if(F predicate) {
    if (empty())
      return;
    for (LinkedListEntry<T> *e = header_->head, *p = nullptr; e != nullptr;) {
      if (predicate(e->element)) {
        LinkedListEntry<T> *next = e->next;
        if (p == nullptr) {
          header_->head = next;
        } else {
          p->next = next;
        }

        if (header_->tail == e) {
          header_->tail = p;
        }

        Allocator::free(e);
        e = next;
      } else {
        p = e;
        e = e->next;
      }
    }
  }

  void remove(T *element) {
    remove_if([&](T *e) {
      return e == element;
    });
  }

  template <typename F>
  T *find_if(F predicate) const {
    for (LinkedListEntry<T> *e = head(); e != nullptr; e = e->next) {
      if (predicate(e->element)) {
        return e->element;
      }
    }
    return nullptr;
  }

  iterator begin() const { return iterator(head()); }

  iterator end() const { return iterator(nullptr); }

  iterator find(T *value) const {
    for (LinkedListEntry<T> *e = head(); e != nullptr; e = e->next) {
      if (e->element == value) {
        return iterator(e);
      }
    }

    return end();
  }

  size_t copy_to_array(T *array[], size_t array_length) const {
    size_t sz = 0;
    for (LinkedListEntry<T> *e = head(); sz < array_length && e != nullptr; e = e->next) {
      array[sz++] = e->element;
    }

    return sz;
  }

  bool contains(const T *el) const {
    for (LinkedListEntry<T> *e = head(); e != nullptr; e = e->next) {
      if (e->element == el) {
        return true;
      }
    }
    return false;
  }

  static LinkedListT make_list(T *const element) {
    LinkedList<T, Allocator> one_element_list;
    one_element_list.push_back(element);
    return one_element_list;
  }

  size_t size() const {
    size_t result = 0;
    for_each([&](T *) {
      ++result;
    });
    return result;
  }

private:
  void alloc_header() {
    if (header_ == nullptr) {
      header_ = reinterpret_cast<LinkedListHeader *>(Allocator::alloc());
      header_->head = header_->tail = nullptr;
    }
  }

  LinkedListEntry<T> *head() const { return header_ != nullptr ? header_->head : nullptr; }

  LinkedListHeader *header_;
  DISALLOW_COPY_AND_ASSIGN(LinkedListT);
};

template <typename T, typename Allocator>
class LinkedListWrapper {
public:
  typedef LinkedListIterator<T> iterator;
  typedef T *value_type;

  LinkedListWrapper(void *ptr) {
    isT = android_api >= __ANDROID_API_T__;
    if (isT) {
      linkTPtr = reinterpret_cast<LinkedListT<T, Allocator> *>(ptr);
    } else {
      linkPtr = reinterpret_cast<LinkedList<T, Allocator> *>(ptr);
    }
  }

  bool empty() const {
    if (isT) {
      return linkTPtr->empty();
    } else {
      return linkPtr->empty();
    }
  }

  void push_front(T *const element) {
    if (isT) {
      linkTPtr->push_front(element);
    } else {
      linkPtr->push_front(element);
    }
  }

  void push_back(T *const element) {
    if (isT) {
      linkTPtr->push_back(element);
    } else {
      linkPtr->push_back(element);
    }
  }

  T *pop_front() { return isT ? linkTPtr->pop_front() : linkPtr->pop_front(); }

  T *front() const { return isT ? linkTPtr->front() : linkPtr->front(); }

  T *back() const { return isT ? linkTPtr->back() : linkPtr->back(); }

  void clear() {
    if (isT) {
      linkTPtr->clear();
    } else {
      linkPtr->clear();
    }
  }

  template <typename F>
  void for_each(F action) const {
    if (isT) {
      linkTPtr->for_each(action);
    } else {
      linkPtr->for_each(action);
    }
  }

  template <typename F>
  bool visit(F action) const {
    if (isT) {
      return linkTPtr->visit(action);
    } else {
      return linkPtr->visit(action);
    }
  }

  template <typename F>
  void remove_if(F predicate) {
    if (isT) {
      linkTPtr->remove_if(predicate);
    } else {
      linkPtr->remove_if(predicate);
    }
  }

  void remove(T *element) {
    if (isT) {
      linkTPtr->remove(element);
    } else {
      linkPtr->remove(element);
    }
  }

  template <typename F>
  T *find_if(F predicate) const {
    if (isT) {
      return linkTPtr->find_if(predicate);
    } else {
      return linkPtr->find_if(predicate);
    }
  }

  iterator begin() const {
    if (isT) {
      return linkTPtr->begin();
    }
    return linkPtr->begin();
  }

  iterator end() const {
    if (isT) {
      return linkTPtr->end();
    }
    return linkPtr->end();
  }

  iterator find(T *value) const {
    if (isT) {
      return linkTPtr->find(value);
    }
    return linkPtr->find(value);
  }

  size_t copy_to_array(T *array[], size_t array_length) const {
    if (isT) {
      return linkTPtr->copy_to_array(array, array_length);
    }
    return linkPtr->copy_to_array(array, array_length);
  }

  bool contains(const T *el) const {
    if (isT) {
      return linkTPtr->contains(el);
    }
    return linkPtr->contains(el);
  }

  size_t size() const {
    if (isT) {
      return linkTPtr->size();
    }
    return linkPtr->size();
  }

private:
  LinkedList<T, Allocator> *linkPtr = nullptr;
  LinkedListT<T, Allocator> *linkTPtr = nullptr;
  bool isT = false;
};
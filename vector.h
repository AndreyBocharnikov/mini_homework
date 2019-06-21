//
// Created by andreybocharnikov on 15.06.2019.
//

#ifndef EXAM_VECTOR_H
#define EXAM_VECTOR_H

#include <variant>
#include <memory>
#include <algorithm>
#include "counted.h"
#include <utility>

template <class T>
struct storage {

    //storage() :size_(0), capacity_(3), ref_count(1), data_((T*) (operator new (3 * sizeof(T)))) {}

    storage(size_t sz, size_t cap, size_t ref, T* d, bool fake) : size_(sz), capacity_(cap), ref_count(ref), fake_size(fake), data_(d){}

    ~storage() {
        if (data_ != nullptr) {
            for (size_t i = 0; i < size_; i++)
                (data_ + i)->~T();
            operator delete(data_);
        }
    }

    size_t size_, capacity_, ref_count;
    bool fake_size = false;
    T* data_ = nullptr;
};


template <class T>
struct vector {

    typedef T* iterator;
    typedef const T* const_iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    T* begin() {
        if (size_ == 0)
            return nullptr;
        if (size_ == 1)
            return &small;
        check_unique();
        return str_ptr->data_;
    }

    const T* begin() const {
        if (size_ == 0)
            return nullptr;
        if (size_ == 1)
            return &small;
        return str_ptr->data_;
    }

    reverse_iterator rbegin() {
        return reverse_iterator(end());
    }

    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(end());
    }

    T* end() {
        if (size_ == 0)
            return nullptr;
        if (size_ == 1)
            return &small + 1;
        check_unique();
        return str_ptr->data_ + str_ptr->size_;
    }

    const T* end() const {
        if (size_ == 0)
            return nullptr;
        if (size_ == 1)
            return &small + 1;
        return str_ptr->data_ + str_ptr->size_;
    }

    reverse_iterator rend() {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rend() const {
        return const_reverse_iterator(begin());
    }

    vector() : size_(0), str_ptr(nullptr) {}

    vector(vector const& rhs) : size_(rhs.size_) {
        if (rhs.str_ptr == nullptr && size_ == 0)
            return;
        if (size_ == 1) {
            small = rhs.small;
        } else {
            str_ptr = rhs.str_ptr;
            str_ptr->ref_count++;
        }
    }

    ~vector() {
        if (size_ == 0)
            return;

        if (size_ == 1) {
            (&small)->~T();
            return;
        }

        str_ptr->ref_count--;
        if (str_ptr->ref_count == 0) {
            delete str_ptr;
        }
    }

    vector& operator= (vector const& rhs) {
        if (&rhs != this) {
            vector(rhs).swap(*this);
        }
        return *this;
    }

    T&operator[] (size_t it) {
        if (is_small())
            return small;
        check_unique();
        return *(str_ptr->data_ + it);
    }

    T const& operator[] (size_t it) const noexcept {
        if (is_small())
            return small;
        return *(str_ptr->data_ + it);
    }

    T& front () {
        if (is_small())
            return small;
        check_unique();
        return *str_ptr->data_;
    }

    T const& front () const noexcept {
        if (is_small())
            return small;
        return *str_ptr->data_;
    }

    T& back () {
        if (is_small())
            return small;
        check_unique();
        return *(str_ptr->data_ + str_ptr->size_ - 1);
    }

    T const& back () const noexcept {
        if (is_small())
            return small;
        return *(str_ptr->data_ + str_ptr->size_ - 1);
    }

    void push_back(T const& val) {
        if (size_ == 0) {
            new (&small) T(val);
            size_ = 1;
            return;
        }

        storage<T>* tmp = nullptr;
        if (size_ == 1) {
            tmp = make_big(tmp);
        }
        if (!is_small() && str_ptr->ref_count > 1) {
            try {
                tmp = make_unique(tmp);
            } catch (...) {
                delete tmp;
                throw;
            }
        }
        if (!is_small() && str_ptr->capacity_ == str_ptr->size_) {
            try {
                tmp = ensure_capacity(tmp);
            } catch (...) {
                delete tmp;
                throw;
            }
        }

        if (tmp != nullptr) {
            try {
                new(tmp->data_ + tmp->size_) T(val);
            } catch (...) {
                delete tmp;
                throw;
            }
        } else {
            new (str_ptr->data_ + str_ptr->size_) T(val);
            ++str_ptr->size_;
            if (!str_ptr->fake_size)
                size_++;
            if (size_ == str_ptr->size_)
                str_ptr->fake_size = false;
            return;
        }

        if (!is_small()) {
            if (str_ptr->ref_count == 1) {
                delete str_ptr;
            } else {
                str_ptr->ref_count--;
            }
        } else {
            (&small)->~T();
        }

        str_ptr = tmp;
        if (str_ptr->fake_size) {
            ++str_ptr->size_;
            if (size_ == str_ptr->size_)
                str_ptr->fake_size = false;
        } else {
            size_ = ++str_ptr->size_;
        }
    }

    void pop_back() {
        //check_unique();
        storage<T>* try_str_ptr = nullptr;
        if (size_ > 1 && str_ptr->ref_count > 1)
            try_str_ptr = make_unique(nullptr);
        if (!is_small()) {
            if (str_ptr->size_ == 2) {
                storage<T>* save_pointer = str_ptr;
                try {
                    new (&small) T(*str_ptr->data_);
                } catch (...) {
                    str_ptr = save_pointer;
                    delete try_str_ptr;
                    throw;
                }
                if (try_str_ptr != nullptr) {
                    str_ptr->ref_count--;
                    str_ptr = try_str_ptr;
                } else {
                    delete save_pointer;
                }
                size_ = 1;
                return;
            }
            if (try_str_ptr == nullptr)
                try_str_ptr = str_ptr;
            else
                str_ptr->ref_count--;
            (try_str_ptr->data_ + try_str_ptr->size_ - 1)->~T();
            size_ = --try_str_ptr->size_;
            str_ptr = try_str_ptr;
        } else {
            size_--;
            (&small)->~T();
        }
    }

    T* data() {
        if (size_ == 1)
            return &small;
        if (size_ == 0)
            return nullptr;
        check_unique();
        return str_ptr->data_;
    }

    const T* data() const {
        if (size_ == 1)
            return &small;
        if (size_ == 0)
            return nullptr;
        return str_ptr->data_;
    }

    bool empty() const noexcept {
        return size() == 0;
    }

    size_t size() const noexcept {
        if (size_ <= 1)
            return size_;
        return str_ptr->size_;
    }

    void reserve(size_t sz) {
        if (is_small()) {
            str_ptr = make_big(nullptr, sz, 1);
            if (size_ <= 1) {
                size_ = 2;
                str_ptr->fake_size = true;
            }
            return;
        }
        if (str_ptr->capacity_ >= sz)
            return;
        //check_unique();
        storage<T>* try_str_ptr = nullptr;
        if (str_ptr->ref_count > 1)
            try_str_ptr = make_unique(nullptr);
        else
            try_str_ptr = str_ptr;
        size_t save_old_capacity = str_ptr->capacity_;
        str_ptr->capacity_ = sz;
        try {
            try_str_ptr = realloc(try_str_ptr);
        } catch (...) {
            str_ptr->capacity_ = save_old_capacity;
            if (try_str_ptr != str_ptr)
                delete try_str_ptr;
            throw;
        }
        if (str_ptr->ref_count > 1)
            str_ptr->ref_count--;
        str_ptr = try_str_ptr;
    }

    size_t capacity() const noexcept {
        if (size_ <= 1)
            return size_;
        return str_ptr->capacity_;
    }

    void shrink_to_fit() {
        if (is_small())
            return;
        if (str_ptr->size_ == str_ptr->capacity_)
            return;
        //check_unique();
        storage<T>* try_str_ptr = nullptr;
        if (str_ptr->ref_count > 1)
            try_str_ptr = make_unique(nullptr);
        else
            try_str_ptr = str_ptr;

        T* try_data = nullptr;
        try {
            try_data = (T*) operator new(sizeof(T) * str_ptr->size_);
        } catch(...) {
            if (try_str_ptr != str_ptr)
                delete try_str_ptr;
            throw;
        }

        try {
            std::uninitialized_copy(str_ptr->data_, str_ptr->data_ + str_ptr->size_, try_data);
        } catch(...) {
            operator delete (try_data);
            if (try_str_ptr != str_ptr)
                delete try_str_ptr;
            throw;
        }

        for (size_t i = 0; i < try_str_ptr->size_; i++)
            (try_str_ptr->data_ + i)->~T();
        operator delete(try_str_ptr->data_);

        if (try_str_ptr != str_ptr)
            str_ptr->ref_count--;
        try_str_ptr->data_ = try_data;
        try_str_ptr->capacity_ = try_str_ptr->size_;
        str_ptr = try_str_ptr;
    }

    void resize(size_t n) {
        if (n == 0) {
            clear();
            return;
        }
        if (size() == n)
            return;
        // T needs default constructor :(
        /*if (is_small()) {
            make_big(n, n);
            std::fill(str_ptr->data_ + 1, str_ptr->data_ + str_ptr->size_, T());
            return;
        }
        size_ = str_ptr->size_ = n;
        if (str_ptr->size_ > n) {
            for (size_t i = str_ptr->size_ - 1; i >= n; i--) {
                (str_ptr->data_ + i)->~T();
            }
        } else {
            ensure_capacity();
            std::fill(str_ptr->data_ + str_ptr->size_, str_ptr->data_ + n, T());
        }*/
    };

    void clear() {
        if (is_small()) {
            size_ = 0;
            (&small)->~T();
            str_ptr = nullptr;
            return;
        }
        check_unique();
        delete str_ptr;
        str_ptr = nullptr;
        size_ = 0;
    }

    iterator insert(const_iterator pos, T const& val) {
        if (size_ == 0) {
            new (&small) T(val);
            size_ = 1;
            return &small;
        }

        if (is_small()) {
            if (pos == &small + 1) {
                push_back(val);
                return str_ptr->data_ + 1;
            } else {
                storage<T>* try_new = make_big(nullptr, 3, 1);
                try {
                    new (try_new->data_ + 1) T(val);
                    try_new->size_++;
                } catch (...) {
                    delete try_new;
                    throw;
                }
                try {
                    std::swap(*try_new->data_, *(try_new->data_ + 1));
                } catch(...) {
                    delete try_new;
                    throw;
                }

                (&small)->~T();
                str_ptr = try_new;
                size_ = str_ptr->size_ = 2;
                return str_ptr->data_;
            }
        }

        if (!is_small() && pos == str_ptr->data_ + str_ptr->size_) {
            push_back(val);
            return str_ptr->data_ + str_ptr->size_ - 1;
        }
        size_t index = pos - str_ptr->data_;

        storage<T>* try_str_ptr = nullptr;
        if (size_ == 1) {
            try_str_ptr = make_big(nullptr);
        }
        if (size_ > 1 && str_ptr->ref_count > 1) {
            try_str_ptr = make_unique(nullptr);
        }
        if (size_ > 1 && str_ptr->capacity_ == str_ptr->size_) {
            try {
                try_str_ptr = ensure_capacity(try_str_ptr);
                try_str_ptr->size_ = str_ptr->size_;
            } catch (...) {
                delete try_str_ptr;
                throw;
            }
        }

        bool diff = false;
        if (try_str_ptr == nullptr)
            try_str_ptr = str_ptr;
        else
            diff = true;

        T* tmp_copy = nullptr;
        try {
            tmp_copy = (T*) operator new(try_str_ptr->capacity_ * sizeof(T));
        } catch (...) {
            if (diff)
                delete try_str_ptr;
            throw;
        }

        for (size_t i = 0; i < index; ++i) {
            try {
                new (tmp_copy + i) T(try_str_ptr->data_[i]);
            } catch (...) {
                for (size_t j = 0; j < i; ++j)
                    (tmp_copy + j)->~T();
                operator delete (tmp_copy);
                if (diff)
                    delete try_str_ptr;
                throw;
            }
        }

        try {
            new (tmp_copy + index) T(val);
        } catch (...) {
            for (size_t j = 0; j < index; ++j)
                (tmp_copy + j)->~T();
            operator delete (tmp_copy);
            if (diff)
                delete try_str_ptr;
            throw;
        }

        for (size_t i = index; i < try_str_ptr->size_; i++) {
            try {
                new (tmp_copy + i + 1) T(try_str_ptr->data_[i]);
            } catch (...) {
                for (size_t j = 0; j <= i; j++)
                    (tmp_copy + j)->~T();
                operator delete (tmp_copy);
                if (diff)
                    delete try_str_ptr;
                throw;
            }
        }

        delete_extra(diff, try_str_ptr);

        size_ = ++try_str_ptr->size_;
        try_str_ptr->data_ = tmp_copy;
        str_ptr = try_str_ptr;
        return str_ptr->data_ + index;
    }

    iterator erase(const_iterator pos) {
        if (is_small()) {
            size_ = 0;
            (&small)->~T();
            str_ptr = nullptr;
            return (&small);
        }
        size_t index = pos - str_ptr->data_;
        storage<T>* try_str_ptr = nullptr;
        if (str_ptr->ref_count > 1)
            try_str_ptr = make_unique(try_str_ptr);

        bool diff = false;
        if (try_str_ptr == nullptr)
            try_str_ptr = str_ptr;
        else
            diff = true;

        T* tmp_copy;
        try {
            tmp_copy = (T *) operator new(try_str_ptr->capacity_ * sizeof(T));
        } catch (...) {
            if (diff)
                delete try_str_ptr;
            throw;
        }

        try {
            std::uninitialized_copy(try_str_ptr->data_, try_str_ptr->data_ + index, tmp_copy);
        } catch (...) {
            operator delete(tmp_copy);
            if (diff)
                delete try_str_ptr;
            throw;
        }

        try {
            std::uninitialized_copy(str_ptr->data_ + index + 1, str_ptr->data_ + str_ptr->size_, tmp_copy + index);
        } catch (...) {
            for (size_t j = 0; j < index; j++)
                (tmp_copy + j)->~T();
            operator delete(tmp_copy);
            if (diff)
                delete try_str_ptr;
            throw;
        }

        delete_extra(diff, try_str_ptr);

        size_ = --try_str_ptr->size_;
        try_str_ptr->data_ = tmp_copy;
        str_ptr = try_str_ptr;
        if (size_ == 1)
            return make_small(size_);
        return str_ptr->data_ + index;
    }

    iterator erase(const_iterator first, const_iterator second) {
        if (first + 1 == second)
            return erase(first);
        size_t left = first - str_ptr->data_, right = second - str_ptr->data_;

        storage<T>* try_str_ptr = nullptr;
        if (str_ptr->ref_count > 1)
            try_str_ptr = make_unique(nullptr);

        bool diff = false;
        if (try_str_ptr == nullptr)
            try_str_ptr = str_ptr;
        else
            diff = true;

        T* tmp_copy = nullptr;
        try {
            tmp_copy = (T*) operator new(try_str_ptr->capacity_ * sizeof(T));
        } catch (...) {
            if (diff)
                delete try_str_ptr;
            throw;
        }
        try {
            std::uninitialized_copy(try_str_ptr->data_, try_str_ptr->data_ + left, tmp_copy);
        } catch (...) {
            operator delete (tmp_copy);
            if (diff)
                delete try_str_ptr;
            throw;
        }

        try {
            std::uninitialized_copy(try_str_ptr->data_ + right, try_str_ptr->data_ + str_ptr->size_, tmp_copy + left);
        } catch (...) {
            for (size_t j = 0; j < left; j++)
                (tmp_copy + j)->~T();
            operator delete (tmp_copy);
            if (diff)
                delete try_str_ptr;
            throw;
        }

        delete_extra(diff, try_str_ptr);

        try_str_ptr->size_ -= right - left;
        size_ = try_str_ptr->size_;
        try_str_ptr->data_ = tmp_copy;
        str_ptr = try_str_ptr;
        if (size_ <= 1)
            return make_small(size_);
        return str_ptr->data_ + left;
    }

    friend bool operator< (vector const& lhs, vector const& rhs) {
        return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }

    friend bool operator> (vector const& lhs, vector const& rhs) {
        return rhs < lhs;
    }

    friend bool operator== (vector const& lhs, vector const& rhs) {
        return (rhs < lhs) == 0 && (lhs < rhs) == 0;
    }

    friend bool operator!= (vector const& lhs, vector const& rhs) {
        return !(lhs == rhs);
    }

    friend bool operator<= (vector const& lhs, vector const& rhs) {
        return !(lhs > rhs);
    }

    friend bool operator>= (vector const& lhs, vector const& rhs) {
        return !(lhs < rhs);
    }

private:
    size_t size_ = 0;
    union {
        storage<T>* str_ptr = nullptr;
        T small;
    };

    storage<T>* make_unique(storage<T> *try_str_ptr) { // try_str_ptr is always nullptr
        //if (try_str_ptr == nullptr) {
            try_str_ptr = new storage<T>(0, str_ptr->capacity_, 1, nullptr, str_ptr->fake_size);
            try {
                try_str_ptr->data_ = (T*) operator new(sizeof(T) * try_str_ptr->capacity_);
            } catch(...) {
                delete try_str_ptr;
                throw;
            }

            try {
                std::uninitialized_copy(str_ptr->data_, str_ptr->data_ + str_ptr->size_, try_str_ptr->data_);
            } catch (...) {
                delete try_str_ptr;
                throw;
            }
            try_str_ptr->size_ = str_ptr->size_;
        //}

        return try_str_ptr;
    }

    storage<T>* make_big(storage<T>* try_str_ptr, size_t capas = 3, size_t size = 1) {
        try_str_ptr = new storage<T>(0, capas, 1, nullptr, false);
        try {
            try_str_ptr->data_ = (T *) (operator new(try_str_ptr->capacity_ * sizeof(T)));
        } catch (...) {
            delete try_str_ptr;
            throw;
        }

        if (size == 1) {
            try {
                if (size_ == 1) {
                    new(try_str_ptr->data_) T(small);
                    try_str_ptr->size_ = 1;
                }
                //else
                  //  new(str_ptr->data_) T();
            } catch (...) {
                delete try_str_ptr;
                throw;
            }
        }

        /*for (size_t i = 1; i < size; i++) {
            try {
                new(str_ptr->data_ + i) T();
            } catch (...) {
                for (size_t j = 1; j < i; j++)
                    (str_ptr->data_ + j)->~T();
                operator delete(str_ptr);
                delete str_ptr;
                throw ;
            }
        }*/

        return try_str_ptr;
    }

    storage<T>* ensure_capacity(storage<T>* try_str_ptr) {
        bool should_delete_here = false;
        if (try_str_ptr == nullptr) {
            should_delete_here = true;
            try_str_ptr = new storage<T>(0, str_ptr->capacity_, str_ptr->ref_count, nullptr, str_ptr->fake_size);
            try {
                try_str_ptr->data_ = (T*) operator new(sizeof(T) * try_str_ptr->capacity_);
                std::uninitialized_copy(str_ptr->data_, str_ptr->data_ + str_ptr->size_, try_str_ptr->data_);
            } catch(...) {
                delete try_str_ptr;
                throw;
            }

            try_str_ptr->size_ = str_ptr->size_;
        }

        try_str_ptr->capacity_ *= 2;

        try {
            try_str_ptr = realloc(try_str_ptr);
        } catch (...) {
            try_str_ptr->capacity_ /= 2;
            if (should_delete_here)
                delete try_str_ptr;
            throw;
        }
        return try_str_ptr;
    }

    storage<T>* realloc (storage<T>* try_str_ptr) { // try_str_ptr shouldnt be nullptr
        T* new_data = (T*) operator new(sizeof(T) * try_str_ptr->capacity_);
        try {
            std::uninitialized_copy(try_str_ptr->data_, try_str_ptr->data_ + try_str_ptr->size_, new_data);
        } catch(...) {
            operator delete(new_data);
            throw;
        }
        for (size_t i = 0; i < try_str_ptr->size_; i++)
            (try_str_ptr->data_ + i)->~T();
        operator delete(try_str_ptr->data_);
        try_str_ptr->data_ = new_data;
        return try_str_ptr;
    }

    void check_unique() {
        if (size_ <= 1)
            return;
        if (str_ptr->ref_count > 1) {
            str_ptr->ref_count--;
            try {
                str_ptr = make_unique(nullptr);
            } catch(...) {
                str_ptr->ref_count++;
                throw;
            }
        }
    }

    iterator make_small(size_t s) {
        if (s == 1) {
            storage<T>* save_ptr = str_ptr;
            try {
                new (&small) T(*str_ptr->data_);
                delete save_ptr;
            } catch(...) {
                delete save_ptr;
                size_ = 0;
                str_ptr = nullptr;
                throw;
            }
            return (&small) + 1;
        } else {
            delete str_ptr;
            str_ptr = nullptr;
            return &small;
        }
    }

    void delete_extra(bool diff, storage<T>* try_str_ptr) {
        if (diff)
            str_ptr->ref_count--;

        if (str_ptr->ref_count == 0 && diff) {
            delete str_ptr;
        }

        for (size_t i = 0; i < try_str_ptr->size_; i++)
            (try_str_ptr->data_ + i)->~T();
        operator delete(try_str_ptr->data_);
    }

    void swap(vector<T> &rhs) {
        if (size_ == 1 && rhs.size_ == 1)
            std::swap(small, rhs.small);

        if (size_ == 1 && rhs.size_ == 0) {
            new (&rhs.small) T(small);
            (&small)->~T();
            std::swap(size_, rhs.size_);
            return;
        }

        if (size_ == 0 && rhs.size_ == 1) {
            new (&small) T(rhs.small);
            (&rhs.small)->~T();
            std::swap(size_, rhs.size_);
            return;
        }

        if (size_ == 1 && !rhs.is_small()) {
            storage<T>* save_str_ptr = rhs.str_ptr;
            try {
                new (&rhs.small) T(small);
                (&small)->~T();
            } catch(...) {
                rhs.str_ptr = save_str_ptr;
                throw;
            }
            str_ptr = save_str_ptr;
            std::swap(size_, rhs.size_);
            return;
        }

        if (!is_small() && rhs.size_ == 1) {
            storage<T>* save_str_ptr = str_ptr;
            try {
                new (&small) T(rhs.small);
                (&rhs.small)->~T();
            } catch (...) {
                str_ptr = save_str_ptr;
                throw;
            }
            rhs.str_ptr = save_str_ptr;
            std::swap(size_, rhs.size_);
            return;
        }

        if ((!is_small() && !rhs.is_small()) || size_ == 0 || rhs.size_ == 0) {
            std::swap(size_, rhs.size_);
            std::swap(str_ptr, rhs.str_ptr);
        }
    }

    bool is_small() const {
        return size_ <= 1;
    }

    friend void swap(vector &lhs, vector &rhs) {
        lhs.swap(rhs);
    }

};

template <class T>
std::ostream& operator<<(std::ostream& s, vector<T> const& a) {
    for (size_t i = 0; i < a.str_ptr->size_; i++)
        s << a.str_ptr->data_[i] << " ";
    return s;
}

#endif //EXAM_VECTOR_H

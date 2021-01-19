//
// Created by beich on 2020/12/25.
//

#pragma once
#include <pthread.h>

class ScopedPthreadMutexLocker {
public:
    explicit ScopedPthreadMutexLocker(pthread_mutex_t *mu) : mu_(mu) {
        pthread_mutex_lock(mu_);
    }

    ~ScopedPthreadMutexLocker() {
        pthread_mutex_unlock(mu_);
    }

private:
    pthread_mutex_t *mu_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedPthreadMutexLocker);
};

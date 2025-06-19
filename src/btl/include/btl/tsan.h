#pragma once

#if defined(__SANITIZE_THREAD__)
#define TSAN_ENABLED
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define TSAN_ENABLED
#endif
#endif

#ifdef TSAN_ENABLED
#define TSAN_ANNOTATE_HAPPENS_BEFORE(addr) \
    AnnotateHappensBefore(__FILE__, __LINE__, (void*)(addr))
#define TSAN_ANNOTATE_HAPPENS_AFTER(addr) \
    AnnotateHappensAfter(__FILE__, __LINE__, (void*)(addr))
#define TSAN_ANNOTATE_RWLOCK_CREATE(addr) \
    AnnotateRWLockCreate(__FILE__, __LINE__, (void*)(addr))
#define TSAN_ANNOTATE_RWLOCK_DESTROY(addr) \
    AnnotateRWLockDestroy(__FILE__, __LINE__, (void*)(addr))
#define TSAN_ANNOTATE_RWLOCK_ACQUIRED(addr, is_w) \
    AnnotateRWLockAcquired(__FILE__, __LINE__, (void*)(addr), (is_w))
#define TSAN_ANNOTATE_RWLOCK_RELEASED(addr, is_w) \
    AnnotateRWLockReleased(__FILE__, __LINE__, (void*)(addr), (is_w))
extern "C" void AnnotateHappensBefore(const char* f, int l, void* addr);
extern "C" void AnnotateHappensAfter(const char* f, int l, void* addr);
extern "C" void AnnotateRWLockCreate(const char *f, int l, void* m);
extern "C" void AnnotateRWLockDestroy(const char *f, int l, void* m);
extern "C" void AnnotateRWLockAcquired(const char *f, int l, void* m, bool is_rw);
extern "C" void AnnotateRWLockReleased(const char *f, int l, void* m, bool is_rw);
#else
#define TSAN_ANNOTATE_HAPPENS_BEFORE(addr)
#define TSAN_ANNOTATE_HAPPENS_AFTER(addr)
#define TSAN_ANNOTATE_RWLOCK_CREATE(addr)
#define TSAN_ANNOTATE_RWLOCK_DESTROY(addr)
#define TSAN_ANNOTATE_RWLOCK_ACQUIRED(addr, is_w)
#define TSAN_ANNOTATE_RWLOCK_RELEASED(addr, is_w)
#endif


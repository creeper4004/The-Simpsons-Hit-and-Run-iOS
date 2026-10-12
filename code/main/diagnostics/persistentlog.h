#ifndef SRR_PERSISTENT_LOG_H
#define SRR_PERSISTENT_LOG_H
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

// Fixed storage, serialized whole records, bounded rotation. No engine allocator.
class PersistentLog {
public:
    PersistentLog() : fd(-1), limit(0), sequence(0) {
        pthread_mutex_init(&mutex, NULL);
        path[0] = previous[0] = 0;
    }
    ~PersistentLog() { if (fd >= 0) close(fd); pthread_mutex_destroy(&mutex); }
    bool Open(const char* directory, size_t maximum) {
        pthread_mutex_lock(&mutex);
        snprintf(path, sizeof(path), "%s/srr2_health.log", directory);
        snprintf(previous, sizeof(previous), "%s/srr2_health.previous.log", directory);
        if (fd >= 0) close(fd);
        rename(path, previous);
        fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
        limit = maximum;
        bool ok = fd >= 0;
        pthread_mutex_unlock(&mutex);
        return ok;
    }
    bool Write(const char* event, const char* detail) {
        pthread_mutex_lock(&mutex);
        if (fd < 0) { pthread_mutex_unlock(&mutex); return false; }
        char record[2048];
        int n = snprintf(record, sizeof(record), "seq=%llu event=%s %s\n",
                         ++sequence, event ? event : "unknown", detail ? detail : "");
        size_t length = n < 0 ? 0 : (size_t)n;
        if (length >= sizeof(record)) length = sizeof(record) - 1;
        for (size_t i = 0; i + 1 < length; ++i)
            if (record[i] == '\n' || record[i] == '\r') record[i] = ' ';
        if (length) record[length - 1] = '\n';
        struct stat st;
        if (fstat(fd, &st) == 0 && (size_t)st.st_size + length > limit) {
            // Open the replacement before retiring the live descriptor.
            if (rename(path, previous) == 0) {
                int next = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
                if (next >= 0) { close(fd); fd = next; }
                else rename(previous, path);
            }
        }
        size_t written = 0;
        while (written < length) {
            ssize_t result = write(fd, record + written, length - written);
            if (result < 0 && errno == EINTR) continue;
            if (result <= 0) break;
            written += (size_t)result;
        }
        pthread_mutex_unlock(&mutex);
        return written == length;
    }
    void Clear() {
        pthread_mutex_lock(&mutex);
        if (fd >= 0) { ftruncate(fd, 0); lseek(fd, 0, SEEK_SET); }
        unlink(previous); sequence = 0;
        pthread_mutex_unlock(&mutex);
    }
    void Flush() { pthread_mutex_lock(&mutex); if (fd >= 0) fsync(fd); pthread_mutex_unlock(&mutex); }
private:
    PersistentLog(const PersistentLog&);
    PersistentLog& operator=(const PersistentLog&);
    pthread_mutex_t mutex;
    int fd;
    size_t limit;
    unsigned long long sequence;
    char path[1024], previous[1024];
};
#endif

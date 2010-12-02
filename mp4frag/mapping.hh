#ifndef __mapping_hh__739ccb21_1898_4774_9e40_797f62fbbdaf
#define __mapping_hh__739ccb21_1898_4774_9e40_797f62fbbdaf

#include <utility>
#include <memory.h>

class Mapping {
public:
    template<class ARG> Mapping(ARG a) { make_mapping(a); }
    ~Mapping();
    const char *data() const { return (const char *)(_data); }
    size_t size() const { return _size; }
private:
    void *_data;
    size_t _size;
    void make_mapping(int);
    void make_mapping(const char *);
};

#endif

#include "mapping.hh"
#include <boost/system/system_error.hpp>
#include <boost/system/error_code.hpp>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

using namespace boost::system;

void Mapping::make_mapping(int fd) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    if ( fstat(fd, &st) == -1 ) {
        throw system_error(errno, get_system_category(), "fstat");
    }
    _size = st.st_size;
    _data = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if ( _data == (void *)(-1) ) {
        throw system_error(errno, get_system_category(), "mmap");
    }
}

void Mapping::make_mapping(const char *name) {
    int fd = open(name, O_RDONLY);
    if ( fd == -1 ) {
        throw system_error(errno, get_system_category(), std::string("opening ") + name);
    }

    struct fd_guard {
        int _fd;
        fd_guard(int f) : _fd(f) {}
        ~fd_guard() { close(_fd); }
    } guard(fd);

    make_mapping(fd);
}

Mapping::~Mapping() {
    if ( _data != (void*)-1 ) {
        munmap((void *)_data, _size);
    }
}

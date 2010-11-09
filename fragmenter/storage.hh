#ifndef __storage_hh__f44f48bb_6766_4afe_8360_2712a92f79bc
#define __storage_hh__f44f48bb_6766_4afe_8360_2712a92f79bc

#include "utility/utils/list2.hh"

#include <boost/function.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <string>
#include <vector>

struct FileDataChunk {
private:
    std::vector<char> _data;
public:
    FileDataChunk(unsigned size) : _data(size) {}
    bool is_eof() const { return sz() == 0; }
    char *data() { return &_data[0]; }
    size_t sz() const { return _data.size(); }
    void resize(size_t sz) { _data.resize(sz); }
    static void *operator new(size_t);
    static void operator delete(void *);
};

struct FileReaderCallback : public ::utility::utils::Link2<FileReaderCallback> {
#if 0
protected:
    virtual void on_read(const ::boost::shared_ptr<FileDataChunk>&) = 0;
    virtual void on_eof() = 0;
    virtual void on_failure(const ::std::string&) = 0;
public:
#else
    ::boost::function<void (const ::boost::shared_ptr<FileDataChunk>&)> _success;
    ::boost::function<void ()> _eof;
    ::boost::function<void (const ::std::string&)> _failure;
#endif
#if 1
    FileReaderCallback( const ::boost::function<void (const ::boost::shared_ptr<FileDataChunk>&)>& s,
                        const ::boost::function<void ()>& e,
                        const ::boost::function<void (const ::std::string&)>& f) :
#else       
    template<class S, class E, class F> FileReaderCallback(typename ::boost::call_traits<S>::param_type s,
                                                           typename ::boost::call_traits<E>::param_type e,
                                                           typename ::boost::call_traits<F>::param_type f) :
#endif                                                           
        _success(s), _eof(e), _failure(f) {}
    FileReaderCallback() {}
    void success(const ::boost::shared_ptr<FileDataChunk>&);
    void eof();
    void failure(const ::std::string&);
};

class DataSource : public boost::enable_shared_from_this<DataSource> {
public:
    virtual ~DataSource() {}
    virtual void aread(off_t offset, size_t sz, const boost::shared_ptr<FileReaderCallback>& cb) = 0;
    virtual std::ostream& repr(std::ostream&) const = 0;
};

inline std::ostream& operator<<(std::ostream& o, const DataSource& ds) {
    return ds.repr(o);
}

boost::shared_ptr<DataSource> get_source(const std::string& name);

int get_open_file_handle(const std::string& filename);

void get_storage_file_data(const std::string& filename, off_t offset, size_t size, FileReaderCallback *callback);

std::pair<unsigned, unsigned> storage_cache_size();

#endif

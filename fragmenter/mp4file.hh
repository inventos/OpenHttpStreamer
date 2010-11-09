#ifndef __mp4file_hh__c27206a8_df1c_4ab2_a1c3_553efc3b7374
#define __mp4file_hh__c27206a8_df1c_4ab2_a1c3_553efc3b7374

#include "mp4.hh"
#include "storage.hh"

#include "utility/utils/list2.hh"

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include <string>

namespace mp4 {

    struct ParseCallback : public ::utility::utils::Link2<ParseCallback> {
        ::boost::function<void (const ::boost::shared_ptr<Context>&)> _success;
        ::boost::function<void (const ::std::string&)> _failure;
    };

    void a_parse_mp4_file(const ::std::string& filename, ParseCallback *cb);
    void a_parse_mp4(const boost::shared_ptr<DataSource>& src, ParseCallback *cb);

}

#endif

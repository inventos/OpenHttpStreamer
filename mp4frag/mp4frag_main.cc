/*
 * 
 * copyright (c) 2010 ZAO Inventos (inventos.ru)
 * copyright (c) 2010 jk@inventos.ru
 * copyright (c) 2010 kuzalex@inventos.ru
 * copyright (c) 2010 artint@inventos.ru
 *
 * This file is part of mp4frag.
 *
 * mp4grag is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mp4frag is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#include "mp4frag.hh"
#include "mp4.hh"
#include "base64.hh"
#include <boost/program_options.hpp>
#include <boost/system/system_error.hpp>
#include <boost/system/error_code.hpp>
#include <boost/foreach.hpp>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace boost::system;

namespace {
    std::string manifest_name = "manifest.f4m";
    std::string docroot = ".";
    std::string basedir = ".";
    std::string video_id("some_video");
    std::vector<std::string> srcfiles;
    int fragment_duration;
}

void parse_options(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("Allowed options");
    desc.add_options()
      ("help", "produce help message")
      ("src", po::value< std::vector<std::string> >(&srcfiles), "source mp4 file name")
      ("docroot", po::value<std::string>(&docroot)->default_value("."), "docroot directory")
      ("basedir", po::value<std::string>(&basedir)->default_value("."), "base directory for manifest file")
      ("video_id", po::value<std::string>(&video_id)->default_value("some_video"), "video id for manifest file")
      ("manifest", po::value<std::string>(&manifest_name)->default_value("manifest.f4m"), "manifest file name")
      ("fragmentduration", po::value<int>(&fragment_duration)->default_value(3000), "single fragment duration, ms")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    

    if (vm.count("help") || argc == 1 || srcfiles.size() == 0) {
        std::cerr << desc << "\n";
        exit(1);
    }
    
}

#include <sys/time.h>

int main(int argc, char **argv) try {
    parse_options(argc, argv);

    std::vector< boost::shared_ptr<Media> > fileinfo_list;

    struct timeval then, now;
    gettimeofday(&then, 0);
    BOOST_FOREACH(std::string& srcfile, srcfiles) {
        fileinfo_list.push_back(make_fragments(srcfile, fragment_duration));
    }
    gettimeofday(&now, 0);
    double diff = now.tv_sec - then.tv_sec + 1e-6*(now.tv_usec - then.tv_usec);
    gettimeofday(&then, 0);
    gettimeofday(&now, 0);
    diff -= now.tv_sec - then.tv_sec + 1e-6*(now.tv_usec - then.tv_usec);

    std::cerr << "Parsed in " << diff << " seconds\n";

    BOOST_FOREACH(boost::shared_ptr<Media>& pmedia, fileinfo_list) {
        // create the directory if needed:
        std::string dirname = pmedia->name + ".d";
        if ( mkdir(dirname.c_str(), 0755) == -1 && errno != EEXIST ) {
            throw system_error(errno, get_system_category(), "mkdir " + dirname);
        }
        for ( unsigned fragment = 1; fragment <= pmedia->fragments.size(); ++fragment ) {
            std::filebuf out;
            std::stringstream fragment_file;
            fragment_file << dirname << "/Seg1-Frag" << fragment;
            if ( out.open(fragment_file.str().c_str(), std::ios::out | std::ios::binary | std::ios::trunc) ) {
                serialize_fragment(&out, pmedia, fragment - 1);
                if ( !out.close() ) {
                    std::stringstream errmsg;
                    errmsg << "Error closing " << fragment_file.str();
                    throw std::runtime_error(errmsg.str());
                }
            }
            else {
                std::stringstream errmsg;
                errmsg << "Error opening " << fragment_file.str();
                throw std::runtime_error(errmsg.str());
            }
        }
    }

    std::stringstream manifestname;
    manifestname << docroot << '/' << basedir << '/' << manifest_name;
    std::filebuf manifest_filebuf;
    if ( manifest_filebuf.open(manifestname.str().c_str(), 
                               std::ios::out | std::ios::binary | std::ios::trunc) ) {
        get_manifest(&manifest_filebuf, fileinfo_list, video_id);
        if ( !manifest_filebuf.close() ) {
            std::stringstream errmsg;
            errmsg << "Error closing " << manifestname.str();
            throw std::runtime_error(errmsg.str());
        }
    }
    else {
        throw std::runtime_error("Error opening " + manifestname.str());
    }
}
catch ( std::exception& e ) {
    std::cerr << e.what() << "\n";
}

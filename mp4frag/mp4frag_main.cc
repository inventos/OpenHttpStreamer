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
#include <boost/program_options.hpp>
#include <boost/system/system_error.hpp>
#include <boost/system/error_code.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>

using namespace boost::system;
namespace bfs = boost::filesystem;
using boost::lexical_cast;

namespace {
    bfs::path manifest_name = "manifest.f4m";
    std::string video_id("some_video");
    std::vector<bfs::path> srcfiles;
    bool produce_template = false;
    bool manifest_only = false;
    int fragment_duration;
}

void parse_options(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("Allowed options");
    desc.add_options()
      ("help", "produce help message")
      ("src", po::value< std::vector<bfs::path> >(&srcfiles), "source mp4 file name")
      ("video_id", po::value<std::string>(&video_id)->default_value("some_video"), "video id for manifest file")
      ("manifest", po::value<bfs::path>(&manifest_name)->default_value("manifest.f4m"), "manifest file name")
      ("fragmentduration", po::value<int>(&fragment_duration)->default_value(3000), "single fragment duration, ms")
      ("index", "make index files instead of full fragments")
      ("nofragments", "make manifest only")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    

    if (vm.count("help") || argc == 1 || srcfiles.size() == 0) {
        std::cerr << desc << "\n";
        exit(1);
    }

    produce_template = vm.count("template") != 0;
    manifest_only = vm.count("nofragments") != 0;
    
}

boost::shared_ptr<Mapping> create_mapping(const std::string& filename) {
    return boost::shared_ptr<Mapping>(new Mapping(filename.c_str()));
}

#include <sys/time.h>

int main(int argc, char **argv) try {
    parse_options(argc, argv);

    std::vector<boost::shared_ptr<Media> > fileinfo_list;

    struct timeval then, now;
    gettimeofday(&then, 0);
    BOOST_FOREACH(bfs::path& srcfile, srcfiles) {
        boost::shared_ptr<Media> pmedia = make_fragments(srcfile.string(), fragment_duration);
        fileinfo_list.push_back(pmedia);
        pmedia->medianame = bfs::complete(srcfile).string();
    }
    gettimeofday(&now, 0);
    double diff = now.tv_sec - then.tv_sec + 1e-6*(now.tv_usec - then.tv_usec);
    gettimeofday(&then, 0);
    gettimeofday(&now, 0);
    diff -= now.tv_sec - then.tv_sec + 1e-6*(now.tv_usec - then.tv_usec);

    std::cerr << "Parsed in " << diff << " seconds\n";

    if ( !manifest_only ) {
    if ( produce_template ) {
        std::filebuf out;
        std::string indexname = (manifest_name.parent_path() / "index").string();
        if ( out.open(indexname.c_str(), std::ios::out | std::ios::binary | std::ios::trunc) ) {
            serialize(&out, fileinfo_list);
            if ( !out.close() ) {
                throw std::runtime_error("Error closing " + indexname);
            }
        }
        else {
                throw std::runtime_error("Error opening " + indexname);
        }
#if TESTING
        Mapping index(indexname.c_str());
        for ( unsigned ix = 0; ix < fileinfo_list.size(); ++ix ) {
            boost::shared_ptr<Media>& pmedia = fileinfo_list[ix];        
            bfs::path mediadir = manifest_name.parent_path() / lexical_cast<std::string>(ix);
            if ( mkdir(mediadir.string().c_str(), 0755) == -1 && errno != EEXIST ) {
                throw system_error(errno, get_system_category(), "mkdir " + mediadir.string());
            }
            for ( unsigned fragment = 1; fragment <= pmedia->fragments.size(); ++fragment ) {
                std::filebuf out;
                std::string fragment_basename = std::string("Seg1-Frag") + boost::lexical_cast<std::string>(fragment);
                bfs::path fragment_file = mediadir / fragment_basename;
                if ( out.open(fragment_file.string().c_str(), std::ios::out | std::ios::binary | std::ios::trunc) ) {
                    get_fragment(&out, ix, fragment - 1, index.data(), index.size(), create_mapping);
                    if ( !out.close() ) {
                        throw std::runtime_error("Error closing " + fragment_file.string());
                    }
                }
                else {
                    throw std::runtime_error("Error opening " + fragment_file.string());
                }
            }
        }
#endif
    }
    else { 
        for ( unsigned ix = 0; ix < fileinfo_list.size(); ++ix ) {
            boost::shared_ptr<Media>& pmedia = fileinfo_list[ix];        
            bfs::path mediadir = manifest_name.parent_path() / lexical_cast<std::string>(ix);
            if ( mkdir(mediadir.string().c_str(), 0755) == -1 && errno != EEXIST ) {
                throw system_error(errno, get_system_category(), "mkdir " + mediadir.string());
            }
            for ( unsigned fragment = 1; fragment <= pmedia->fragments.size(); ++fragment ) {
                std::filebuf out;
                std::string fragment_basename = std::string("Seg1-Frag") + boost::lexical_cast<std::string>(fragment);
                bfs::path fragment_file = mediadir / fragment_basename;
                if ( out.open(fragment_file.string().c_str(), std::ios::out | std::ios::binary | std::ios::trunc) ) {
                    serialize_fragment(&out, pmedia, fragment - 1);
                    if ( !out.close() ) {
                        throw std::runtime_error("Error closing " + fragment_file.string());
                    }
                }
                else {
                    throw std::runtime_error("Error opening " + fragment_file.string());
                }
            }
        }
    }
    }

    std::filebuf manifest_filebuf;
    if ( manifest_filebuf.open(manifest_name.string().c_str(), 
                               std::ios::out | std::ios::binary | std::ios::trunc) ) {
        get_manifest(&manifest_filebuf, fileinfo_list, video_id);
        if ( !manifest_filebuf.close() ) {
            std::stringstream errmsg;
            errmsg << "Error closing " << manifest_name;
            throw std::runtime_error(errmsg.str());
        }
    }
    else {
        throw std::runtime_error("Error opening " + manifest_name.string());
    }
}
catch ( std::exception& e ) {
    std::cerr << e.what() << "\n";
}

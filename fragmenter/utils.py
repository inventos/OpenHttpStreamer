import os, re, os.path, sys, subprocess


def suffix_check(env):
    libpath = ["/lib", "/usr/lib"]
    if os.getenv("LD_LIBRARY_PATH"):
        libpath.append(os.getenv("LD_LIBRARY_PATH"))
        
    if env.has_key("LIBPATH"):
        libpath.extend(env["LIBPATH"])
    
#    ldsoconf = os.listdir("/etc/ld.so.conf.d")
#    for libdir in ldsoconf:
#        f = open("/etc/ld.so.conf.d/" + libdir)
#        for s in f.readlines():
#            if s and s.find("#") == -1:
#                libpath.append(s)
#
#    f = open("/etc/ld.so.conf")
#    s = f.readline()
#    for s in f.readlines():
#        if s and s.find("#") == -1:
#            libpath.append(s)
        
    boost_mask = re.compile("libboost_\w*(\-[\w\-]*?)?\.so")
    boost_lib_suffix = ""
    found = False
    for libdir in libpath:
        if os.path.exists(libdir):
            file_list = os.listdir(libdir)
            for name in file_list:
                boost_fnd = boost_mask.search(name)
                if boost_fnd and not found or (found and len(boost_lib_suffix) > len(boost_fnd.group(1))):
                    #aghrrr, I love python
                    if boost_fnd.group(1):
                        boost_lib_suffix = boost_fnd.group(1)
                    else:
                        boost_lib_suffix = ""
                    found = True
                    break
            
            if found:
                break
                    
    #print "boost_lib_suffix is ", boost_lib_suffix
    return boost_lib_suffix

def run_cmd_silently(command):
    #command = [arg[0], "-M", arg[1], dirname + "/" + name]
    #command = arg[0] + " -M " + arg[1] + " " + dirname + "/" + name
    new_stdout = os.open("/dev/null", os.O_WRONLY, 0644)
    new_stderr = os.open("/tmp/error.log", os.O_WRONLY | os.O_CREAT, 0644)
    
    subproc = subprocess.Popen(command, stdout = new_stdout, stderr = new_stderr)
    retcode = subproc.wait()
    os.close(new_stdout)
    os.close(new_stderr)
    out_info = open("/tmp/error.log").read()
    os.remove("/tmp/error.log")
    return (retcode, out_info)

need_exit = False
def header_check(path, env, extra_includes, src_pathes):

    def visit(arg, dirname, names):
        global need_exit
        if need_exit:
            return

        #print arg
        for enabled_path in range(3 + arg[2], len(arg)):
            #print dirname, "==",  arg[1] + arg[enabled_path]
            if dirname == arg[1] + arg[enabled_path]:
                break
        else:
            return
        
#        name_com = re.compile(".\.(c|cc|cpp)$")
        for name in names:
#            ext_start = name.rfind(".")
            #print name
            ext = name[name.rfind("."):]
            #print ext
            if ext == ".c" or ext == ".cc" or ext == ".cpp":
                cmd = [arg[0], "-M"]
                for i in range(3, 3 + arg[2]):
                    cmd.append(arg[i])
                cmd.append(dirname + "/" + name)
                #print cmd
                (retcode, out_info) = run_cmd_silently(cmd)
                if retcode != 0:
                    print out_info
                    need_exit = True

    print "Start checking necessary header files..."
    #os.path.walk(path, visit, [env['CXX']])
    #os.path.walk("/home/artint/Projects/CPP/videocycle/fragmenter", visit, [env['CXX'], "-Ilibutility", "/home/artint/Projects/CPP/videocycle/fragmenter", "", "/libutility/utility/src"])
    args = [env['CXX'], path, len(extra_includes)]
    
    #print extra_includes
    for extra_include in extra_includes:
        args.append("-I"+extra_include)
        
    for src_path in src_pathes:
        args.append(src_path)
    #os.path.walk(path, visit, [env['CXX'], "-I" + extra_includes, path, "", "/libutility/utility/src"])
    os.path.walk(path, visit, args)
    if not need_exit:
        print "All necessary headers present"
        return True
    else: 
        return False

def compiler_check():
    for ac_prog in ["g++", "c++", "gpp", "aCC", "CC", "cxx" "cc++"]:# too exotic cl.exe FCC KCC RCC xlC_r xlC:
        (retcode, _) = run_cmd_silently(["which", ac_prog]) 
        if retcode == 0:
            print "Using compiler name ", ac_prog
            return ac_prog
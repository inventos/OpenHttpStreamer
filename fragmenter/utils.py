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

def run_cmd_silently(command, stdout):
    #command = [arg[0], "-M", arg[1], dirname + "/" + name]
    #command = arg[0] + " -M " + arg[1] + " " + dirname + "/" + name
    if stdout:
        stdout_name = "/tmp/error.log"
        stderr_name = "/dev/null"
    else:
        stdout_name = "/dev/null"
        stderr_name = "/tmp/error.log"
        
    new_stdout = os.open(stdout_name, os.O_WRONLY | os.O_CREAT, 0644)
    new_stderr = os.open(stderr_name, os.O_WRONLY | os.O_CREAT, 0644)
    
    subproc = subprocess.Popen(command, stdout = new_stdout, stderr = new_stderr)
    retcode = subproc.wait()
    os.close(new_stdout)
    os.close(new_stderr)
    out_info = open("/tmp/error.log").read()
    os.remove("/tmp/error.log")
    return (retcode, out_info)

need_exit = False
def header_check(path, env, extra_includes, src_pathes):

    print path
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

def ver_to_str(int_ver):
    return "%d.%d.%d" % (int_ver // 10000, int_ver // 100 % 100, int_ver % 100) 

def compiler_check():
    
    #Configure.CheckCXX()
    
    
    gcc_int_ver = 40300
    for ac_prog in ["g++", "c++", "gpp", "aCC", "CC", "cxx" "cc++"]:# too exotic cl.exe FCC KCC RCC xlC_r xlC:
        (retcode, _) = run_cmd_silently(["which", ac_prog], True) 
        if retcode == 0:
            print "Using compiler name ", ac_prog
            break
            
    (ret, out) = run_cmd_silently([ac_prog, "--version"], True)

    gcc_ver = re.search("\(GCC\) (\d*)\.(\d*)\.(\d*)", out)
    rsl_gcc_ver = 0
    if gcc_ver and len(gcc_ver.groups()) >= 3:
        for i in range(3):
            rsl_gcc_ver = rsl_gcc_ver * 100 + int(gcc_ver.group(i + 1))
    
    print rsl_gcc_ver, ":", gcc_int_ver
    if rsl_gcc_ver < gcc_int_ver:
        print "GCC version not satisfied. Need ", ver_to_str(gcc_int_ver), ", has ", ver_to_str(rsl_gcc_ver)
        return None
    else:
        print "GCC version satisfies"
        return ac_prog
        
def boost_ver_check(context):
    boost_int_ver = 13900
    ver_check = '''
    #include <boost/version.hpp>
    #include <iostream>  
    int main(void)
    {
        std::cout << BOOST_VERSION; 
    }
    '''
    
    print "Checking for BoostVer..."
    result = context.TryCompile(ver_check, '.cc')
    if result == 0:
        print "Can't find boost headers. Probably boost not installed."
        context.Result(0)
        return False
    
    (result, out) = context.TryRun(ver_check, '.cc')
    #print result, ":", out
    if result == 0:
        print "Unknown error."
        context.Result(0)
        return False
    
    #http://www.boost.org/doc/libs/1_39_0/libs/config/doc/html/boost_config/boost_macro_reference.html#boost_config.boost_macro_reference.boost_informational_macros
    
    boost_present_ver = int(out) // 100000 * 10000 + int(out) % 10000
    if boost_present_ver < boost_int_ver:
        print "Boost version not satisfied. Need ", ver_to_str(boost_int_ver), ", has ", ver_to_str(boost_present_ver)
        context.Result(0)
        return False

    print "Boost version satisfies"
    context.Result(1)
    return True
import os, re, os.path, sys, subprocess, pickle


#I don't like it -  procedure is quite complex for solved tasks
def suffix_check(env, boost_names):
    used_boost_names = boost_names[:]
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
        
    boost_mask = re.compile(r"libboost_(\w*)(\-[\w\-]*?)?\.so$")
    #boost_lib_suffix = ""
    
    #for boost_name in boost_names:
    boost_suffixes = {}
    for libdir in libpath:
        if not os.path.exists(libdir):
            break
        
        for name in os.listdir(libdir):
            boost_fnd = boost_mask.search(name)
            if not boost_fnd:
                continue
            
            boost_name_fnd = boost_fnd.group(1)
            #print name, ":", boost_name_fnd
            if not boost_name_fnd in used_boost_names:
                continue
            
            
            found_suffix = "" if not boost_fnd.group(2) else boost_fnd.group(2) 
            if not boost_suffixes.has_key(boost_name_fnd) or len(found_suffix) < len(boost_suffixes[boost_name_fnd]):
                boost_suffixes[boost_name_fnd] = str(found_suffix)
     
    for ind in range(len(boost_names)):
        boost_names[ind] += boost_suffixes[boost_names[ind]]               

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

def compiler_ver_check(context, compiler, version):
    print "Checking wether compiler version satisfies... ",
#def compiler_check(param):
    
    #Configure.CheckCXX()
    
    
#    gcc_int_ver = 40100
#    for ac_prog in ["g++", "c++", "gpp", "aCC", "CC", "cxx" "cc++"]:# too exotic cl.exe FCC KCC RCC xlC_r xlC:
#    if not param:
#        ac_prog = "g++"
#    else:
#        ac_prog = param
#    (retcode, _) = run_cmd_silently(["which", ac_prog], True) 
#    if retcode == 0:
#        print "Using compiler name ", ac_prog
##            break
#    else:
#        return None
#            
#    ac_prog = "g++"
    
    ver_reg = re.match("(\d)\.(\d*)\.(\d*)", version)
    if ver_reg:
        gcc_int_ver = int(ver_reg.group(1)) * 10000 + int(ver_reg.group(2)) * 100 + int(ver_reg.group(3)) 

    (ret, out) = run_cmd_silently([compiler, "--version"], True)

    gcc_ver = re.search(r"\(.*?\)\s*(\d*)\.(\d*)\.(\d*)", out)
    rsl_gcc_ver = 0
    if gcc_ver and len(gcc_ver.groups()) >= 3:
        for i in range(3):
            rsl_gcc_ver = rsl_gcc_ver * 100 + int(gcc_ver.group(i + 1))
    
    #print rsl_gcc_ver, ":", gcc_int_ver
    if rsl_gcc_ver < gcc_int_ver:
        context.Result(0)
        print "GCC version not satisfied. Need ", ver_to_str(gcc_int_ver), ", has ", ver_to_str(rsl_gcc_ver)
        return False
    else:
        #print "GCC version satisfies"
        context.Result(1)
        return True

        
def boost_ver_check(context, version):
    #boost_int_ver = 13900
    ver_reg = re.match("(\d)\.(\d*)\.(\d*)", version)
    if ver_reg:
        boost_int_ver = int(ver_reg.group(1)) * 10000 + int(ver_reg.group(2)) * 100 + int(ver_reg.group(3)) 
    
    #print boost_int_ver
    ver_check = '''
    #include <boost/version.hpp>
    #include <iostream>  
    int main(void)
    {
        std::cout << BOOST_VERSION; 
    }
    '''
    
    print "Checking wether boost version satisfies... ",
    result = context.TryCompile(ver_check, '.cc')
    if result == 0:
        context.Result(0)
        print "Can't find boost headers. Probably boost not installed."
        return False
    
    (result, out) = context.TryRun(ver_check, '.cc')
    #print result, ":", out
    if result == 0:
        context.Result(0)
        print "Unknown error."
        return False
    
    #http://www.boost.org/doc/libs/1_39_0/libs/config/doc/html/boost_config/boost_macro_reference.html#boost_config.boost_macro_reference.boost_informational_macros
    
    boost_present_ver = int(out) // 100000 * 10000 + int(out) % 10000
    if boost_present_ver < boost_int_ver:
        context.Result(0)
        print "Boost version not satisfied. Need ", ver_to_str(boost_int_ver), ", has ", ver_to_str(boost_present_ver)
        return False

    context.Result(1)
    #print "Boost version satisfies"
    
    return True

#code used from AutoconfRecepies : http://scons.org/wiki/AutoconfRecipes
# This code was tested on Intel, AMD 64, and Cell (Playstation 3)
# Python already knows what endian it is, just ask Python.
def checkEndian(context):
    context.Message("checking endianess ... ")
    import struct

    array = struct.pack('cccc', '\x01', '\x02', '\x03', '\x04')

    i = struct.unpack('i', array)

    # Little Endian
    if i == struct.unpack('<i', array):
        context.Result("little")
        return "little"

    # Big Endian
    elif i == struct.unpack('>i', array):
        context.Result("big")
        return "big"

    context.Result("unknown")
    return "unknown"

def lib_check(env, lib_list, checked_libs, chk_conf):
    chk_lib_list = lib_list[:]

    for checked_lib in checked_libs:
        if checked_lib in chk_lib_list:
            chk_lib_list.remove(checked_lib)
    
    #print chk_lib_list
    if chk_lib_list != []:
        print "Start checking necessary library files..."
        for lib in chk_lib_list:
            if not chk_conf.CheckLib(lib):
                print lib, " not found. Exiting"
                return False
             
        print "All necessary libraries present"
    
        checked_libs.extend(chk_lib_list)
    
    env = chk_conf.Finish()
    return True

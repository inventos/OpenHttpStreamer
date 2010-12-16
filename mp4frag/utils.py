import os, re, os.path, sys, subprocess, pickle


#I don't like it -  procedure is quite complex for solved tasks
def suffix_check(libpath, boost_names):
    boost_mask = re.compile(r"libboost_(\w*)(\-[\w\-]*?)?\.so$")
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
            if not boost_name_fnd in boost_names:
                continue
            
            if not boost_fnd.group(2):
                found_suffix = ""
            else:
                found_suffix =  boost_fnd.group(2) 
            if not boost_suffixes.has_key(boost_name_fnd) or len(found_suffix) < len(boost_suffixes[boost_name_fnd]):
                boost_suffixes[boost_name_fnd] = str(found_suffix)
     
    for ind in range(len(boost_names)):
        boost_names[ind] = "boost_" + boost_names[ind] + boost_suffixes[boost_names[ind]]               

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

def ver_to_str(int_ver):
    return "%d.%d.%d" % (int_ver // 10000, int_ver // 100 % 100, int_ver % 100) 

def compiler_ver_check(context, compiler, version):
    print "Checking wether compiler version satisfies... ",
   
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

def lib_check(chk_conf, lib_list):
    #print "Before start", lib_list
    print "Start checking necessary library files..."
    for lib in lib_list:
        if not chk_conf.CheckLib(lib):
        #if not context.CheckLib(lib):
            #context.Result(0)
            print lib, " not found. Exiting"
            return False
    
    #context.Result(1)
    print "All necessary libraries present"
    #print "Before exit", lib_list
    return True

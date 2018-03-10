#!/usr/bin/env python

import json
import sys, os
import re

def gen_api_errmsg_file(meta_file, msgfile_h, msgfile_c):

    try:
        fs_h = open(msgfile_h, "w")
        fs_h.write("#ifndef __AL_ERRMSG_H__\n")
        fs_h.write("#define __AL_ERRMSG_H__\n\n")
        fs_h.write("#ifdef __cplusplus\n")
        fs_h.write("extern \"C\" {\n")
        fs_h.write("#endif /* __cplusplus */\n\n")
        fs_h.write("enum{\n");
    except IOError, e:
        print e

    try:
        fs_c = open(msgfile_c, "w")
        fs_c.write("#include <stdio.h>\n")
        fs_c.write("#include <string.h>\n")
        fs_c.write("#include \"al_errmsg.h\"\n")
        fs_c.write("#include \"al_common.h\"\n\n")
        fs_c.write("char* al_errmsg[AL_END];\n\n")                   
        fs_c.write("static void CONSTRUCT_FUN al_error_init(void){\n")
    except IOError, e:
        print e
        
    with open(meta_file) as loadf:
        for line in loadf.readlines():
            val=re.split('=|\n', line)
            fs_h.write("    "+val[0]+"="+str(512+int(val[1]))+",\n")
            fs_c.write("    al_errmsg["+val[0]+"]="+"\""+val[2]+"\";\n")

    fs_h.write("    AL_END\n")
    fs_h.write("};\n")

    fs_h.write("extern const char* al_get_errmsg(int no);\n\n")
    fs_h.write("#ifdef __cplusplus\n")
    fs_h.write("}\n")
    fs_h.write("#endif /* __cplusplus */\n\n")

    fs_h.write("#endif")

    fs_c.write("};\n\n");

    fs_c.write("const char* al_get_errmsg(int no) {\n")
    fs_c.write("    if(no < AL_NO_ERROR){\n")
    fs_c.write("        return strerror(no);\n")
    fs_c.write("    }else{\n")
    fs_c.write("        return al_errmsg[no];\n")
    fs_c.write("    }\n")
    fs_c.write("}\n")    
 
    fs_c.close()
    fs_h.close()

gen_api_errmsg_file("../common/message.txt", "../include/al_errmsg.h", "../alpha/al_error.c");

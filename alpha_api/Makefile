OBJ=al_api.o \
         al_ether.o \
         db/sqlite3.o \
         db/al_db.o  \
         conf/al_kni_nic_conf.o\
         conf/al_load_group.o \
         conf/al_load_filter.o \
         conf/al_load_ip.o \
         conf/al_load_mac.o

CXXFLAGS = -c -g -std=c++11 -fPIC
CFLAGS = -c -g  -fPIC
INC = -I$(ROOT_PATH)/include -I$(ROOT_PATH)/alpha_api/db -I$(ROOT_PATH)/alpha_api
LD_LIBPATH = -L$(ROOT_PATH)/library
LD_LIBRARY=-lrestbed

TARGET = $(ROOT_PATH)/library/libalapi.so
TARGET1 = $(ROOT_PATH)/include/al_api.h
TARGET3 = api_test
TARGET4 = $(ROOT_PATH)/include/al_to_main.h

all:  $(TARGET) 

$(TARGET):  $(OBJ)
	$(CXX) -shared $(OBJ) $(LD_LIBPATH) $(LD_LIBRARY) -o $@
	@cp -f ../library/* /usr/lib64/

$(TARGET3): test/main.o
	$(CC) $< -o $@ $(LD_LIBRARY) $(LD_LIBPATH) -ldl -lalapi

%.o:%.cpp
	$(CXX) $(INC) $(CXXFLAGS) $< -o $@

%.o:%.c
	$(CC) $(INC) $(CFLAGS) $< -o $@

clean:
	-$(RM) $(OBJ) $(TARGET) $(TARGET1) $(TARGET2)


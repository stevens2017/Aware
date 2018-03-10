OBJ =  alpha_gen.o \
            ../alpha_api/db/al_db.o \
            ../alpha_api/db/sqlite3.o

CFLAGS = -c -g 
INC = -I./  -I../alpha_api/db -I../ -I../include
LD_LIB = -lm -ldl -lpthread

TARGET = alpha_gen

all:  $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) $(LD_LIB) -o $@

%.o:%.cpp
	$(CXX) $(INC) $(CFLAGS) -std=c++11 $< -o $@

clean:
	-$(RM) $(OBJ) $(TARGET)



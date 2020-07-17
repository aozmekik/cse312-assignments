CXX = g++
CXXFLAGS = -std=c++11 
LDFLAGS =  -std=c++11 

SRC = part3.cpp
OBJ = $(SRC:.cc=.o)
EXEC = part3

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(OBJ) $(LBLIBS)

clean:
	rm -rf $(OBJ) $(EXEC)
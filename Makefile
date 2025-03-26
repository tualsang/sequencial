# Define the compiler and linker
CC=g++
LD=g++

# Define the include paths
CXXFLAGS=-I"C:\Users\tuals\Downloads\curl\include" -I"C:\Users\tuals\Downloads\rapidjson-master\include"

# Define the library paths
LDFLAGS=-L"C:\Users\tuals\Downloads\curl\lib" -lcurl

# Define the default target
all: level_client

# Define the target for level_client
level_client: level_client.o
    $(LD) $< -o $@ $(LDFLAGS)

# Define the compilation rule for level_client.o
level_client.o: level_client.cpp
    $(CC) $(CXXFLAGS) -c $< -o $@

# Define the clean target
clean:
    -rm level_client level_client.o
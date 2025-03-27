CC=g++
LD=g++
CXXFLAGS=-IC:/rapidjson/include
LDFLAGS=-lcurl

all: level_client

level_client: level_client.cpp
    $(LD) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
    rm -f level_client

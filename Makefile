CC=g++
LD=g++
CXXFLAGS=-IC:/Users/tuals/Downloads/curl/include -IC:/Users/tuals/Downloads/rapidjson-master/include
LDFLAGS=-LC:/Users/tuals/Downloads/curl/lib -lcurl

all: level_client

level_client: level_client.cpp
    $(LD) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
    rm -f level_client
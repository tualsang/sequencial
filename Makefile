CC=g++
LD=g++
CURL_INC=-IC:\Users\\tuals\\Downloads\\curl\\include
RAPIDJSON_INC=-IC:\Users\\tuals\\Downloads\\rapidjson-master\\include
CURL_LIB=-LC:\Users\\tuals\\Downloads\\curl\\lib
LDFLAGS=-lcurl
CXXFLAGS=$(CURL_INC) $(RAPIDJSON_INC)

all: level_client par_level_client

level_client: level_client.cpp
    $(LD) $(CXXFLAGS) $< -o $@ $(CURL_LIB) $(LDFLAGS)

par_level_client: par_level_client.cpp
    $(LD) $(CXXFLAGS) $< -o $@ $(CURL_LIB) $(LDFLAGS)

clean:
    -rm level_client par_level_client
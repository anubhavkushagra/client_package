g++ -O3 -std=c++17 remote_load_client.cpp build/kv.pb.cc build/kv.grpc.pb.cc \
    -I/usr/local/include -I./build \
    -L/usr/local/lib -lgrpc++ -lprotobuf -lpthread -ldl \
    -o remote_load_client


./remote_load_client 10000

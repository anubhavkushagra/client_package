g++ -O3 -std=c++17 remote_load_client.cpp build/kv.pb.cc build/kv.grpc.pb.cc \
    -I/usr/local/include -I./build \
    -L/usr/local/lib -lgrpc++ -lprotobuf -lpthread -ldl \
    -o remote_load_client


./remote_load_client 10000




To restart your full system (Storage + 3 API Nodes + HAProxy), run these commands in order:

1. Start the Storage Node
bash
./storage_node > /dev/null 2>&1 &
2. Start the API Nodes (on 3 different ports)
bash
./api_node_haproxy 50063 > /dev/null 2>&1 &
./api_node_haproxy 50064 > /dev/null 2>&1 &
./api_node_haproxy 50065 > /dev/null 2>&1 &
3. Start the HAProxy Load Balancer
bash
haproxy -f haproxy_lb/lb/haproxy.cfg &
To verify everything is running:
You can check the process list again with:

bash
ps aux | grep -E "storage_node|api_node_haproxy|haproxy"
And then you can run your client to test it:

bash
./load_client_haproxy 200

#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <grpcpp/grpcpp.h>
#include <fstream>
#include "kv.grpc.pb.h"

std::string ReadFile(const std::string &filename) {
  std::ifstream ifs(filename);
  if (!ifs.is_open()) {
    std::cerr << "CRITICAL ERROR: Failed to open " << filename 
              << ". Make sure server.crt is in the current directory." << std::endl;
    exit(1);
  }
  return std::string((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cout << "Usage: ./remote_load_client <THREADS>" << std::endl;
    return 0;
  }

  int threads = std::stoi(argv[1]);
  if (threads <= 0) {
    std::cout << "Threads must be between 1 and 5000" << std::endl;
    std::cout << "Usage: ./remote_load_client <THREADS>" << std::endl;
    return 0;
  }

  // NOTE: Change this target to the IP of the HAProxy machine!
  std::string target = "192.168.0.109:50060";
  
  // Hardcoded for testing, usually retrieved dynamically
  std::string jwt = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJjbGllbnRfaWQiOiJjbGllbn"
                    "QtMSIsImlzcyI6ImVzY3Jvdy1zdGFjayJ9.6rSg25u53rD-D"
                    "Jk0gM62V79rL8O110c9hZ2672I2_E0";

  // === Create Stub Pool with SSL ===
  int stub_count = 64; 
  std::vector<std::unique_ptr<kv::KVService::Stub>> stub_pool;

  grpc::SslCredentialsOptions ssl_opts;
  ssl_opts.pem_root_certs = ReadFile("server.crt");

  for(int i = 0; i < stub_count; i++) {
      grpc::ChannelArguments pool_args;
      pool_args.SetLoadBalancingPolicyName("round_robin");
      pool_args.SetSslTargetNameOverride("localhost");
      pool_args.SetInt("grpc.channel_id", i);
      
      auto chan = grpc::CreateCustomChannel("ipv4:" + target, grpc::SslCredentials(ssl_opts), pool_args);
      stub_pool.push_back(kv::KVService::NewStub(chan));
  }

  std::cout << "Pre-generating keys for remote laptop (starting at 1,000,000)..." << std::endl;
  std::vector<std::string> pregen_keys;
  for (int i = 0; i < 100000; i++) {
    // CRITICAL: Offset keys by 1,000,000 so they don't collide with the first laptop's keys
    pregen_keys.push_back("key_" + std::to_string(i + 1000000));
  }

  long TOTAL_REQUESTS = 200000;
  long reqs_per_thread = TOTAL_REQUESTS / threads;

  std::atomic<long> successful_requests{0};
  std::atomic<long> failed_requests{0};
  std::atomic<long> total_latency_us{0};

  std::cout << "Target: " << TOTAL_REQUESTS << " across " << threads << " threads." << std::endl;
  std::cout << "Starting benchmark..." << std::endl;

  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> pool;
  for (int i = 0; i < threads; i++) {
    pool.emplace_back([&, i]() {
      long local_success = 0;
      long local_failed = 0;
      long local_latency = 0;

      auto& stub = stub_pool[i % stub_pool.size()];
      unsigned int key_idx = (pregen_keys.size() / threads) * i;
      
      for (long j = 0; j < reqs_per_thread; j++) {
        grpc::ClientContext call_ctx;
        call_ctx.AddMetadata("authorization", jwt);
        kv::SingleRequest req;
        req.set_type(kv::PUT);
        
        req.set_key(pregen_keys[(key_idx++) % pregen_keys.size()]);
        req.set_value("val");
        
        kv::SingleResponse resp;

        auto req_start = std::chrono::high_resolution_clock::now();
        grpc::Status status = stub->ExecuteSingle(&call_ctx, req, &resp);
        auto req_end = std::chrono::high_resolution_clock::now();

        if (status.ok()) {
          local_success++;
          local_latency += std::chrono::duration_cast<std::chrono::microseconds>(req_end - req_start).count();
        } else {
          local_failed++;
        }
      }
      successful_requests += local_success;
      failed_requests += local_failed;
      total_latency_us += local_latency;
    });
  }
  
  for (auto &t : pool) t.join();

  auto end_time = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end_time - start_time;

  double rps = successful_requests / elapsed.count();
  double avg_latency = successful_requests > 0 ? (double)total_latency_us / successful_requests / 1000.0 : 0;

  std::cout << "\n--- Benchmark Results ---" << std::endl;
  std::cout << "Total Requests:  " << (successful_requests + failed_requests) << std::endl;
  std::cout << "Successful:      " << successful_requests << std::endl;
  std::cout << "Dropped/Failed:  " << failed_requests << std::endl;
  std::cout << "Total Time:      " << elapsed.count() << " seconds" << std::endl;
  std::cout << "Throughput:      " << rps << " requests/sec" << std::endl;
  std::cout << "Avg Latency:     " << avg_latency << " ms" << std::endl;
  std::cout << "-------------------------" << std::endl;

  return 0;
}
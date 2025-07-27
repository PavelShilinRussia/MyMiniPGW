#ifndef PGW_SERVER_CONFIG_H
#define PGW_SERVER_CONFIG_H


#include <cstdint>
#include <iostream>
#include <vector>



struct pgw_server_config
{

  std::string udp_ip;
  uint32_t udp_port;
  uint32_t session_timeout_sec;
  std::string cdr_file;
  uint32_t http_port;
  uint32_t graceful_shutdown_rate;
  std::string log_file;
  std::string log_level;
  std::vector<std::string> blacklist;

};


#endif
#ifndef PGW_CLIENT_CONFIG_H
#define PGW_CLIENT_CONFIG_H
#include <cstdint>
#include <string>



struct pgw_client_config
{
  std::string server_ip;
  uint32_t server_port;
  std::string log_file;
  std::string log_level;
};


#endif
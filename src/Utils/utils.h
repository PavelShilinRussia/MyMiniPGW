#include "../Configs/pgw_client_config.h"
#include "../Configs/pgw_server_config.h"
#include <vector>
#include <string>

std::vector<uint8_t> encode_bcd(const std::string& imsi);

std::string decode_bcd(const std::vector<uint8_t>& bcd);

pgw_server_config load_pgw_server_config(const std::string& config_path);

pgw_client_config load_pgw_client_config(const std::string& config_path);

bool validate_pgw_client_config(const pgw_client_config& config);

bool validate_pgw_server_config(const pgw_server_config& config);
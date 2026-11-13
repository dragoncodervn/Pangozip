#pragma once

void print_usage(const char *prog);
int cmd_create(const std::vector<std::string> &args);
static double ratio_percent(uint64_t size, uint64_t comp);
int cmd_list(const std::vector<std::string>& args);
int cmd_extract(const std::vector<std::string>& args);
int cmd_version(const std::vector<std::string>& args);

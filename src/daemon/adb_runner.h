#pragma once
#include <string>

std::string ExtractEmbeddedAdb();
std::string FindAdb();
std::string ExecuteAdb(std::string cmd);
void RunInjection();

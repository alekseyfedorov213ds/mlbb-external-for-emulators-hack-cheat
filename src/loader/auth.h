#pragma once
#include <string>

std::string GetHwid();
void InitKeyAuthCreds();
std::string UrlEncode(const std::string& s);
std::string SendRequest(std::string params);
std::string GetJsonValue(std::string json, std::string key);
void InitKeyAuth();

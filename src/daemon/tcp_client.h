#pragma once

void NetworkThread();
bool IsPortOpen(const char* host, int port, int timeoutMs);

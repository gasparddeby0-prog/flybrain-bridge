#pragma once
// UDP local : envoi vers 127.0.0.1:47800, réception sur 127.0.0.1:47801 (non bloquant)
bool net_init();
void net_send(const char* data, int len);
int net_recv(char* buf, int cap);

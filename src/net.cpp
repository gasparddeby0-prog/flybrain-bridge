// Réseau UDP isolé du reste : Geode inclut déjà windows.h (et l'ancien winsock.h),
// ce fichier est compilé sans l'en-tête précompilé pour pouvoir utiliser winsock2.
#include <winsock2.h>
#include <ws2tcpip.h>
#include "net.hpp"

namespace {
    SOCKET g_sock = INVALID_SOCKET;
    sockaddr_in g_dst{};
}

bool net_init() {
    if (g_sock != INVALID_SOCKET) return true;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_sock == INVALID_SOCKET) return false;
    sockaddr_in me{};
    me.sin_family = AF_INET;
    me.sin_port = htons(47801);
    me.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(g_sock, reinterpret_cast<sockaddr*>(&me), sizeof(me));
    u_long nonBlocking = 1;
    ioctlsocket(g_sock, FIONBIO, &nonBlocking);
    g_dst = me;
    g_dst.sin_port = htons(47800);
    return true;
}

void net_send(const char* data, int len) {
    if (g_sock == INVALID_SOCKET) return;
    sendto(g_sock, data, len, 0, reinterpret_cast<sockaddr*>(&g_dst), sizeof(g_dst));
}

int net_recv(char* buf, int cap) {
    if (g_sock == INVALID_SOCKET) return -1;
    return recv(g_sock, buf, cap, 0);
}

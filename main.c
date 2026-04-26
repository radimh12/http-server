#include "http_server.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PROGRAM_NAME "http_server"
#define DEFAULT_HOST "localhost"
#define DEFAULT_PORT "1202"
#define BACKLOG 10
#define MESSAGE_MAX_SIZE 4096

struct ip_address {
  uint8_t version;
  char str[INET6_ADDRSTRLEN];
};

static const struct addrinfo addrinfo_hints = {
  .ai_family = AF_UNSPEC,
  .ai_socktype = SOCK_STREAM,
  .ai_flags = AI_PASSIVE,
};

static char message_buffer[MESSAGE_MAX_SIZE];

static void get_ip_address(struct sockaddr *sockaddr, struct ip_address *ip) {
  void *src;
  int family = sockaddr->sa_family;

  if (family == AF_INET) {
    auto addr = (struct sockaddr_in *)(sockaddr);
    src = &(addr->sin_addr);
    ip->version = 4;
  } else {
    auto addr = (struct sockaddr_in6 *)(sockaddr);
    src = &(addr->sin6_addr);
    ip->version = 6;
  }

  if (inet_ntop(family, src, ip->str, sizeof ip->str) == NULL) {
    perror("error: inet_ntop");
    ip->str[0] = 0;
  }
}

int main(void) {
  struct addrinfo *addrinfo_list;
  int res;

  if ((res = getaddrinfo(DEFAULT_HOST, DEFAULT_PORT, &addrinfo_hints, &addrinfo_list)) != 0) {
    fprintf(stderr, "error: getaddrinfo: %s\n", gai_strerror(res));
    return 1;
  }

  int server_sockfd = -1;
  struct ip_address server_ip;

  for (struct addrinfo *addrinfo = addrinfo_list; addrinfo; addrinfo = addrinfo->ai_next) {
    int sockfd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    if (sockfd == -1) {
      perror("error: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) != 0) {
      perror("error: setsockopt");
      return 1;
    }

    if (bind(sockfd, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0) {
      close(sockfd);
      perror("error: bind");
      continue;
    }

    get_ip_address(addrinfo->ai_addr, &server_ip);

    server_sockfd = sockfd;
    break;
  }

  if (server_sockfd == -1) {
    fprintf(stderr, "error: failed to bind socket\n");
    return 1;
  }

  freeaddrinfo(addrinfo_list);

  if (listen(server_sockfd, BACKLOG) != 0) {
    perror("error: listen");
    return 1;
  }

  printf("Listening on %s (port = %s, IPv%d, socket = %d)\n", server_ip.str, DEFAULT_PORT, server_ip.version,
         server_sockfd);

  for (;;) {
    struct sockaddr_storage addr;
    socklen_t addr_size = sizeof addr;

    int sockfd = accept(server_sockfd, (struct sockaddr *)(&addr), &addr_size);
    if (sockfd == -1) {
      perror("error: accept");
      continue;
    }

    struct ip_address ip;
    get_ip_address((struct sockaddr *)(&addr), &ip);
    printf("Accepted connection from %s\n", ip.str);

    // message size is limited to buffer size - 1 so that the string is always zero-terminated
    ssize_t message_size = recv(sockfd, message_buffer, sizeof message_buffer - 1, 0);
    if (message_size == -1)
      perror("error: recv");

    close(sockfd);

    struct string message = {.data = message_buffer, .size = message_size};
    message_parse(message);
  }

  return 0;
}

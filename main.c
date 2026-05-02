#include "http_server.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PROGRAM_NAME "http_server"
#define DEFAULT_HOST "localhost"
#define DEFAULT_PORT "1202"
#define BACKLOG 10
#define MESSAGE_MAX_SIZE 8192

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

static const char index_html[] = {
#embed "resources/index.html"
};

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

static void send_message(int sockfd, const void *data, size_t size) {
  ssize_t res = send(sockfd, data, size, 0);
  if (res == -1) perror("error: send");
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

    printf("--------------------------------------------------\n");
    printf("Accepted connection from %s\n", ip.str);

    ssize_t request_size = recv(sockfd, message_buffer, sizeof message_buffer, 0);
    if (request_size == -1) perror("error: recv");

    // message size is limited to buffer size - 1 so that the string is always zero-terminated
    if (request_size == MESSAGE_MAX_SIZE) {
      // read everything that is left in the socket
      do {
        request_size = recv(sockfd, message_buffer, sizeof message_buffer, MSG_DONTWAIT);
        printf(">> %ld\n", request_size);
      } while (request_size > 0);

      fprintf(stderr, "error: recv: exceeded max message size\n");
      char response[] = "HTTP/1.1 501\r\n\r\n";
      send_message(sockfd, response, sizeof response - 1);
      goto close;
    }

    struct string request = {.data = message_buffer, .size = request_size};
    struct message message;

    if (message_parse(request, &message)) {
      printf("method: %.*s\n", message.method.size, message.method.data);
      printf("http version: %u.%u\n", message.version.major, message.version.minor);
      printf("request target: %.*s\n", message.request_target.path.size, message.request_target.path.data);
      printf("query: %.*s\n", message.request_target.query.size, message.request_target.query.data);

      if (equals(message.request_target.path, str("/"))) {
        char response[] = "HTTP/1.1 200\r\n\r\n";
        send_message(sockfd, response, sizeof response - 1);
        send_message(sockfd, index_html, sizeof index_html);
      } else {
        char response[] = "HTTP/1.1 404\r\n\r\n";
        send_message(sockfd, response, sizeof response - 1);
      }
    } else {
      fprintf(stderr, "failed to parse request message\n");
      char response[] = "HTTP/1.1 501\r\n\r\n";
      send_message(sockfd, response, sizeof response - 1);
    }

  close:
    close(sockfd);
  }

  return 0;
}

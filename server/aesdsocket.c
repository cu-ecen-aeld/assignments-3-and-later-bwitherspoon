#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#define PATH "/var/tmp/aesdsocketdata"
#define PORT "9000"
#define BACKLOG 5
#define INITIAL_BUFFER_CAPACITY 8192

static bool running = false;

static void signal_handler(int signo) {
  if (signo == SIGINT || signo == SIGTERM) {
    printf("Caught signal, exiting\n");
    syslog(LOG_INFO, "Caught signal, exiting");
    running = false;
  } else {
    fprintf(stderr, "Unexpected signal\n");
  }
}

static void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
  int opt;
  bool daemon = false;
  pid_t pid;
  struct addrinfo *servinfo;
  int servfd;
  int peerfd;
  int filefd;
  struct sockaddr_storage peer_addr;
  socklen_t peer_addr_size;
  char peer_addr_str[INET6_ADDRSTRLEN];
  int status = 0;
  char *buffer;
  size_t buffer_index = 0;
  size_t buffer_capacity = INITIAL_BUFFER_CAPACITY;
  ssize_t count;

  while ((opt = getopt(argc, argv, "d")) != -1) {
    switch (opt) {
      case 'd':
        daemon = true;
        break;
      case '?':
        return -1;
    }
  }

  openlog(NULL, 0, LOG_USER);

  servfd = socket(PF_INET, SOCK_STREAM, 0);
  if (servfd == -1) {
    perror("socket");
    return -1;
  }

  struct addrinfo hints = {0};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    return -1;
  }

  {
    int option = 1;
    struct addrinfo *ptr;
    for (ptr = servinfo; ptr != NULL; ptr = ptr->ai_next) {
      servfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
      if (servfd == -1) {
        continue;
      }
      setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

      if (bind(servfd, ptr->ai_addr, ptr->ai_addrlen) == 0) {
        break;
      }

      close(servfd);
    }

    freeaddrinfo(servinfo);

    if (ptr == NULL) {
      fprintf(stderr, "Could not bind\n");
      return -1;
    }
  }

  if (daemon) {
    pid = fork();
    if (pid != 0) {
      exit(0);
    } else if (pid == -1){
      perror("fork");
      return -1;
    }

    if (setsid() == -1) {
      perror("setsid");
      return -1;
    }

    if (chdir("/") == -1) {
      perror("setsid");
      return -1;
    }

    for (int fd = 0; fd < 3; fd++) {
      close(fd);
    }

    open("/dev/null", O_RDWR);
    dup(0);
    dup(0);
  }

  running = true;

  struct sigaction act = {0};
  act.sa_handler = signal_handler;

  if (sigaction(SIGINT, &act, NULL) == -1) {
    perror("sigaction");
    return -1;
  }

  if (sigaction(SIGTERM, &act, NULL) == -1) {
    perror("sigaction");
    return -1;
  }

  status = listen(servfd, BACKLOG);
  if (status == -1) {
    perror("listen");
    return -1;
  }

  buffer = malloc(buffer_capacity);
  if (buffer == NULL) {
    perror("malloc");
    return -1;
  }

  while (running) {
    peer_addr_size = sizeof peer_addr;
    peerfd = accept(servfd, (struct sockaddr *)&peer_addr, &peer_addr_size);
    if (peerfd == -1) {
      if (errno != EINTR) {
        status = -1;
      }
      break;
    }

    inet_ntop(peer_addr.ss_family, get_in_addr((struct sockaddr *)&peer_addr),
              peer_addr_str, sizeof peer_addr_str);

    syslog(LOG_INFO, "Accepted connection from %s", peer_addr_str);
    printf("Accepted connection from %s\n", peer_addr_str);

    filefd = open(PATH, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (filefd == -1) {
      perror("open");
      status = -1;
      break;
    }

    for (bool delimiter = false; !delimiter;) {
      count =
          recv(peerfd, &buffer[buffer_index], buffer_capacity - buffer_index, 0);
      if (count == -1) {
        if (errno != EINTR) {
          perror("recv");
        }
        break;
      }
      if (count == 0) {
        break;
      }

      const size_t buffer_end =  buffer_index + count;
      for (size_t i = buffer_index; i < buffer_end; i++) {
        buffer_index++;
        if (buffer[i] == '\n') {
          delimiter = true;
          break;
        }
      }

      if (!delimiter) {
        if (buffer_index == buffer_capacity) {
          buffer = realloc(buffer, 2 * buffer_capacity);
          if (buffer == NULL) {
            perror("realloc");
            break;
          }
          buffer_capacity = 2 * buffer_capacity;
        }
      } else {
        if (write(filefd, buffer, buffer_index) == -1) {
          perror("write");
          break;
        }

        buffer_index = 0;

        if (lseek(filefd, 0, SEEK_SET) == -1) {
          perror("lseek");
          break;
        }

        do {
          count = read(filefd, buffer, buffer_capacity);
          if (count == -1) {
            perror("read");
            break;
          }
          if (send(peerfd, buffer, count, 0) == -1) {
            perror("send");
            break;
          }
        } while (count != 0);
      }
    }

    buffer_index = 0;

    close(peerfd);
    close(filefd);

    syslog(LOG_INFO, "Closed connection from %s", peer_addr_str);
    printf("Closed connection from %s\n", peer_addr_str);
  }

  unlink(PATH);

  close(servfd);

  free(buffer);

  return status;
}

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/io.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <math.h>
#include <ctype.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>
#include "vec.h"

#define VERSION (1)
#define CHORD_TIME (10000)

int singleSound = 1;

int vec_expand_(char **data, int *length, int *capacity, int memsz) {
  if (*length + 1 > *capacity) {
    void *ptr;
    int n = (*capacity == 0) ? 1 : *capacity << 1;
    ptr = realloc(*data, n * memsz);
    if (ptr == NULL) return -1;
    *data = ptr;
    *capacity = n;
  }
  return 0;
}
void vec_splice_(char **data, int *length, int *capacity, int memsz,
                 int start, int count
) {
  (void) capacity;
  memmove(*data + start * memsz,
          *data + (start + count) * memsz,
          (*length - start - count) * memsz);
}

static double noteToFreq(int note) {
    double a = 440;
    return (a / 32) * pow(2, ((note - 9) / 12.0));
}

static void printHelp(void)
{
    printf("usage: midibeep [port]\n");
}

static void beep_on(void)
{
    outb(inb(0x61)|3, 0x61);
}

static void beep_off(void)
{
    outb(inb(0x61)&0xfc, 0x61);
}

static void set_freq(double freq)
{
    uint32_t count;

    // Calculate freq
    count = 1193180 / freq;

    outb(0xb6, 0x43);
    outb(count & 0xff, 0x42);
    outb((count>>8) & 0xff, 0x42);
}

void signalHandler(int p_signame)
{
    outb(inb(0x61)&0xfc, 0x61);
    exit(0);
    signalSet(p_signame);
}

void signalSet(int p_signame)
{
    if (signal(p_signame, signalHandler) == SIG_ERR) {
        printf("Error: Signal set error");
        exit(1);
    }
}

void *chord_thread(vec_double_t *chord_ary)
{
    while (1) {
        if (singleSound == 0) {
            int i;
            double *val;
            vec_foreach_ptr(chord_ary, val, i) {
                usleep(CHORD_TIME);
                set_freq(*val);
            }
        } else {
            usleep(CHORD_TIME);
        }
    }
    return 0;
}

void udp_send(int sock, const char *data, struct sockaddr *addr) {
    if (sendto(sock, data, sizeof(data), 0, addr, sizeof(*addr)) != sizeof(data)) {
        perror("sendto() failed.");
        return -1;
    }
    return 0;
}


int main(int argc, char *argv[])
{
    if (argc != 2) {
        printHelp();
        return 1;
    }

    int isPlaying = 0;
    int playingNote = 0;
    pthread_t chord_thread_id;
    vec_double_t chord_ary;
    vec_init(&chord_ary);

    signalSet(SIGINT);
    if ((ioperm(0x0040, 4, 1)) || (ioperm(0x0061, 1, 1))) {
        perror("ioperm");
        return 2;
    }

    int sock;
    struct sockaddr_in addr;
    struct sockaddr_in cl_addr = {0};
    unsigned int cl_addr_len = 0;

    char buf[2048];

    sock = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(50000);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr *)&addr, sizeof(addr));

    pthread_create(&chord_thread_id, NULL, &chord_thread, &chord_ary);

    // printf("ip address: %s\n", inet_ntoa(cl_addr.sin_addr));

    while (1) {
        memset(buf, 0, sizeof(buf));
        recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&cl_addr, &cl_addr_len);

        if (buf[0] == '\0') {
            vec_push(&chord_ary, noteToFreq(buf[1]));
            if (chord_ary.length == 1) {
                set_freq(noteToFreq(buf[1]));
                beep_on();
                singleSound = 1;
            } else {
                singleSound = 0;
            }
        } else if (buf[0] == '\1') {
            vec_remove(&chord_ary, noteToFreq(buf[1]));
            if (chord_ary.length == 0) {
                beep_off();
                singleSound = 0;
            } else if (chord_ary.length == 1) {
                singleSound = 1;
            }
        } else if (buf[0] == '\2') {
            vec_clear(&chord_ary);
            beep_off();
        }
    }

    beep_off();
    close(socket);

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include <arpa/inet.h>

#include <sys/socket.h>

#define PACKET_SIZE 64

#define MISSING_ADDRESS 1
#define INVALID_ADDRESS 2
#define SOCKET_NAME_ERR 3
#define SOCKET_CREATION 4
#define PROGRAM_OUT_MEM 5

unsigned long get_timestamp_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (unsigned long) (ts.tv_sec * 1000000000LL + ts.tv_nsec);
}

static volatile
    unsigned int seq = 1;

static volatile 
    unsigned char exiting = 1;

static void sig_int(int signo) {
	exiting = 0;
}

struct thread_args {
    int                 fd;
    unsigned char      *pkt;
    struct sockaddr_in  dst;
    char               *add;  
};

unsigned long 
ntohll(unsigned long val) {
    // Verifica se il sistema è little-endian
    static const int num = 42;
    if (*(char *)&num == 42) {
        unsigned int high = htonl((unsigned int)(val >> 32));
        unsigned int low  = htonl((unsigned int)val);
        return ((unsigned long)low << 32) | high;
    }
    return val;
}

unsigned long 
htonll(unsigned long val) {
    // Verifica se il sistema è little-endian
    static const int num = 42;
    if (*(char *)&num == 42) {
        unsigned int high = htonl((unsigned int)(val >> 32));
        unsigned int low  = htonl((unsigned int)val);
        return ((unsigned long)low << 32) | high;
    }
    return val;
}

static unsigned short 
checksum(void *pkt, unsigned int size) {

    int i;
    unsigned int   sum    = 0;
    unsigned char* buffer = (unsigned char*) pkt;

    /**
     * Per calcolare il checksum, si utilizza questa
     * strategia: si suddivide la PDU ICMP in sequenze
     * da 16 bit ciascuna. Le sequenze vengono sommate
     * ma ad ogni somma, occorre rappresentare solo
     * gli ultimi 16 bit del risultato. Alla fine,
     * ogni bit deve essere invertito: complementazione
     * del risultato ad 1.
    */

    for (i = 0; i < size; i += 2) {
        unsigned char  b1 = (unsigned char) buffer[i];
        unsigned char  b2 = (unsigned char) buffer[i + 1];
        unsigned short wd = (b1 << 8) | b2;
        sum = sum + wd;
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (unsigned short) htons(~sum & 0xFFFF);
}

#define TIMESTAMP_SIZE sizeof(unsigned long)

static void 
fill_icmp_request(
    unsigned char* pkt) {

    struct icmphdr* icmp;

    unsigned long timestamp = get_timestamp_ns();
    timestamp = htonll(timestamp);

    memset(pkt, 0x00, PACKET_SIZE + sizeof(struct icmphdr));
    memcpy(pkt + sizeof(struct icmphdr), &timestamp, TIMESTAMP_SIZE);
    memset(pkt + sizeof(struct icmphdr) + TIMESTAMP_SIZE, 0xFF, PACKET_SIZE - TIMESTAMP_SIZE);

    /**
     * La funzione genera un pacchetto di tipo
     * ICMP ECHO REQUEST con un payload di 64 
     * bytes. I primi 8 byte contengono il
     * riferimento temporale che serve per
     * calcolare il RTT.
    */

    icmp = (struct icmphdr*) pkt;
    icmp->type       = ICMP_ECHO;
    icmp->code       = 0;
    icmp->checksum   = 0;
    icmp->un.echo.id = getpid();
    icmp->un.echo.sequence = seq++;

    icmp->checksum = checksum(pkt, PACKET_SIZE + sizeof(struct icmphdr));
    return;
}

static void 
fill_icmp_reply(
    unsigned char* pkt,
    unsigned int   psz,
    char*          add) {

    struct icmphdr* icmp;
    struct iphdr*   ipv4;

    unsigned long  now  = get_timestamp_ns();
    unsigned long  past;
    double         rtt;

    ipv4 = (struct iphdr*)   pkt;
    icmp = (struct icmphdr*) (pkt + (ipv4->ihl * 4));

    if (icmp->type == ICMP_ECHOREPLY) {
        /**
         * Verifica che il messaggio ricevuto sia
         * ICMP ECHO REPLY
        */
        if (psz >= (sizeof(struct icmphdr) + sizeof(struct iphdr) + TIMESTAMP_SIZE)) {
            /**
             * Verifica che il messaggio sia destinato al processo
             * corrente.
            */
            if(getpid() == icmp->un.echo.id) {
                memcpy(&past, pkt + (ipv4->ihl * 4) + sizeof(struct icmphdr), TIMESTAMP_SIZE);
                past = ntohll(past);
                rtt  = (double) (now - past) / 1000000.0;
                /**
                 * Dopo aver calcolato il RTT, stampa le statistiche
                 * a video. Il RTT è calcolato in ms, sottraendo dal
                 * timestamp attuale quello contenuto nel pacchetto.
                 * Entrambe le quantità sono espresse in nanosecondi.
                */
                printf("ping: %d bytes with seq = %d from %s, ttl = %d, rtt = %.2f ms\n",
                       psz - (sizeof(struct icmphdr) + sizeof(struct iphdr)),
                       icmp->un.echo.sequence,
                       add,
                       ipv4->ttl,
                       rtt);
            }
        }
    }
    return;
}

void*
sender(void* args) {

    int nbytes;
    int rbytes;

    struct thread_args *params;
    struct icmphdr*     icmp;
    struct iphdr*       ipv4;

    socklen_t len = sizeof(struct sockaddr);

    params = (struct thread_args*) args;
    nbytes =  PACKET_SIZE + sizeof(struct icmphdr);

    while (exiting) {
        fill_icmp_request(params->pkt);
        rbytes = sendto(params->fd, params->pkt, nbytes, 0, (struct sockaddr *)&params->dst, sizeof(struct sockaddr));
        fflush(stdout);
        sleep(1);
    }
    pthread_exit(NULL);
}

void*
receiver(void* args) {

    int nbytes;
    int rbytes;

    struct thread_args *params;
    struct icmphdr*     icmp;
    struct iphdr*       ipv4;

    socklen_t len = sizeof(struct sockaddr);

    params = (struct thread_args*) args;
    nbytes =  PACKET_SIZE + sizeof(struct icmphdr) + sizeof(struct iphdr);

    while (exiting) {
        memset(params->pkt, 0x00, nbytes);
        rbytes = recvfrom(params->fd, params->pkt, nbytes, 0, (struct sockaddr *)&params->dst, &len);
        if(rbytes == nbytes) {
            fill_icmp_reply(params->pkt, rbytes, params->add);
            fflush(stdout);
        }
    }
    pthread_exit(NULL);
}



int
main(int argc, char** argv) {

    int err;
    int fd;

    pthread_t tx;
    pthread_t rx;

    struct thread_args tx_args;
    struct thread_args rx_args;

    unsigned char*  out_pkt = NULL;
    unsigned char*  in_pkt = NULL;

    struct sockaddr_in src;
    struct sockaddr_in dst;

    if(argc != 2) {
        printf("ping: missing target IP address.\n");
        return MISSING_ADDRESS;
    }
    err = inet_aton(argv[1], &dst.sin_addr);
    if(err == 0) {
        printf("ping: the first argument must be an IP address.\n");
        return INVALID_ADDRESS;
    }
    out_pkt = (unsigned char*) malloc((unsigned int) (PACKET_SIZE + sizeof(struct icmphdr)));
    in_pkt  = (unsigned char*) malloc((unsigned int) (PACKET_SIZE + sizeof(struct icmphdr) + sizeof(struct iphdr)));
    if(out_pkt == NULL || in_pkt == NULL) {
        printf("ping: out of memory.\n");
        return PROGRAM_OUT_MEM;
    }
    fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd < 0) {
        printf("ping: can't create socket.\n");
        return SOCKET_CREATION;
    }

    tx_args.fd = fd;
    tx_args.pkt = out_pkt;
    tx_args.dst = dst;
    tx_args.add = argv[1];

    rx_args.fd = fd;
    rx_args.pkt = in_pkt;
    rx_args.dst = dst;
    rx_args.add = argv[1];

    /**
     * Il processo utilizza due thread: il primo serve per
     * generare traffico ICMP, l'altro ascolta le risposte
     * provenienti dallo stesso socket.
    */

    if (pthread_create(&tx, NULL, sender, (void *)&tx_args)) {
        printf("ping: can't create sender.\n");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&rx, NULL, receiver, (void *)&rx_args)) {
        printf("ping: can't create receiver.\n");
        exit(EXIT_FAILURE);
    }

    pthread_join(tx, NULL);
    pthread_join(rx, NULL);

    close(fd);
    return 0;
}
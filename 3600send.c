/*
 * CS3600, Spring 2013
 * Project 4 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#include <math.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "3600sendrecv.h"
//Defining Window Size
#define WINDOWSIZE 40

static int DATA_SIZE = 1460;
unsigned int sequence = 0;
unsigned int startindex = 0;
//Creating struct to determine if space in buffer is taken or not.
struct packetdata {
	struct header_t *header;
  unsigned short length;
  unsigned int sequence;
  unsigned int valid;
  unsigned char *offset;
  unsigned int ack;
  unsigned int eof;
	char packdata[1460];
};

//creating a sender buffer with an array of structures
struct packetdata senderbuffer[WINDOWSIZE];

//Helper function to find empty slot
int validcheck() {
	for(int i = 0; i < WINDOWSIZE; i++) {
		if(senderbuffer[i].valid == 0) { //found a free slot
				return i;
		}
	}
}

int sequenceindex(unsigned int sequence) {
  return sequence - startindex;
}


void usage() {
  printf("Usage: 3600send host:port\n");
  exit(1);
}

/**
 * Reads the next block of data from stdin
 */
int get_next_data(char *data, int size) {
  return read(0, data, size);
}

/**
 * Builds and returns the next packet, or NULL
 * if no more data is available.
 */
void *get_next_packet(int sequence, int *len) {

  char *data = malloc(DATA_SIZE);
  int data_len = get_next_data(data, DATA_SIZE);

  if (data_len == 0) {
    free(data);
    return NULL;
  }

  header *myheader = make_header(sequence, data_len, 0, 0);
  void *packet = malloc(sizeof(header) + data_len);
  memcpy(packet, myheader, sizeof(header));
  memcpy(((char *) packet) +sizeof(header), data, data_len);

  free(myheader);

  *len = sizeof(header) + data_len;
//	int sendindex = validcheck();
	int sendindex = sequenceindex(sequence);
	senderbuffer[sendindex].length = *len;
	senderbuffer[sendindex].sequence = sequence;
	senderbuffer[sendindex].valid = 1; //slot is no longer available!
	senderbuffer[sendindex].ack = 0;
  senderbuffer[sendindex].eof = 0;
  senderbuffer[sendindex].header = packet;
//	senderbuffer[sendindex].offset = ?
//	strcpy(senderbuffer[sendindex].packdata,data);

  free(data);
  return packet;
}


int send_next_packet(int sock, struct sockaddr_in out) {
  int packet_len = 0;
  void *packet = get_next_packet(sequence, &packet_len);

  if (packet == NULL) 
    return 0;

  mylog("[send data] %d (%d)\n", sequence, packet_len - sizeof(header));

  
    if (sendto(sock, packet, packet_len, 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
      perror("sendto");
      exit(1);
    }
  return 1;
}

void send_final_packet(int sock, struct sockaddr_in out) {
  header *myheader = make_header(sequence+1, 0, 1, 0);
  mylog("[send eof]\n");

  if (sendto(sock, myheader, sizeof(header), 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
    perror("sendto");
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  /**
   * I've included some basic code for opening a UDP socket in C, 
   * binding to a empheral port, printing out the port number.
   * 
   * I've also included a very simple transport protocol that simply
   * acknowledges every received packet.  It has a header, but does
   * not do any error handling (i.e., it does not have sequence 
   * numbers, timeouts, retries, a "window"). You will
   * need to fill in many of the details, but this should be enough to
   * get you started.
   */

  // extract the host IP and port
  if ((argc != 2) || (strstr(argv[1], ":") == NULL)) {
    usage();
  }

  char *tmp = (char *) malloc(strlen(argv[1])+1);
  strcpy(tmp, argv[1]);

  char *ip_s = strtok(tmp, ":");
  char *port_s = strtok(NULL, ":");
 
  // first, open a UDP socket  
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  // next, construct the local port
  struct sockaddr_in out;
  out.sin_family = AF_INET;
  out.sin_port = htons(atoi(port_s));
  out.sin_addr.s_addr = inet_addr(ip_s);

  // socket for received packets
  struct sockaddr_in in;
  socklen_t in_len;

  // construct the socket set
  fd_set socks;

  // construct the timeout
  struct timeval t;
  t.tv_sec = 6;
  t.tv_usec = 0;
    
  char *sendbuffer[WINDOWSIZE * 1500];
  memset(sendbuffer,0,WINDOWSIZE * 1500);
  unsigned int startsequence = 0;
  int gotcount = 0;
  int windowsize = WINDOWSIZE;
  sequence = 0;
  startindex = 0;
  int nextwindow = 1;
  int senteof = 0;
  while (senteof==0) {

    // make the window
    if (nextwindow) {
      nextwindow = 0;
      int s;
      for (s=0; s<WINDOWSIZE; s++) {

        int packet_len = 0;
        void *packet = get_next_packet(sequence, &packet_len);
        if (packet == NULL) {

          senteof= 1;
          // got eof
          //mylog("[send eof]\n");
          int sendindex = sequenceindex(sequence);
          senderbuffer[sendindex].length = 0;
          senderbuffer[sendindex].eof = 1;
          senderbuffer[sendindex].ack = 0;
          senderbuffer[sendindex].sequence = sequence;
          senderbuffer[sendindex].valid = 1;

          send_final_packet(sock, out);
          break;
        }
        
        mylog("[send data] %d (%d)\n", sequence, packet_len - sizeof(header));
        if (sendto(sock, packet, packet_len, 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
          perror("sendto");
          exit(1);
        }

        sequence = sequence + 1;
        windowsize = s;
      }
    } else {
      // send the window again
      int s;
      for (s=0; s<=windowsize; s++) {
        
        if( senderbuffer[s].ack == 0 ) {
          mylog("[resend data] %d\n", senderbuffer[s].sequence);
          header* packet = senderbuffer[s].header;
          unsigned int packet_len = senderbuffer[s].length;
          if (sendto(sock, packet, packet_len, 0, (struct sockaddr *) &out, (socklen_t) sizeof(out)) < 0) {
            perror("sendto");
            exit(1);
          }
        }
      }
    }
    

    // get the ack
    int done = 0;
    while (!done) {
      
      FD_ZERO(&socks);
      FD_SET(sock, &socks);

      // wait to receive, or for a timeout
      if (select(sock + 1, &socks, NULL, NULL, &t)) {
        unsigned char buf[10000];
        int buf_len = sizeof(buf);
        int received;
        if ((received = recvfrom(sock, &buf, buf_len, 0, (struct sockaddr *) &in, (socklen_t *) &in_len)) < 0) {
          perror("recvfrom");
          exit(1);
        }

        header *myheader = get_header(buf);

        if ((myheader->magic == MAGIC) && (myheader->ack == 1)) {
          
          mylog("[recv ack] %d\n", myheader->sequence);
          
          if (myheader->sequence >= startindex && myheader->sequence <= startindex+WINDOWSIZE){
          
            unsigned int thissequence = myheader->sequence;
            
            int s = sequenceindex(thissequence);
            if (senderbuffer[s].ack==0){
              senderbuffer[s].ack = 1;
              if (senderbuffer[s].eof){
                senteof = 1;
              }
              gotcount = gotcount+1;
              if (gotcount==windowsize){
                nextwindow=1;
                done=1;
                startindex=startindex+WINDOWSIZE;
                if( senteof ) {
                  // finished
                  mylog("[completed]\n");
                  return 0;
                }
                
              }
            }
          }
        } else {
          mylog("[recv corrupted ack] %x %d\n", MAGIC, sequence);
        }
      } else {
        mylog("[error] timeout occurred\n");
        done = 1;
      }
    }
  }

  mylog("[completed]\n");

  return 0;
}


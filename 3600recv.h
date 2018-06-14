/*
 * CS3600, Spring 2013
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#ifndef __3600RECV_H__
#define __3600RECV_H__

//Global vars for recv, organization purposes
unsigned int recvslidingwindow = 100;
unsigned int recvendsequence = -1;
unsigned int recvseqlen = 1;
unsigned int last_seq_eof = 0;
int recvbuffer = 1500;
char *window;
#endif


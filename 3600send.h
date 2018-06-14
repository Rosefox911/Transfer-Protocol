/*
 * CS3600, Spring 2013
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#ifndef __3600SEND_H__
#define __3600SEND_H__
struct bufferdata {
  int valid;
  int sequence;
  int length;
  unsigned char *offset;
};
#endif


/*
 * Copyright (c) 2013, Aalto University.
 * All rights reserved.
 *
 * Simple Output Analyzer for Multicast Examples
 *
 * Usage:
 * 1. Switch Mote output timestamp format to miliseconds.
 * 2. Dump Mote output to a file.
 * 3. grep -e SEED -e RECV <file>
 * 4. Pipe the output of 3) to the standard input of this program.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#define     MAX_ID 11
#define ITERATIONS 100
const int   root = 1;

/* timestamps of packet arrival */
int arrival[MAX_ID+1][ITERATIONS+1];
/* sequence of packets in arrival order */
int received[MAX_ID+1][ITERATIONS+1];
/* reception count */
int count[MAX_ID+1];
/* sum of per-packet delay from the seed */
int sum_delay[MAX_ID+1];
/* count of out of order packets (RFC 5236) */
int out_of_order[MAX_ID+1];

double sum_square[MAX_ID+1];
double mean[MAX_ID+1];
double var[MAX_ID+1];

void read_to_eol(void)
{
  char c;
  do {
    c = getc(stdin);
  } while (c != '\n');
}

int main(int argc, char *argv[])
{
  int timestamp, id, sequence;
  char type[32];
  char c;
  int i, j, k;

  memset(arrival, 0, sizeof(int)*(MAX_ID+1)*(ITERATIONS+1));
  memset(received, 0, sizeof(int)*(MAX_ID+1)*(ITERATIONS+1));
  memset(count, 0, sizeof(int)*(MAX_ID+1));
  memset(out_of_order, 0, sizeof(int)*(MAX_ID+1));
  memset(sum_delay, 0, sizeof(int)*(MAX_ID+1));

  /* parse the input
   * Cooja timestamp should be in miliseconds or any integral type */
  while (scanf("%d", &timestamp) == 1) {
    do {
      c = getc(stdin);
    }
    while (c != ':');
    scanf("%d%s%d", &id, type, &sequence);
    if (id <= MAX_ID && sequence <= ITERATIONS) {
      arrival[id][sequence] = timestamp;
      received[id][count[id]] = sequence;
      count[id] ++;
    }
    read_to_eol();
  }

  /* calculate and print per-packet delays */
  for (i = 1;i <= ITERATIONS;i ++) {
    printf("#%03d", i);
    for (j = 1;j <= MAX_ID;j ++) {
      if (j == root) {
        printf(" ROOT ");
        continue;
      }
      if (arrival[j][i] > 0) {
        arrival[j][i] -= arrival[root][i];
        sum_delay[j] += arrival[j][i];
        printf(" %5d", arrival[j][i]);
      } else {
        printf(" LOST ");
      }
    } /* for j - NODE */
    printf("\n");
  } /* for i - ITERATIONS */

  /* compute the means and variance */
  for (i = 1;i <= MAX_ID;i ++) {
    if (i == root) {
      mean[i] = sum_square[i] = var[i] = 0;
      continue;
    }
    mean[i] = (double)sum_delay[i]/(count[i]);
    sum_square[i] = 0;
    for (j = 1;j <= ITERATIONS;j ++) {
      if (arrival[i][j] == 0) { continue; }
      sum_square[i] += (arrival[i][j] - mean[i])*(arrival[i][j] - mean[i]);
    }
    var[i] = sum_square[i]/(count[i]);
  }

  /* compute out of order rates */
  for (i = 1;i <= MAX_ID;i ++) {
    out_of_order[i] = 0;
    if (i == root) {
      continue;
    }
    k = 1;
    for (j = 0;j < count[i];j ++) {
      while (arrival[i][k] == 0) {
        k ++;
      }
      if (k != received[i][j]) {
        out_of_order[i] ++;
      }
      k ++;
    }
  }

  /* number of lost packets for each node */
  printf("LOSS");
  for (i = 1;i <= MAX_ID;i ++) {
    printf(" %5d", ITERATIONS - count[i]);
  }
  printf("\n");

  /* number of out of order packets for each node */
  printf("PODN");
  for (i = 1;i <= MAX_ID;i ++) {
    printf(" %5d", out_of_order[i]);
  }
  printf("\n");

  /* average per-packet delay for each node */
  printf("MEAN");
  for (i = 1;i <= MAX_ID;i ++) {
    printf(" %.2lf", mean[i]);
  }
  printf("\n");

  /* standard deviation */
  printf("STDD");
  for (i = 1;i <= MAX_ID;i ++) {
    printf(" %.2lf", sqrt(var[i]));
  }
  printf("\n");
  return 0;
}


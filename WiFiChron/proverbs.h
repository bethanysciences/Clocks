//*********************************************************************************************************************
//
// PROVERBS.C - find and return a proverb
//
// This reads the Huffman compressed bit data in comproverb.c and decompresses it back to a character stream.
// I have achieved about a 54% data compression using this technique.
//
// Grahame Marsh March 2009
//
//********************************************************************************************************************* 

#include "UserConf.h"
#ifdef WANT_PROVERBS
#ifndef PROVERBS_H_INCLUDED
#define PROVERBS_H_INCLUDED

#include "comproverbs.h"

void getProverb(char *buf, int8_t sz);
void nextProverbPart(char *buf, int8_t sz);
byte dehuffChar();
boolean dehuffFind(word index);

#endif
#endif	/* WANT_PROVERBS */
//********************************************************************************************************************* 

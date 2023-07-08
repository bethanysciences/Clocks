#include "UserConf.h"
#ifdef WANT_PROVERBS
#ifndef COMP_PROVERBS_H_INCLUDED
#define COMP_PROVERBS_H_INCLUDED
#include <Arduino.h>
#include <avr/pgmspace.h>

// Automatically generated Huffman compressed proverb list

// Grahame Marsh March 2009

#if defined(PROVERBS_32K)

#define DATA_INPUT_BYTES  59073
#define DATA_OUTPUT_BYTES 32718
#define DATA_COMPRESSION  55.4%
#define PROVERB_COUNT     1582
#define MAX_PROVERB_LEN   82
#define ROOT_NODE         118
#define HL                255

#elif defined(PROVERBS_16K)

#define DATA_INPUT_BYTES  28934
#define DATA_OUTPUT_BYTES 16176
#define DATA_COMPRESSION  55.9%
#define PROVERB_COUNT     995
#define MAX_PROVERB_LEN   39
#define ROOT_NODE         114
#define HL                255

#elif defined(PROVERBS_8K)

#define DATA_INPUT_BYTES  14555
#define DATA_OUTPUT_BYTES 8208
#define DATA_COMPRESSION  56.4%
#define PROVERB_COUNT     589
#define MAX_PROVERB_LEN   31
#define ROOT_NODE         110
#define HL                255

#elif defined(PROVERBS_4K)

#define DATA_INPUT_BYTES  7388
#define DATA_OUTPUT_BYTES 4210
#define DATA_COMPRESSION  57.0%
#define PROVERB_COUNT     343
#define MAX_PROVERB_LEN   26
#define ROOT_NODE         110
#define HL                255

#elif defined(PROVERBS_2K)

#define DATA_INPUT_BYTES  3437
#define DATA_OUTPUT_BYTES 1965
#define DATA_COMPRESSION  57.2%
#define PROVERB_COUNT     181
#define MAX_PROVERB_LEN   22
#define ROOT_NODE         102
#define HL                255

#endif

// A single node in the Huffman decode tree
typedef union t_hufftree
{
    struct t_node
    {
        byte    left;          // left node index for value = 0
        byte    right;         // right node index for value = 1
    } node;
    struct t_choose
    {
        byte    node[2];       // array [0 .. 1] or [left index .. right index]
    } choose;
    struct t_leaf
    {
        byte    code;          // set to HL value (illegal node) to show a leaf
        byte    ch;            // leaf character value
    } leaf;
    uint16_t buf;
} THuffTree;

extern const byte proverbs [];

extern const THuffTree tree [];

#endif
#endif

//*********************************************************************************************************************
//
// PROVERBS.C - find and return a proverb
//
// This reads the Huffman compressed bit data in comproverb.c and decompresses it back to a character stream.
// I have achieved about a 54% data compression using this technique. Can take upto about 1s work. 
//
// Grahame Marsh March 2009
//
//********************************************************************************************************************* 

#include "UserConf.h"
#ifdef WANT_PROVERBS
#include "proverbs.h"
  
byte accumulator;   // current byte being decompressed
byte shift;         // current bit being decompressed
byte *ptr;	    // next byte of data to be decompressed

boolean Pend;       // at end of proverb

// pick a random proverb
void getProverb(char *buf, int8_t sz)
{
    char c;
    int8_t i;

    i = 0;
    if (dehuffFind(rand() % PROVERB_COUNT))
    {
	do
	{
	    c = (char)dehuffChar();
	    buf[i++] = c;
	}  
	while (c && (i < sz));
    } else {
	c = 0;
	buf[i] = 0;
    }
    Pend = (c == 0) ? true : false;
}

// get the next block of characters into the buffer
void nextProverbPart(char *buf, int8_t sz)
{
    int8_t i;
    char c;

    if (Pend) return;

    i = 0;
    do {
	c = (char) dehuffChar();
	buf[i++] = c;
    }  while ((c != 0) && (i < sz));

    Pend = (c == 0) ? true : false;
    return;
}

//
// read a single character from the compressed bit data
byte dehuffChar ()
{
byte index;
boolean choose;
    THuffTree ltree;
    
    for (index = ROOT_NODE;;)
    {          
    
// read a single bit out of the accumulator and note if it is a '0' or '1'    
        choose = (accumulator & shift) != 0;                              
// move the bit tester to the right one bit        
        shift >>= 1;                       
// the bit tester has reached zero so it is time to get the next value 
// from the data array (*ptr) and reset the bit tester back to the lh bit        
        if (shift == 0)
        {
            accumulator = pgm_read_byte(ptr++);
            shift = 0x80;    
        }                                                                
// now get the new index depending on the value read above        

	ltree.buf = pgm_read_word(&tree[index].buf);
        index = ltree.choose.node[choose];          
// if the new indexed node is a leaf then return the character
// the accumulator and bit tester are pointing at the start of the next character
// which will be read if this function is called again        
	ltree.buf = pgm_read_word(&tree[index].buf);
        if (ltree.leaf.code == HL)
            return ltree.leaf.ch;                 
    }
}

// point the decompressor at the indexth proverb
// return false if index is more that the number of proverbs    
// return true on success
boolean dehuffFind (word index)
{
// point at the start of the compressed proverb data
    ptr = (byte *)proverbs;                                 
// read the first byte of data to be decompressed    
    accumulator = pgm_read_byte(ptr++);                        
// set the bit tester to the lh bit    
    shift = 0x80;                  
// if index is zero, i.e. the first proverb then return as we are done    
    if (index == 0)
        return true;
// or else skip proverbs until the indexth proverb is pointed at       
    if (index < PROVERB_COUNT)
    {
        do      
        {                                                       
// decompress until a whole proverb has been skipped        
            while (dehuffChar ())
                ;               
// repeat until index counted down to zero                        
        } while (--index);
        return true;
    } else                                
// return false if index is more that the number of proverbs    
        return false;    
}

#endif /* WANT_PROVERBS */
//*** QED *************************************************************************************************************

/*
 * Firmware generator for s5k4ecgx (5MP Camera) from Samsung
 * a quarter-inch optical format 1.4 micron 5 megapixel (Mp)
 * CMOS image sensor.
 * Copyright (C) 2012, Linaro, Sangwook Lee <sangwook.lee@linaro.org>
 *
 * HOW TO COMPILE
 * $ gcc -o genfw genfw.c -lz
 *
 */

#include <stdio.h> 
#include <string.h> 
#include <zlib.h>

typedef struct tagCatalogueRecord { 
  unsigned int addr;  
  unsigned short val; 

} __attribute__((packed)) CatalogueRecord; 

/* Total register sets */
#include "s5k4ecgx_evt1_1.c"

unsigned long getFileCRC(FILE *);
unsigned long calcCRC (const unsigned char *, signed long,
		       unsigned long, unsigned long *);
void makeCRCtable(unsigned long *, unsigned long);

#define BUFFER_LEN       4096L
#define FW_FILE_NAME	"s5k4ecgx.bin"
#define BUFFER_SIZE (ENTRY_NUMBER+1)*6

unsigned long getFileCRC(FILE *s)
{
	/* File size */
	unsigned char buf [BUFFER_SIZE];
	unsigned long crc = ~0;
	size_t len;

//	crc = crc32(crc, Z_NULL, 0);
	while ( (len = fread(buf, 1, sizeof(buf), s)) != 0 )
		crc = crc32((crc ^ 0xffffffff),buf,len) ^ 0xffffffff;

	return crc;
}

CatalogueRecord newRecord(int nCopies, int val) 
{ 
	CatalogueRecord record; 
	record.addr = nCopies; 
	record.val = 0; 

	return record; 
} 

/* printRecord: prints a single record */ 
int printRecord(CatalogueRecord record) 
{ 
	return 0; 
} 

/* createFile: creates a binary file */ 
int createFile(char *fileName) 
{ 
	CatalogueRecord record; 
	FILE *catalogue;
	int i;
 
	catalogue = fopen(fileName, "wb"); 
	record = newRecord(ENTRY_NUMBER, 0); 
	fwrite(&record, sizeof(CatalogueRecord), 1, catalogue); 
	for (i = 0; i < ENTRY_NUMBER; i++)
		fwrite(&firmware[i], sizeof(CatalogueRecord), 1, catalogue); 
	fclose(catalogue); 

	return 0; 
}

int main() 
{
	unsigned long crc;
	int ret;
	char *fileName = FW_FILE_NAME;
	FILE *catalogue; 

	createFile(fileName); 
	printf("Making firmware: %s\n", fileName);

	catalogue = fopen(fileName, "rb"); 
	crc = getFileCRC(catalogue);
	printf("Got CRC value %#lx (size %ld)\n", crc, sizeof(unsigned long));
	fclose(catalogue); 

	printf("Try to append CRC to %s\n", fileName);
	// Open for append
	catalogue = fopen(fileName, "a+");

	/* Make 4 bytes CRC */ 
	fwrite(&crc, 4, 1, catalogue);
	fclose(catalogue);
	printf("Done\n"); 

	return 0; 
}

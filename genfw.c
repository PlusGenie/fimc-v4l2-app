
#include <stdio.h> 
#include <string.h> 
#include <zlib.h>

typedef struct tagCatalogueRecord { 
  unsigned int addr;  
  unsigned short val; 

} __attribute__((packed)) CatalogueRecord; 

/* Total register sets */
//#define ENTRY_NUMBER 2750
/* Register array */
#include "s5k4ecgx_evt1_1.c"

unsigned long getFileCRC(FILE *);
unsigned long calcCRC (const unsigned char *, signed long,
		       unsigned long, unsigned long *);
void makeCRCtable(unsigned long *, unsigned long);

#define CRCPOLY_LE 0xedb88320
#define CRCPOLY_BE 0x04c11db7
#define POLYNOMIAL CRCPOLY_BE

#define BUFFER_LEN       4096L      // Length of buffer
#define BUFFER_SIZE (ENTRY_NUMBER+1)*6

unsigned long getFileCRC2(FILE *s)
{
	/* File size */
	unsigned char buf [BUFFER_SIZE];
	unsigned long CRC = 0;
	size_t len;

	CRC = crc32(0L, Z_NULL, 0);

#if 0
	while ( (len = fread(buf, 1, sizeof(buf), s)) != NULL )
		CRC = crc32(CRC, buf, (unsigned long)len);
#endif
	len = fread(buf, 1, sizeof(buf), s);
	CRC = crc32(CRC, buf, (unsigned long)len);
	printf("len %d\n", len);

	return CRC;
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

/* printFile: displays the contents of the given file */ 
int printFile(char *fileName) 
{ 
	FILE *catalogue; 
	CatalogueRecord record; 
	catalogue = fopen(fileName, "rb"); 

	fread(&record, sizeof(CatalogueRecord), 1, catalogue); 

	while (!feof(catalogue)) { 
		printRecord(record); 
		fread(&record, sizeof(CatalogueRecord), 1, catalogue); 
	} 
	fclose(catalogue); 

	return 0; 

} 

int main() 
{
	unsigned long crc;
	int ret;
	char *fileName = "s5k4ecgx.bin";
	FILE *catalogue; 

	createFile(fileName); 
	printf("Generated firmware file: %s\n", fileName);

	catalogue = fopen(fileName, "rb"); 
	crc = getFileCRC2(catalogue);
	printf("Got CRC value %#lx (size %d)\n", crc, sizeof(unsigned long));
	fclose(catalogue); 

	printf("Try to append CRC to %s\n", fileName);
#if 1
	// Open for append
	catalogue = fopen(fileName, "a+"); 
	fwrite(&crc, sizeof(crc), 1, catalogue);
	fclose(catalogue);
	printf("Added CRC to %s\n", fileName); 
#endif
	//printFile("s5k4ecgx.bin"); 

	return 0; 
}

/*  This app is based on 
 *  from http://www.tmarris.com/cprog/binaryfiles.pdf
 *  http://mwultong.blogspot.com/2007/01/c-crc32-crc32cpp.html
 */


#include <stdio.h> 
#include <string.h> 

typedef struct tagCatalogueRecord { 
  unsigned int addr;  
  unsigned short val; 

} CatalogueRecord; 

/* Total register sets */
#define ENTRY_NUMBER 2750
/* Register array */
#include "s5k4ecgx_evt1_1.c"

unsigned long getFileCRC(FILE *);
unsigned long calcCRC (const unsigned char *, signed long,
		       unsigned long, unsigned long *);
void makeCRCtable(unsigned long *, unsigned long);

unsigned long getFileCRC(FILE *s)
{
	/* File size */
	unsigned char buf [(ENTRY_NUMBER+1)*6 + 4];
	unsigned long CRC = 0;
	unsigned long table[256];
	size_t len;

	makeCRCtable(table, 0xEDB88320);

	while ( (len = fread(buf, 1, sizeof(buf), s)) != NULL )
		CRC = calcCRC(buf, (unsigned long) len, CRC, table);

	return CRC;
}

unsigned long calcCRC(const unsigned char *mem, signed long size,
		      unsigned long CRC, unsigned long *table)
{
	CRC = ~CRC;

	while(size--)
		CRC = table[(CRC ^ *(mem++)) & 0xFF] ^ (CRC >> 8);

	return ~CRC;
}

void makeCRCtable(unsigned long *table, unsigned long id)
{
	unsigned long i, j, k;

	for(i = 0; i < 256; ++i) {
		k = i;
		for(j = 0; j < 8; ++j) {
			if (k & 1) k = (k >> 1) ^ id;
			else k >>= 1;
		}
		table[i] = k;
	}
}

CatalogueRecord newRecord(int nCopies) 
{ 
	CatalogueRecord record; 
	record.addr = nCopies; 

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

	record = newRecord(ENTRY_NUMBER); 
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
	printf("Try to append CRC to %s\n", fileName);

	catalogue = fopen(fileName, "rb"); 
	crc = getFileCRC(catalogue);
	printf("Got CRC value %#lx (size %d)\n", crc, sizeof(unsigned long));
	fclose(catalogue); 

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

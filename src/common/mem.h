
#ifndef __MEM_H
#define __MEM_H

static inline unsigned short LE2(void const *ptr) // read little endian short
{
	unsigned char const *p = (unsigned char const *)ptr;
	return (p[1] << 8) | p[0];
}

static inline unsigned long LE4(void const *ptr) // read little endian long
{
	unsigned char const *p = (unsigned char const *)ptr;
	return (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
}

static inline unsigned long long LE8(void const *ptr) // read little endian long
{
	unsigned char const *p = (unsigned char const *)ptr;
	unsigned long h = (p[7] << 24) | (p[6] << 16) | (p[5] << 8) | p[4];
	unsigned long l = (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
	return (unsigned long long)h << 32 | l;
}

static inline unsigned short BE2(void const *ptr) // read big endian short
{
	unsigned char const *p = (unsigned char const *)ptr;
	return (p[0] << 8) | p[1];
}

static inline unsigned long BE4(void const *ptr) // read big endian long
{
	unsigned char const *p = (unsigned char const *)ptr;
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static inline unsigned long long BE8(void const *ptr) // read big endian long
{
	unsigned char const *p = (unsigned char const *)ptr;
	return ((unsigned long long)BE4(p) << 32) | BE4(p + 4);
}

#endif



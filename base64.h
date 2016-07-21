#ifndef _BASE64_H_
#define _BASE64_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

char *base64_encode(const unsigned char *data,
	size_t input_length,
	size_t *output_length);
void build_decoding_table();
unsigned char *base64_decode(const char *data,
	size_t input_length,
	size_t *output_length);
#endif

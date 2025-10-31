#include "strings.h"

void strcpy(char *dest, const char *src) {
	int i = 0;
	while (src[i] != 0) {
		dest[i] = src[i];
		i++;
	}
	dest[i] = 0;
}

int strcmp(const char *str1, const char *str2) {
	int i = 0;
	while (str1[i] != 0 && str2[i] != 0) {
		if (str1[i] != str2[i]) {
			return str1[i] - str2[i];
		}
		i++;
	}
	return str1[i] - str2[i];
}

int64_t strlen(const char *str) {
	int64_t length = 0;
	while (str[length] != 0) {
		length++;
	}
	return length;
}
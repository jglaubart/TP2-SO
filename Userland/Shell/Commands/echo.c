// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "commands.h"

#ifdef ANSI_4_BIT_COLOR_SUPPORT
#include "ansiColors.h"
#endif

int _echo(int argc, char *argv[]) {
	int j = 1;
	while (j < argc) {
		char *arg = argv[j];
		for (int i = 0; i < strlen(arg); i++) {
			switch (arg[i]) {
				case '\\':
					switch (arg[i + 1]) {
						case 'n':
							printf("\n");
							i++;
							break;
						case 'e':
						#ifdef ANSI_4_BIT_COLOR_SUPPORT
							i++;
							parseANSI(arg, &i);
						#else
							while (arg[i] != 'm') i++;  // ignores escape code, assumes valid format
							i++;
						#endif
							break;
						case 'r':
							printf("\r");
							i++;
							break;
						case '\\':
							i++;
						default:
							putchar(arg[i]);
							break;
					}
					break;
				default:
					putchar(arg[i]);
					break;
			}
		}
		printf(" ");
		j++;
	}
	printf("\n");
	return 0;
}
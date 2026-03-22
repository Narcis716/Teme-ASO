#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define HIVE        HKEY_LOCAL_MACHINE
#define CHEIE_PATH  "SOFTWARE\\Microsoft\\Windows\\CurrentVersion"

int main(void)
{
    HKEY  hCheie;
    DWORD nrSubchei   = 0;
    DWORD lungMaxNume = 0;
    LONG  rezultat;

    rezultat = RegOpenKeyEx(HIVE, CHEIE_PATH, 0, KEY_READ, &hCheie);
    if (rezultat != ERROR_SUCCESS) {
        fprintf(stderr, "Eroare: Nu s-a putut deschide cheia. Cod: %ld\n", rezultat);
        return EXIT_FAILURE;
    }

    rezultat = RegQueryInfoKey(hCheie, NULL, NULL, NULL,
                               &nrSubchei, &lungMaxNume,
                               NULL, NULL, NULL, NULL, NULL, NULL);
    if (rezultat != ERROR_SUCCESS) {
        fprintf(stderr, "Eroare RegQueryInfoKey: %ld\n", rezultat);
        RegCloseKey(hCheie);
        return EXIT_FAILURE;
    }

    printf("HKEY_LOCAL_MACHINE\\%s\n", CHEIE_PATH);
    printf("Numar subchei: %lu\n\n", (unsigned long)nrSubchei);

    if (nrSubchei == 0) {
        printf("(cheia nu are subchei)\n");
        RegCloseKey(hCheie);
        return EXIT_SUCCESS;
    }

    lungMaxNume++;
    char *numeCheie = (char *)malloc(lungMaxNume * sizeof(char));
    if (!numeCheie) {
        fprintf(stderr, "Eroare: memorie insuficienta.\n");
        RegCloseKey(hCheie);
        return EXIT_FAILURE;
    }

    for (DWORD i = 0; i < nrSubchei; i++) {
        DWORD dim = lungMaxNume;

        rezultat = RegEnumKeyEx(hCheie, i, numeCheie, &dim,
                                NULL, NULL, NULL, NULL);
        if (rezultat == ERROR_SUCCESS)
            printf("  [%3lu]  %s\n", (unsigned long)i + 1, numeCheie);
        else if (rezultat == ERROR_NO_MORE_ITEMS)
            break;
        else
            fprintf(stderr, "  [%3lu]  <eroare: %ld>\n", (unsigned long)i + 1, rezultat);
    }

    free(numeCheie);
    RegCloseKey(hCheie);

    printf("\nEnumerare finalizata.\n");
    return EXIT_SUCCESS;
}
#!/bin/bash
echo "Hallo Welt!" > file_vault/file_1
cat file_vault/file_1
echo "Ein zweiter Test." > file_vault/file_1
cat file_vault/file_1
ls -la file_vault/
mkdir file_vault/dir_1
echo "Eine weitere Datei." > file_vault/dir_1/file_2
echo "Mit mehr Inhalt." >> file_vault/dir_1/file_2
cat file_vault/dir_1/file_2
echo "Und Sonderzeichen: /.:($)=%!" >> file_vault/dir_1/file_2
cat file_vault/dir_1/file_2
ls -la file_vault/
ls -la file_vault/dir_1
exit 0

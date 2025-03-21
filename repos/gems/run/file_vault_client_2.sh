#!/bin/bash
ls="ls -la --time-style=+%F"

$ls file_vault/
$ls file_vault/dir_1
cat file_vault/file_1
cat file_vault/dir_1/file_2
exit 0

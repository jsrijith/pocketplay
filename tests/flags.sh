# Compiler flags for every PC test. The sanitizers stop a test at a memory error or
# undefined behaviour, which could otherwise pass unnoticed.
FLAGS="-Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all"

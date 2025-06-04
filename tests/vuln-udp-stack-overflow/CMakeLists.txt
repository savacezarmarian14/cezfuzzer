cmake_minimum_required(VERSION 3.10)
project(vuln_udp_stack_overflow C)

# Setăm standardul C pentru toate target-urile
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# =================================================================================
# 1. Build server-stack-overflow (serverul vulnerabil)
# =================================================================================
add_executable(server-stack-overflow
    server.c
)

# Flag-uri de compilare pentru server:
#   - Wall, Wextra: afișează toate warning-urile
#   - fno-stack-protector: dezactivează stack canary (pentru a lăsa stack overflow neprotejate)
#   - z execstack: marchează stack-ul ca executabil (dacă vei folosi shellcode în PoC)
target_compile_options(server-stack-overflow PRIVATE
    -Wall
    -Wextra
    -fno-stack-protector
    -z execstack
)

# Flag-uri de linking pentru server:
#   - no-pie: dezactivează PIE (codul executabil rămâne la o adresă fixă)
#   - z execstack (din nou, pentru ca stack-ul să fie executabil la runtime)
target_link_options(server-stack-overflow PRIVATE
    -no-pie
    -z execstack
)

# =================================================================================
# 2. Build client-stack-overflow (clientul „sigur”)
# =================================================================================
add_executable(client-stack-overflow
    client.c
)

# Flag-uri de compilare pentru client:
#   - Wall, Wextra: afișează toate warning-urile
target_compile_options(client-stack-overflow PRIVATE
    -Wall
    -Wextra
)

# (Clientul nu are nevoie de execstack / no-pie / fno-stack-protector
#  decât dacă vrei să testezi și el în mod explicit exploatarea locală,
#  însă pentru un simplu client UDP nu e necesar.)


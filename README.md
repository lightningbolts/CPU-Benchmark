# CPU-Benchmark

Linux: gcc prime.c -DCURL_STATICLIB -I/home/timberlake2025/Code/Taipan-Benchmarks/openssl-openssl-3.2.0/build/include -L/home/timberlake2025/Code/Taipan-Benchmarks/openssl-openssl-3.2.0/build/lib -static -lssl -lcrypto -o prime_linux

Mac: clang prime.c -DCURL_STATICLIB -I/path/to/openssl/build/include -L/path/to/openssl/build/lib -lssl -lcrypto -o prime_macos_[arch]

Windows: gcc prime_windows.c -DCURL_STATICLIB -IC:\Users\timberlake2025\Desktop\Code\openssl-openssl-3.2.0\build\include -LC:\Users\timberlake2025\Desktop\Code\openssl-openssl-3.2.0\build\lib -static -lssl -lcrypto -lcrypt32 -lws2_32 -o prime_windows
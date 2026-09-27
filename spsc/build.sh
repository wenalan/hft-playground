clang++ -std=c++20 -O2 -g -fsanitize=thread -fno-omit-frame-pointer -pthread spsc.cpp -o spsc_tsan


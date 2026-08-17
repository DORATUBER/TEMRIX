#pragma once

extern "C" long syscall0(long num);
extern "C" long syscall1(long num, long a1);
extern "C" long syscall2(long num, long a1, long a2);
extern "C" long syscall3(long num, long a1, long a2, long a3);
extern "C" long syscall4(long num, long a1, long a2, long a3, long a4);
extern "C" long syscall5(long num, long a1, long a2, long a3, long a4, long a5);
extern "C" long syscall6(long num, long a1, long a2, long a3, long a4, long a5, long a6);

inline long syscall(long num) { return syscall0(num); }
inline long syscall(long num, long a1) { return syscall1(num, a1); }
inline long syscall(long num, long a1, long a2) { return syscall2(num, a1, a2); }
inline long syscall(long num, long a1, long a2, long a3) { return syscall3(num, a1, a2, a3); }
inline long syscall(long num, long a1, long a2, long a3, long a4) { return syscall4(num, a1, a2, a3, a4); }
inline long syscall(long num, long a1, long a2, long a3, long a4, long a5) { return syscall5(num, a1, a2, a3, a4, a5); }
inline long syscall(long num, long a1, long a2, long a3, long a4, long a5, long a6) { return syscall6(num, a1, a2, a3, a4, a5, a6); }
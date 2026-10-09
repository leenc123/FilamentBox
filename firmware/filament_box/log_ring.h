#pragma once
// log_ring.h — 内存环形调试日志：logLine() 双写 Serial + 缓冲，供 /log 页面读取
// 80 行 × 110 字符 ≈ 8.8KB DRAM；满时覆盖最旧行；重启清空
#include <Arduino.h>

#define LOG_RING_LINES 80
#define LOG_LINE_LEN 110

void logLine(const String& s);  // 双写 Serial + 缓冲（\n 转空格，超长截断加 ~）
uint8_t logCount();             // 当前行数
uint32_t logSeq();              // 单调序号（/log/stream 游标用）
String logAt(uint8_t idx);      // idx=0 最旧
uint32_t logSeqAt(uint8_t idx); // 与 logAt 同一下标的序号
void logClear();
String logUptime();             // "[hh:mm:ss] " 前缀

// log_ring.cpp
#include "log_ring.h"

static String ring[LOG_RING_LINES];
static uint32_t seqs[LOG_RING_LINES];
static uint8_t head = 0;  // 下一写入位
static uint8_t count = 0;
static uint32_t seq = 0;

String logUptime() {
  unsigned long s = millis() / 1000;
  char buf[16];
  snprintf(buf, sizeof(buf), "[%02lu:%02lu:%02lu] ",
           (s / 3600) % 100, (s % 3600) / 60, s % 60);
  return String(buf);
}

void logLine(const String& s) {
  Serial.println(s);
  String line = logUptime();
  line += s;
  line.replace("\r", "");
  line.replace("\n", " ");
  if (line.length() > LOG_LINE_LEN) line = line.substring(0, LOG_LINE_LEN - 1) + "~";
  ring[head] = line;
  seqs[head] = ++seq;
  head = (uint8_t)((head + 1) % LOG_RING_LINES);
  if (count < LOG_RING_LINES) count++;
}

uint8_t logCount() { return count; }
uint32_t logSeq() { return seq; }

String logAt(uint8_t idx) {
  if (idx >= count) return "";
  uint8_t start = (uint8_t)((head + LOG_RING_LINES - count) % LOG_RING_LINES);
  return ring[(uint8_t)((start + idx) % LOG_RING_LINES)];
}

uint32_t logSeqAt(uint8_t idx) {
  if (idx >= count) return seq;
  uint8_t start = (uint8_t)((head + LOG_RING_LINES - count) % LOG_RING_LINES);
  return seqs[(uint8_t)((start + idx) % LOG_RING_LINES)];
}

void logClear() {
  for (uint8_t i = 0; i < LOG_RING_LINES; i++) ring[i] = "";
  head = 0;
  count = 0;
}

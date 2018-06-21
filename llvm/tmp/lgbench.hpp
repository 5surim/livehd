//  This file is distributed under the BSD 3-Clause License. See LICENSE for details.
#ifndef LGBENCH_H
#define LGBENCH_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <chrono>

class LGBench {
private:
int parseLine(char* line){
    // This assumes that a digit will be found and the line ends in " Kb".
    int i = strlen(line);
    const char* p = line;
    while (*p <'0' || *p > '9') p++;
    line[i-3] = '\0';
    i = atoi(p);
    return i;
}

int getValue(){ //Note: this value is in KB!
    FILE* file = fopen("/proc/self/status", "r");
    int result = -1;
    char line[128];

    while (fgets(line, 128, file) != NULL){
        if (strncmp(line, "VmRSS:", 6) == 0){
            result = parseLine(line);
            break;
        }
    }
    fclose(file);
    return result;
}
protected:
  typedef std::chrono::time_point<std::chrono::system_clock> Time_Point;
  struct Time_Sample {
    Time_Point  tp;
    int mem;
    std::string name;
  };
  std::vector<Time_Sample> record;
  Time_Point start_time;
  int start_mem;
  bool end_called;
public:
  LGBench() {
    end_called = false;
    start();
  };

  ~LGBench() {
    if (end_called)
      return;
    end();
  }

  void start() {
    start_time = std::chrono::system_clock::now();
    start_mem = getValue();
  }

  void sample(std::string name) {
    Time_Sample s;
    s.tp  = std::chrono::system_clock::now();
    s.mem = getValue();
    s.name = name;

    record.push_back(s);
  }

  void end() {
    sample("end");

    Time_Point prev = start_time;
    int prev_mem = start_mem;
    for(const auto &s:record) {
      std::chrono::duration<double> t = s.tp - prev;

      if (s.name == "end" && t.count() < 0.01)
        continue;

      int m;
      if (prev_mem>s.mem)
        m = prev_mem - s.mem;
      else
        m = s.mem - prev_mem;

      std::cerr << s.name << " in " << t.count() << " secs, " << m << " KB delta " << s.mem << "KB abs\n";

      prev = s.tp;
      prev_mem = s.mem;
    }
  }

};
#endif


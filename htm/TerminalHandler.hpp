#ifndef __HTM_TERMINAL_HANDLER__
#define __HTM_TERMINAL_HANDLER__

#include "Headers.hpp"

namespace et
{
class TerminalHandler
{
public:
  TerminalHandler();
  void start();
  string pollUserTerminal();
  void appendData(const string &data);
  inline bool isRunning() { return run; }
  void stop();

protected:
  int masterFd;
  int childPid;
  bool run;
};
} // namespace htm

#endif // __HTM_TERMINAL_HANDLER__

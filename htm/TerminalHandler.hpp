#ifndef __HTM_TERMINAL_HANDLER__
#define __HTM_TERMINAL_HANDLER__

#include "Headers.hpp"

namespace htm {
class TerminalHandler {
 public:
  TerminalHandler();
  void connectToRouter(const string& idPasskey);
  void run();

 protected:
  int routerFd;

  void runUserTerminal(int masterFd, pid_t childPid);
};
}  // namespace htm

#endif  // __HTM_TERMINAL_HANDLER__

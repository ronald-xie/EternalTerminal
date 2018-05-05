#ifndef __MULTIPLEXER_STATE_HPP__
#define __MULTIPLEXER_STATE_HPP__

#include "Headers.hpp"

#include "HTM.pb.h"
#include "TerminalHandler.hpp"

namespace et {
const int UUID_LENGTH = 36;

class MultiplexerState {
 public:
  MultiplexerState();
  inline const State& getState() { return state; }
  void appendData(const string &uid, const string &data);
  void newTab(const string& tabId, const string& paneId);
  void closePane(const string& paneId);
  void update();

 protected:
  State state;
  map<string, shared_ptr<TerminalHandler>> terminals;
};
}  // namespace et

#endif  // __MULTIPLEXER_STATE_HPP__
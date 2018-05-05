#include "MultiplexerState.hpp"

#include "sole.hpp"
#include "RawSocketUtils.hpp"
#include "base64.hpp"

namespace et {
MultiplexerState::MultiplexerState() {
  state.set_shell(string(::getenv("SHELL")));
  Tab *t = state.add_tab();
  t->set_id(sole::uuid4().str());
  t->set_order(0);
  t->set_active(true);
  PaneOrSplitPane *posp = t->mutable_pane();
  Pane *p = posp->mutable_pane();
  p->set_id(sole::uuid4().str());
  p->set_active(true);
  terminals[p->id()] = shared_ptr<TerminalHandler>(new TerminalHandler());
  terminals[p->id()]->start();
}

void MultiplexerState::appendData(const string &uid, const string &data) {
  if (terminals.find(uid) == terminals.end()) {
    LOG(FATAL) << "Tried to write to non-existant terminal";
  }
  terminals[uid]->appendData(data);
}

void MultiplexerState::newTab(const string &tabId, const string &paneId) {
  Tab *t = state.add_tab();
  t->set_id(tabId);
  t->set_order(state.tab_size() - 1);
  t->set_active(true);
  PaneOrSplitPane *posp = t->mutable_pane();
  Pane *p = posp->mutable_pane();
  p->set_id(paneId);
  p->set_active(true);

  terminals[p->id()] = shared_ptr<TerminalHandler>(new TerminalHandler());
  terminals[p->id()]->start();
}

void MultiplexerState::closePane(const string &paneId) {
  if (terminals.find(paneId) == terminals.end()) {
    LOG(FATAL) << "Tried to close a pane that didn't exist";
  }
  terminals[paneId]->stop();
  // TODO: Delete the pane from the state
}

void MultiplexerState::update() {
  char header;
  for (auto &it : terminals) {
    const string &paneId = it.first;
    shared_ptr<TerminalHandler> &terminal = it.second;
    string terminalData = terminal->pollUserTerminal();
    if (terminalData.length()) {
      header = APPEND_TO_PANE + '0';
      int32_t length =
          base64::Base64::EncodedLength(terminalData) + paneId.length();
      LOG(ERROR) << "WRITING TO " << paneId << ":" << length;
      RawSocketUtils::writeAll(STDOUT_FILENO, (const char *)&header, 1);
      RawSocketUtils::writeB64(STDOUT_FILENO, (const char *)&length, 4);
      RawSocketUtils::writeAll(STDOUT_FILENO, &(paneId[0]), paneId.length());
      RawSocketUtils::writeB64(STDOUT_FILENO, &terminalData[0],
                               terminalData.length());
      LOG(ERROR) << "WROTE TO " << paneId << ":" << terminalData;
      fflush(stdout);
    }
    if (!terminal->isRunning()) {
      closePane(paneId);
      header = SERVER_CLOSE_PANE + '0';
      int32_t length = paneId.length();
      LOG(ERROR) << "CLOSING " << paneId << ":" << length;
      RawSocketUtils::writeAll(STDOUT_FILENO, (const char *)&header, 1);
      RawSocketUtils::writeB64(STDOUT_FILENO, (const char *)&length, 4);
      RawSocketUtils::writeAll(STDOUT_FILENO, &(paneId[0]), paneId.length());
      fflush(stdout);
      terminals.erase(it.first);
      // Only erase one terminal per loop to avoid erase + iterate
      break;
    }
  }
}
}  // namespace et
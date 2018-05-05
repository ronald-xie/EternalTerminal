#include "Headers.hpp"

#include "HTM.pb.h"

#include "RawSocketUtils.hpp"
#include "sole.hpp"
#include "LogHandler.hpp"
#include "TerminalHandler.hpp"
#include "base64.hpp"

#include <termios.h>

namespace google
{
}
namespace gflags
{
}
using namespace google;
using namespace gflags;

namespace et
{
int UTF8Length(const string &s)
{
  int len = 0;
  const char *c = s.c_str();
  while (*c)
    len += (*c++ & 0xc0) != 0x80;
  return len;
}

void Main(int argc, char **argv)
{
  // Start by writing ESC + [###q
  char buf[] = {
      0x1b,
      0x5b,
      '#',
      '#',
      '#',
      'q',
  };
  fwrite(buf, 1, sizeof(buf), stdout);
  fflush(stdout);

  // Begin by sending an empty state (TODO: Daemonize HTM and support
  // re-attaching to the daemon)

  State s;
  s.set_shell(string(::getenv("SHELL")));
  Tab *t = s.add_tab();
  t->set_id(sole::uuid4().str());
  t->set_order(0);
  t->set_active(true);
  PaneOrSplitPane *posp = t->mutable_pane();
  Pane *p = posp->mutable_pane();
  p->set_id(sole::uuid4().str());
  p->set_active(true);

  map<string, shared_ptr<TerminalHandler>> terminals;
  terminals[p->id()] = shared_ptr<TerminalHandler>(new TerminalHandler());
  terminals[p->id()]->start();
  LOG(ERROR) << "Starting terminal";

  {
    unsigned char header = INIT_STATE + '0';
    string jsonString;
    auto status = google::protobuf::util::MessageToJsonString(s, &jsonString);
    VLOG(1) << "STATUS: " << status;
    int32_t length = jsonString.length() + 1;
    LOG(ERROR) << "SENDING INIT OF SIZE:" << length;
    RawSocketUtils::writeB64(STDOUT_FILENO, (const char *)&length, 4);
    RawSocketUtils::writeAll(STDOUT_FILENO, (const char *)&header, 1);
    RawSocketUtils::writeAll(STDOUT_FILENO, &jsonString[0], jsonString.length());
    fflush(stdout);
  }

  while (true)
  {
    char header;
    // Data structures needed for select() and
    // non-blocking I/O.
    fd_set rfd;
    timeval tv;

    FD_ZERO(&rfd);
    FD_SET(STDIN_FILENO, &rfd);
    tv.tv_sec = 0;
    tv.tv_usec = 10000;
    select(STDIN_FILENO + 1, &rfd, NULL, NULL, &tv);

    // Check for data to receive; the received
    // data includes also the data previously sent
    // on the same master descriptor (line 90).
    if (FD_ISSET(STDIN_FILENO, &rfd))
    {
      LOG(ERROR) << "READING FROM STDIN";
      int32_t length;
      RawSocketUtils::readB64(STDIN_FILENO, (char *)&length, 4);
      LOG(ERROR) << "READ LENGTH: " << length;
      RawSocketUtils::readAll(STDIN_FILENO, (char *)&header, 1);
      header -= '0';
      length -= 1;
      LOG(ERROR) << "Got message header: " << int(header);
      switch (header)
      {
      case INSERT_KEYS:
      {
        string uid = string(p->id().c_str(), p->id().length());
        RawSocketUtils::readAll(STDIN_FILENO, &uid[0], uid.length());
        length -= uid.length();
        LOG(ERROR) << "READING FROM " << uid << ":" << length;
        string data(length, '\0');
        RawSocketUtils::readAll(STDIN_FILENO, &data[0], length);
        LOG(ERROR) << "READ FROM " << uid << ":" << data << " " << length;
        if (terminals.find(uid) == terminals.end()) {
          LOG(FATAL) << "Tried to write to non-existant terminal";
        }
        terminals[uid]->appendData(data);
        el::Loggers::flushAll();
        break;
      }
      case NEW_TAB:
      {
        string tabId = string(p->id().length(), '0');
        RawSocketUtils::readAll(STDIN_FILENO, &tabId[0], tabId.length());
        string paneId = string(p->id().length(), '0');
        RawSocketUtils::readAll(STDIN_FILENO, &paneId[0], paneId.length());

        Tab *t = s.add_tab();
        t->set_id(tabId);
        t->set_order(0);
        t->set_active(true);
        PaneOrSplitPane *posp = t->mutable_pane();
        Pane *p = posp->mutable_pane();
        p->set_id(paneId);
        p->set_active(true);

        terminals[p->id()] = shared_ptr<TerminalHandler>(new TerminalHandler());
        terminals[p->id()]->start();
        break;
      }
      default:
      {
        LOG(FATAL) << "Got unknown packet header: " << int(header);
      }
      }
    }

    for (auto &it : terminals)
    {
      const string &paneId = it.first;
      shared_ptr<TerminalHandler> &terminal = it.second;
      string terminalData = terminal->pollUserTerminal();
      if (terminalData.length())
      {
        header = APPEND_TO_PANE + '0';
        int32_t length = base64::Base64::EncodedLength(terminalData) + paneId.length() + 1;
        LOG(ERROR) << "WRITING TO " << paneId << ":" << length;
        RawSocketUtils::writeB64(STDOUT_FILENO, (const char *)&length, 4);
        RawSocketUtils::writeAll(STDOUT_FILENO, (const char *)&header, 1);
        RawSocketUtils::writeAll(STDOUT_FILENO, &(paneId[0]), paneId.length());
        RawSocketUtils::writeB64(STDOUT_FILENO, &terminalData[0], terminalData.length());
        LOG(ERROR) << "WROTE TO " << paneId << ":" << terminalData;
        fflush(stdout);
      }
    }
  }
}
}

int main(int argc, char **argv)
{
  setvbuf(stdin, NULL, _IONBF, 0);  //turn off buffering
  setvbuf(stdout, NULL, _IONBF, 0); //turn off buffering

  // Turn on raw terminal mode
  termios terminal_backup;
  termios terminal_local;
  tcgetattr(0, &terminal_local);
  memcpy(&terminal_backup, &terminal_local, sizeof(struct termios));
  cfmakeraw(&terminal_local);
  tcsetattr(0, TCSANOW, &terminal_local);

  // Setup easylogging configurations
  el::Configurations defaultConf = et::LogHandler::SetupLogHandler(&argc, &argv);
  defaultConf.setGlobally(el::ConfigurationType::ToStandardOutput, "false");
  el::Loggers::setVerboseLevel(3);
  // default max log file size is 20MB for etserver
  string maxlogsize = "20971520";
  et::LogHandler::SetupLogFile(&defaultConf,
                               "/tmp/htm.log",
                               maxlogsize);

  // Reconfigure default logger to apply settings above
  el::Loggers::reconfigureLogger("default", defaultConf);

  GOOGLE_PROTOBUF_VERIFY_VERSION;

  et::Main(argc, argv);

  tcsetattr(0, TCSANOW, &terminal_backup);

  return 0;
}

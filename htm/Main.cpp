#include "Headers.hpp"

#include "HTM.pb.h"

#include "LogHandler.hpp"
#include "MultiplexerState.hpp"
#include "RawSocketUtils.hpp"

#include <termios.h>

namespace google {}
namespace gflags {}
using namespace google;
using namespace gflags;

namespace et {
void Main(int argc, char **argv) {
  // Start by writing ESC + [###q
  char buf[] = {
      0x1b, 0x5b, '#', '#', '#', 'q',
  };
  fwrite(buf, 1, sizeof(buf), stdout);
  fflush(stdout);

  // Begin by sending an empty state (TODO: Daemonize HTM and support
  // re-attaching to the daemon)
  MultiplexerState state;

  LOG(ERROR) << "Starting terminal";

  {
    unsigned char header = INIT_STATE + '0';
    string jsonString;
    auto status = google::protobuf::util::MessageToJsonString(state.getState(),
                                                              &jsonString);
    VLOG(1) << "STATUS: " << status;
    int32_t length = jsonString.length();
    LOG(ERROR) << "SENDING INIT OF SIZE:" << length;
    RawSocketUtils::writeAll(STDOUT_FILENO, (const char *)&header, 1);
    RawSocketUtils::writeB64(STDOUT_FILENO, (const char *)&length, 4);
    RawSocketUtils::writeAll(STDOUT_FILENO, &jsonString[0],
                             jsonString.length());
    fflush(stdout);
  }

  while (true) {
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
    if (FD_ISSET(STDIN_FILENO, &rfd)) {
      LOG(ERROR) << "READING FROM STDIN";
      RawSocketUtils::readAll(STDIN_FILENO, (char *)&header, 1);
      header -= '0';
      LOG(ERROR) << "Got message header: " << int(header);
      int32_t length;
      RawSocketUtils::readB64(STDIN_FILENO, (char *)&length, 4);
      LOG(ERROR) << "READ LENGTH: " << length;
      switch (header) {
        case INSERT_KEYS: {
          string uid = string(UUID_LENGTH, '0');
          RawSocketUtils::readAll(STDIN_FILENO, &uid[0], uid.length());
          length -= uid.length();
          LOG(ERROR) << "READING FROM " << uid << ":" << length;
          string data(length, '\0');
          RawSocketUtils::readAll(STDIN_FILENO, &data[0], length);
          LOG(ERROR) << "READ FROM " << uid << ":" << data << " " << length;
          state.appendData(uid, data);
          break;
        }
        case NEW_TAB: {
          string tabId = string(UUID_LENGTH, '0');
          RawSocketUtils::readAll(STDIN_FILENO, &tabId[0], tabId.length());
          string paneId = string(UUID_LENGTH, '0');
          RawSocketUtils::readAll(STDIN_FILENO, &paneId[0], paneId.length());
          state.newTab(tabId, paneId);
          break;
        }
        case CLIENT_CLOSE_PANE: {
          string paneId = string(UUID_LENGTH, '0');
          RawSocketUtils::readAll(STDIN_FILENO, &paneId[0], paneId.length());
          state.closePane(paneId);
          break;
        }
        default: { LOG(FATAL) << "Got unknown packet header: " << int(header); }
      }
    }

    state.update();
  }
}
}  // namespace et

int main(int argc, char **argv) {
  setvbuf(stdin, NULL, _IONBF, 0);   // turn off buffering
  setvbuf(stdout, NULL, _IONBF, 0);  // turn off buffering

  // Turn on raw terminal mode
  termios terminal_backup;
  termios terminal_local;
  tcgetattr(0, &terminal_local);
  memcpy(&terminal_backup, &terminal_local, sizeof(struct termios));
  cfmakeraw(&terminal_local);
  tcsetattr(0, TCSANOW, &terminal_local);

  // Setup easylogging configurations
  el::Configurations defaultConf =
      et::LogHandler::SetupLogHandler(&argc, &argv);
  defaultConf.setGlobally(el::ConfigurationType::ToStandardOutput, "false");
  el::Loggers::setVerboseLevel(3);
  // default max log file size is 20MB for etserver
  string maxlogsize = "20971520";
  et::LogHandler::SetupLogFile(&defaultConf, "/tmp/htm.log", maxlogsize);

  // Reconfigure default logger to apply settings above
  el::Loggers::reconfigureLogger("default", defaultConf);

  GOOGLE_PROTOBUF_VERIFY_VERSION;

  et::Main(argc, argv);

  tcsetattr(0, TCSANOW, &terminal_backup);

  return 0;
}

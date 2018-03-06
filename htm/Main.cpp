#include "Headers.hpp"

#include "HTM.pb.h"

#include "RawSocketUtils.hpp"

using namespace et;

namespace google {}
namespace gflags {}
using namespace google;
using namespace gflags;

namespace htm {
void Main(int argc, char **argv) {
  // Start by writing ESC + [###q
  char buf[] = { 0x1b, 0x5b, '#', '#', '#', 'q', };
  fwrite(buf, 1, sizeof(buf), stdout);
  fflush(stdout);

  // Begin by sending an empty state (TODO: Daemonize HTM and support
  // re-attaching to the daemon)
  State s;
  unsigned char header = INIT_STATE;
  RawSocketUtils::writeAll(STDOUT_FILENO, (const char*)&header, 1);
  RawSocketUtils::writeProtoJson(STDOUT_FILENO, s);

  while(true) {
    RawSocketUtils::readAll(STDIN_FILENO, (char*)&header, 1);
    LOG(ERROR) << "Got message header: " << int(header);
    switch(header) {
      case UPDATE_WINDOWS: {
        auto newWindowList = RawSocketUtils::readProtoJson<WindowList>(STDIN_FILENO);
        LOG(ERROR) << "Got new window list";
        break;
      }
      case INSERT_KEYS: {
        auto paneDataPair = RawSocketUtils::readProtoJson<PaneDataPair>(STDIN_FILENO);
        LOG(ERROR) << "Got new keys for pane: " << paneDataPair.pane_id() << " -> " << paneDataPair.data();
        break;
      }
      default: {
        LOG(FATAL) << "Got unknown packet header: " << int(header);
      }
    }
  }
}
}

int main(int argc, char** argv) {
  ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  htm::Main(argc, argv);
  return 0;
}

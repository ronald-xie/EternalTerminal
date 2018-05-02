#include "Headers.hpp"

#include "HTM.pb.h"

#include "RawSocketUtils.hpp"
#include "sole.hpp"

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

  Buffer *b = s.add_buffer();
  b->set_pane_id(p->id());
  b->set_content("");

  {
    unsigned char header = INIT_STATE;
    string jsonString;
    auto status = google::protobuf::util::MessageToJsonString(s, &jsonString);
    VLOG(1) << "STATUS: " << status;
    uint32_t length = jsonString.length() + 1;
    RawSocketUtils::writeB64(STDOUT_FILENO, (const char *)&length, 4);
    RawSocketUtils::writeAll(STDOUT_FILENO, (const char *)&header, 1);
    RawSocketUtils::writeAll(STDOUT_FILENO, &jsonString[0], jsonString.length());
    fflush(stdout);
  }

  while (true)
  {
    unsigned char header;
    RawSocketUtils::readAll(STDIN_FILENO, (char *)&header, 1);
    header -= '0';
    LOG(ERROR) << "Got message header: " << int(header);
    switch (header)
    {
    default:
    {
      LOG(FATAL) << "Got unknown packet header: " << int(header);
    }
    }
  }
}
}

int main(int argc, char **argv)
{
  ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  et::Main(argc, argv);
  return 0;
}

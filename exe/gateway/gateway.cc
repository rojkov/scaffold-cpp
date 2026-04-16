#include "src/core/event/dispatcher_impl.hh"

int main() {
  carrot::event::DispatcherPtr dispatcher = std::make_unique<carrot::event::DispatcherImpl>();
  dispatcher->Run();

  return 0;
}

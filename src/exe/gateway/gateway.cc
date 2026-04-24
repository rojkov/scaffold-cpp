#include "core/common/signal_monitor.hh"
#include "core/event/dispatcher_impl.hh"

auto main() -> int {
  carrot::event::DispatcherSharedPtr dispatcher = std::make_shared<carrot::event::DispatcherImpl>();
  carrot::common::SignalMonitor signal_monitor(dispatcher);
  carrot::event::Command activate_signal_handling{.destination_ = &signal_monitor};

  // TODO: we can submit this command from the constructor of SignalMonitor.
  dispatcher->SubmitCommand(activate_signal_handling);
  dispatcher->Run();

  return 0;
}

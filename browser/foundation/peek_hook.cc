#include "myarc/peek_hook.h"

namespace myarc {
namespace {

PeekThrottleFactory g_factory = nullptr;

}  // namespace

void SetPeekThrottleFactory(PeekThrottleFactory factory) {
  g_factory = factory;
}

void MaybeAddPeekThrottle(content::NavigationThrottleRegistry& registry) {
  if (g_factory) {
    g_factory(registry);
  }
}

}  // namespace myarc

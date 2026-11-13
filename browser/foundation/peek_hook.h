#ifndef MYARC_PEEK_HOOK_H_
#define MYARC_PEEK_HOOK_H_

namespace content {
class NavigationThrottleRegistry;
}

namespace myarc {

// Chrome's navigation throttle list calls MaybeAddPeekThrottle for every
// navigation without depending on the browser window code that owns Peek.
// That code sets the factory when its first window starts. UI thread only.
using PeekThrottleFactory = void (*)(content::NavigationThrottleRegistry&);
void SetPeekThrottleFactory(PeekThrottleFactory factory);
void MaybeAddPeekThrottle(content::NavigationThrottleRegistry& registry);

}  // namespace myarc

#endif  // MYARC_PEEK_HOOK_H_

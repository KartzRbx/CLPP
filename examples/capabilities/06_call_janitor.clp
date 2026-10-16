link "./lib/calls.clp" as Calls;
<< An application-level call janitor; independent of the event's internal ~> manager.
signal<number> changed;
auto subscription = changed::Connect(func(value) -> void { post("received:" .: value); });
Calls.Defer(func() -> void { subscription::Disconnect(); post("disconnected"); });
Calls.Defer(func() -> void { post("last acquired, first cleaned"); });
Calls.Defer(func() -> void { report("one cleanup failed"); });
changed::Fire(7);
post(Calls.Pending());
post("cleanup failures:" .: Calls.Clean());
changed::Fire(9);
post(subscription.Connected);
post(Calls.Pending());
post(Calls.Clean()); << Idempotent after draining.

# PPP queue construction regression

This fixture builds actual NED interfaces and queues through the same capacity
hook used by `LeoChannelConstructor`. It checks the LEO PPP interface at t=0 and t=1s, including:

- Original default queue types.
- Explicit queue selection: drop-tail, drop-head, PriorityQueue, and OmittedPacketQueue.
- Default capacity 300, an explicit per-queue override of 73, and unlimited (-1).
- Preservation of a compound queue's inner capacities.
- A queue without `packetCapacity`, and deletion before initialization.

This is a **construction-only** regression. It does not initialize PPP, send
packets, exercise congestion control, or prove handover/telemetry correctness.
After it passes, a normal LEO experiment is still needed for those checks.

After rebuilding the `leosatellites` library and its INET/OS3 dependencies, run from
this directory in an OMNeT++ environment (release libraries shown):

```bash
opp_makemake -f --make-so -o queue_compatibility
make MODE=release

args=(
  -u Cmdenv
  -n '.:../../src:../../../inet4.5/src:../../../os3/src'
  -l ../../../inet4.5/src/INET
  -l ../../../os3/src/os3
  -l ../../src/leosatellites
  -l ./queue_compatibility
)
opp_run "${args[@]}" -c Defaults
for run in {0..3}; do
  opp_run "${args[@]}" -c Selected -r "$run" || break
done
```

Each successful run prints `PASS: queue selection and capacity at t=0 and t=1s`.

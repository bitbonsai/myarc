# Native workspace storage retry

## Observed baseline through0022

Fresh marked disposable Profile, real fixture Favorite and known-written metadata. The original file was retained byte-for-byte in an owned backup. Replacing only its metadata filename with an empty directory caused a real atomic-write failure after native Create Space. The new Space remained in memory and an inspected native warning asked the user to fix disk space/permissions and retry saving. A native retry control was absent.

Clean quit/relaunch against the same faulty path retained the original backup and showed the real native page with workspace fallback. Workspace mutation controls were absent. The visible warning asked to retry loading, again without a native retry control. The service already implements `RetryLoad()`/`RetrySave()`; the UI does not connect them. [Structured baseline](../evidence/workspace-storage-retry.json).

This was deliberate input/I/O fault injection in one synthetic profile, not repair of an acceptance snapshot. No workspace IDs or fields were manufactured. The unsaved memory mutation was not counted as durable. The bounded AX walk omitted the warning label, so its text was inspected in actual captures instead. No complete accessibility claim follows.

Private run `/tmp/myarc-m0-runtime/storage-fault-20261026T024335Z/` retains the exact backup, fault audit, real writer error and write/read captures. Apps/helpers/server closed; faulty directory and backup retained. Proposed narrow repair exposes real state-specific retry actions, preserving bad input and using the existing Profile-bound service. No automatic reset, invented recovery database or durability claim.

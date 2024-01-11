Move orch.* out into a lua script
Implement a target object in C, make it available to _G
Remove target from one() pcall _G

Expected flow:
 - orch(1) stalls child until signalled (after lua setup)
 - dofile(libexec/orch.lua)
 - orch.lua executes the user script in a pcall
 - orch.lua starts processing the queue
   - queued action types: release write, match, one
   - release only valid once, before one / match
   - when hit a match block, block for input
   - when hit a one block, pcall it (no hold/release/write)
       then block for input and multiplex

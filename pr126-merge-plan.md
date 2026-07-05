# Plan: Resolving merge conflicts for PR #126 ("V2 - Increase frametime accuracy")

## Summary

PR #126 (`v2` → `main`) branched off commit `0bd595b` (merge of PR #100 "neon"),
back on 2026-06-18-ish. Since then, `main` has picked up two substantial pieces
of work that touch the *same* hot paths `v2` rewrote:

- **PR #101** "Improve timing precision to sub-millisecond accuracy and fix
  accumulative drift" — rewrote `EffectsManager::Start()`'s frame-timing loop.
- **PR #103** "schedule-and-robustness" — added `ISchedule`-based effect
  switching, heartbeat/blackout frames when nothing is scheduled, and socket
  reconnect diagnostics (`RecordConnectFailure`/`RecordConnectSuccess`,
  `_lastSocketError`).
- **PR #102** "Optimize color math and enhance performance" — rewrote the hot
  paths in `ledfeature.h::GetPixelData()` and `SocketChannel::WorkerLoop()` for
  performance (avoiding extra copies/allocations), and fixed a reversed-pixel
  correctness bug for multi-row features.

`v2`'s stated purpose is *also* "increase frametime accuracy" plus adding the
`StockBanner` effect. So both sides indepedently rewrote the same frame-timing
and socket-I/O code with **overlapping but not identical goals**. This is a
real semantic merge, not just a textual one — several hunks require blending
logic from both sides rather than picking "ours" or "theirs" wholesale.

Trial merge (`git merge v2` on top of current `main`, not committed) produces
conflicts in exactly 4 files:

| File | Conflict hunks | Nature |
| --- | --- | --- |
| `interfaces.h` | 1 | Cosmetic (parameter rename only) |
| `ledfeature.h` | 2 | One cosmetic-ish, one **real correctness/behavior difference** |
| `effectsmanager.h` | 4 | **Substantial** — parallel rewrites of the same worker-thread loop |
| `socketchannel.h` | 5 | **Substantial** — parallel rewrites of `WorkerLoop`/`ConnectSocket` |

Everything else in the PR (`apihelpers.cpp/.h`, `effects/stockbannereffect.h`,
`main.cpp`, `secrets.h.example`, `tests/*`, `webserver.h`) auto-merges cleanly
and needs no manual attention — those are additive (new effect, new secret
key, new test) and don't overlap with `main`'s concurrent changes.

---

## File-by-file analysis and recommended resolution

### 1. `interfaces.h` — trivial

```
virtual vector<uint8_t> GetDataFrame(system_clock::time_point targetTime) const = 0;   // main
virtual vector<uint8_t> GetDataFrame(system_clock::time_point displayTime) const = 0;  // v2
```

Pure parameter-name difference on a pure-virtual declaration; no behavior
implication by itself. **Resolution:** pick whichever name matches the final
`ledfeature.h` implementation (see below) for consistency. Since we're keeping
main's per-feature `TimeOffset()` semantics (see #2), the parameter still
represents a base time before per-feature offsetting — `targetTime` reads more
accurately, but this is a naming call, not a functional one.

### 2. `ledfeature.h` — one cosmetic, one real behavior fix to preserve

**Hunk A (`GetPixelData` fast-path condition):**

- `main`: `if (full-canvas-bounds-match)` → fast path always taken for a
  full-canvas feature, regardless of `_reversed`.
- `v2`: `if (full-canvas-bounds-match && (!_reversed || _height == 1))` →
  fast path skipped for **multi-row reversed** features, falling back to the
  slow per-pixel path (which reverses each row independently, keeping
  `canvasY` fixed).

This is not cosmetic — it's a correctness fix. `main`'s
`Utilities::ConvertPixelsToByteArray(pixels, reversed, redGreenSwap)` (from
PR #102) reverses the **entire flattened pixel buffer** when `reversed` is
true, which is only equivalent to a proper per-row reversal when there's a
single row. For a multi-row reversed feature that happens to cover the full
canvas, `main`'s fast path would incorrectly reverse pixels *across* rows,
not just within each row. `v2`'s extra `_height == 1` guard exists precisely
to avoid hitting that bug.

**Resolution:** keep `v2`'s guard (`&& (!_reversed || _height == 1)`) grafted
onto `main`'s fast-path condition. This is an "and both", not an "or" —
verify by re-reading `main`'s `Utilities::ConvertPixelsToByteArray` at merge
time in case it changes again before the merge lands.

**Hunk B (`GetDataFrame` body):**

- `main`: takes `targetTime`, adds `TimeOffset()` (this feature's own,
  individually-configured offset) converted to microseconds, then computes
  seconds/microseconds from the result.
- `v2`: takes `displayTime` and uses it as-is — no per-feature offset applied
  inside `GetDataFrame` at all.

This isn't a style difference, it's a **semantic regression** if taken
as-is: `v2` moved the time-offset math out of `GetDataFrame` and into
`EffectsManager::Start()` (see effectsmanager.h below), where it computes a
single **maximum** offset across *all* features on the canvas and bakes that
into a shared `displayTime` used for every feature's frame. That means, under
`v2`, every feature effectively gets the *same* (largest) offset rather than
its own — a controller with `timeOffset: 0` would inherit a neighboring
controller's `timeOffset: 0.2` if that neighbor's offset happens to be the
max on the canvas.

**Resolution:** keep `main`'s behavior — apply `TimeOffset()` per feature
inside `GetDataFrame`, using the plain (non-offset-adjusted) `displayTime`
computed by `EffectsManager`. Do **not** carry over `v2`'s "max offset across
features" logic. This preserves the documented API contract
(`feature.timeOffset` is per-feature, see `to_json`/`from_json` for
`ILEDFeature`) and is a deliberate rejection of one of `v2`'s changes, not
just conflict-marker plumbing — flag this clearly in the PR/commit message so
the `v2` author can confirm this wasn't an intentional design change they
still want.

### 3. `effectsmanager.h` — the core of the merge, needs a rewrite, not a pick

`main`'s `Start()` (from PR #101 + #103) gained, since the `v2` branch point:

- Schedule-based effect switching (`ISchedule::IsActive()`), checked every
  frame while something is active, every 500ms while idle (CPU-saving).
- Heartbeat/blackout frames (clear canvas, send black frames every 2s) when
  no scheduled effect is active, with a log message on state transitions.
- Drift detection + logging (compares intended packet timestamp to wall
  clock, logs if divergence > 200ms).
- A "sane reset" if the loop falls more than 1s behind.

`v2`'s `Start()` (the actual point of this PR) independently reworked the
timing math to fix accuracy problems:

- `frameDuration` computed via `duration_cast<steady_clock::duration>(duration<double>(1.0/fps))`
  instead of `nanoseconds(1'000'000'000LL / fps)` — avoids truncating to
  whole nanoseconds up front, which is where `main`'s current drift can
  originate for FPS values that don't divide evenly.
- Schedules frames by `frameIndex` (`steadyStartTime + frameDuration * (frameIndex + 1)`)
  and, if the loop falls more than one frame behind, **jumps** `frameIndex`
  forward based on elapsed time rather than looping through every missed
  frame — avoiding a "bursty catch-up" of queued frames after a stall.
- Sends frames through `SocketChannel::CompressFrame()` before enqueueing
  (this method already exists on both branches, just wasn't being invoked
  from the render loop on `main`).
- Clamps FPS to a minimum of 1 (`max<uint16_t>(1, _fps)`).

**Resolution — don't pick a side, merge the two loops' concerns:**

1. Keep `main`'s outer control flow as the skeleton: schedule check →
   active/inactive branch → heartbeat/blackout → drift detection → sane
   reset.
2. Replace `main`'s frame-duration/frame-time math (integer-nanosecond
   `frameDurationNs`, `frameCount`) with `v2`'s double-precision
   `frameDuration` + `frameIndex` approach, including the "jump forward on
   large stall" catch-up-avoidance logic. Apply it to *both* the "effect
   active" path and the "heartbeat" path, since `main` currently uses the
   same `packetTimestamp` for both.
3. Carry over `v2`'s FPS-clamp (`max<uint16_t>(1, _fps)`) — cheap safety net,
   no conflict with anything on `main`.
4. Do **not** carry over `v2`'s "max offset across features" `displayTime`
   computation — per the `ledfeature.h` decision above, per-feature
   `TimeOffset()` application stays inside `GetDataFrame`, so
   `EffectsManager` just needs to pass a shared, un-offset `displayTime`/
   `packetTimestamp` (naming to match whatever `interfaces.h` settles on).
5. Add `feature->Socket()->CompressFrame(...)` before
   `EnqueueFrame(...)` in both the "active effect" and "heartbeat" send
   sites in `main`'s loop — this looks like a straightforward win `v2`
   added and `main` never got, not something in tension with the
   scheduling work.
6. Keep `main`'s `SetEffects()`, which also resets `_currentEffectIndex` to
   0 when effects were empty — `v2`'s version of this method dropped that
   line; that's an unrelated behavior loss on `v2`'s side with no apparent
   justification (auto-merges cleanly today only because `v2` didn't touch
   this hunk in a way that conflicts — worth double-checking it doesn't
   silently regress once the file is hand-edited).
7. `to_from_json_map`: union both branches' new entries —
   `main` added `AuroraEffect`, `v2` added `StockBanner`. Both need to stay;
   this is an additive list, not a real conflict.

This file is the highest-risk part of the merge because the resulting logic
needs fresh reasoning (and a fresh read of the merged result end-to-end)
rather than a mechanical resolution — recommend writing it out by hand and
re-reading the whole `Start()` method once assembled, rather than resolving
hunk-by-hunk in isolation.

### 4. `socketchannel.h` — mostly a performance-vs-correctness pick, one real feature to preserve

Conflicts cluster in `WorkerLoop()` and the tail of `ConnectSocket()`.

**`WorkerLoop()`:**

- `main` (PR #102, performance-focused): declares `combinedBuffer` once
  outside the loop and `.reserve(4096)`s it up front, reusing the capacity
  across iterations (`combinedBuffer.clear()` inside the loop instead of
  reallocating); avoids copying the whole `_frameQueue` just to size the
  buffer; decouples response-reading from sending — `ReadSocketResponse()`
  is polled at most once per `pollInterval` (1000ms) via `lastPollTime`,
  rather than on every send.
- `v2`: declares `combinedBuffer` fresh inside every loop iteration; copies
  the entire `_frameQueue` (`auto queueCopy = _frameQueue;`) just to
  pre-compute `tempBytes` for a `.reserve()` call, then drains the real queue
  separately; reads the response inline via `SendFrame()`'s return value
  (`optional<ClientResponse>`) on every send, not throttled.

Given `kMaxBatchSize == 1` on both sides, `v2`'s queue-copy-for-sizing is a
wasted full-queue copy every iteration for a one-element reserve — that's
pure regression relative to `main`'s PR #102 optimization, and it isn't
something `v2`'s "frametime accuracy" goal needs.

**Resolution:** keep `main`'s `WorkerLoop()` structure and buffer/queue
handling wholesale (persistent buffer, throttled polling). The only thing to
check: does `v2`'s inline-response-per-send interact with anything the
`StockBanner` effect or the new tests depend on (e.g. a test asserting on
`_lastClientResponse` freshness)? Skim `tests/tests.cpp`'s diff (it merges
clean, but re-read it once `socketchannel.h` is finalized) to confirm no test
assumes response-per-frame semantics.

**`ConnectSocket()` tail (the actual conflicting hunk):**

- `main`: inline `_reconnectCount++` + conditional log ("Connected" first
  time, "Reconnection #N" debug-level after).
- `v2`: calls `RecordConnectSuccess()` (sets `_isConnected = true`,
  increments `_reconnectCount`, clears `_lastSocketError`, all under
  `_mutex`) then logs "Connection number N" at info level every time.

`v2` also introduces `RecordConnectFailure(error)` calls throughout the rest
of `ConnectSocket()` (every early-return branch) — these are **not** in
conflict with `main` at all (they auto-merge cleanly as pure insertions),
which means today's auto-merge already gives you `RecordConnectFailure(...)`
calls scattered through the function, feeding a `_lastSocketError` /
presumably `_failedConnectCount` that `main` doesn't otherwise populate or
lock consistently outside of this new code path. `main` doesn't set
`_isConnected = true` under the same lock discipline at the point of success
that `RecordConnectFailure` uses to set `_isConnected = false` — i.e. `main`
sets `_isConnected = true` elsewhere (in `SendFrame`, per the earlier grep)
without going through a matching helper.

**Resolution:**
1. Keep `v2`'s `RecordConnectFailure`/`RecordConnectSuccess` helpers and all
   call sites (this is a legitimate diagnostics feature, matches the
   `webserver.h` diff exposing `_lastSocketError`/failure counts via the
   API, and doesn't conflict with any of `main`'s concurrent work).
2. For the actual conflicting tail hunk, resolve to **`RecordConnectSuccess()`**
   (not `main`'s inline increment) so success and failure go through
   matching, mutex-guarded bookkeeping — but keep `main`'s two-tier log
   message (info-level once, debug-level on reconnects) since it's more
   useful operationally than `v2`'s uniform info-level log, which would spam
   the log on every reconnect. I.e. call `RecordConnectSuccess()`, then branch
   the log line on `GetReconnectCount() == 1` for level.
3. After resolving, grep the whole file for any other place `main` sets
   `_isConnected`/`_reconnectCount` directly (bypassing the new helpers) and
   decide whether to route those through the helpers too for consistency —
   likely a small follow-up cleanup, not required to unblock the merge.

---

## Execution plan (in progress — see status below)

Because C++ headers all compile together (nearly-header-only codebase, per
`CLAUDE.md`), the tree won't build at all while *any* file still has literal
`<<<<<<<` conflict markers in it — so a merge can't be built/tested
file-by-file in the naive sense. The approach used instead:

1. **Branch**: `merge/pr-126-v2`, created off the up-to-date `main` tip
   (commit `77b2bf7`). Work happens here, not on `main`.
2. **Start the merge** with `git merge v2 --no-commit --no-ff`, which
   reproduces the 4-file conflict set analyzed above (everything else
   auto-merges cleanly).
3. **Get to a compilable baseline immediately**: resolve all 4 conflicted
   files mechanically by keeping `main`'s side (`git checkout --ours <file>`)
   as a placeholder, `git add` them, and commit. This is a real merge commit
   (parents: `main` tip + `v2` tip) whose tree is functionally identical to
   `main` plus `v2`'s cleanly-auto-merged additions (the `StockBanner`
   effect, its catalog registration, the new secret key, the new tests). It
   deliberately does **not** yet contain `v2`'s intended changes to the 4
   conflicted files — that's the point, it's the safe starting line.
4. **Replace each placeholder with its real resolution, one file at a time,
   each its own commit, building and running the test suite after each
   one** (per this repo's `run-tests` skill: `make`, `make -C tests`, run
   `ndscpp` in the background, run the suite):
   - Step A — `interfaces.h`: apply the real resolution (parameter name
     decision, see above).
   - Step B — `ledfeature.h`: apply the real resolution (keep the
     `_reversed`/`_height==1` fast-path guard; keep `main`'s per-feature
     `TimeOffset()` application inside `GetDataFrame`).
   - Step C — `socketchannel.h`: apply the real resolution (adopt `main`'s
     `WorkerLoop` wholesale; keep `v2`'s `RecordConnectFailure`/
     `RecordConnectSuccess` helpers and call sites; resolve the
     `ConnectSocket()` tail to call `RecordConnectSuccess()` while keeping
     `main`'s tiered log-level behavior).
   - Step D — `effectsmanager.h` (**deferred, out of scope for this pass**):
     still sitting at the `main`-only placeholder. This is the one file that
     needs genuine hand-written blending (see the `effectsmanager.h`
     section above) rather than a pick, and is intentionally left for a
     separate session/pass. Until this step happens, the branch does not
     contain `v2`'s frametime-accuracy improvements or its `CompressFrame()`
     wiring into the render loop — only the `StockBanner` effect and the
     other 3 files' resolutions.
5. **Once Step D is eventually done**: full manual smoke test — run `ndscpp`
   with a config that has (a) a multi-row `reversed: true` feature, and (b)
   two features on the same canvas with different `timeOffset` values, and
   visually confirm both still behave correctly (no row-reversal corruption,
   no offset bleed-through between features).
6. **Before opening/updating the PR**: call out explicitly, in the merge
   commit message or PR description, that `v2`'s "shared max time-offset
   across features" approach was deliberately not carried over — see the
   question drafted below to send to the PR author, in case it was actually
   an intended design change rather than an incidental simplification.

### Status

| Step | File | Status |
| --- | --- | --- |
| 1–3 | Branch + merge started + placeholder commit | ✅ done |
| A | `interfaces.h` | ✅ done (combined with C, see note below) |
| B | `ledfeature.h` | ✅ done |
| C | `socketchannel.h` | ✅ done (combined with A, see note below) |
| D | `effectsmanager.h` | ✅ done |

All 4 conflicting files are resolved. `make` and `make -C tests` pass, all
14 tests pass (including `ReversedMatrixFeatureMirrorsRowsForHardware`).
The PR author confirmed (2026-07-05) the `TimeOffset` question above wasn't
an intentional design decision, which validates the Step B/D resolution to
keep `main`'s per-feature offsetting.

Branch: `merge/pr-126-v2`, based on `main` @ `77b2bf7`, merging in `v2`
@ `ba3f726`. Each completed step is its own commit — see `git log
merge/pr-126-v2`.

**Remaining before this is ready to land**, per the execution plan above:
manual smoke test (multi-row reversed feature + differing per-feature
`timeOffset` values, run visually against real or simulated hardware) and
a final read-through of the assembled diff. No code conflicts remain.

### Notes from executing Steps 1–3, A, B, C

Two things came up during execution that are worth recording so a future
session doesn't have to rediscover them:

1. **`git checkout --ours <file>` is not a safe placeholder for "resolve
   conflicts to main's side."** It replaces a conflicted file's entire
   working-tree content with `main`'s version of the whole file — including
   hunks that `v2` changed *without* conflicting with `main` (i.e. hunks
   git's real 3-way merge would have taken from `v2` automatically). This
   silently dropped, from the initial placeholder commit: `ledfeature.h`'s
   per-row reversed-pixel fix in the slow path, `interfaces.h`'s
   `GetFailedConnectCount()`/`GetLastSocketError()` declarations, and
   `socketchannel.h`'s entire `RecordConnectFailure`/`RecordConnectSuccess`
   diagnostics feature. All of these were recovered while doing Steps A–C
   by reconstructing the actual 3-way merge per file (`git merge-file -p
   <ours> <merge-base> <theirs>`) and resolving only the real conflict
   markers within it, rather than resolving from the `--ours` placeholder.
   **If resuming for Step D (`effectsmanager.h`), do the same** — don't
   hand-edit from the current placeholder; regenerate the true 3-way merge
   for that file first, since it likely has the same problem (any
   non-conflicting `v2` insertions in `effectsmanager.h` were also wiped by
   the placeholder step).
2. **Steps A (`interfaces.h`) and C (`socketchannel.h`) turned out to be
   compilation-coupled**, not independent: `interfaces.h`'s new
   `ISocketChannel` pure virtuals (`GetFailedConnectCount`/
   `GetLastSocketError`) require `socketchannel.h` to implement them or
   `SocketChannel` becomes an abstract, uninstantiable class. They landed
   in one commit together. `ledfeature.h` (Step B) also consumes those two
   new methods (in its `to_json`), but its own core resolution (the
   fast-path guard and `TimeOffset()` decision) doesn't depend on them, so
   it stayed a separate commit, applied after A+C.
3. **Two incidental, merge-induced build breaks were fixed along the way**
   (separate commit, before A/B/C): `effects/stockbannereffect.h`'s and
   `tests/tests.cpp`'s `Update`/`UpdateCurrentEffect` overrides used
   `milliseconds` to match `v2`'s own (now-superseded) interface, and
   needed retyping to `microseconds` to match `main`'s interface — both
   parameters are unused, so this is behavior-neutral. A third,
   pre-existing and unrelated `-Wreorder` bug on `main` itself (in
   `socketchannel.h`'s constructor initializer list) was also fixed there,
   since it blocked the `-Werror` test build entirely.

## Question sent to the PR author — answered

In the `v2` branch, `EffectsManager::Start()` now computes one `displayTime`
per frame by taking the **maximum** `TimeOffset()` across all of a canvas's
features, and applies that single shared value to every feature's frame.
Previously (and still on `main`), each `ILEDFeature::GetDataFrame()` applied
its *own* `TimeOffset()` individually, so two features with different
`timeOffset` values on the same canvas would each get their own offset
rather than both inheriting the largest one present.

**Author's answer (2026-07-05):** not a deliberate change — they don't
recall doing it, and guess it may be an incidental side effect of
vibe-coded visual work on the scrolling stock banner. Their guidance: take
whatever the shortest path to resolving the merge is. This confirms the
Step B resolution already applied in `ledfeature.h` (keep `main`'s
per-feature `TimeOffset()` application, don't carry over `v2`'s shared/max
offset) was the right call — no further changes needed on that front.

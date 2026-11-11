# Idle floor: a fence per submission (2026-11-11)

Finding: baseline rank 5 ("idle floor") was `vkCreateFence` in vrento's `TextureContext::Submit`, once per frame. On NVIDIA
each fence opens a new `/dev/nvidia0` client and runs about a dozen ioctls (strace of noita: openat + 4 close + 4 fcntl
+ 8 ioctl per frame; gdb on `openat` of /dev/nvidia0: 402 of 492 calls came from `CreateFence`). Fix (vrento
`perf-fence-pool`): a completed submission fence goes back to an idle list on its queue and is reset for the next
submission. A pooled exportable semaphore in owe was tried first and gave nothing (reverted).

Method: `bench/run.py --paced 5` (60 fps realtime, one core = 100 %) and uncapped, 1080p, RTX 4070, same machine, base =
lab-all 59a3b26d build, new = this branch. Raw: `2026-11-11-fence-{before,after}.json`.

| wallpaper | uncapped ms | CPU ms/frame | CPU % at 60 fps |
|---|---|---|---|
| water-effect-13 | 0.639 -> 0.231 | 0.563 -> 0.147 | 7.3 -> 3.9 |
| nebula | 1.068 -> 0.273 | 0.734 -> 0.118 | 7.4 -> 4.9 |
| noita | 0.624 -> 0.192 | 0.560 -> 0.139 | 7.5 -> 4.7 (perf stat: 6-8 -> 2.5) |
| rain | 5.93 -> 4.59 | 3.88 -> 3.01 | 21.8 -> 19.7 |
| sculpture-forest | 7.67 -> 5.82 | 4.35 -> 2.91 | 25.1 -> 18.9 |
| reze | 7.23 -> 7.31 | 4.37 -> 4.46 | 30.3 -> 29.4 |
| ocarina-of-time | 28.6 -> 33.9 (noise: machine shared with other benches) | | 97.5 -> 96.0 |

Images are byte-identical to the base build on noita, water-effect-13 and nebula (particle scenes are not
deterministic run to run even on the base build). Remaining per-frame syscalls are the sync-fd export (3 ioctls, fd dup).

## Fast hasher for the frame-graph maps (vrento `perf-fast-hash`)

Interleaved A/B, 3 rounds, medians (`2026-11-11-fasthash-ab.json`, config `pose` = before, `fx` = after), uncapped ms / CPU ms:
ocarina 30.6 -> 28.9, arknights 6.34 -> 5.81, reze 7.36 -> 7.18 (CPU 4.26 -> 3.50), rain 5.00 -> 4.66,
sculpture-forest 6.57 -> 6.27, littlebigplanet unchanged (12.4). Source: SipHash (`rstd::hash::DefaultHasher`, a byte at a
time) was 7-10 % of Ocarina's samples.

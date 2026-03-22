# Multi-Core Cache Coherence Simulator

A multi-core cache hierarchy simulator implementing the **MESI coherence protocol** with LRU replacement, snooping bus, and inclusive write-back policy.

---

## Basics

**Cache Line:** 64 bytes  
**Physical Address:** 32 bits

| Field  | Bits | Formula |
|--------|------|---------|
| Offset | 6    | `address & ((1 << 6) - 1)` |
| Set    | log₂(#sets) | `(address >> 6) & (numSets - 1)` |
| Tag    | 32 - (Offset + Set) | `address >> (6 + bitSets)` |

---

## Cache Structure

<img width="1781" height="1780" alt="graph" src="https://github.com/user-attachments/assets/58e93321-4750-4910-8585-3a95edcd4ed6" />


Each cache level is a **set-associative cache** backed by an LRU eviction policy.  
Every set is an `LRUCache` — a doubly linked list paired with a hash map for O(1) lookup and O(1) LRU updates.  
Each node in the list is a `CacheLine` struct holding a 64-byte data block, a tag, and a MESI state.

| Level | Sets | Ways | Size  |
|-------|------|------|-------|
| L1    | 4    | 4    | 1 KB  |
| L2    | 8    | 8    | 4 KB  |
| L3    | 16   | 11   | 11 KB |

---

## Cache Hierarchy

![Cache Hierarchy](cache_hierarchy.png)
<img width="2501" height="2321" alt="core" src="https://github.com/user-attachments/assets/904411fc-cdac-4952-9410-9a9ca153bd47" />

Each core owns a **private** L1, L2, and L3. All L3s connect to a shared snooping bus.  
On a read miss, the request cascades down: L1 → L2 → L3 → Bus → RAM.  
On a write, the requesting core invalidates all other copies via the bus before modifying its L1.

---

## MESI Protocol

| State | Meaning |
|-------|---------|
| **M** odified  | Dirty, sole owner. Must write back before eviction. |
| **E** xclusive | Clean, sole owner. Can upgrade to M on write without bus transaction. |
| **S** hared    | Clean, multiple cores may have copies. Must notify bus before write. |
| **I** nvalid   | Line is not valid. Treated as a miss. |

---

## API

### `bus.h`
| Function | Description |
|----------|-------------|
| `addListener(L3)` | Register a core's L3 with the bus |
| `readBus(address, core_id)` | Snoop all other L3s on a read miss. Returns dirty data if found |
| `writeBus(address, core_id)` | Invalidate all other copies on a write |
| `printStats()` | Print bus traffic metrics |

### `core.h`
| Function | Description |
|----------|-------------|
| `write(address, data)` | CPU write — checks bus before modifying L1 |
| `read(address)` | CPU read — returns value from closest cache level |
| `printStats()` | Print per-level hit/miss rates and AMAT |
| `calculateAMAT()` | Compute Average Memory Access Time across all levels |

### `lru.h`
| Function | Description |
|----------|-------------|
| `get(key)` | O(1) lookup, moves line to MRU position |
| `put(key, data, state)` | Insert new line at MRU, evict LRU if full |
| `contains(key)` | Check if tag exists in set |
| `peek()` | View LRU victim without removing |
| `eviction()` | Remove and return LRU victim |
| `remove(tag)` | Remove specific line by tag |

### `setAssociativeCache.h`
| Function | Description |
|----------|-------------|
| `readBlock(address)` | Miss cascade — fetch from next level or bus |
| `write(address, data)` | Modify data in place, set state to MODIFIED |
| `writeBlock(address, data, state)` | Write a full 64-byte block, used during eviction and coherence |
| `evictIfNeeded(setIdx)` | LRU eviction with dirty writeback to next level |
| `backInvalidate(address)` | Pull fresh data from L1/L2 down before eviction |
| `flushToMe(address)` | Flush dirty data down WITHOUT removing lines — used by snoop |
| `snoop(address, type)` | Respond to bus read (type=0) or write (type=1) request |
| `invalidateUp(address)` | Invalidate L1/L2 after snoop write — ensures full coherence |
| `set/tag/offset(address)` | Address decomposition helpers |
| `printStats(levelName)` | Hit rate, miss rate, evictions, coherence events |
| `printCache(levelName)` | Dump all valid lines with state and data |

---

## Design Decisions

**Why `flushToMe` instead of `backInvalidate` in snoop?**  
`backInvalidate` removes lines as it flushes — this caused the snooped L3 line to disappear before snoop could read its state, resulting in a null pointer crash. `flushToMe` pushes dirty data down without removing lines, keeping the L3 line intact for the bus response.

**Why `writeBus` fires after `readBlock` in `Core::write`?**  
The requesting core needs to fetch the current data from other cores before invalidating them. Firing `writeBus` first would invalidate the only source of fresh data, forcing a stale RAM read.

**Why `invalidateUp` in snoop write case?**  
`writeBus` only snoops L3. Without `invalidateUp`, the target core's L1 would still hold a stale MODIFIED copy that would be incorrectly returned on the next read.

**Why check `state != INVALID` in `readBlock` hit path?**  
INVALID lines physically remain in the set after invalidation. Without this check, `contains()` would return true and the cache would serve a stale value instead of fetching fresh data.

---

## Performance Metrics

[Add your stats output here after running test cases]

```
========== CORE 0 ==========
L1 hit rate:
L2 hit rate:
L3 hit rate:
AMAT:
Coherence invalidations:
Wasted snoops:

=== Bus Stats ===
Read transactions:
Write transactions:
Wasted snoop ratio:
```

---

## Test Cases

| Test | Description | Expected |
|------|-------------|----------|
| Basic read/write | Single core write then read | Returns written value |
| Cross-core read | Core 0 writes, Core 1 reads | Core 1 gets correct value |
| Write contention | Core 0 writes, Core 1 overwrites, Core 0 reads | Core 0 sees Core 1's value |
| Three-core contention | All cores write, last writer wins | All cores see last written value |
| False sharing | Different words, same cache line | Both cores see correct values |
| Ping pong | Alternating writes between cores | Final reader sees last write |
| Temporal locality | Same address accessed repeatedly | L1 hits after first miss |

---

## Build & Run

```bash
g++ -std=c++17 -o cache_sim bus.cpp core.cpp lru.cpp setAssociativeCache.cpp
./cache_sim
```

For debug output with address sanitizer:
```bash
g++ -std=c++17 -fsanitize=address -g -o cache_sim bus.cpp core.cpp lru.cpp setAssociativeCache.cpp
./cache_sim
```

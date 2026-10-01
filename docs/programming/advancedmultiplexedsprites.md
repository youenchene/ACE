# Using advanced multiplexed sprites

## About advanced multiplexed sprites

The [advanced multiplexed sprite manager](../../include/ace/managers/advancedmultiplexedsprite.h)
adds animation frames, 32px-wide images and attached 4bpp images to the
[ordinary multiplexed sprite manager](multiplexedsprites.md). One advanced
object contains several independently positioned, enabled and animated logical
elements, sharing a fixed set of consecutive hardware channels and copied frames.

"4 colors" means **three opaque colors plus transparent index 0** (2bpp).
"16 colors" means **15 opaque colors plus transparent index 0** (4bpp).
The application must load the palette and configure sprite/playfield priority.
Attached sprites use palette entries 17..31 for opaque indices 1..15. Ordinary
2bpp pieces use the palette group of their hardware channel; see the ordinary
manager's palette description.

## Hardware resource budget

| Width | Depth | Channels per object | Valid starting channels |
|-------|-------|---------------------|-------------------------|
| 16px | 2bpp | 1 | 0..7 |
| 32px | 2bpp | 2 | 0..6 |
| 16px | 4bpp | 2 | 0, 2, 4, 6 |
| 32px | 4bpp | 4 | 0, 2, 4 |

All required channels must be free in the shared multiplexer pool. They remain
reserved even when the object or its elements are disabled. Logical element
count does not multiply this reservation: those elements reuse the channels at
different vertical positions. It also does not increase the hardware's per-line
capacity.

Attachment always pairs an **even channel with the following odd channel**.
Low planes 0/1 go to the even channel; high planes 2/3 go to the odd channel with
ATTACH set. Both halves have the same X and Y. A 32px image uses a second 16px
column at **X + 16**, rather than shifting the attached half:

| Piece of a 32px/4bpp image | Channel offset | Position | Source planes |
|--------------------------|----------------|----------|---------------|
| Left low | 0 | X, Y | 0/1 |
| Left high, attached | 1 | X, Y | 2/3 |
| Right low | 2 | X + 16, Y | 0/1 |
| Right high, attached | 3 | X + 16, Y | 2/3 |

This implements width with multiple 16px hardware pieces, not AGA wide-sprite mode.
For 32px/2bpp starting on an odd channel, the two columns use different hardware
palette groups; populate both groups appropriately if they should look identical.

## Initialization and compatible strips

Include `<ace/managers/advancedmultiplexedsprite.h>`. There is **no separate
advanced manager constructor**. Create the view, then initialize the shared
manager once, before adding objects and before loading the view:

```c
spriteMultiplexedManagerCreate(pView, uwRawCopPos, NULL);
```

The view is borrowed until `spriteMultiplexedManagerDestroy`. Passing `NULL` as
the third argument requests an owned zeroed chip-memory blank. A supplied blank
must instead be a borrowed, aligned, zeroed chip-memory `ULONG` retained until
teardown. Initialization controls all eight pointers; do not share these pointer
registers with another sprite manager. Initialization returns `void`; Add returns
`NULL` if the shared manager is unavailable.

```c
tAdvancedMultiplexedSprite *advancedMultiplexedSpriteAdd(
  UBYTE ubChannelIndex, tBitMap *pStrip1, tBitMap *pStrip2,
  UBYTE uwSpriteHeight, UBYTE ubNumberOfMultiplexedSprites
);
```

- `ubChannelIndex` is the first hardware channel, subject to the table above.
- `pStrip1` is required; `pStrip2` is optional (`NULL`). Both must have the same
  width and depth, but can contain different numbers of frames.
- `uwSpriteHeight` is the height of one frame in pixels, **1..255**. Despite its
  name its type is `UBYTE`; validate wider values before calling.
- `ubNumberOfMultiplexedSprites` is fixed logical element capacity, **1..255**.
  It is not the number of animation frames or hardware pieces.
- Total animation frame count must fit a `UWORD` (at most 65535).

Strips must be **vertical, pixel-only, packed interleaved bitmaps**, width 16 or
32px, depth 2 or 4, with `BMF_INTERLEAVED` set. Rows must be nonzero and an exact
multiple of frame height. There are no gaps, control rows or terminators between
frames. Let `byteWidth` be 2 for 16px or 4 for 32px:

```text
BytesPerRow = byteWidth * Depth
Planes[n]   = Planes[0] + byteWidth * n
```

Every row stores the complete plane 0 row, then plane 1, and so on. For a 32px
row, each plane contains the left word followed by the right word. Separate-plane
or padded layouts are rejected. `bitmapCreate(width, height * frameCount, depth,
BMF_CLEAR | BMF_INTERLEAVED)` produces the expected layout when the dimensions
are supported by the bitmap allocator.

Frames are zero-based, top to bottom through strip 1, then top to bottom through
strip 2. For example, a 32x48 strip with height 16 supplies frames 0, 1 and 2;
an optional 32x32 second strip supplies frames 3 and 4. SetFrame does not advance
automatically: the application chooses the frame for each logical element.

Add returns an owned object, or `NULL` for invalid input, channel conflicts or
reported allocation failure. Partial allocations are released on reported
failure; underlying ACE helpers retain their own allocation/error handling.
Creation/removal may temporarily enable OS access. Initialize with sprite DMA
disabled and check Add before loading/enabling the display.

## Ownership and initial state

Add copies each frame into owned 16px/2bpp pieces. **The original strips may be
destroyed immediately after Add returns.** Later edits to those strips do not
update the copied frames. Every logical element initially uses frame 0 at `(0,0)`
and is **enabled**; the whole object also starts enabled. Position or disable
unused elements before the first displayed frame, otherwise they compete at Y=0.

The wrapper owns its copied frames, logical elements, arrays and child ordinary
sprites. The children own two DMA streams each. Do not remove children yourself
or change their internal pointers/counts. The exposed `ubMultiplexedCount` is
logical capacity, `ubSpriteCount` is hardware piece count and `uwAnimCount` is
frame count; they are not mutable capacity controls. Use public setters rather
than editing element state or frame pointers directly. Struct layout is not a
stable binary ABI.

## Public API

```c
void advancedMultiplexedSpriteSetFrame(tAdvancedMultiplexedSprite *pSprite,
  UBYTE ubIndex, UWORD uwFrame);
void advancedMultiplexedSpriteSetEnabled(tAdvancedMultiplexedSprite *pSprite,
  UBYTE ubIndex, UBYTE isEnabled);
void advancedMultiplexedSpriteSetPos(tAdvancedMultiplexedSprite *pSprite,
  UBYTE ubIndex, WORD wX, WORD wY);
void advancedMultiplexedSpriteProcess(tAdvancedMultiplexedSprite *pSprite);
void advancedMultiplexedSpriteProcessChannel(tAdvancedMultiplexedSprite *pSprite);
void advancedMultiplexedSpriteRemove(tAdvancedMultiplexedSprite *pSprite);
```

- Indices are zero-based and must be below logical capacity. Null sprite
  arguments and invalid indices/frames are no-ops.
- `SetFrame` selects a frame independently for one element. Valid frames are
  0..`uwAnimCount - 1`. Selecting the current frame does nothing.
- `SetEnabled` enables/disables all pieces of one element together.
- `SetPos` sets signed, view-relative coordinates. There are no separate X/Y
  setters in this API.
- `pSprite->isEnabled` is the whole-object switch; it is sampled by Process every
  frame and propagated to every child. There is no whole-object setter.
- `Process` propagates logical state and prepares every child DMA stream.
- `ProcessChannel` publishes **all** child channel pointers. Its argument is the
  advanced object, not a channel number.
- `Remove` releases all children, frames and wrapper allocations. Observe the
  DMA/copper teardown contract below before calling it.

## Scheduling and coordinates

The ordinary scheduling rules apply to each child: ascending Y, then logical
index for ties, accepting only elements with **VSTART >= previous VSTOP + 1**.
One scanline is needed for DMA to reload the next control pair. For height 16,
Y=40 followed by Y=57 is valid; Y=56 is too early. Horizontal separation does
not permit vertical overlap on the same reserved channels. Overlapping elements
are omitted for that frame; disabling an early element does not hide later ones.

Coordinates are relative to the **view**, not a viewport or scrolling camera.
Add viewport offsets yourself. For each 16px column, the hardware values are:

```text
HSTART = view->ubPosX - 1 + elementX + columnOffset
VSTART = view->ubPosY + elementY
VSTOP  = VSTART + frameHeight
```

`columnOffset` is 0 or 16. HSTART must be 0..511, VSTART at least 26 and VSTOP at
most 511. There is **no software clipping or channel reassignment**. These checks
are not view-rectangle clipping: negative view-relative coordinates may still
be representable. An unrepresentable column is omitted as a whole, with attached
halves staying together. Thus one column of a 32px image can be omitted while the
other remains; an invalid vertical range omits the whole logical image.

## Copper and per-frame contract

In **block mode**, pass 0 as `uwRawCopPos`. The shared manager owns an initial
blank block and the child pointer blocks. After all objects have been processed
and published, `copProcessBlocks()` rebuilds the copper data and swaps buffers.

In **raw mode**, reserve **16 consecutive MOVE command slots** starting at
`uwRawCopPos`, two per hardware channel, even if this object uses fewer channels.
They must execute before sprite fetching. Initialization sets destinations and
blank values in both raw buffers; publishing changes pointer values in the back
buffer. Supply enough list capacity and avoid other managers' reservations:
the manager does not size or bounds-check the raw list for you.
`copProcessBlocks()` performs the swap in raw mode too.

On **every displayed frame**, set logical state, call `Process` and then
`ProcessChannel` for **every advanced object**, including unchanged or disabled
ones. Also process any independently owned ordinary channels. Finally perform
the view's copper processing/swap **once**. Do not process child channels again
or swap per object. The two DMA streams per child are tied to the two copper
buffers, so skipping static channels can leave stale pointers/data.

Neither processing call waits for the beam. Before preparing a buffer again,
ensure its previous displayed frame is finished. The example follows the demo's
end-of-loop `vPortWaitForEnd` pattern; use the last viewport and ensure the wait
covers your sprite vertical range. Processing waits for prior blits before CPU
pixel reads but does not provide display synchronization.

## Minimal block-mode example

These lifecycle fragments assume ACE is initialized, `s_pView` is a created,
unloaded block-mode view, and `s_pVp` is its last viewport. The caller handles OS
takeover/restoration and initially keeps sprite DMA disabled. The example creates
two solid 32x16 frames in a 4bpp strip, and two logical elements on channels 0..3.
On failure the caller still owns the unloaded view and must destroy it.

```c
#include <ace/managers/advancedmultiplexedsprite.h>
#include <ace/managers/blit.h>
#include <ace/managers/system.h>

static tAdvancedMultiplexedSprite *s_pSprites;

static UBYTE createSprites(void) {
  spriteMultiplexedManagerCreate(s_pView, 0, NULL);
  tBitMap *pStrip = bitmapCreate(32, 32, 4, BMF_CLEAR | BMF_INTERLEAVED);
  if(!pStrip) {
    spriteMultiplexedManagerDestroy();
    return 0;
  }
  blitRect(pStrip, 0, 0, 32, 16, 1);
  blitRect(pStrip, 0, 16, 32, 16, 15);
  blitWait();
  s_pSprites = advancedMultiplexedSpriteAdd(0, pStrip, NULL, 16, 2);
  bitmapDestroy(pStrip); // Add copied the pixels; no source lifetime to retain.
  if(!s_pSprites) {
    spriteMultiplexedManagerDestroy();
    return 0;
  }
  advancedMultiplexedSpriteSetPos(s_pSprites, 0, 40, 40);
  advancedMultiplexedSpriteSetPos(s_pSprites, 1, 80, 64);
  advancedMultiplexedSpriteSetFrame(s_pSprites, 1, 1);
  s_pVp->pPalette[17] = 0x0FFF;
  s_pVp->pPalette[31] = 0x0F80; // Use the appropriate view/global palette.
  viewLoad(s_pView);
  systemSetDmaBit(DMAB_SPRITE, 1);
  return 1;
}

static void frameSprites(void) {
  // Set positions, frames or enables here, before processing.
  advancedMultiplexedSpriteProcess(s_pSprites);
  advancedMultiplexedSpriteProcessChannel(s_pSprites);
  viewProcessManagers(s_pView);
  copProcessBlocks();
  vPortWaitForEnd(s_pVp);
}

static void destroySprites(void) {
  blitWait();
  systemSetDmaBit(DMAB_SPRITE, 0);
  viewLoad(NULL);
  advancedMultiplexedSpriteRemove(s_pSprites);
  s_pSprites = NULL;
  spriteMultiplexedManagerDestroy();
  viewDestroy(s_pView);
  s_pView = NULL;
}
```

Both elements are enabled by default. Configure sprite/playfield priority if the
playfield obscures them. For another object, select free channels and process it
before the same frame's copper swap.

## Safe teardown

Finish outstanding blits, **disable sprite DMA**, then unload the view with
`viewLoad(NULL)`. Remove all advanced wrappers before calling
`spriteMultiplexedManagerDestroy`, and destroy the view last. The manager must
still have a live view/copper list while removing its blocks. Free any supplied
borrowed blank only after manager teardown. The wrapper's source strips do not
need to survive until this point.

If removing an object while retaining a loaded view, leave DMA disabled until
both copper buffers have been rebuilt with blank/replacement pointers for every
removed child channel. In raw mode this requires publishing those channel
numbers with `spriteMultiplexedProcessChannel` and processing/swapping copper
twice; in block mode both buffers must rebuild from the retained blank blocks.
Continue the ordinary per-frame contract for surviving/replacement objects.
Unloading the view avoids this live-removal drain. Disabling an element or the
whole object is not enough to make its allocations safe to free.

## Performance and validation scope

Frame conversion directly splits source planes/columns into owned pixel-only
frames without temporary blits. Child channels are reserved before conversion.
Selecting the current animation frame avoids redundant work. Each child retains
its sorted order, only re-sorts when invalidated, and uses per-buffer dirty masks
and cached offsets to reuse unchanged pixels. Per-frame child processing performs
no allocations or logging and issues no asynchronous blits. These are structural
improvements; no runtime speedup, frame-time or cycle-count measurement is implied.

For the current repair, host tests and 400-frame AROS emulator runs passed for
both ordinary and advanced demos. This does not establish coverage of every
chipset, physical Amiga, video mode or application timing arrangement.

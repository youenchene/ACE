# Using multiplexed sprites

## About multiplexed sprites

The [multiplexed sprite manager](../../include/ace/managers/multiplexedsprite.h)
reuses one hardware sprite channel for several vertically separated, 16px-wide,
2bpp images. It builds a DMA stream containing a control pair and pixels for each
accepted element, followed by a zero control pair to terminate the stream. There
is no pointer between elements: the hardware reads the stream sequentially.

Amiga has eight hardware sprite channels, numbered 0..7. Multiplexing increases
the number of images displayed over a frame, not the number available on one
scanline. A 2bpp image has **three opaque colors plus transparent index 0**.
Channels 0/1 use palette entries 17..19, 2/3 use 21..23, 4/5 use 25..27, and 6/7
use 29..31. The application sets the display palette and sprite/playfield priority.

For animation strips, 32px-wide images, or attached 4bpp images, use the
[advanced multiplexed sprite manager](advancedmultiplexedsprites.md). Its
"16 colors" means **15 opaque colors plus transparent index 0**, using even/odd
channel pairs. Both APIs share the same manager initialization and channel pool.

## Initialization and copper lists

Include `<ace/managers/multiplexedsprite.h>`. Create a live view and its copper
list, then call this once, before adding sprites and before `viewLoad`:

```c
void spriteMultiplexedManagerCreate(
  const tView *pView, UWORD uwRawCopPos, ULONG pBlankSprite[1]
);
```

- `pView` is borrowed and must survive until manager destruction. This is one
  global manager, not one independent manager per view. Destroy it before recreating it.
- `pBlankSprite == NULL` requests an owned, zeroed four-byte chip-memory blank.
  Otherwise supply a borrowed, suitably aligned, zeroed chip-memory `ULONG` and
  retain it until teardown. A stack variable or fast-memory blank is unsuitable.
- Initialization blanks all eight hardware pointers. Do not combine this with
  another sprite manager or copper writer controlling the same sprite pointers.
- Initialization returns `void`; subsequent `spriteMultiplexedAdd` returns
  `NULL` if initialization failed. Check every allocation/Add result. Reported
  allocation failures are propagated; ACE helpers retain their own internal
  allocation/error handling. Creation and removal may temporarily enable OS access.

### Block copper mode

`uwRawCopPos` is ignored (pass 0). The manager creates an initial all-channel blank
block and a two-MOVE pointer block for each reserved channel. Call
`copProcessBlocks()` after publishing every channel: it rebuilds the copper data
and swaps the buffers. Removed channels retain blank pointer blocks for reuse
until manager destruction.

### Raw copper mode

Reserve **16 consecutive copper command slots**, not bytes, starting at
`uwRawCopPos`: high and low pointer MOVEs for each of the eight channels.
Initialization writes the register destinations and blank values into both raw
buffers. Channel `n` uses slots `uwRawCopPos + 2*n` and the following slot.
Place these MOVEs before sprite fetching for the frame, size the raw list to
include them, and keep other managers' reservations separate. The manager does
not allocate or bounds-check this reservation.

`spriteMultiplexedProcessChannel` updates pointer values in the **back** copper
buffer. `copProcessBlocks()` still swaps buffers in raw mode, although it does
not rebuild blocks. Perform exactly one copper swap per displayed frame after
publishing all channels; do not add another swap for each sprite.

## Sources, capacity and ownership

```c
tMultiplexedSprite *spriteMultiplexedAdd(
  UBYTE ubChannelIndex, UBYTE uwSpriteHeight,
  UBYTE ubNumberOfMultiplexedSprites
);
```

This reserves one free channel in 0..7. Maximum element height and element
capacity must both be **1..255**; zero is invalid. The height parameter really is
`UBYTE`, despite its `uw` name: validate wider application values before passing
them. Capacity is fixed at creation and does not guarantee that every element
can fit on screen. Add returns `NULL` for invalid input, a channel conflict, or
reported allocation failure.

The whole channel starts enabled, but **all elements start disabled**, at `(0,0)`
with no source. Set their bitmaps, positions and enabled state before displaying.
Element indices are zero-based, less than the requested capacity.

Each source is a **pixel-only**, packed interleaved bitmap:

- width 16px, depth 2, `BMF_INTERLEAVED`;
- `BytesPerRow == 4`, with `Planes[1] == Planes[0] + 2`;
- non-null pixel storage and `Rows` in 1..the constructor's maximum height;
- each row contains one 16-bit word for plane 0 followed by one for plane 1.

Do not include sprite control or terminator rows. This differs from the basic
`sprite.h` API. An ordinary source is a single frame; split a vertical animation
strip into compatible frames yourself, or use the advanced API.

Sources are **borrowed** and never modified or freed by this manager. They may
be shared by elements or channels. Keep them alive until replaced or removed,
even while disabled. To animate, select another bitmap. After editing a source's
pixels, call `spriteMultiplexedSetBitmap` again, even with the same pointer, to
invalidate both DMA copies. Do not edit pixels concurrently with processing.

The manager owns element metadata, ordering storage and two chip-memory DMA
bitmaps per channel. Each DMA bitmap has `(maximumHeight + 1) * capacity + 1`
four-byte rows, including room for headers and the final terminator.

## Public element API

These functions return `void`. Null sprite pointers and out-of-range indices
are no-ops. Invalid values leave the previous state unchanged, except that
attachment on even channels is always forced off.

```c
void spriteMultiplexedSetBitmap(tMultiplexedSprite *pSprite,
  UBYTE ubIndex, tBitMap *pBitmap);
void spriteMultiplexedSetElement(tMultiplexedSprite *pSprite,
  UBYTE ubIndex, UWORD uwHeight, UBYTE isEnabled, UBYTE isAttached);
void spriteMultiplexedSpriteSetPos(tMultiplexedSprite *pSprite,
  UBYTE ubIndex, WORD wX, WORD wY);
void spriteMultiplexedSetEnabled(tMultiplexedSprite *pSprite,
  UBYTE ubIndex, UBYTE isEnabled);
void spriteMultiplexedSetAttached(tMultiplexedSprite *pSprite,
  UBYTE ubIndex, UBYTE isAttached);
void spriteMultiplexedSetHeight(tMultiplexedSprite *pSprite,
  UBYTE ubIndex, UWORD uwHeight);
void spriteMultiplexedRequestMetadataUpdate(tMultiplexedSprite *pSprite,
  UBYTE ubIndex);
```

- `SetBitmap` validates the source, sets element height to `Rows`, and requests a
  pixel refresh in both buffers. Passing `NULL` does not detach a source; disable
  the element instead.
- `SetElement` configures height, enable and attachment without allocating or
  discarding pixels. Height must be 1..the constructor maximum and, when a source
  exists, no greater than its `Rows`. An invalid height rejects the whole call.
- `SetHeight` uses the same bounds and displays the first requested source rows.
- `SetEnabled` hides one element without terminating the chain or hiding later ones.
- `SetAttached` sets ATTACH only on odd channels. For manual 4bpp attachment,
  reserve an even channel and its following odd channel; put low planes on the
  even channel and high planes on the odd channel. Keep positions, heights,
  enables and scheduling identical on both. The ordinary API does not coordinate
  pairs for you; the advanced API does.
- `RequestMetadataUpdate` is a legacy escape hatch after direct metadata edits:
  it invalidates ordering and both pixel copies. All setter invariants still apply.

`pSprite->isEnabled` is the whole-channel switch, sampled every frame; there is
no whole-channel setter. Otherwise prefer setters. Exposed structs are not a
stable binary ABI; do not change owned buffers, counts or pointers directly.

## Coordinates and scheduling

Positions are signed `WORD` values relative to the **view**, not a viewport or
camera. Add a viewport's vertical offset yourself when positioning within it.
The manager computes:

```text
HSTART = view->ubPosX - 1 + x
VSTART = view->ubPosY + y
VSTOP  = VSTART + height
```

Hardware HSTART must be 0..511, VSTART at least 26, and VSTOP at most 511. The
PAL-safe start bound is also used on NTSC: starting before the initial sprite
control fetch can prevent later elements in the DMA chain from displaying. Negative
view-relative coordinates can be valid after adding the view origin; they do
not wrap as unsigned coordinates. These are hardware representability checks,
not clipping against the view rectangle. There is **no software clipping or
automatic reassignment to another channel**. Unrepresentable elements are omitted.

Elements are considered in ascending Y, with logical index breaking ties. An
element is accepted only if its VSTART is at least the previous accepted
element's **VSTOP + 1**. The VSTOP scanline reloads the next control pair, so a
one-scanline gap is required. For height 16 at Y=40, the next element may start
at Y=57, not Y=56. Horizontal separation does not allow two elements on the same
channel to overlap vertically. Disabled, invalid and overlapping elements are
skipped for that frame; later eligible elements remain visible.

## Processing every frame

```c
void spriteMultiplexedProcess(tMultiplexedSprite *pSprite);
void spriteMultiplexedProcessChannel(UBYTE ubChannelIndex);
```

For **every managed channel, every displayed frame**, including static or
disabled ones: set state, call `Process`, then `ProcessChannel`, then perform
the frame's copper processing/swap once. The first function prepares the DMA
buffer associated with the current back copper buffer; the second publishes
its pointer. Invalid channel numbers are ignored. Publication can prepare a
missing back-buffer stream defensively, but callers should use the explicit order.

Neither function waits for the beam. Synchronize the loop so the previous frame
using a DMA buffer has finished before rewriting it. The example uses
`vPortWaitForEnd` on the last viewport, as in the demo. Ensure your timing also
covers any sprites extending beyond that viewport. Two DMA buffers are tied to
the two copper buffers: dirty flags are an optimization, not permission to skip
processing static channels.

## Minimal block-mode example

These are lifecycle fragments for an application with ACE initialized and a
created, unloaded block-mode view `s_pView` and its last viewport `s_pVp`.
The caller handles OS takeover/restoration and calls the frame function once per
frame. The source is a solid 16x16 square; both elements borrow the same bitmap.
On creation failure, the caller still owns and must destroy the unloaded view.

```c
#include <ace/managers/multiplexedsprite.h>
#include <ace/managers/blit.h>
#include <ace/managers/system.h>

static tMultiplexedSprite *s_pSprites;
static tBitMap *s_pPixels;

static UBYTE createSprites(void) {
  spriteMultiplexedManagerCreate(s_pView, 0, NULL);
  s_pPixels = bitmapCreate(16, 16, 2, BMF_CLEAR | BMF_INTERLEAVED);
  s_pSprites = spriteMultiplexedAdd(0, 16, 2);
  if(!s_pPixels || !s_pSprites) {
    spriteMultiplexedRemove(s_pSprites);
    s_pSprites = NULL;
    if(s_pPixels) { bitmapDestroy(s_pPixels); s_pPixels = NULL; }
    spriteMultiplexedManagerDestroy();
    return 0;
  }
  blitRect(s_pPixels, 0, 0, 16, 16, 1);
  blitWait();
  for(UBYTE i = 0; i < 2; ++i) {
    spriteMultiplexedSetBitmap(s_pSprites, i, s_pPixels);
    spriteMultiplexedSpriteSetPos(s_pSprites, i, 40, 40 + 24 * i);
    spriteMultiplexedSetEnabled(s_pSprites, i, 1);
  }
  s_pVp->pPalette[17] = 0x0FFF; // Use the appropriate view/global palette.
  viewLoad(s_pView);
  systemSetDmaBit(DMAB_SPRITE, 1);
  return 1;
}

static void frameSprites(void) {
  spriteMultiplexedProcess(s_pSprites);
  spriteMultiplexedProcessChannel(0);
  viewProcessManagers(s_pView);
  copProcessBlocks();
  vPortWaitForEnd(s_pVp);
}

static void destroySprites(void) {
  blitWait();
  systemSetDmaBit(DMAB_SPRITE, 0);
  viewLoad(NULL);
  spriteMultiplexedRemove(s_pSprites);
  s_pSprites = NULL;
  bitmapDestroy(s_pPixels);
  s_pPixels = NULL;
  spriteMultiplexedManagerDestroy();
  viewDestroy(s_pView);
  s_pView = NULL;
}
```

Start with sprite DMA disabled during setup (also on the failure path). Configure
sprite/playfield priority for your display if the playfield obscures the squares.
For additional channels, repeat both processing calls before the same copper swap.

## Removal and safe teardown

```c
void spriteMultiplexedRemove(tMultiplexedSprite *pSprite);
void spriteMultiplexedManagerDestroy(void);
```

For final teardown: finish outstanding blits, **disable sprite DMA**, unload the
view with `viewLoad(NULL)`, remove any advanced wrappers first, remove ordinary
objects, destroy the manager, then destroy the view. Free borrowed sources after
their last user is removed, and any borrowed blank after manager teardown.
`Remove(NULL)` and destruction of an uninitialized manager are no-ops. Manager
destruction frees remaining ordinary objects, but cannot dispose of advanced
wrappers safely on their behalf.

For removal while keeping a view loaded, disable sprite DMA before freeing the
object and leave DMA disabled until **both copper buffers** contain blank or
replacement pointers. In raw mode, publish the removed channel with
`spriteMultiplexedProcessChannel(channel)` and process/swap copper twice; in
block mode, allow both buffers to rebuild from the retained blank block. Continue
the normal processing contract for surviving/replacement channels. Otherwise
unload the view instead. Clearing `isEnabled` alone is not a memory-lifetime barrier.

## Performance and validation scope

Processing uses a persistent Y/index order and only sorts when invalidated;
insertion sort benefits from small changes to that order. Per-buffer dirty masks
and cached stream offsets avoid copying unchanged pixels at unchanged offsets.
Headers and the terminator are prepared each frame. The ordinary processing path
allocates nothing, logs nothing and issues no asynchronous blits; it waits for
prior blits before CPU reads. These are structural improvements, not measured
frame-time or cycle-count claims.

For the current repair, host tests and 400-frame AROS emulator runs passed for
both ordinary and advanced demos. This is not validation of every chipset,
physical Amiga, video mode or application timing arrangement.

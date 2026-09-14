// Replay/score packed-image codec (TH10 0x004359b0 compressor family and the
// 0x0044b220 block scrambler). Both are the ABI boundaries previously left
// extern by src/ReplaySave.cpp and src/ScoreSave.cpp; the function names
// match those declarations so the callers bind automatically.
//
// The compressor is an Okumura-style LZSS encoder backed by a binary search
// tree over the 0x2000-byte text ring. Native storage: one record of three
// dwords (parent, left child, right child) per ring position at
// DAT_00477858 + 12 * node, node 0 doubling as NIL and node 0x2000 as the
// root sentinel whose right-child slot is DAT_0048f860. The zeroing pass in
// the compressor only clears nodes 0..0x1fff; the root sentinel's right
// child is seeded with node 1 explicitly.
#include <stdlib.h>
#include <string.h>

#include "ReplayPackedCodec.hpp"

namespace th10 {

namespace {

const u32 kRingSize = 0x2000;   // N
const u32 kMaxMatch = 18;       // F
const u32 kRootNode = 0x2000;   // tree[kRootNode].right == DAT_0048f860

struct TreeNode {
    u32 parent;
    u32 left;
    u32 right;
};

TreeNode g_Tree[kRootNode + 1]; // TH10 dword_477858 (+0x47785c/+0x477860)
u8 g_TextRing[kRingSize];       // TH10 byte_48F868

// TH10 0x00436330 (__usercall EAX). Rightmost descendant of node's left
// subtree.
i32 FindRightmostLeftDescendantEaxAbi(i32 node)
{
    i32 result = static_cast<i32>(g_Tree[node].left);
    while (g_Tree[result].right != 0)
        result = static_cast<i32>(g_Tree[result].right);
    return result;
}

// TH10 0x00436260 (__usercall EDX/ESI: EDX = surviving child, ESI = node).
// Replaces the (zero- or one-child) node with its child. For a leaf the
// "child" is NIL node 0, whose parent slot is used as scratch — native quirk
// preserved (tree[0].parent accumulates garbage).
void SpliceChildUpEdxEsiAbi(i32 child, i32 node)
{
    g_Tree[child].parent = g_Tree[node].parent;
    const u32 parent = g_Tree[node].parent;
    if (g_Tree[parent].right == static_cast<u32>(node))
        g_Tree[parent].right = static_cast<u32>(child);
    else
        g_Tree[parent].left = static_cast<u32>(child);
    g_Tree[node].parent = 0;
}

// TH10 0x004362b0 (__usercall EAX/ECX: EAX = node being replaced, ECX =
// replacement already deleted from its own position). Moves the replacement
// into the node's entire link set.
void SpliceReplacementEaxEcxAbi(i32 node, i32 replacement)
{
    const u32 parent = g_Tree[node].parent;
    if (g_Tree[parent].left == static_cast<u32>(node))
        g_Tree[parent].left = static_cast<u32>(replacement);
    else
        g_Tree[parent].right = static_cast<u32>(replacement);
    g_Tree[replacement].parent = g_Tree[node].parent;
    g_Tree[replacement].left = g_Tree[node].left;
    g_Tree[replacement].right = g_Tree[node].right;
    g_Tree[g_Tree[replacement].left].parent = static_cast<u32>(replacement);
    g_Tree[g_Tree[replacement].right].parent = static_cast<u32>(replacement);
    g_Tree[node].parent = 0;
}

// TH10 0x00436210 (native fastcall, ECX = node). Removes node from the tree.
// Nodes whose parent slot is zero are treated as absent; position 0 (NIL)
// is subject to that test against whatever scratch value tree[0].parent
// currently holds — quirk preserved.
void DeleteMatchNodeEcxAbi(i32 node)
{
    if (g_Tree[node].parent == 0)
        return;
    if (g_Tree[node].right != 0 && g_Tree[node].left != 0) {
        const i32 replacement = FindRightmostLeftDescendantEaxAbi(node);
        DeleteMatchNodeEcxAbi(replacement);
        SpliceReplacementEaxEcxAbi(node, replacement);
        return;
    }
    const i32 child = (g_Tree[node].right != 0)
                          ? static_cast<i32>(g_Tree[node].right)
                          : static_cast<i32>(g_Tree[node].left);
    SpliceChildUpEdxEsiAbi(child, node);
}

// TH10 0x00436000 (native fastcall: ECX = previous ring position, unused by
// the callee; EDX = ring position to insert; stack = out match position).
// Walks the search tree comparing 18 ring bytes, records the best position
// and either attaches the position as a new leaf or, on a full 18-byte
// match, evicts the previous owner and takes over its link set. Returns the
// best match length.
i32 InsertMatchNodeEcxEdxStackAbi(i32 /*unused_previous_position*/,
                                  i32 position, i32 *out_match_position)
{
    if (position == 0)
        return 0;

    i32 node = static_cast<i32>(g_Tree[kRootNode].right);
    i32 best_length = 0;
    for (;;) {
        // First differing ring byte (the native walks the 18 bytes in six
        // 6-byte chunks; the result is identical).
        i32 compare = 0;
        i32 diff = 0;
        while (compare < static_cast<i32>(kMaxMatch)) {
            diff = static_cast<i32>(
                       g_TextRing[(static_cast<u32>(position) +
                                   static_cast<u32>(compare)) &
                                  (kRingSize - 1)]) -
                   static_cast<i32>(
                       g_TextRing[(static_cast<u32>(node) +
                                   static_cast<u32>(compare)) &
                                  (kRingSize - 1)]);
            if (diff != 0)
                break;
            ++compare;
        }
        if (compare >= best_length) {
            best_length = compare;
            *out_match_position = node;
            if (compare >= static_cast<i32>(kMaxMatch))
                break;
        }
        u32 *const child = (diff < 0) ? &g_Tree[node].left
                                      : &g_Tree[node].right;
        if (*child == 0) {
            *child = static_cast<u32>(position);
            g_Tree[position].parent = static_cast<u32>(node);
            g_Tree[position].left = 0;
            g_Tree[position].right = 0;
            return best_length;
        }
        node = static_cast<i32>(*child);
    }

    // Full 18-byte match: the previous owner leaves the tree and the new
    // position takes over its parent link and subtree.
    const u32 parent = g_Tree[node].parent;
    if (g_Tree[parent].left == static_cast<u32>(node))
        g_Tree[parent].left = static_cast<u32>(position);
    else
        g_Tree[parent].right = static_cast<u32>(position);
    g_Tree[position].parent = g_Tree[node].parent;
    g_Tree[position].left = g_Tree[node].left;
    g_Tree[position].right = g_Tree[node].right;
    g_Tree[g_Tree[position].left].parent = static_cast<u32>(position);
    g_Tree[g_Tree[position].right].parent = static_cast<u32>(position);
    g_Tree[node].parent = 0;
    return best_length;
}

// MSB-first flag/data bit packer shared by the encoder body. The flag byte
// is flushed whenever the mask wraps below bit 0x01, exactly like the native
// shr/test/store sequence.
struct BitWriter {
    u8 *cursor;
    u8 flag_byte;
    u8 flag_mask;

    void PutBit(u32 set)
    {
        if (set != 0)
            flag_byte |= flag_mask;
        flag_mask >>= 1;
        if (flag_mask == 0) {
            *cursor++ = flag_byte;
            flag_byte = 0;
            flag_mask = 0x80;
        }
    }
};

} // namespace

// TH10 0x004359b0. Native stdcall (retn 0xc) body.
void *CompressReplayImageStdcallAbi(const void *image, i32 size,
                                    u32 *out_size)
{
    const u8 *input = static_cast<const u8 *>(image);

    // Native allocates 2 * size (lea eax, [ebp+ebp]); a negative size wraps
    // through the same unsigned malloc argument — quirk preserved.
    u8 *const output = static_cast<u8 *>(malloc(2U * static_cast<u32>(size)));
    if (output == 0)
        return 0;

    u8 *out_cursor = output;
    BitWriter writer;
    writer.cursor = output;
    writer.flag_byte = 0;
    writer.flag_mask = 0x80;

    *out_size = 0;
    memset(g_TextRing, 0, kRingSize);
    // Native zeroing pass covers nodes 0..0x1fff only; the root sentinel is
    // seeded below and its parent/left slots keep stale values (never read).
    for (u32 node = 0; node < kRingSize; ++node) {
        g_Tree[node].parent = 0;
        g_Tree[node].left = 0;
        g_Tree[node].right = 0;
    }

    // Initial fill: up to 18 input bytes land at ring[1..18]. The native
    // stops on getc() EOF (byte == -1); because the byte is zero-extended
    // before the test the check is dead — preserved as unreachable.
    u32 consumed = 0;
    i32 loaded = 0;
    while (static_cast<i32>(consumed) < size && loaded < static_cast<i32>(kMaxMatch)) {
        const i32 byte = input[consumed];
        ++consumed;
        if (byte == -1)
            break; // never taken: bytes are zero-extended
        g_TextRing[1 + loaded] = static_cast<u8>(byte);
        ++loaded;
    }

    // Root insert of ring position 1 (native stores DAT_0048f860 = 1 and
    // tree[1].parent = 0x2000, tree[1].left = tree[1].right = 0).
    g_Tree[kRootNode].right = 1;
    g_Tree[1].parent = kRootNode;
    g_Tree[1].left = 0;
    g_Tree[1].right = 0;

    i32 match_length = 0;      // EBX-tracked length at the loop head
    i32 remaining = loaded;    // bytes still to flush past the input end
    u32 ring_position = 1;     // DAT_0048f860-adjacent var_18 cursor
    i32 next_match_length = 0; // var_1c
    i32 match_position = 0;    // var_c ("i", filled by the insert helper)

    while (remaining > 0) {
        if (match_length > remaining) {
            next_match_length = remaining;
            match_length = remaining;
        }

        u32 run;
        if (match_length > 2) {
            // Match token: clear flag bit, 12 bits of position (MSB first),
            // then 4 bits of length - 3. While emitting the length bits the
            // native reuses its match-length register as scratch for the
            // flag accumulator and reloads it from var_1c on every set bit;
            // the two always hold the same clamped value, so the reload is
            // semantically a no-op — noted, not observable.
            writer.PutBit(0);
            for (u32 bit = 0x1000U; bit != 0; bit >>= 1)
                writer.PutBit(static_cast<u32>(match_position) & bit);
            const u32 length_code = static_cast<u32>(match_length - 3);
            for (u32 bit = 8U; bit != 0; bit >>= 1) {
                if ((length_code & bit) != 0)
                    match_length = next_match_length; // native reload
                writer.PutBit(length_code & bit);
            }
            run = static_cast<u32>(match_length);
        } else {
            // Literal: set flag bit, then 8 bits of ring[ring_position].
            writer.PutBit(1);
            const u8 literal = g_TextRing[ring_position];
            for (u32 bit = 0x80U; bit != 0; bit >>= 1)
                writer.PutBit(literal & bit);
            run = 1;
        }

        // Absorb `run` ring positions: delete the ring slot that is about to
        // be overwritten, consume one input byte into it, advance the
        // cursor and re-insert while input remains.
        for (u32 emitted = 0; emitted < run; ++emitted) {
            const u32 overwrite = (ring_position + kMaxMatch) & (kRingSize - 1);
            DeleteMatchNodeEcxAbi(static_cast<i32>(overwrite));
            if (static_cast<i32>(consumed) < size) {
                const i32 byte = input[consumed];
                ++consumed;
                if (byte == -1)
                    --remaining; // never taken: bytes are zero-extended
                else
                    g_TextRing[overwrite] = static_cast<u8>(byte);
            } else {
                --remaining;
            }
            ring_position = (ring_position + 1) & (kRingSize - 1);
            if (remaining != 0)
                next_match_length = InsertMatchNodeEcxEdxStackAbi(
                    0, static_cast<i32>(ring_position), &match_position);
        }
        match_length = next_match_length;
    }

    // Tail: one more clear flag bit plus twelve zero position bits, whose
    // mask wraps flush the pending flag byte (if any).
    for (i32 bit = 0; bit < 13; ++bit)
        writer.PutBit(0);

    *out_size = static_cast<u32>(writer.cursor - output);
    return output;
}

// TH10 0x0044b220. Native __userpurge (initial key in AL) body.
void ScrambleReplayImageUserpurgeAbi(u8 initial_key, void *buffer, i32 size,
                                     u8 key_step, i32 block_size,
                                     i32 size_again)
{
    // Copy span: the smaller of `size_again` and `size`.
    u32 copy_size = static_cast<u32>(size_again);
    if (size_again > size)
        copy_size = static_cast<u32>(size);

    // Trailing partial block is skipped when it is at least a quarter of a
    // block; the odd trailing byte is always skipped.
    const i32 remainder = size % block_size;
    const i32 skipped_tail = (remainder >= block_size / 4) ? 0 : remainder;
    const i32 scramble_length = size - (skipped_tail + (size & 1));

    u8 *const scratch = static_cast<u8 *>(malloc(copy_size));
    if (scratch == 0)
        return;
    memcpy(scratch, buffer, copy_size);

    u8 *out = static_cast<u8 *>(buffer);
    u8 key = initial_key;
    i32 remaining = scramble_length;
    while (size_again > 0 && remaining > 0) {
        i32 block = block_size;
        if (remaining < block_size) {
            block_size = remaining; // native persists the shrink
            block = remaining;
        }

        // Odd offsets of the block, descending, then even offsets,
        // descending; the running key advances once per output byte.
        const u8 *const block_end = scratch + static_cast<u32>(block);
        const u8 *read = block_end - 1;
        for (i32 i = (block + 1) / 2; i > 0; --i) {
            *out++ = static_cast<u8>(key ^ *read);
            read -= 2;
            key = static_cast<u8>(key + key_step);
        }
        read = block_end - 2;
        for (i32 i = block / 2; i > 0; --i) {
            *out++ = static_cast<u8>(key ^ *read);
            read -= 2;
            key = static_cast<u8>(key + key_step);
        }

        remaining -= block;
        size_again -= block;
    }

    free(scratch);
}

} // namespace th10

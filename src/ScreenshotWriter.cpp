// Screenshot capture: the back-buffer snapshot writer (0x00420670) and the
// _beginthread worker that streams the BMP to disk (0x00420540).
#include "ScreenshotWriter.hpp"

#include "MainChainContext.hpp"
#include "Th10Types.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace th10 {

namespace {

// Snapshot scratch block shared with the writer thread. TH10
// DAT_00474038..DAT_00492338 region as laid out by the two functions.
void *g_SnapshotFileHandle;             // TH10 0x474C38 (hObject)
u8 g_SnapshotFileHeader[14];            // TH10 0x492138
void *g_SnapshotInfoHeader;             // TH10 0x492148
void *g_SnapshotPixelData;              // TH10 0x49214C
Win32CriticalSection g_SnapshotLock;    // TH10 0x4922A4
u8 g_SnapshotFileWorkerDepth;           // TH10 0x49231E
u32 g_SnapshotFileWorkerHandle;         // TH10 0x492134

// TH10 0x44B620 boundary: opens (or reuses) the snapshot file handle into
// g_SnapshotFileHandle under g_SnapshotLock, keeping the caller inside the
// critical section and having bumped g_SnapshotFileWorkerDepth.
void OpenSnapshotFileBoundary();

// TH10 0x44B810 boundary: thiscall text-log push onto the 0x474F70 Text
// manager with the message in the first stack slot.
void PushTextLogMessage(const char *message);

} // namespace

namespace {

extern "C" void TH10_STDCALL Sleep(u32 milliseconds);
extern "C" i32 TH10_STDCALL WriteFile(void *handle, const void *data,
                                      u32 size, u32 *written, void *overlapped);
extern "C" i32 TH10_STDCALL CloseHandle(void *handle);
extern "C" void TH10_STDCALL LeaveCriticalSection(void *crit);
extern "C" u32 _beginthread(void (*start)(void), u32 stack_size,
                            void *argument);

} // namespace

// FUNCTION: TH10 0x00420540
void SnapshotFileWriterThread()
{
    OpenSnapshotFileBoundary();
    void *handle = g_SnapshotFileHandle;
    if (handle == reinterpret_cast<void *>(-1))
        goto finish;

    {
        u32 written;
        if (WriteFile(handle, g_SnapshotFileHeader, 14, &written, 0) == 0 ||
            written != 14) {
            CloseHandle(handle);
            LeaveCriticalSection(&g_SnapshotLock);
            --g_SnapshotFileWorkerDepth;
        }
        handle = g_SnapshotFileHandle;
        if (handle == reinterpret_cast<void *>(-1))
            goto finish;
        if (WriteFile(handle, g_SnapshotInfoHeader, 0x28, &written, 0) == 0 ||
            written != 0x28) {
            CloseHandle(handle);
            LeaveCriticalSection(&g_SnapshotLock);
            --g_SnapshotFileWorkerDepth;
        }
        handle = g_SnapshotFileHandle;
        if (handle == reinterpret_cast<void *>(-1))
            goto finish;
        if (WriteFile(handle, g_SnapshotPixelData, 0xE1000, &written, 0) ==
                0 ||
            written != 0xE1000) {
            CloseHandle(handle);
            LeaveCriticalSection(&g_SnapshotLock);
            --g_SnapshotFileWorkerDepth;
        }
        handle = g_SnapshotFileHandle;
        if (handle != reinterpret_cast<void *>(-1)) {
            CloseHandle(handle);
            LeaveCriticalSection(&g_SnapshotLock);
            --g_SnapshotFileWorkerDepth;
        }
    }

finish:
    if (g_SnapshotInfoHeader != 0) {
        free(g_SnapshotInfoHeader);
        g_SnapshotInfoHeader = 0;
    }
    if (g_SnapshotPixelData != 0) {
        free(g_SnapshotPixelData);
        g_SnapshotPixelData = 0;
    }
    g_SnapshotFileWorkerHandle = 0;
}

// FUNCTION: TH10 0x00420670
i32 CaptureMainChainSnapshot(MainChainContext *context,
                             const char *output_path)
{
    // The writer thread and the caller share the whole scratch block; the
    // native spins on the context busy word instead of taking a lock.
    while (*reinterpret_cast<u32 *>(reinterpret_cast<u8 *>(context) +
                                    0x50c) != 0)
        Sleep(10);

    // Obtain the back-buffer wrapper through device vtable slot 0x48/4.
    u8 *const context_bytes = reinterpret_cast<u8 *>(context);
    void *device = *reinterpret_cast<void **>(context_bytes + 0x8);
    void *surface = 0;
    {
        void **vtable = *reinterpret_cast<void ***>(device);
        typedef void (TH10_STDCALL *GetSurfaceFn)(
            void *, u32, u32, u32, void **);
        GetSurfaceFn get_surface;
        memcpy(&get_surface, &vtable[0x48 / 4], sizeof(get_surface));
        get_surface(device, 0, 0, 0, &surface);
    }

    // BITMAPFILEHEADER at +0x510: 'BM', size 54, reserved 0, data offset
    // 54 (the pixel bytes are added to the size field later).
    u32 *const header = reinterpret_cast<u32 *>(context_bytes + 0x510);
    header[0] = 0;
    header[1] = 0;
    header[2] = 0;
    *reinterpret_cast<u16 *>(context_bytes + 0x51c) = 0;
    *reinterpret_cast<u16 *>(context_bytes + 0x510) = 0x4d42; // 'BM'
    *reinterpret_cast<u32 *>(context_bytes + 0x51a) = 54;
    *reinterpret_cast<u32 *>(context_bytes + 0x512) = 54;
    // File name copy at +0x528 including the terminator.
    {
        u8 *dst = context_bytes + 0x528;
        const char *src = output_path;
        for (;;) {
            const char c = *src++;
            *dst++ = c;
            if (c == 0)
                break;
        }
    }

    const u32 render_mode =
        *reinterpret_cast<u32 *>(context_bytes + 0xec);
    if (render_mode == 22) {
        // 24bpp capture path.
        void *info = malloc(0x2c);
        *reinterpret_cast<void **>(context_bytes + 0x520) = info;
        if (info == 0) {
            PushTextLogMessage("snapShotScreen : ");
        } else {
            memset(info, 0, 0x2c);
            void *pixels = malloc(0xE1000);
            *reinterpret_cast<void **>(context_bytes + 0x524) = pixels;
            if (pixels == 0) {
                PushTextLogMessage("snapShotScreen : ");
            } else {
                *reinterpret_cast<u32 *>(context_bytes + 0x512) += 0xE1000;
                u8 *const info_bytes = static_cast<u8 *>(info);
                *reinterpret_cast<u16 *>(info_bytes + 0xe) = 24;
                *reinterpret_cast<u32 *>(info_bytes) = 40;
                *reinterpret_cast<u32 *>(info_bytes + 4) = 640;
                *reinterpret_cast<u32 *>(info_bytes + 8) = 480;
                *reinterpret_cast<u16 *>(info_bytes + 0xc) = 1;
                *reinterpret_cast<u32 *>(info_bytes + 0x10) = 0;

                // Lock through the surface wrapper vtable slot 0x34/4. The
                // descriptor out-param is {pitch, bits} in that order.
                struct LockedSurface {
                    u32 pitch;
                    u8 *bits;
                } desc;
                desc.pitch = 0;
                desc.bits = 0;
                {
                    void **vtable = *reinterpret_cast<void ***>(surface);
                    typedef void (TH10_STDCALL *LockFn)(
                        void *, LockedSurface *, u32, u32);
                    LockFn lock_surface;
                    memcpy(&lock_surface, &vtable[0x34 / 4],
                           sizeof(lock_surface));
                    lock_surface(surface, &desc, 0, 0);
                }
                // Bottom-up copy: 480 rows of 640 pixels, 32-bit source to
                // 24-bit destination.
                u8 *dst_row = static_cast<u8 *>(pixels);
                const u8 *src_row =
                    desc.bits + 479 * desc.pitch;
                for (i32 y = 479; y > -1; --y) {
                    const u8 *src = src_row;
                    u8 *dst = dst_row;
                    for (u32 x = 0; x != 640; ++x) {
                        dst[0] = src[0];
                        dst[1] = src[1];
                        dst[2] = src[2];
                        src += 4;
                        dst += 3;
                    }
                    dst_row += 640 * 3;
                    src_row -= desc.pitch;
                }
                {
                    void **vtable = *reinterpret_cast<void ***>(surface);
                    typedef void (TH10_STDCALL *UnlockFn)(
                        void *);
                    UnlockFn unlock_surface;
                    memcpy(&unlock_surface, &vtable[0x38 / 4],
                           sizeof(unlock_surface));
                    unlock_surface(surface);
                }
                g_SnapshotFileWorkerHandle =
                    _beginthread(SnapshotFileWriterThread, 0, 0);
            }
        }
    } else if (render_mode == 23) {
        PushTextLogMessage("16bit ");
    } else {
        PushTextLogMessage("error ? .\\src\\game\\mother.cpp\r\n");
        if (surface != 0) {
            void **vtable = *reinterpret_cast<void ***>(surface);
            typedef void (TH10_STDCALL *ReleaseFn)(void *);                    ReleaseFn release_surface;
                    memcpy(&release_surface, &vtable[2],
                           sizeof(release_surface));
                    release_surface(surface);
        }
        return 1;
    }

    if (surface != 0) {
        void **vtable = *reinterpret_cast<void ***>(surface);
        typedef void (TH10_STDCALL *ReleaseFn)(void *);                    ReleaseFn release_surface;
                    memcpy(&release_surface, &vtable[2],
                           sizeof(release_surface));
                    release_surface(surface);
    }
    return 0;
}

} // namespace th10

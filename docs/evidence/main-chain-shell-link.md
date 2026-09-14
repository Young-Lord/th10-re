# Main chain shell-link resolution (0x0043a290)

Reconstruction in `src/MainChainHostEnvironment.cpp`
(`ResolveMainChainShellLinkPath`). Native usercall: EBX = destination
buffer, EDI = capacity in wide chars, stack = source path (`ret 4`);
returns 1 when a link resolved, 0 otherwise (the 0x00439ff0 caller
ignores the result). GS cookie stores/`__security_check_cookie` calls
are compiler artifacts and are not modeled.

## Verified body

1. Early out returning 0 when the destination buffer is null
   (`test ebx,ebx`).
2. `CoInitialize(NULL)` (IAT 0x466308).
3. `CoCreateInstance(CLSID_ShellLink (0x46954c), NULL,
   CLSCTX_INPROC_SERVER, IID_IShellLinkA (0x46931c), &shell_link)`
   (IAT 0x466304). Failure jumps to the CoUninitialize tail and returns
   the 0 result slot.
4. `shell_link->QueryInterface(IID_IPersistFile (0x46997c),
   &persist_file)` (vtable slot 0). Failure releases the shell link
   (slot 2) before the tail.
5. `operator new(2 * capacity)` (0x452493) for the wide-path scratch —
   the result is not checked — and
   `MultiByteToWideChar(CP_ACP, 0, source, -1, wide, capacity)`
   (IAT 0x466104); the returned character count is ignored.
6. `persist_file->Load(wide, STGM_READ)` (IPersistFile slot 5, 0x14).
   Failure frees the wide buffer (0x4524a1) and releases the persist
   file.
7. `shell_link->GetPath(destination, capacity, &find_data_scratch, 0)`
   (IShellLinkA slot 3, 0x0c; the WIN32_FIND_DATAA scratch is never
   read). Success sets the 1 result. Both failure and success then free
   the wide buffer and release the persist file.
8. Every post-create path releases the shell link, calls
   `CoUninitialize` (IAT 0x466300) and returns the result slot.

The GUID literals at 0x46954c/0x46931c/0x46997c were verified byte
against the standard CLSID_ShellLink / IID_IShellLinkA /
IID_IPersistFile values.

## Caller context

`InitializeMainChainHostEnvironment` resolves the launch path through
this helper in a do/while loop while the resolved target keeps a ".lnk"
extension, then compares the resolved path against GetModuleFileNameA to
set the DAT_00492518 alternate-launch flag.

-- Generate a small project-specific variant of Xmake's NSIS template. Keeping the transformation
-- here avoids carrying a full fork of the upstream template while still failing loudly if it
-- changes around either patched block.

local function replaceOnce(contents, needle, replacement)
    local first, last = contents:find(needle, 1, true)
    assert(first, "Xmake's NSIS template changed; expected block was not found")
    assert(not contents:find(needle, last + 1, true),
           "Xmake's NSIS template changed; expected block is no longer unique")
    return contents:sub(1, first - 1) .. replacement .. contents:sub(last + 1)
end

function main(package)
    local template = path.join(os.programdir(), "scripts", "xpack", "nsis", "makensis.nsi")
    local contents = assert(io.readfile(template), "cannot read Xmake's NSIS template")

    -- Xmake scans every installed file after extraction to calculate an optional Installed Apps
    -- size. On machines with real-time scanning this leaves the UI at 100% for a long time.
    contents = replaceOnce(contents, [[
    ; write size to reg
    ${GetSize} "$InstDir" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD ${RootKey} ${RegUninstall} "EstimatedSize" "$0"
]], [[
    ; Do not rescan the installed Qt tree here. EstimatedSize is optional uninstall metadata.
]])

    contents = replaceOnce(contents, [[
  ; add uninstaller
  WriteUninstaller "uninstall.exe"
]], [[
  SetDetailsPrint textonly
  DetailPrint "Finalizing installation..."
  SetDetailsPrint both

  ; add uninstaller
  WriteUninstaller "uninstall.exe"
]])

    -- Keep Explorer's environment refresh, but do not let an unresponsive top-level window hold
    -- the completed installer for five seconds.
    contents = replaceOnce(
        contents,
        [[SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000]],
        [[SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=1000]])

    local generated = path.join(package:builddir(), "mksync-nsis-template.nsi")
    os.mkdir(path.directory(generated))
    io.writefile(generated, contents)
    package:set("specfile", generated)
end

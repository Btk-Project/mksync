

-- Do not define options named bindir/libdir: those names belong to Xmake's install API and
-- shadowing them breaks target:bindir()/target:libdir(), including Qt's xpack install hook.
function mks_output_dir(kind)
    return path.join(get_config("outputdir"), kind, "$(plat)-$(arch)-$(kind)-$(runtimes)")
end

rule("mode.mydebug")
    on_config(function (target)

        if is_mode("mydebug") then
            if not target:get("symbols") then
                target:set("symbols", "debug")
            end
            if not target:get("optimize") then
                target:set("optimize", "none")
            end
            target:add("cuflags", "-G")
        end
    end)

-- Generate Wayland client headers and private protocol code from vendored XML files.
rule("mks.wayland_client_protocol")
    set_extensions(".xml")

    on_config(function(target)
        import("core.base.option")

        local rule_name = "mks.wayland_client_protocol"
        if target:rule("c++.build") then
            local cpp_rule = target:rule("c++.build"):clone()
            cpp_rule:add("deps", rule_name, {order = true})
            target:rule_add(cpp_rule)
        end

        local output_dir = path.join(target:autogendir(), "rules", rule_name)
        os.mkdir(output_dir)
        target:add("includedirs", output_dir)

        local source_batch = target:sourcebatches()[rule_name]
        if not option.get("dry-run") and source_batch and source_batch.sourcefiles then
            for _, protocol in ipairs(source_batch.sourcefiles) do
                local base_name = path.basename(protocol)
                os.touch(path.join(output_dir, base_name .. ".h"))
                target:add("files", path.join(output_dir, base_name .. ".c"),
                           {always_added = true})
            end
        end
    end)

    before_buildcmd_file(function(target, batchcmds, source_file, opt)
        import("lib.detect.find_tool")

        local scanner = find_tool("wayland-scanner")
        assert(scanner, "wayland-scanner not found; install the Wayland development tools")

        local rule_name = "mks.wayland_client_protocol"
        local output_dir = path.join(target:autogendir(), "rules", rule_name)
        local base_name = path.basename(source_file)
        local header = path.join(output_dir, base_name .. ".h")
        local source = path.join(output_dir, base_name .. ".c")

        batchcmds:show_progress(opt.progress,
                                "${color.build.object}generating Wayland protocol %s", base_name)
        batchcmds:vexecv(scanner.program, {"client-header", source_file, header})
        batchcmds:vexecv(scanner.program, {"private-code", source_file, source})
        batchcmds:add_depfiles(source_file)
        batchcmds:set_depmtime(os.mtime(source))
        batchcmds:set_depcache(target:dependfile(source))
    end)
rule_end()

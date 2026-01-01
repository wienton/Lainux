#!/usr/bin/env lua

local config = {
    binary = "installer.lain",
    source_files = {
        "src/installer/installer.c",
        "src/utils/network_connection/network_sniffer.c",
        "src/utils/network_connection/network_state.c",
        "src/installer/ui/logo.c",
        "src/installer/vm/vm.c",
        "src/installer/system_utils/log_message.c",
        "src/installer/system_utils/run_command.c",
        "src/installer/disk_utils/disk_info.c",
        "src/installer/locale/lang.c",
        "src/installer/locale/ru.c",
        "src/installer/locale/en.c",
        "src/installer/settings/settings.c",
        "src/installer/cleanup/cleaner.c"
    },
    libs = {"-lncurses", "-lcurl", "-lcrypto"},
    flags = {"-Wextra", "-O2", "-std=c11", "-D_DEFAULT_SOURCE"},
    driver_main = "lainux-driver"
}

local function log(msg)
    local time = os.date("%H:%M:%S")
    print(string.format("[%s] %s", time, msg))
end

local function error_exit(msg)
    io.stderr:write(string.format("[ERROR] %s\n", msg))
    os.exit(1)
end

local function file_exists(path)
    local file = io.open(path, "r")
    if file then
        file:close()
        return true
    end
    return false
end

local function validate_sources()
    for _, src in ipairs(config.source_files) do
        if not file_exists(src) then
            error_exit("Source file not found: " .. src)
        end
    end
end

local function prompt_build()
    io.write("build installer? (y/N): ")
    io.flush()
    local response = io.read("*l"):lower()
    return response == "y" or response == "yes"
end

local function build_drivers()
    log("compiling Drivers")
end

local function execute_command(cmd)
    local handle = io.popen(cmd .. " 2>&1", "r")
    local output = handle:read("*a")
    local success = handle:close()
    return success, output
end

local function compile()
    log("compiling " .. config.binary)

    local cmd = {"gcc"}

    for _, src in ipairs(config.source_files) do
        table.insert(cmd, src)
    end

    for _, lib in ipairs(config.libs) do
        table.insert(cmd, lib)
    end

    for _, flag in ipairs(config.flags) do
        table.insert(cmd, flag)
    end

    table.insert(cmd, "-o")
    table.insert(cmd, "bin/" .. config.binary)

    local full_cmd = table.concat(cmd, " ")
    log("executing: " .. full_cmd)

    local success, output = execute_command(full_cmd)

    if not success then
        error_exit("compilation failed:\n" .. output)
    end

    log("build successful")
    log("binary saved as 'bin/" .. config.binary .. "'")
end

local function main()
    log("**** || lainux lua script for make binary files || ****")

    validate_sources()

    if not prompt_build() then
        log("build cancelled.")
        return
    end

    build_drivers()
    compile()
end

main()

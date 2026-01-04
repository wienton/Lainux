/* The main file, from which we extract the configurations, server, minimal, security, cybersecurity, etc. */

local CONFIGURATIONS = {
    minimal = {
        name = "Minimal",
        description = "Base system only, no graphical interface",
        size = "~500MB",
        packages = {
            base = {"base", "linux", "linux-firmware"},
            tools = {"sudo", "nano", "git"}
        },
        features = {"CLI only", "Lightweight"}
    },

    standard = {
        name = "Standard",
        description = "Full system with desktop environment",
        size = "~2GB",
        packages = {
            base = {"base", "linux", "linux-firmware"},
            desktop = {"xorg", "i3-gaps", "lightdm"},
            network = {"networkmanager"},
            tools = {"sudo", "git", "firefox"}
        },
        features = {"Desktop", "Graphical interface", "Recommended"}
    },

    development = {
        name = "Development",
        description = "Complete development toolset",
        size = "~4GB",
        packages = {
            base = {"base", "linux", "linux-firmware"},
            desktop = {"xorg", "i3-gaps", "lightdm"},
            dev = {"docker", "nodejs", "python", "rust", "neovim"},
            tools = {"git", "postgresql", "nginx"}
        },
        features = {"Development tools", "Docker", "Databases"}
    },

    server = {
        name = "Server",
        description = "Optimized for server tasks",
        size = "~1.5GB",
        packages = {
            base = {"base", "linux", "linux-firmware"},
            server = {"nginx", "postgresql", "redis", "openssh"},
            tools = {"sudo", "git", "htop"}
        },
        features = {"Server optimized", "No GUI", "Security"}
    },

    security = {
        name = "Security",
        description = "Security-focused system",
        size = "~2.5GB",
        packages = {
            base = {"base", "linux-hardened", "linux-firmware"},
            security = {"openssh", "fail2ban", "ufw", "audit"},
            tools = {"sudo", "git", "wireshark-cli"}
        },
        features = {"Hardened kernel", "Security tools"}
    },

    cybersecurity = {
        name = "Cybersecurity",
        description = "Complete security analysis toolkit",
        size = "~3.5GB",
        packages = {
            base = {"base", "linux-hardened", "linux-firmware"},
            security = {"wireshark-qt", "nmap", "metasploit", "john"},
            tools = {"docker", "python", "git"}
        },
        features = {"Pentesting tools", "Forensics", "Analysis"}
    }
}

-- Get list of configurations for menu display
function get_configurations_list()
    local list = {}
    for key, config in pairs(CONFIGURATIONS) do
        table.insert(list, {
            id = key,
            name = config.name,
            description = config.description,
            size = config.size,
            features = config.features
        })
    end
    return list
end

-- Get specific configuration by ID
function get_configuration(id)
    return CONFIGURATIONS[id]
end

-- Get packages for installation
function get_packages(id)
    local config = CONFIGURATIONS[id]
    if not config then return {} end

    local packages = {}
    for _, category in pairs(config.packages) do
        for _, pkg in ipairs(category) do
            table.insert(packages, pkg)
        end
    end
    return packages
end

-- Validate configuration exists
function validate_configuration(id)
    return CONFIGURATIONS[id] ~= nil
end

-- Export functions
return {
    get_configurations_list = get_configurations_list,
    get_configuration = get_configuration,
    get_packages = get_packages,
    validate_configuration = validate_configuration
}

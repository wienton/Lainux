package main

import (
	"bufio"
	"fmt"
	"net"
	"net/http"
	"os"
	"os/exec"
	"runtime"
	"strconv"
	"strings"
	"time"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
)

// ---------- Localization ----------
type Phrases struct {
	Title                string
	Logo                 string
	VersionLine          string
	SystemTime           string
	Navigation           string
	Arch                 string
	Kernel               string
	ExitConfirm          string
	ExitAction           string

	// Menu
	MenuItems [9]string

	// Hardware info
	HardwareTitle    string
	SystemOverview   string
	HostnameLabel    string
	ArchLabel        string
	KernelLabel      string
	CPUInfo          string
	CPUCores         string
	Memory           string
	Graphics         string
	Advanced         string
	VirtSupport      string
	VirtNotAvailable string
	FirmwareUEFI     string
	FirmwareBIOS     string
	UptimeLabel      string
	LoadAvgLabel     string

	// Requirements
	RequirementsTitle    string
	RAMWarning           string
	CPUWarning           string
	DiskSpaceWarning     string
	MeetsRequirements    string
	MayNotPerform        string

	// Disk selection
	NoDisksFound         string

	// VM
	VMTitle              string
	VMRequirements       string

	// Config
	ConfigTitle          string
	ConfigSaved          string

	// Network
	NetworkChecking      string
	NetworkConnected     string
	NetworkOffline       string
	PublicIP             string
	NoInterface          string

	// Common
	PressAnyKey          string
	Back                 string
	Quit                 string
}

var phrases = map[string]Phrases{
	"EN": {
		Title:       "LAINUXOS INSTALLER",
		Logo:        "LainuxOS",
		VersionLine: "Version v0.3 | UEFI Ready | Secure Boot Compatible",
		SystemTime:  "System time",
		Navigation:  "Navigate: ↑ ↓ • Select: Enter • Exit: q",
		Arch:        "Arch",
		Kernel:      "Kernel",
		ExitConfirm: "Exit Lainux installer?",
		ExitAction:  "EXIT",

		MenuItems: [9]string{
			"Install on Hardware",
			"Install on Virtual Machine",
			"Hardware Information",
			"System Requirements Check",
			"Configuration Selection",
			"View Disk Information",
			"Check Network",
			"Network Diagnostics",
			"Exit Installer",
		},

		HardwareTitle:    "HARDWARE INFORMATION",
		SystemOverview:   "System Overview",
		HostnameLabel:    "Hostname",
		ArchLabel:        "Architecture",
		KernelLabel:      "Kernel",
		CPUInfo:          "CPU",
		CPUCores:         "Cores",
		Memory:           "Memory",
		Graphics:         "Graphics",
		Advanced:         "Advanced",
		VirtSupport:      "Virtualization: Supported",
		VirtNotAvailable: "Virtualization: Not available",
		FirmwareUEFI:     "Firmware: UEFI",
		FirmwareBIOS:     "Firmware: Legacy BIOS",
		UptimeLabel:      "Uptime",
		LoadAvgLabel:     "Load avg",

		RequirementsTitle:    "SYSTEM REQUIREMENTS CHECK",
		RAMWarning:           "WARNING: Minimum 1GB RAM recommended",
		CPUWarning:           "WARNING: Dual-core CPU recommended",
		DiskSpaceWarning:     "WARNING: At least 2GB free space required in /tmp",
		MeetsRequirements:    "✓ System meets minimum requirements",
		MayNotPerform:        "⚠ System may not perform optimally",

		NoDisksFound:         "No suitable disks found. Please check connections.",

		VMTitle:              "VIRTUAL MACHINE INSTALLATION",
		VMRequirements:       "Requirements:\n- KVM or virtualization support\n- 20GB free disk space\n- 4GB RAM\n- Internet connection",

		ConfigTitle:          "SELECT CONFIGURATION",
		ConfigSaved:          "Configuration saved to lainux-config.txt",

		NetworkChecking:      "Checking network...",
		NetworkConnected:     "Connected ✓",
		NetworkOffline:       "No connection ✗",
		PublicIP:             "Public IP",
		NoInterface:          "No active interface found",

		PressAnyKey:          "Press any key to continue...",
		Back:                 "b: Back",
		Quit:                 "q: Quit",
	},
	"RU": {
		Title:       "УСТАНОВЩИК LAINUXOS",
		Logo:        "LainuxOS",
		VersionLine: "Версия v0.3 | Готов к работе с UEFI | Поддержка Secure Boot",
		SystemTime:  "Системное время",
		Navigation:  "Навигация: ↑ ↓ • Выбор: Enter • Выход: q",
		Arch:        "Архитектура",
		Kernel:      "Ядро",
		ExitConfirm: "Выйти из установщика Lainux?",
		ExitAction:  "ВЫХОД",

		MenuItems: [9]string{
			"Установить на оборудование",
			"Установить в виртуальную машину",
			"Информация об оборудовании",
			"Проверка системных требований",
			"Выбор конфигурации",
			"Информация о дисках",
			"Проверить сеть",
			"Диагностика сети",
			"Выйти из установщика",
		},

		HardwareTitle:    "ИНФОРМАЦИЯ ОБ ОБОРУДОВАНИИ",
		SystemOverview:   "Обзор системы",
		HostnameLabel:    "Имя хоста",
		ArchLabel:        "Архитектура",
		KernelLabel:      "Ядро",
		CPUInfo:          "Процессор",
		CPUCores:         "Ядра",
		Memory:           "Память",
		Graphics:         "Графика",
		Advanced:         "Дополнительно",
		VirtSupport:      "Виртуализация: Поддерживается",
		VirtNotAvailable: "Виртуализация: Недоступна",
		FirmwareUEFI:     "Прошивка: UEFI",
		FirmwareBIOS:     "Прошивка: Legacy BIOS",
		UptimeLabel:      "Время работы",
		LoadAvgLabel:     "Средняя нагрузка",

		RequirementsTitle:    "ПРОВЕРКА СИСТЕМНЫХ ТРЕБОВАНИЙ",
		RAMWarning:           "ВНИМАНИЕ: Рекомендуется минимум 1 ГБ ОЗУ",
		CPUWarning:           "ВНИМАНИЕ: Рекомендуется двухъядерный процессор",
		DiskSpaceWarning:     "ВНИМАНИЕ: Требуется минимум 2 ГБ свободного места в /tmp",
		MeetsRequirements:    "✓ Система соответствует минимальным требованиям",
		MayNotPerform:        "⚠ Система может работать нестабильно",

		NoDisksFound:         "Подходящие диски не найдены. Проверьте подключение.",

		VMTitle:              "УСТАНОВКА ВИРТУАЛЬНОЙ МАШИНЫ",
		VMRequirements:       "Требования:\n- Поддержка KVM или виртуализации\n- 20 ГБ свободного места\n- 4 ГБ ОЗУ\n- Подключение к интернету",

		ConfigTitle:          "ВЫБЕРИТЕ КОНФИГУРАЦИЮ",
		ConfigSaved:          "Конфигурация сохранена в lainux-config.txt",

		NetworkChecking:      "Проверка сети...",
		NetworkConnected:     "Подключено ✓",
		NetworkOffline:       "Нет подключения ✗",
		PublicIP:             "Публичный IP",
		NoInterface:          "Активный интерфейс не найден",

		PressAnyKey:          "Нажмите любую клавишу для продолжения...",
		Back:                 "b: Назад",
		Quit:                 "q: Выход",
	},
}

// ---------- Styles ----------
var (
	titleStyle   = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("#cba6f7"))
	greenStyle   = lipgloss.NewStyle().Foreground(lipgloss.Color("#a6e3a1"))
	redStyle     = lipgloss.NewStyle().Foreground(lipgloss.Color("#f38ba8"))
	hintStyle    = lipgloss.NewStyle().Foreground(lipgloss.Color("#6c7086"))
	docStyle     = lipgloss.NewStyle().Margin(1, 2)
)

// ---------- Types ----------
type step int

const (
	stepWelcome step = iota
	stepMenu
	stepSelectDisk
	stepInstallHardware
	stepInstallVM
	stepHardwareInfo
	stepRequirements
	stepConfig
	stepDiskInfo
	stepNetworkCheck
	stepNetworkDiag
	stepExitConfirm
)

type model struct {
	currentStep    step
	menuIndex      int
	selectedDisk   string
	diskSize       string
	diskModel      string
	lang           string
	networkOnline  bool
	publicIP       string
	userInput      string
}

// ---------- Helpers ----------
func normalizeKey(k string) string {
	switch k {
	case "й", "Й", "q", "Q": return "q"
	case "б", "Б", "b", "B": return "b"
	case "у", "У", "y", "Y": return "y"
	case "н", "Н", "n", "N": return "n"
	case "д", "Д", "l", "L": return "l"
	case "enter", "\r", "\n": return "enter"
	case "esc": return "esc"
	case "up", "down": return k
	default: return strings.ToLower(k)
	}
}

func runCommand(name string, args ...string) (string, error) {
	cmd := exec.Command(name, args...)
	out, err := cmd.CombinedOutput()
	return string(out), err
}

func getHardwareInfo() map[string]string {
	info := make(map[string]string)

	hostname, _ := os.Hostname()
	info["hostname"] = hostname

	if out, _ := runCommand("uname", "-m"); out != "" {
		info["arch"] = strings.TrimSpace(out)
	}
	if out, _ := runCommand("uname", "-r"); out != "" {
		info["kernel"] = strings.Split(strings.TrimSpace(out), "-")[0]
	}

	if out, _ := runCommand("lscpu"); out != "" {
		for _, line := range strings.Split(out, "\n") {
			if strings.Contains(line, "Model name") {
				info["cpu"] = strings.TrimSpace(strings.SplitN(line, ":", 2)[1])
				break
			}
		}
	}
	if info["cpu"] == "" {
		if out, _ := runCommand("grep", "model name", "/proc/cpuinfo"); out != "" {
			lines := strings.Split(out, "\n")
			if len(lines) > 0 {
				info["cpu"] = strings.TrimSpace(strings.SplitN(lines[0], ":", 2)[1])
			}
		}
	}

	info["cores"] = strconv.Itoa(runtime.NumCPU())

	if out, _ := runCommand("free", "-h"); out != "" {
		lines := strings.Split(out, "\n")
		if len(lines) > 1 {
			fields := strings.Fields(lines[1])
			if len(fields) > 1 {
				info["ram"] = fields[1]
			}
		}
	}

	if out, _ := runCommand("lspci"); out != "" {
		for _, line := range strings.Split(out, "\n") {
			l := strings.ToLower(line)
			if strings.Contains(l, "vga") || strings.Contains(l, "3d") || strings.Contains(l, "display") {
				parts := strings.SplitN(line, ":", 2)
				if len(parts) == 2 {
					info["gpu"] = strings.TrimSpace(parts[1])
					break
				}
			}
		}
	}

	if out, _ := runCommand("uptime", "-p"); out != "" {
		info["uptime"] = strings.TrimSpace(out)
	} else if out, _ := runCommand("uptime"); out != "" {
		info["uptime"] = strings.TrimSpace(out)
	}

	if data, err := os.ReadFile("/proc/loadavg"); err == nil {
		parts := strings.Fields(string(data))
		if len(parts) >= 3 {
			info["load"] = parts[0] + " " + parts[1] + " " + parts[2]
		}
	}

	if _, err := os.Stat("/sys/firmware/efi"); err == nil {
		info["firmware"] = "uefi"
	} else {
		info["firmware"] = "bios"
	}

	if out, _ := runCommand("grep", "-E", "(vmx|svm)", "/proc/cpuinfo"); out != "" {
		info["virt"] = "supported"
	} else {
		info["virt"] = "none"
	}

	return info
}

func getDisks() []map[string]string {
	out, err := exec.Command("lsblk", "-d", "-o", "NAME,SIZE,MODEL", "-n").Output()
	if err != nil {
		return nil
	}
	var disks []map[string]string
	for _, line := range strings.Split(string(out), "\n") {
		fields := strings.Fields(line)
		if len(fields) >= 2 {
			name := fields[0]
			if strings.Contains(name, "sr") || strings.HasPrefix(name, "loop") {
				continue
			}
			size := fields[1]
			model := strings.Join(fields[2:], " ")
			if model == "" {
				model = "Unknown"
			}
			disks = append(disks, map[string]string{
				"name":  name,
				"size":  size,
				"model": model,
			})
		}
	}
	return disks
}

func checkNetwork() (bool, string) {
	conn, err := net.DialTimeout("tcp", "1.1.1.1:53", 3*time.Second)
	if err != nil {
		return false, ""
	}
	conn.Close()

	client := &http.Client{Timeout: 5 * time.Second}
	resp, err := client.Get("https://api.ipify.org")
	if err != nil {
		return true, ""
	}
	defer resp.Body.Close()
	ip, _ := bufio.NewReader(resp.Body).ReadString('\n')
	return true, strings.TrimSpace(ip)
}

func (m model) View() string {
	p := phrases[m.lang]

	switch m.currentStep {
	case stepWelcome:
		logo := titleStyle.Render(p.Logo)
		version := p.VersionLine
		now := time.Now().Format("2006-01-02 15:04:05")
		timeLine := fmt.Sprintf("%s: %s", p.SystemTime, now)
		start := greenStyle.Render("• Start Installation")
		return docStyle.Render(fmt.Sprintf("\n%s\n\n%s\n\n%s\n\n%s\n\n%s", logo, version, timeLine, start, hintStyle.Render("Press Enter to begin")))

	case stepMenu:
		var menu []string
		for i, item := range p.MenuItems {
			if i == m.menuIndex {
				menu = append(menu, greenStyle.Render("→ "+item))
			} else {
				menu = append(menu, "  "+item)
			}
		}
		info := getHardwareInfo()
		footer := fmt.Sprintf("%s | %s: %s | %s: %s", p.Navigation, p.Arch, info["arch"], p.Kernel, info["kernel"])
		return docStyle.Render(fmt.Sprintf("%s\n\n%s\n\n%s", titleStyle.Render(p.Title), strings.Join(menu, "\n"), hintStyle.Render(footer)))

	case stepSelectDisk:
		disks := getDisks()
		if len(disks) == 0 {
			return docStyle.Render(redStyle.Render(p.NoDisksFound))
		}
		var lines []string
		for i, disk := range disks {
			mark := "  "
			if i == m.menuIndex%len(disks) {
				mark = "→ "
			}
			lines = append(lines, fmt.Sprintf("%s/dev/%s (%s) — %s", mark, disk["name"], disk["size"], disk["model"]))
		}
		return docStyle.Render(fmt.Sprintf("%s\n\n%s\n\n%s", titleStyle.Render("Select Target Disk"), strings.Join(lines, "\n"), hintStyle.Render("Enter: Select | b: Back")))

	case stepInstallHardware:
		return docStyle.Render(fmt.Sprintf("%s\n\n%s\n\n%s",
			titleStyle.Render("Installation Summary"),
			fmt.Sprintf("Target: /dev/%s (%s %s)", m.selectedDisk, m.diskSize, m.diskModel),
			hintStyle.Render("Hardware installation simulated. Press 'b' to go back.")))

	case stepInstallVM:
		return docStyle.Render(fmt.Sprintf("%s\n\n%s\n\n%s",
			titleStyle.Render(p.VMTitle),
			p.VMRequirements,
			hintStyle.Render("VM setup simulated. Press 'b' to go back.")))

	case stepHardwareInfo:
		info := getHardwareInfo()
		lines := []string{
			titleStyle.Render(p.HardwareTitle),
			"",
			p.SystemOverview + ":",
			fmt.Sprintf("  %s: %s", p.HostnameLabel, info["hostname"]),
			fmt.Sprintf("  %s: %s", p.ArchLabel, info["arch"]),
			fmt.Sprintf("  %s: %s", p.KernelLabel, info["kernel"]),
			"",
			p.CPUInfo + ":",
			fmt.Sprintf("  %s", info["cpu"]),
			fmt.Sprintf("  %s: %s", p.CPUCores, info["cores"]),
			"",
			p.Memory + ":",
			fmt.Sprintf("  %s", info["ram"]),
			"",
			p.Graphics + ":",
			fmt.Sprintf("  %s", info["gpu"]),
			"",
			p.Advanced + ":",
			fmt.Sprintf("  %s", func() string {
				if info["virt"] == "supported" {
					return p.VirtSupport
				}
				return p.VirtNotAvailable
			}()),
			fmt.Sprintf("  %s", func() string {
				if info["firmware"] == "uefi" {
					return p.FirmwareUEFI
				}
				return p.FirmwareBIOS
			}()),
			fmt.Sprintf("  %s: %s", p.UptimeLabel, info["uptime"]),
			fmt.Sprintf("  %s: %s", p.LoadAvgLabel, info["load"]),
		}
		return docStyle.Render(strings.Join(lines, "\n") + "\n\n" + hintStyle.Render(p.PressAnyKey))

	case stepRequirements:
		info := getHardwareInfo()
		var warnings []string
		ram := info["ram"]
		if strings.HasSuffix(ram, "G") {
			if val, err := strconv.ParseFloat(strings.TrimSuffix(ram, "G"), 64); err == nil && val < 1 {
				warnings = append(warnings, p.RAMWarning)
			}
		} else if strings.HasSuffix(ram, "M") {
			warnings = append(warnings, p.RAMWarning)
		}
		cores, _ := strconv.Atoi(info["cores"])
		if cores < 2 {
			warnings = append(warnings, p.CPUWarning)
		}
		status := p.MeetsRequirements
		if len(warnings) > 0 {
			status = p.MayNotPerform
		}
		lines := []string{
			titleStyle.Render(p.RequirementsTitle),
			fmt.Sprintf("RAM: %s", info["ram"]),
			fmt.Sprintf("Cores: %s", info["cores"]),
			"",
		}
		for _, w := range warnings {
			lines = append(lines, redStyle.Render("⚠ "+w))
		}
		lines = append(lines, "", greenStyle.Render(status))
		return docStyle.Render(strings.Join(lines, "\n") + "\n\n" + hintStyle.Render(p.PressAnyKey))

	case stepConfig:
		os.WriteFile("lainux-config.txt", []byte("# Configuration saved\nType=minimal"), 0644)
		return docStyle.Render(fmt.Sprintf("%s\n\n%s\n\n%s",
			titleStyle.Render(p.ConfigTitle),
			greenStyle.Render(p.ConfigSaved),
			hintStyle.Render(p.PressAnyKey)))

	case stepDiskInfo:
		disks := getDisks()
		if len(disks) == 0 {
			return docStyle.Render(p.NoDisksFound)
		}
		var lines []string
		for _, d := range disks {
			lines = append(lines, fmt.Sprintf("/dev/%s (%s) — %s", d["name"], d["size"], d["model"]))
		}
		return docStyle.Render(fmt.Sprintf("%s\n\n%s\n\n%s",
			titleStyle.Render("Disk Information"),
			strings.Join(lines, "\n"),
			hintStyle.Render(p.PressAnyKey)))

	case stepNetworkCheck:
		status := p.NetworkOffline
		style := redStyle
		if m.networkOnline {
			status = p.NetworkConnected
			style = greenStyle
		}
		ipLine := ""
		if m.networkOnline && m.publicIP != "" {
			ipLine = fmt.Sprintf("\n  %s: %s", p.PublicIP, m.publicIP)
		}
		return docStyle.Render(fmt.Sprintf("%s\n\n  %s\n  %s%s\n\n%s",
			titleStyle.Render("Network Status"),
			p.NetworkChecking,
			style.Render(status),
			ipLine,
			hintStyle.Render(p.PressAnyKey)))

	case stepNetworkDiag:
		out, _ := runCommand("ip", "route")
		if strings.Contains(out, "dev") {
			fields := strings.Fields(out)
			for i, f := range fields {
				if f == "dev" && i+1 < len(fields) {
					return docStyle.Render(fmt.Sprintf("Active interface: %s\n\n%s", fields[i+1], hintStyle.Render(p.PressAnyKey)))
				}
			}
		}
		return docStyle.Render(redStyle.Render(p.NoInterface) + "\n\n" + hintStyle.Render(p.PressAnyKey))

	case stepExitConfirm:
		return docStyle.Render(fmt.Sprintf("%s\n\n%s\n\n%s (%s): %s",
			p.ExitConfirm,
			hintStyle.Render("Type to confirm:"),
			p.ExitAction,
			p.Quit,
			m.userInput))
	}

	return ""
}

func (m model) Init() tea.Cmd {
	return nil
}

func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	p := phrases[m.lang]

	switch msg := msg.(type) {
	case tea.KeyMsg:
		key := normalizeKey(msg.String())

		switch m.currentStep {
		case stepWelcome:
			if key == "enter" {
				m.currentStep = stepMenu
			} else if key == "q" || key == "esc" {
				m.currentStep = stepExitConfirm
				m.userInput = ""
			}

		case stepMenu:
			switch key {
			case "l":
				m.lang = "RU"
				if m.lang == "RU" {
					m.lang = "EN"
				}
				// Toggle
				if m.lang == "EN" {
					m.lang = "RU"
				} else {
					m.lang = "EN"
				}
			case "q", "esc":
				m.currentStep = stepExitConfirm
				m.userInput = ""
			case "up":
				m.menuIndex = (m.menuIndex - 1 + 9) % 9
			case "down":
				m.menuIndex = (m.menuIndex + 1) % 9
			case "enter":
				switch m.menuIndex {
				case 0:
					m.currentStep = stepSelectDisk
				case 1:
					m.currentStep = stepInstallVM
				case 2:
					m.currentStep = stepHardwareInfo
				case 3:
					m.currentStep = stepRequirements
				case 4:
					m.currentStep = stepConfig
				case 5:
					m.currentStep = stepDiskInfo
				case 6:
					m.currentStep = stepNetworkCheck
				case 7:
					m.currentStep = stepNetworkDiag
				case 8:
					m.currentStep = stepExitConfirm
					m.userInput = ""
				}
			}

		case stepSelectDisk:
			if key == "b" {
				m.currentStep = stepMenu
			} else if key == "q" || key == "esc" {
				m.currentStep = stepExitConfirm
				m.userInput = ""
			} else if key == "enter" {
				disks := getDisks()
				if len(disks) > 0 {
					disk := disks[m.menuIndex%len(disks)]
					m.selectedDisk = disk["name"]
					m.diskSize = disk["size"]
					m.diskModel = disk["model"]
					m.currentStep = stepInstallHardware
				}
			} else if key == "up" {
				m.menuIndex--
				if m.menuIndex < 0 {
					m.menuIndex = 0
				}
			} else if key == "down" {
				m.menuIndex++
			}

		case stepInstallHardware, stepInstallVM, stepHardwareInfo, stepRequirements,
			stepConfig, stepDiskInfo, stepNetworkCheck, stepNetworkDiag:
			if key == "b" || key == "esc" {
				m.currentStep = stepMenu
			} else if key == "q" {
				m.currentStep = stepExitConfirm
				m.userInput = ""
			}

		case stepExitConfirm:
			if key == "enter" && m.userInput == p.ExitAction {
				return m, tea.Quit
			} else if key == "backspace" {
				if len(m.userInput) > 0 {
					m.userInput = m.userInput[:len(m.userInput)-1]
				}
			} else if key == "esc" {
				m.currentStep = stepMenu
			} else if len(key) == 1 && len(m.userInput) < 10 {
				m.userInput += strings.ToUpper(key)
			}
		}
	}

	return m, nil
}

func initialModel() model {
	online, ip := checkNetwork()
	return model{
		currentStep:   stepWelcome,
		lang:          "EN",
		networkOnline: online,
		publicIP:      ip,
	}
}

func main() {
	p := tea.NewProgram(initialModel(), tea.WithAltScreen())
	if _, err := p.Run(); err != nil {
		fmt.Printf("Error: %v\n", err)
		os.Exit(1)
	}
}

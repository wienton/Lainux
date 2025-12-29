package main

import (
	"fmt"
	"net/http"
	"os"
	"os/exec"
	"runtime"
	"strings"
	"time"

	"github.com/charmbracelet/bubbles/list"
	"github.com/charmbracelet/bubbles/spinner"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
)

type Phrases struct {
	Title            string
	WelcomeTitle     string
	WelcomeSubtitle  string
	SystemInfo       string
	Arch             string
	Network          string
	Online           string
	Offline          string
	StartInstall     string
	Back             string
	Quit             string
	Next             string

	DiskTitle        string
	NoDisks          string
	SizeLabel        string
	DiskType         string
	DiskVendor       string
	DiskModel        string
	DiskPartitions   string
	PartCount        string
	SelDisk          string

	NetStatus        string
	QemuDetect       string
	Options          string
	Timezone         string
	Hostname         string
	RootPassword     string
	Swap             string
	Filesystem       string
	SSHServer        string
	GuestAgent       string
	CheckNet         string
	LangToggle       string
	Ready            string
	Enabled          string
	Disabled         string
}

var loc = map[string]Phrases{
	"EN": {
		Title: "LAINUXOS INSTALLER",
		WelcomeTitle: "LainuxOS",
		WelcomeSubtitle: "A minimal, modern Linux distribution",
		SystemInfo: "System Information",
		Arch: "Architecture",
		Network: "Network",
		Online: "Online",
		Offline: "Offline",
		StartInstall: "Start Install",
		Back: "b: Back",
		Quit: "q: Quit",
		Next: "Enter: Next",

		DiskTitle: "Select Target Disk",
		NoDisks: "No usable disks found",
		SizeLabel: "Size",
		DiskType: "Type",
		DiskVendor: "Vendor",
		DiskModel: "Model",
		DiskPartitions: "Partitions",
		PartCount: "partitions",
		SelDisk: "Selected Disk",

		NetStatus: "Net",
		QemuDetect: "QEMU",
		Options: "Configuration Options",
		Timezone: "Timezone",
		Hostname: "Hostname",
		RootPassword: "Show Root Password",
		Swap: "Swap",
		Filesystem: "Filesystem",
		SSHServer: "SSH Server",
		GuestAgent: "QEMU Guest Agent",
		CheckNet: "Check Net",
		LangToggle: "Language",
		Ready: "READY TO INSTALL ON",
		Enabled: "ON",
		Disabled: "OFF",
	},
	"RU": {
		Title: "УСТАНОВЩИК LAINUXOS",
		WelcomeTitle: "LainuxOS",
		WelcomeSubtitle: "Минималистичный и современный дистрибутив Linux",
		SystemInfo: "Системная информация",
		Arch: "Архитектура",
		Network: "Сеть",
		Online: "В сети",
		Offline: "Нет сети",
		StartInstall: "Начать установку",
		Back: "b: Назад",
		Quit: "q: Выход",
		Next: "Enter: Далее",

		DiskTitle: "Выберите диск",
		NoDisks: "Подходящие диски не найдены",
		SizeLabel: "Размер",
		DiskType: "Тип",
		DiskVendor: "Производитель",
		DiskModel: "Модель",
		DiskPartitions: "Разделы",
		PartCount: "разделов",
		SelDisk: "Выбранный диск",

		NetStatus: "Сеть",
		QemuDetect: "QEMU",
		Options: "Настройки конфигурации",
		Timezone: "Часовой пояс",
		Hostname: "Имя хоста",
		RootPassword: "Показать пароль root",
		Swap: "Раздел подкачки",
		Filesystem: "Файловая система",
		SSHServer: "SSH-сервер",
		GuestAgent: "QEMU Guest Agent",
		CheckNet: "Проверить сеть",
		LangToggle: "Язык",
		Ready: "ГОТОВ К УСТАНОВКЕ НА",
		Enabled: "ВКЛ",
		Disabled: "ВЫКЛ",
	},
}

var (
	docStyle     = lipgloss.NewStyle().Margin(1, 2)
	titleStyle   = lipgloss.NewStyle().Bold(true).Foreground(lipgloss.Color("#cba6f7"))
	subTitleStyle = lipgloss.NewStyle().Foreground(lipgloss.Color("#89b4fa"))
	infoStyle    = lipgloss.NewStyle().Foreground(lipgloss.Color("#a6e3a1"))
	activeStyle  = lipgloss.NewStyle().Foreground(lipgloss.Color("#a6e3a1"))
	warningStyle = lipgloss.NewStyle().Foreground(lipgloss.Color("#f38ba8"))
	hintStyle    = lipgloss.NewStyle().Foreground(lipgloss.Color("#6c7086"))
	centerStyle  = lipgloss.NewStyle().Padding(1, 2)
)

type DiskInfo struct {
	Device     string
	Size       string
	Type       string
	Vendor     string
	Model      string
	Partitions int
}

type diskItem struct {
	info DiskInfo
}

func (d diskItem) Title() string       { return d.info.Device }
func (d diskItem) Description() string { return "" }
func (d diskItem) FilterValue() string { return d.info.Device }

type step int

const (
	welcomeStep step = iota
	diskStep
	optionsStep
	summaryStep
)

type model struct {
	list            list.Model
	spinner         spinner.Model
	choice          DiskInfo
	currentStep     step
	online          bool
	isQEMU          bool
	installGuest    bool
	installSSH      bool
	enableSwap      bool
	showRootPass    bool
	timezone        string
	hostname        string
	filesystem      string
	lang            string // "EN" or "RU"
}

type checkNetMsg bool

func normalizeKey(k string) string {
	switch k {
	case "й", "Й", "q", "Q": return "q"
	case "б", "Б", "b", "B": return "b"
	case "г", "Г", "g", "G": return "g"
	case "т", "Т", "t", "T": return "t"
	case "ш", "Ш", "i", "I": return "i"
	case "д", "Д", "l", "L": return "l"
	case "с", "С", "s", "S": return "s"
	case "ы", "Ы", "h", "H": return "h"
	case "а", "А", "a", "A": return "a"
	case "ф", "Ф", "f", "F": return "f"
	case "п", "П", "p", "P": return "p"
	case "enter", "ctrl+c": return k
	default: return strings.ToLower(k)
	}
}

func checkInternet() tea.Cmd {
	return func() tea.Msg {
		endpoints := []string{"https://1.1.1.1", "https://8.8.8.8", "https://google.com/generate_204"}
		client := &http.Client{Timeout: 3 * time.Second}
		for _, url := range endpoints {
			if resp, err := client.Get(url); err == nil {
				resp.Body.Close()
				return checkNetMsg(true)
			}
		}
		return checkNetMsg(false)
	}
}

func detectDiskType(name string) string {
	if strings.HasPrefix(name, "loop") {
		return "loop"
	}
	if strings.HasPrefix(name, "nvme") {
		return "NVMe SSD"
	}
	if strings.HasPrefix(name, "sd") || strings.HasPrefix(name, "hd") {
		rotPath := fmt.Sprintf("/sys/block/%s/queue/rotational", name)
		data, err := os.ReadFile(rotPath)
		if err == nil {
			if strings.TrimSpace(string(data)) == "1" {
				return "HDD"
			}
			return "SATA SSD"
		}
	}
	return "Unknown"
}

func getDisks() []list.Item {
	cmd := exec.Command("lsblk", "-d", "-o", "NAME,SIZE,TRAN,VENDOR,MODEL", "-n", "-P")
	out, err := cmd.Output()
	if err != nil {
		return []list.Item{diskItem{info: DiskInfo{Device: "ERROR", Size: "lsblk failed"}}}
	}

	lines := strings.Split(strings.TrimSpace(string(out)), "\n")
	var items []list.Item

	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		props := make(map[string]string)
		fields := strings.Split(line, " ")
		for _, f := range fields {
			f = strings.TrimSpace(f)
			if f == "" {
				continue
			}
			if kv := strings.SplitN(f, "=", 2); len(kv) == 2 {
				key := kv[0]
				val := strings.Trim(kv[1], `"`)
				props[key] = val
			}
		}

		name := props["NAME"]
		if name == "" {
			continue
		}
		// Skip optical, rom, cdrom
		if strings.Contains(name, "sr") || strings.Contains(name, "rom") || strings.HasPrefix(name, "loop") {
			continue
		}

		size := props["SIZE"]
		vendor := props["VENDOR"]
		model := props["MODEL"]
		if model == "" {
			model = "Generic"
		}
		if vendor == "" {
			vendor = "Unknown"
		}

		// Count partitions
		partCount := 0
		if partOut, err := exec.Command("lsblk", "-n", "-o", "NAME").CombinedOutput(); err == nil {
			partLines := strings.Split(string(partOut), "\n")
			for _, pl := range partLines {
				pl = strings.TrimSpace(pl)
				if strings.HasPrefix(pl, name) && pl != name && len(pl) > len(name) {
					partCount++
				}
			}
		}

		diskType := detectDiskType(name)

		items = append(items, diskItem{
			info: DiskInfo{
				Device:     "/dev/" + name,
				Size:       size,
				Type:       diskType,
				Vendor:     vendor,
				Model:      model,
				Partitions: partCount,
			},
		})
	}

	if len(items) == 0 {
		return []list.Item{diskItem{info: DiskInfo{Device: "NO DISKS", Size: "None detected"}}}
	}
	return items
}

func initialModel() model {
	s := spinner.New()
	s.Spinner = spinner.Dot
	s.Style = activeStyle

	delegate := list.NewDefaultDelegate()
	// delegate.Width not a field — removed
	l := list.New([]list.Item{}, delegate, 0, 0)
	l.SetShowStatusBar(false)
	l.SetFilteringEnabled(false)

	data, _ := os.ReadFile("/proc/cpuinfo")
	qemu := strings.Contains(string(data), "QEMU") || strings.Contains(string(data), "KVM")

	return model{
		list:         l,
		spinner:      s,
		isQEMU:       qemu,
		timezone:     "UTC",
		hostname:     "lainux",
		filesystem:   "ext4",
		lang:         "EN",
		currentStep:  welcomeStep,
	}
}

func (m model) Init() tea.Cmd {
	return tea.Batch(checkInternet(), m.spinner.Tick)
}

func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	p := loc[m.lang]

	switch msg := msg.(type) {
	case tea.KeyMsg:
		key := normalizeKey(msg.String())
		switch key {
		case "ctrl+c", "q":
			return m, tea.Quit
		case "l":
			if m.lang == "EN" {
				m.lang = "RU"
			} else {
				m.lang = "EN"
			}
		case "b":
			if m.currentStep > welcomeStep {
				m.currentStep--
			}
		case "enter":
			switch m.currentStep {
			case welcomeStep:
				m.currentStep = diskStep
				m.list.SetItems(getDisks())
				m.list.Title = p.DiskTitle
			case diskStep:
				if i, ok := m.list.SelectedItem().(diskItem); ok {
					m.choice = i.info
					m.currentStep = optionsStep
				}
			case optionsStep:
				m.currentStep = summaryStep
			}
		}

		if m.currentStep == optionsStep {
			switch key {
			case "g":
				if m.isQEMU {
					m.installGuest = !m.installGuest
				}
			case "s":
				m.installSSH = !m.installSSH
			case "a":
				m.enableSwap = !m.enableSwap
			case "p":
				m.showRootPass = !m.showRootPass
			case "f":
				if m.filesystem == "ext4" {
					m.filesystem = "btrfs"
				} else {
					m.filesystem = "ext4"
				}
			case "h":
				hostnames := []string{"lainux", "desktop", "workstation", "server"}
				for i, h := range hostnames {
					if h == m.hostname {
						m.hostname = hostnames[(i+1)%len(hostnames)]
						break
					}
				}
			case "t":
				zones := []string{"UTC", "GMT", "MSK", "PST", "EST", "CET"}
				for i, z := range zones {
					if z == m.timezone {
						m.timezone = zones[(i+1)%len(zones)]
						break
					}
				}
			case "i":
				return m, checkInternet()
			}
		}

	case checkNetMsg:
		m.online = bool(msg)

	case spinner.TickMsg:
		var cmd tea.Cmd
		m.spinner, cmd = m.spinner.Update(msg)
		return m, cmd

	case tea.WindowSizeMsg:
		h, v := docStyle.GetFrameSize()
		m.list.SetSize(msg.Width-h, msg.Height-v)
	}

	if m.currentStep == diskStep {
		var cmd tea.Cmd
		m.list, cmd = m.list.Update(msg)
		return m, cmd
	}

	return m, nil
}

func (m model) View() string {
	p := loc[m.lang]

	netStatus := p.Offline
	netStyle := warningStyle
	if m.online {
		netStatus = p.Online
		netStyle = activeStyle
	}

	header := fmt.Sprintf("  %s | %s: %s | QEMU: %v | [l] %s: %s\n\n",
		titleStyle.Render(p.Title), p.NetStatus, netStyle.Render(netStatus), m.isQEMU, p.LangToggle, m.lang)

	switch m.currentStep {
	case welcomeStep:
		sysInfo := fmt.Sprintf(
			"  %s: %s\n  %s: %s\n  %s: %v",
			p.Arch, runtime.GOARCH,
			p.Network, netStyle.Render(netStatus),
			p.QemuDetect, m.isQEMU,
		)
		welcome := centerStyle.Render(
			titleStyle.Render(p.WelcomeTitle) + "\n\n" +
				subTitleStyle.Render(p.WelcomeSubtitle) + "\n\n" +
				titleStyle.Render(p.SystemInfo) + "\n" +
				infoStyle.Render(sysInfo) + "\n\n" +
				activeStyle.Render("• "+p.StartInstall) + "\n\n" +
				hintStyle.Render(fmt.Sprintf("[Enter] %s | [q] %s", p.Next, p.Quit)),
		)
		return docStyle.Render(welcome)

	case diskStep:
		items := m.list.Items()
		if len(items) == 1 {
			if d, ok := items[0].(diskItem); ok {
				if d.info.Device == "NO DISKS" || d.info.Device == "ERROR" {
					content := header + warningStyle.Render(p.NoDisks)
					return docStyle.Render(content)
				}
			}
		}

		var lines []string
		lines = append(lines, header)
		lines = append(lines, titleStyle.Render(p.DiskTitle)+"\n")

		for i, item := range items {
			d, ok := item.(diskItem)
			if !ok {
				continue
			}
			mark := "  "
			if i == m.list.Index() {
				mark = "→ "
			}
			lines = append(lines, fmt.Sprintf("%s%s", mark, activeStyle.Render(d.info.Device)))
			lines = append(lines, fmt.Sprintf("    %s: %s", p.SizeLabel, d.info.Size))
			lines = append(lines, fmt.Sprintf("    %s: %s", p.DiskType, d.info.Type))
			if d.info.Vendor != "Unknown" {
				lines = append(lines, fmt.Sprintf("    %s: %s", p.DiskVendor, d.info.Vendor))
			}
			if d.info.Model != "Generic" {
				lines = append(lines, fmt.Sprintf("    %s: %s", p.DiskModel, d.info.Model))
			}
			lines = append(lines, fmt.Sprintf("    %s: %d %s", p.DiskPartitions, d.info.Partitions, p.PartCount))
			lines = append(lines, "")
		}

		lines = append(lines, hintStyle.Render(fmt.Sprintf("[Enter] %s | [b] %s | [q] %s", p.Next, p.Back, p.Quit)))
		return docStyle.Render(strings.Join(lines, "\n"))

	case optionsStep:
		var qemuOpt string
		if m.isQEMU {
			status := p.Disabled
			if m.installGuest {
				status = activeStyle.Render(p.Enabled)
			}
			qemuOpt = fmt.Sprintf("  [g] %s: %s\n", p.GuestAgent, status)
		}

		sshStatus := p.Disabled
		if m.installSSH {
			sshStatus = activeStyle.Render(p.Enabled)
		}

		swapStatus := p.Disabled
		if m.enableSwap {
			swapStatus = activeStyle.Render(p.Enabled)
		}

		passStatus := p.Disabled
		if m.showRootPass {
			passStatus = activeStyle.Render(p.Enabled)
		}

		content := header + titleStyle.Render(p.Options) + "\n\n"
		content += fmt.Sprintf("  %s: %s\n", p.SelDisk, activeStyle.Render(m.choice.Device))
		content += fmt.Sprintf("  %s: %s\n", p.SizeLabel, m.choice.Size)
		content += fmt.Sprintf("  [t] %s: %s\n", p.Timezone, activeStyle.Render(m.timezone))
		content += fmt.Sprintf("  [h] %s: %s\n", p.Hostname, activeStyle.Render(m.hostname))
		content += fmt.Sprintf("  [f] %s: %s\n", p.Filesystem, activeStyle.Render(m.filesystem))
		content += fmt.Sprintf("  [s] %s: %s\n", p.SSHServer, sshStatus)
		content += fmt.Sprintf("  [a] %s: %s\n", p.Swap, swapStatus)
		content += fmt.Sprintf("  [p] %s: %s\n", p.RootPassword, passStatus)
		content += qemuOpt
		content += fmt.Sprintf("  [i] %s\n", p.CheckNet)
		content += "\n  " + hintStyle.Render(fmt.Sprintf("%s | %s | %s", p.Next, p.Back, p.Quit))
		return docStyle.Render(content)

	case summaryStep:
		final := header + "\n  " + activeStyle.Bold(true).Render(p.Ready+" "+m.choice.Device) +
			"\n\n  " + fmt.Sprintf("%s: %s | %s: %s | %s: %s",
				p.Filesystem, m.filesystem,
				p.Timezone, m.timezone,
				p.SSHServer, func() string {
					if m.installSSH {
						return p.Enabled
					}
					return p.Disabled
				}()) +
			"\n\n  " + hintStyle.Render("Press 'q' to exit demo mode.")
		return docStyle.Render(final)
	}

	return ""
}

func main() {
	p := tea.NewProgram(initialModel(), tea.WithAltScreen())
	if _, err := p.Run(); err != nil {
		fmt.Printf("Error: %v\n", err)
		os.Exit(1)
	}
}

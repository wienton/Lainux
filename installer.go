package main

import (
	"fmt"
	"os"
	"os/exec"
	"strings"
	"time"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/dialog"
	"fyne.io/fyne/v2/layout"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

type Installer struct {
	app            fyne.App
	window         fyne.Window
	currentContent *fyne.Container

	// data
	disks        []string
	selectedDisk string
	hostname     string
	username     string
	password     string
	timezone     string
	keyboard     string
	installGUI   bool

	// widgets
	diskList    *widget.List
	hostnameEntry *widget.Entry
	usernameEntry *widget.Entry
	passwordEntry *widget.Entry
	guiCheck    *widget.Check
	progressBar *widget.ProgressBar
	currentStep *widget.Label
}

func main() {
	installer := &Installer{
		app:        app.New(),
		hostname:   "lainux-pc",
		username:   "lainux-user",
		timezone:   "UTC",
		keyboard:   "us",
		installGUI: true,
	}

	installer.window = installer.app.NewWindow("LainuxOS Installer")
	installer.window.Resize(fyne.NewSize(800, 600))
	installer.window.CenterOnScreen()

	// load date
	installer.detectDisks()

	// show one page
	installer.showWelcomePage()

	installer.window.ShowAndRun()
}

func (i *Installer) detectDisks() {
	// sim
	i.disks = []string{
		"/dev/sda - SATA HDD, 500GB",
		"/dev/sdb - SATA SSD, 1TB",
		"/dev/nvme0n1 - NVMe SSD, 256GB",
	}
}

func (i *Installer) showWelcomePage() {
	title := widget.NewLabelWithStyle("Welcome to LainuxOS Installer", fyne.TextAlignCenter, fyne.TextStyle{Bold: true})
	title.TextStyle = fyne.TextStyle{Bold: true}
	title.Alignment = fyne.TextAlignCenter

	content := widget.NewLabel(`This installer will guide you through setting up LainuxOS on your computer.

System Requirements:
• Minimum 10 GB of free disk space
• 2 GB RAM (4 GB recommended)
• 64-bit processor
• Internet connection (recommended)

Please make sure you have backed up any important data before proceeding.`)
	content.Wrapping = fyne.TextWrapWord

	nextBtn := widget.NewButton("Next", func() {
		i.showDiskPage()
	})
	nextBtn.Importance = widget.HighImportance

	i.currentContent = container.NewVBox(
		title,
		layout.NewSpacer(),
		content,
		layout.NewSpacer(),
		container.NewCenter(nextBtn),
	)

	i.window.SetContent(i.currentContent)
}

func (i *Installer) showDiskPage() {
	title := widget.NewLabelWithStyle("Select Installation Disk", fyne.TextAlignCenter, fyne.TextStyle{Bold: true})

	if i.diskList == nil {
		i.diskList = widget.NewList(
			func() int {
				return len(i.disks)
			},
			func() fyne.CanvasObject {
				return widget.NewLabel("template")
			},
			func(id widget.ListItemID, obj fyne.CanvasObject) {
				obj.(*widget.Label).SetText(i.disks[id])
			},
		)

		i.diskList.OnSelected = func(id widget.ListItemID) {
			i.selectedDisk = i.disks[id]
		}
	}

	backBtn := widget.NewButton("Back", func() {
		i.showWelcomePage()
	})

	nextBtn := widget.NewButton("Next", func() {
		if i.selectedDisk == "" {
			dialog.ShowInformation("No Disk Selected",
				"Please select a disk for installation", i.window)
			return
		}
		i.showSystemPage()
	})
	nextBtn.Importance = widget.HighImportance

	buttons := container.NewHBox(
		backBtn,
		layout.NewSpacer(),
		nextBtn,
	)

	i.currentContent = container.NewBorder(
		title,
		buttons,
		nil, nil,
		container.NewVBox(
			widget.NewLabel("Choose the disk where LainuxOS will be installed:"),
			i.diskList,
		),
	)

	i.window.SetContent(i.currentContent)
}

func (i *Installer) showSystemPage() {
	title := widget.NewLabelWithStyle("System Configuration", fyne.TextAlignCenter, fyne.TextStyle{Bold: true})

	// Hostname
	hostnameLabel := widget.NewLabel("Computer Name:")
	if i.hostnameEntry == nil {
		i.hostnameEntry = widget.NewEntry()
		i.hostnameEntry.SetText(i.hostname)
		i.hostnameEntry.OnChanged = func(text string) {
			i.hostname = text
		}
	}

	if i.guiCheck == nil {
		i.guiCheck = widget.NewCheck("Install Desktop Environment", func(checked bool) {
			i.installGUI = checked
		})
		i.guiCheck.SetChecked(i.installGUI)
	}

	backBtn := widget.NewButton("Back", func() {
		i.showDiskPage()
	})

	nextBtn := widget.NewButton("Next", func() {
		if i.hostname == "" {
			dialog.ShowInformation("Invalid Hostname",
				"Please enter a computer name", i.window)
			return
		}
		i.showUserPage()
	})
	nextBtn.Importance = widget.HighImportance

	buttons := container.NewHBox(
		backBtn,
		layout.NewSpacer(),
		nextBtn,
	)

	form := container.NewVBox(
		hostnameLabel,
		i.hostnameEntry,
		layout.NewSpacer(),
		i.guiCheck,
	)

	i.currentContent = container.NewBorder(
		title,
		buttons,
		nil, nil,
		form,
	)

	i.window.SetContent(i.currentContent)
}

func (i *Installer) showUserPage() {
	title := widget.NewLabelWithStyle("Create User Account", fyne.TextAlignCenter, fyne.TextStyle{Bold: true})

	usernameLabel := widget.NewLabel("Username:")
	if i.usernameEntry == nil {
		i.usernameEntry = widget.NewEntry()
		i.usernameEntry.SetText(i.username)
		i.usernameEntry.OnChanged = func(text string) {
			i.username = text
		}
	}


	passwordLabel := widget.NewLabel("Password:")
	if i.passwordEntry == nil {
		i.passwordEntry = widget.NewPasswordEntry()
		i.passwordEntry.OnChanged = func(text string) {
			i.password = text
		}
	}

	backBtn := widget.NewButton("Back", func() {
		i.showSystemPage()
	})

	nextBtn := widget.NewButton("Next", func() {
		if i.username == "" {
			dialog.ShowInformation("Invalid Username",
				"Please enter a username", i.window)
			return
		}
		if i.password == "" {
			dialog.ShowInformation("Invalid Password",
				"Please enter a password", i.window)
			return
		}
		i.showSummaryPage()
	})
	nextBtn.Importance = widget.HighImportance

	buttons := container.NewHBox(
		backBtn,
		layout.NewSpacer(),
		nextBtn,
	)

	form := container.NewVBox(
		usernameLabel,
		i.usernameEntry,
		layout.NewSpacer(),
		passwordLabel,
		i.passwordEntry,
	)

	i.currentContent = container.NewBorder(
		title,
		buttons,
		nil, nil,
		form,
	)

	i.window.SetContent(i.currentContent)
}

func (i *Installer) showSummaryPage() {
	title := widget.NewLabelWithStyle("Installation Summary", fyne.TextAlignCenter, fyne.TextStyle{Bold: true})

	summaryText := fmt.Sprintf(`Installation Disk: %s

Computer Name: %s
Username: %s
Desktop Environment: %v

WARNING: All data on the selected disk will be permanently erased!
Make sure you have backed up any important files.`,
		i.selectedDisk, i.hostname, i.username, i.installGUI)

	summary := widget.NewLabel(summaryText)
	summary.Wrapping = fyne.TextWrapWord

	backBtn := widget.NewButton("Back", func() {
		i.showUserPage()
	})

	installBtn := widget.NewButton("Install LainuxOS", func() {
		dialog.ShowConfirm("Confirm Installation",
			fmt.Sprintf("Are you sure you want to install LainuxOS on %s?\n\nThis will ERASE ALL DATA on this disk!", i.selectedDisk),
			func(confirmed bool) {
				if confirmed {
					i.startInstallation()
				}
			}, i.window)
	})
	installBtn.Importance = widget.DangerImportance

	buttons := container.NewHBox(
		backBtn,
		layout.NewSpacer(),
		installBtn,
	)

	i.currentContent = container.NewBorder(
		title,
		buttons,
		nil, nil,
		container.NewVScroll(summary),
	)

	i.window.SetContent(i.currentContent)
}

func (i *Installer) startInstallation() {
	title := widget.NewLabelWithStyle("Installing LainuxOS", fyne.TextAlignCenter, fyne.TextStyle{Bold: true})


	i.progressBar = widget.NewProgressBar()
	i.progressBar.Min = 0
	i.progressBar.Max = 100
	i.progressBar.Value = 0


	i.currentStep = widget.NewLabel("Preparing installation...")
	i.currentStep.Wrapping = fyne.TextWrapWord


	content := container.NewVBox(
		i.progressBar,
		layout.NewSpacer(),
		i.currentStep,
		layout.NewSpacer(),
	)

	i.currentContent = container.NewBorder(
		title,
		nil,
		nil, nil,
		content,
	)

	i.window.SetContent(i.currentContent)

	go i.performInstallation()
}

func (i *Installer) performInstallation() {
	steps := []struct {
		name   string
		action func() error
	}{
		{"Preparing disk...", i.prepareDisk},
		{"Creating filesystems...", i.createFilesystems},
		{"Mounting partitions...", i.mountPartitions},
		{"Installing base system...", i.installBaseSystem},
		{"Configuring system...", i.configureSystem},
		{"Creating user account...", i.createUser},
		{"Installing bootloader...", i.installBootloader},
		{"Finalizing installation...", i.finalizeInstall},
	}

	for idx, step := range steps {

		progress := float64(idx+1) / float64(len(steps)) * 100
		i.progressBar.SetValue(progress)
		i.currentStep.SetText(step.name)

		if err := step.action(); err != nil {
			i.showError(fmt.Sprintf("Error during %s: %v", step.name, err))
			return
		}

		time.Sleep(1 * time.Second)
	}
	i.showCompletionPage()
}

func (i *Installer) prepareDisk() error {
	return nil
}

func (i *Installer) createFilesystems() error {
	return nil
}

func (i *Installer) mountPartitions() error {
	return nil
}

func (i *Installer) installBaseSystem() error {
	return nil
}

func (i *Installer) configureSystem() error {
	return nil
}

func (i *Installer) createUser() error {
	return nil
}

func (i *Installer) installBootloader() error {
	return nil
}

func (i *Installer) finalizeInstall() error {
	return nil
}

func (i *Installer) showError(message string) {
	i.app.QueueEvent(func() {
		dialog.ShowError(fmt.Errorf(message), i.window)

		backBtn := widget.NewButton("Back to Summary", func() {
			i.showSummaryPage()
		})

		i.currentContent = container.NewVBox(
			widget.NewLabel("Installation Failed"),
			widget.NewLabel(message),
			backBtn,
		)

		i.window.SetContent(i.currentContent)
	})
}

func (i *Installer) showCompletionPage() {
	i.app.QueueEvent(func() {
		title := widget.NewLabelWithStyle("🎉 Installation Complete!", fyne.TextAlignCenter, fyne.TextStyle{Bold: true})

		message := widget.NewLabel(`LainuxOS has been successfully installed on your computer!

You can now:
• Reboot into your new system
• Remove the installation media
• Start using LainuxOS!`)
		message.Wrapping = fyne.TextWrapWord
		message.Alignment = fyne.TextAlignCenter

		rebootBtn := widget.NewButton("Reboot Now", func() {
			cmd := exec.Command("reboot")
			cmd.Run()
			i.window.Close()
		})
		rebootBtn.Importance = widget.HighImportance

		exitBtn := widget.NewButton("Exit Installer", func() {
			i.window.Close()
		})

		i.currentContent = container.NewVBox(
			title,
			layout.NewSpacer(),
			message,
			layout.NewSpacer(),
			container.NewCenter(
				container.NewHBox(
					rebootBtn,
					exitBtn,
				),
			),
		)

		i.window.SetContent(i.currentContent)
	})
}

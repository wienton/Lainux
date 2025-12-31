package ui

import (
	"github.com/rivo/tview"
)

func Run() error {
	app := tview.NewApplication()

	welcome := tview.NewTextView().
		SetText("Добро пожаловать в установщик вашего дистрибутива!").
		SetTextAlign(tview.AlignCenter)

	app.SetRoot(welcome, true).
		SetFocus(welcome)

	return app.Run()
}

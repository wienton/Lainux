package main

import (
	"fmt"
	"os"

	"github.com/wienton/LainuxOS/go/internal/ui"
)

func main() {
	if err := ui.Run(); err != nil {
		fmt.Fprintf(os.Stderr, "Ошибка: %v\n", err)
		os.Exit(1)
	}
}

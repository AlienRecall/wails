package main

import (
	"github.com/wailsapp/wails/v2/pkg/options/dialog"
	"io/ioutil"

	"github.com/wailsapp/wails/v2"
)

type Notepad struct {
	runtime *wails.Runtime
}

func (n *Notepad) WailsInit(runtime *wails.Runtime) error {
	n.runtime = runtime
	return nil
}

func (n *Notepad) LoadNotes() (string, error) {

	selectedFiles := n.runtime.Dialog.Open(&dialog.OpenDialog{
		DefaultFilename: "notes.md",
		Filters:         "*.md",
		AllowFiles:      true,
	})

	// selectedFiles is a string slice. Get the first selection
	if len(selectedFiles) == 0 {
		// Cancelled
		return "", nil
	}

	// Load notes
	noteData, err := ioutil.ReadFile(selectedFiles[0])
	if err != nil {
		return "", err
	}

	return string(noteData), nil
}

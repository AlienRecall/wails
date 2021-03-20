package main

import (
	"fmt"
	"github.com/wailsapp/wails/v2/pkg/menu/keys"
	"strconv"
	"sync"

	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/menu"
)

// Menu struct
type Menu struct {
	runtime *wails.Runtime

	dynamicMenuCounter        int
	dynamicMenuOneItems       []*menu.MenuItem
	lock                      sync.Mutex
	dynamicMenuItems          map[string]*menu.MenuItem
	anotherDynamicMenuCounter int

	// Menus
	removeMenuItem        *menu.MenuItem
	dynamicMenuOneSubmenu *menu.MenuItem
}

// WailsInit is called at application startup
func (m *Menu) WailsInit(runtime *wails.Runtime) error {
	// Perform your setup here
	m.runtime = runtime

	return nil
}

func (m *Menu) incrementcounter() int {
	m.dynamicMenuCounter++
	return m.dynamicMenuCounter
}

func (m *Menu) decrementcounter() int {
	m.dynamicMenuCounter--
	return m.dynamicMenuCounter
}

func (m *Menu) addDynamicMenusOneMenu(data *menu.CallbackData) {

	// Lock because this method will be called in a gorouting
	m.lock.Lock()
	defer m.lock.Unlock()

	// Get this menu's parent
	mi := data.MenuItem
	parent := mi.Parent()
	counter := m.incrementcounter()
	menuText := "Dynamic Menu Item " + strconv.Itoa(counter)
	newDynamicMenu := menu.Text(menuText, nil, nil)
	m.dynamicMenuOneItems = append(m.dynamicMenuOneItems, newDynamicMenu)
	parent.Append(newDynamicMenu)

	// If this is the first dynamic menu added, let's add a remove menu item
	if counter == 1 {
		m.removeMenuItem = menu.Text("Remove "+menuText, keys.CmdOrCtrl("-"), m.removeDynamicMenuOneMenu)
		parent.Prepend(m.removeMenuItem)
	} else {
		// Test if the remove menu hasn't already been removed in another thread
		if m.removeMenuItem != nil {
			m.removeMenuItem.Label = "Remove " + menuText
		}
	}
	m.runtime.Menu.UpdateApplicationMenu()
}

func (m *Menu) removeDynamicMenuOneMenu(_ *menu.CallbackData) {
	//
	// Lock because this method will be called in a goroutine
	m.lock.Lock()
	defer m.lock.Unlock()

	// Get the last menu we added
	lastItemIndex := len(m.dynamicMenuOneItems) - 1
	lastMenuAdded := m.dynamicMenuOneItems[lastItemIndex]

	// Remove from slice
	m.dynamicMenuOneItems = m.dynamicMenuOneItems[:lastItemIndex]

	// Remove the item from the menu
	lastMenuAdded.Remove()

	// Update the counter
	counter := m.decrementcounter()

	// If we deleted the last dynamic menu, remove the "Remove Last Item" menu
	if counter == 0 {
		// Remove it!
		m.removeMenuItem.Remove()
	} else {
		// Update label
		menuText := "Dynamic Menu Item " + strconv.Itoa(counter)
		m.removeMenuItem.Label = "Remove " + menuText
	}

	// 	parent.Append(menu.Text(menuText, menuText, menu.Key("[")))
	m.runtime.Menu.UpdateApplicationMenu()
}

func (m *Menu) createDynamicMenuTwo(data *menu.CallbackData) {

	// Hide this menu
	data.MenuItem.Hidden = true

	// Create our submenu
	dm2 := menu.SubMenu("Dynamic Menus 2", menu.NewMenuFromItems(
		menu.Text("Insert Before Random Menu Item", keys.CmdOrCtrl("]"), m.insertBeforeRandom),
		menu.Text("Insert After Random Menu Item", keys.CmdOrCtrl("["), m.insertAfterRandom),
		menu.Separator(),
	))

	//m.runtime.Menu.On("Insert Before Random", m.insertBeforeRandom)
	//m.runtime.Menu.On("Insert After Random", m.insertAfterRandom)

	// Initialise dynamicMenuItems
	m.dynamicMenuItems = make(map[string]*menu.MenuItem)

	// Create some random menu items
	m.anotherDynamicMenuCounter = 5
	for index := 0; index < m.anotherDynamicMenuCounter; index++ {
		text := "Other Dynamic Menu Item " + strconv.Itoa(index+1)
		item := menu.Text(text, nil, nil)
		m.dynamicMenuItems[text] = item
		dm2.Append(item)
	}

	m.dynamicMenuOneSubmenu.InsertAfter(dm2)
	m.runtime.Menu.UpdateApplicationMenu()
}

func (m *Menu) insertBeforeRandom(_ *menu.CallbackData) {

	// Lock because this method will be called in a goroutine
	m.lock.Lock()
	defer m.lock.Unlock()

	// Pick a random menu
	var randomMenuItem *menu.MenuItem
	for _, randomMenuItem = range m.dynamicMenuItems {
		break
	}

	if randomMenuItem == nil {
		return
	}

	m.anotherDynamicMenuCounter++
	text := "Other Dynamic Menu Item " + strconv.Itoa(m.anotherDynamicMenuCounter)
	newItem := menu.Text(text, nil, nil)
	m.dynamicMenuItems[text] = newItem

	m.runtime.Log.Info(fmt.Sprintf("Inserting menu item '%s' before menu item '%s'", newItem.Label, randomMenuItem.Label))

	randomMenuItem.InsertBefore(newItem)
	m.runtime.Menu.UpdateApplicationMenu()
}

func (m *Menu) insertAfterRandom(_ *menu.CallbackData) {

	// Pick a random menu
	var randomMenuItem *menu.MenuItem
	for _, randomMenuItem = range m.dynamicMenuItems {
		break
	}

	if randomMenuItem == nil {
		return
	}

	m.anotherDynamicMenuCounter++
	text := "Other Dynamic Menu Item " + strconv.Itoa(m.anotherDynamicMenuCounter)
	newItem := menu.Text(text, nil, nil)
	m.dynamicMenuItems[text] = newItem

	m.runtime.Log.Info(fmt.Sprintf("Inserting menu item '%s' after menu item '%s'", newItem.Label, randomMenuItem.Label))

	randomMenuItem.InsertBefore(newItem)
	m.runtime.Menu.UpdateApplicationMenu()
}

func (m *Menu) processPlainText(callbackData *menu.CallbackData) {
	label := callbackData.MenuItem.Label
	fmt.Printf("\n\n\n\n\n\n\nMenu Item label = `%s`\n\n\n\n\n", label)
}

func (m *Menu) createApplicationMenu() *menu.Menu {

	m.dynamicMenuOneSubmenu = menu.SubMenuWithID("Dynamic Menus 1", menu.NewMenuFromItems(
		menu.Text("Add Menu Item", keys.CmdOrCtrl("+"), m.addDynamicMenusOneMenu),
		menu.Separator(),
	))

	// Create menu
	myMenu := menu.DefaultMacMenu()

	windowMenu := menu.SubMenu("Test", menu.NewMenuFromItems(
		menu.Togglefullscreen(),
		menu.Minimize(),
		menu.Zoom(),

		menu.Separator(),

		menu.Copy(),
		menu.Cut(),
		menu.Delete(),

		menu.Separator(),

		menu.Front(),

		menu.SubMenu("Test Submenu", menu.NewMenuFromItems(
			menu.Text("Plain text", nil, m.processPlainText),
			menu.Text("Show Dynamic Menus 2 Submenu", nil, m.createDynamicMenuTwo),
			menu.SubMenu("Accelerators", menu.NewMenuFromItems(
				menu.SubMenu("Modifiers", menu.NewMenuFromItems(
					menu.Text("Shift accelerator", keys.Shift("o"), nil),
					menu.Text("Control accelerator", keys.Control("o"), nil),
					menu.Text("Command accelerator", keys.CmdOrCtrl("o"), nil),
					menu.Text("Option accelerator", keys.OptionOrAlt("o"), nil),
					menu.Text("Combo accelerator", keys.Combo("o", keys.CmdOrCtrlKey, keys.ShiftKey), nil),
				)),
				menu.SubMenu("System Keys", menu.NewMenuFromItems(
					menu.Text("Backspace", keys.Key("Backspace"), nil),
					menu.Text("Tab", keys.Key("Tab"), nil),
					menu.Text("Return", keys.Key("Return"), nil),
					menu.Text("Escape", keys.Key("Escape"), nil),
					menu.Text("Left", keys.Key("Left"), nil),
					menu.Text("Right", keys.Key("Right"), nil),
					menu.Text("Up", keys.Key("Up"), nil),
					menu.Text("Down", keys.Key("Down"), nil),
					menu.Text("Space", keys.Key("Space"), nil),
					menu.Text("Delete", keys.Key("Delete"), nil),
					menu.Text("Home", keys.Key("Home"), nil),
					menu.Text("End", keys.Key("End"), nil),
					menu.Text("Page Up", keys.Key("Page Up"), nil),
					menu.Text("Page Down", keys.Key("Page Down"), nil),
					menu.Text("NumLock", keys.Key("NumLock"), nil),
				)),
				menu.SubMenu("Function Keys", menu.NewMenuFromItems(
					menu.Text("F1", keys.Key("F1"), nil),
					menu.Text("F2", keys.Key("F2"), nil),
					menu.Text("F3", keys.Key("F3"), nil),
					menu.Text("F4", keys.Key("F4"), nil),
					menu.Text("F5", keys.Key("F5"), nil),
					menu.Text("F6", keys.Key("F6"), nil),
					menu.Text("F7", keys.Key("F7"), nil),
					menu.Text("F8", keys.Key("F8"), nil),
					menu.Text("F9", keys.Key("F9"), nil),
					menu.Text("F10", keys.Key("F10"), nil),
					menu.Text("F11", keys.Key("F11"), nil),
					menu.Text("F12", keys.Key("F12"), nil),
					menu.Text("F13", keys.Key("F13"), nil),
					menu.Text("F14", keys.Key("F14"), nil),
					menu.Text("F15", keys.Key("F15"), nil),
					menu.Text("F16", keys.Key("F16"), nil),
					menu.Text("F17", keys.Key("F17"), nil),
					menu.Text("F18", keys.Key("F18"), nil),
					menu.Text("F19", keys.Key("F19"), nil),
					menu.Text("F20", keys.Key("F20"), nil),
				)),
				menu.SubMenu("Standard Keys", menu.NewMenuFromItems(
					menu.Text("Backtick", keys.Key("`"), nil),
					menu.Text("Plus", keys.Key("+"), nil),
				)),
			)),
			m.dynamicMenuOneSubmenu,
			&menu.MenuItem{
				Label:       "Disabled Menu",
				Type:        menu.TextType,
				Accelerator: keys.Combo("p", keys.CmdOrCtrlKey, keys.ShiftKey),
				Disabled:    true,
			},
			&menu.MenuItem{
				Label:  "Hidden Menu",
				Type:   menu.TextType,
				Hidden: true,
			},
			&menu.MenuItem{
				Label:       "Checkbox Menu 1",
				Type:        menu.CheckboxType,
				Accelerator: keys.CmdOrCtrl("l"),
				Checked:     true,
				Click: func(data *menu.CallbackData) {
					fmt.Printf("The '%s' menu was clicked\n", data.MenuItem.Label)
					fmt.Printf("It is now %v\n", data.MenuItem.Checked)
				},
			},
			menu.Checkbox("Checkbox Menu 2", false, nil, nil),
			menu.Separator(),
			menu.Radio("😀 Option 1", true, nil, nil),
			menu.Radio("😺 Option 2", false, nil, nil),
			menu.Radio("❤️ Option 3", false, nil, nil),
		)),
	))

	myMenu.Append(windowMenu)
	return myMenu
}
